#include <cstdint>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iterator>
#include <cassert>
#include <climits>

// -----------------------------------------------------------------------------
// RV32IMAC simulator with full trace output
// Supports: I (base), M (multiply/divide), A (atomic), C (compressed)
// -----------------------------------------------------------------------------

struct CPU {
  // ─── Core state ────────────────────────────────────────────────────────────
  uint32_t pc = 0;
  uint32_t x[32]{};          // integer registers
  uint64_t cycles = 0;
  std::vector<uint8_t> mem;  // flat little-endian memory
  
  // Atomic extension state
  bool has_reservation = false;
  uint32_t reservation_addr = 0;
  
  explicit CPU(size_t mem_size) : mem(mem_size) {}

  // ─── Memory access helpers ─────────────────────────────────────────────────
  uint32_t fetch32(uint32_t addr) const {
    assert(addr + 3 < mem.size());
    return mem[addr] | mem[addr+1]<<8 | mem[addr+2]<<16 | mem[addr+3]<<24;
  }
  
  uint16_t fetch16(uint32_t addr) const {
    assert(addr + 1 < mem.size());
    return mem[addr] | mem[addr+1]<<8;
  }

  void store32(uint32_t addr, uint32_t v) {
    assert(addr + 3 < mem.size());
    mem[addr]   = v & 0xff;
    mem[addr+1] = v >> 8;
    mem[addr+2] = v >> 16;
    mem[addr+3] = v >> 24;
  }

  static uint32_t sx(uint32_t v, int bits) {  // sign-extend *bits*
    uint32_t m = 1u << (bits - 1);
    return (v ^ m) - m;
  }

  // ─── Pretty-printing helpers ───────────────────────────────────────────────
  std::string regs_str() const {
    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (int i = 0; i < 32; ++i) {
      os << "x" << std::setw(2) <<std::dec << i
         << ":0x" << std::setw(8) << std::hex << x[i]
         << ((i % 8 == 7) ? "\n" : "  ");
    }
    return os.str();
  }

  std::string disasm(uint32_t ins, bool compressed) const {
    if (compressed) {
      return disasm_compressed(ins);
    }
    
    uint32_t opc = ins & 0x7f;
    uint32_t rd  = (ins >> 7) & 0x1f;
    uint32_t f3  = (ins >> 12) & 7;
    uint32_t rs1 = (ins >> 15) & 0x1f;
    uint32_t rs2 = (ins >> 20) & 0x1f;
    uint32_t f7  = ins >> 25;

    auto imm_i = [&]{ return sx(ins>>20,12); };
    auto imm_u = [&]{ return ins & 0xfffff000u; };
    auto imm_s = [&]{ return sx(((ins>>7)&0x1f)|((ins>>20)&0xfe0),12); };
    auto imm_b = [&]{
      uint32_t v = ((ins>>7)&0x1e)|((ins>>20)&0x7e0)|((ins<<4)&0x800)|((ins>>19)&0x1000);
      return sx(v,13);
    };
    auto imm_j = [&]{
      uint32_t v = ((ins>>21)&0x3ff)<<1;
      v |= ((ins>>20)&1)<<11;
      v |= ((ins>>12)&0xff)<<12;
      v |= (ins>>31)<<20;
      return sx(v,21);
    };

    std::ostringstream os;
    os << std::dec;
    switch (opc) {
    case 0x37: os << "lui  x"<<rd<<",0x"<<std::hex<<imm_u(); break;
    case 0x17: os << "auipc x"<<rd<<",0x"<<std::hex<<imm_u(); break;
    case 0x6f: os << "jal  x"<<rd<<","<<imm_j(); break;
    case 0x67: os << "jalr x"<<rd<<",x"<<rs1<<","<<imm_i(); break;
    case 0x63:{ const char* m[] = {"beq","bne","","","blt","bge","bltu","bgeu"};
      os << m[f3] << " x"<<rs1<<",x"<<rs2<<","<<imm_b();
    } break;
    case 0x03:{ const char* m[] = {"lb","lh","lw","","lbu","lhu"};
      os << m[f3] << " x"<<rd<<","<<imm_i()<<"(x"<<rs1<<")"; } break;
    case 0x23:{ const char* m[] = {"sb","sh","sw"};
      os << m[f3] << " x"<<rs2<<","<<imm_s()<<"(x"<<rs1<<")"; } break;
    case 0x13:{ const char* m[] = {"addi","","slti","sltiu","xori","","ori","andi"};
      if(f3==1) os<<"slli"; else if(f3==5) os<<( (ins>>30)?"srai":"srli" ); else os<<m[f3];
      if(f3==1||f3==5) os << " x"<<rd<<",x"<<rs1<<","<<(imm_i() & 31);
      else os << " x"<<rd<<",x"<<rs1<<","<<imm_i();
    } break;
    case 0x33:{
      if(f7==1) { // M extension
        const char* m[] = {"mul","mulh","mulhsu","mulhu","div","divu","rem","remu"};
        os<<m[f3];
      } else { // Base RV32I
        const char* m[] = {"add","sll","slt","sltu","xor","srl","or","and"};
        if(f3==0 && f7) os<<"sub"; else if(f3==5 && f7) os<<"sra"; else os<<m[f3];
      }
      os << " x"<<rd<<",x"<<rs1<<",x"<<rs2; } break;
    case 0x2f: { // A extension
      if (f3 == 2) {
        uint32_t f5 = ins >> 27;
        if (f5 == 0x00) os << "amoadd.w";
        else if (f5 == 0x01) os << "amoswap.w";
        else if (f5 == 0x02) os << "lr.w";
        else if (f5 == 0x03) os << "sc.w";
        else if (f5 == 0x04) os << "amoxor.w";
        else if (f5 == 0x08) os << "amoor.w";
        else if (f5 == 0x0c) os << "amoand.w";
        else if (f5 == 0x10) os << "amomin.w";
        else if (f5 == 0x14) os << "amomax.w";
        else if (f5 == 0x18) os << "amominu.w";
        else if (f5 == 0x1c) os << "amomaxu.w";
        else os << "amo.unknown";
        os << " x"<<rd<<",x"<<rs2<<",(x"<<rs1<<")";
      }
    } break;
    case 0x0f: {
      if (f3 == 0) os << "fence";
      else if (f3 == 1) os << "fence.i";
      else os << "fence.unknown";
    } break;
    case 0x73: os << "ecall"; break;
    default:   os << "illegal"; break;
    }
    return os.str();
  }

  std::string disasm_compressed(uint16_t ins) const {
    std::ostringstream os;
    uint32_t op = ins & 3;
    uint32_t funct3 = (ins >> 13) & 7;
    
    switch (op) {
      case 0: // C0 quadrant
        switch (funct3) {
          case 0: os << "c.addi4spn"; break;
          case 2: os << "c.lw"; break;
          case 6: os << "c.sw"; break;
          default: os << "c.illegal"; break;
        }
        break;
      case 1: // C1 quadrant
        switch (funct3) {
          case 0: os << "c.addi/c.nop"; break;
          case 1: os << "c.jal"; break;
          case 2: os << "c.li"; break;
          case 3: os << "c.lui/c.addi16sp"; break;
          case 4: os << "c.arith"; break;
          case 5: os << "c.j"; break;
          case 6: os << "c.beqz"; break;
          case 7: os << "c.bnez"; break;
        }
        break;
      case 2: // C2 quadrant
        switch (funct3) {
          case 0: os << "c.slli"; break;
          case 2: os << "c.lwsp"; break;
          case 4: {
            if ((ins >> 12) & 1) {
              if (((ins >> 2) & 0x1f) == 0) os << "c.jalr";
              else os << "c.add";
            } else {
              if (((ins >> 2) & 0x1f) == 0) os << "c.jr";
              else os << "c.mv";
            }
          } break;
          case 6: os << "c.swsp"; break;
          default: os << "c.illegal"; break;
        }
        break;
      default: os << "illegal"; break;
    }
    return os.str();
  }

  // ─── Compressed instruction expansion ─────────────────────────────────────
  uint32_t expand_compressed(uint16_t ins) {
    uint32_t op = ins & 3;
    uint32_t funct3 = (ins >> 13) & 7;
    
    // Common register mappings
    uint32_t rd_prime = ((ins >> 2) & 7) + 8;  // x8-x15
    uint32_t rs1_prime = ((ins >> 7) & 7) + 8;
    uint32_t rs2_prime = rd_prime;
    uint32_t rd = (ins >> 7) & 0x1f;
    uint32_t rs1 = rd;
    uint32_t rs2 = (ins >> 2) & 0x1f;
    
    switch (op) {
      case 0: // C0 quadrant
        switch (funct3) {
          case 0: { // C.ADDI4SPN -> addi rd', x2, nzuimm
            uint32_t nzuimm = ((ins >> 6) & 2) | ((ins >> 7) & 0x30) |
                             ((ins >> 1) & 0x3c0) | ((ins >> 4) & 0xc0);
            if (nzuimm == 0) return 0; // Reserved
            return 0x00010113 | (rd_prime << 7) | (nzuimm << 20);
          }
          case 2: { // C.LW -> lw rd', offset(rs1')
            uint32_t offset = ((ins >> 6) & 4) | ((ins >> 10) & 0x18) | ((ins << 1) & 0x40);
            return 0x00002003 | (rd_prime << 7) | (rs1_prime << 15) | (offset << 20);
          }
          case 6: { // C.SW -> sw rs2', offset(rs1')
            uint32_t offset = ((ins >> 6) & 4) | ((ins >> 10) & 0x18) | ((ins << 1) & 0x40);
            uint32_t imm_11_5 = (offset >> 5) & 0x7f;
            uint32_t imm_4_0 = offset & 0x1f;
            return 0x00002023 | (imm_4_0 << 7) | (rs1_prime << 15) | (rs2_prime << 20) | (imm_11_5 << 25);
          }
        }
        break;
        
      case 1: // C1 quadrant
        switch (funct3) {
          case 0: { // C.ADDI/C.NOP -> addi rd, rd, nzimm
            int32_t nzimm = ((ins >> 2) & 0x1f) | ((int32_t)(ins << 25) >> 27);
            if (ins & 0x1000) nzimm |= 0xffffffe0; // sign extend
            if (rd == 0 && nzimm == 0) return 0x00000013; // NOP
            return 0x00000013 | (rd << 7) | (rd << 15) | ((nzimm & 0xfff) << 20);
          }
          case 1: { // C.JAL -> jal x1, offset
            int32_t offset = ((ins >> 3) & 0xe) | ((ins >> 7) & 0x10) | ((ins << 3) & 0x20) |
                            ((ins >> 1) & 0x40) | ((ins << 1) & 0x80) | ((ins >> 2) & 0x300) |
                            ((ins << 9) & 0x400) | ((int32_t)(ins << 20) >> 21);
            // Encode JAL immediate: imm[20|10:1|11|19:12]
            uint32_t imm20 = (offset >> 20) & 1;
            uint32_t imm10_1 = (offset >> 1) & 0x3ff;
            uint32_t imm11 = (offset >> 11) & 1;
            uint32_t imm19_12 = (offset >> 12) & 0xff;
            uint32_t jal_imm = (imm20 << 31) | (imm10_1 << 21) | (imm11 << 20) | (imm19_12 << 12);
            return 0x0000006f | (1 << 7) | jal_imm;
          }
          case 2: { // C.LI -> addi rd, x0, imm
            int32_t imm = ((ins >> 2) & 0x1f) | ((int32_t)(ins << 25) >> 27);
            if (ins & 0x1000) imm |= 0xffffffe0; // sign extend
            return 0x00000013 | (rd << 7) | ((imm & 0xfff) << 20);
          }
          case 3: { // C.LUI/C.ADDI16SP
            if (rd == 2) { // C.ADDI16SP -> addi x2, x2, nzimm
              int32_t nzimm = ((ins >> 3) & 0x20) | ((ins >> 1) & 0x180) | 
                             ((ins << 4) & 0x40) | ((ins << 1) & 0x10) | ((int32_t)(ins << 19) >> 23);
              if (nzimm == 0) return 0; // Reserved
              return 0x00010113 | ((nzimm & 0xfff) << 20);
            } else { // C.LUI -> lui rd, nzuimm
              int32_t nzuimm = ((int32_t)(ins << 14) >> 14);
              if (nzuimm == 0 || rd == 0) return 0; // Reserved
              return 0x00000037 | (rd << 7) | ((nzuimm & 0xfffff) << 12);
            }
          }
          case 4: { // ALU ops
            uint32_t funct2 = (ins >> 10) & 3;
            switch (funct2) {
              case 0: { // C.SRLI -> srli rd', rd', shamt
                uint32_t shamt = ((ins >> 2) & 0x1f) | ((ins >> 7) & 0x20);
                return 0x00005013 | (rd_prime << 7) | (rd_prime << 15) | (shamt << 20);
              }
              case 1: { // C.SRAI -> srai rd', rd', shamt
                uint32_t shamt = ((ins >> 2) & 0x1f) | ((ins >> 7) & 0x20);
                return 0x40005013 | (rd_prime << 7) | (rd_prime << 15) | (shamt << 20);
              }
              case 2: { // C.ANDI -> andi rd', rd', imm
                int32_t imm = ((int32_t)(ins << 19) >> 26) | ((ins >> 7) & 0x20);
                return 0x00007013 | (rd_prime << 7) | (rd_prime << 15) | ((imm & 0xfff) << 20);
              }
              case 3: { // Register-register ops
                uint32_t funct1 = (ins >> 12) & 1;
                uint32_t funct2_2 = (ins >> 5) & 3;
                if (funct1 == 0) {
                  switch (funct2_2) {
                    case 0: // C.SUB -> sub rd', rd', rs2'
                      return 0x40000033 | (rd_prime << 7) | (rd_prime << 15) | (rs2_prime << 20);
                    case 1: // C.XOR -> xor rd', rd', rs2'
                      return 0x00004033 | (rd_prime << 7) | (rd_prime << 15) | (rs2_prime << 20);
                    case 2: // C.OR -> or rd', rd', rs2'
                      return 0x00006033 | (rd_prime << 7) | (rd_prime << 15) | (rs2_prime << 20);
                    case 3: // C.AND -> and rd', rd', rs2'
                      return 0x00007033 | (rd_prime << 7) | (rd_prime << 15) | (rs2_prime << 20);
                  }
                }
              }
            }
            break;
          }
          case 5: { // C.J -> jal x0, offset
            int32_t offset = ((ins >> 3) & 0xe) | ((ins >> 7) & 0x10) | ((ins << 3) & 0x20) |
                            ((ins >> 1) & 0x40) | ((ins << 1) & 0x80) | ((ins >> 2) & 0x300) |
                            ((ins << 9) & 0x400) | ((int32_t)(ins << 20) >> 21);
            // Encode JAL immediate: imm[20|10:1|11|19:12]
            uint32_t imm20 = (offset >> 20) & 1;
            uint32_t imm10_1 = (offset >> 1) & 0x3ff;
            uint32_t imm11 = (offset >> 11) & 1;
            uint32_t imm19_12 = (offset >> 12) & 0xff;
            uint32_t jal_imm = (imm20 << 31) | (imm10_1 << 21) | (imm11 << 20) | (imm19_12 << 12);
            return 0x0000006f | jal_imm;
          }
          case 6: { // C.BEQZ -> beq rs1', x0, offset
            int32_t offset = ((ins >> 3) & 6) | ((ins << 3) & 0x18) | ((ins << 1) & 0x60) |
                            ((ins >> 2) & 0x180) | ((int32_t)(ins << 19) >> 23);
            uint32_t imm_12_10_5 = (offset >> 5) & 0x1fff;
            uint32_t imm_4_1_11 = (offset & 0x1f) | ((offset & 0x800) >> 5);
            return 0x00000063 | (imm_4_1_11 << 7) | (rs1_prime << 15) | (imm_12_10_5 << 20);
          }
          case 7: { // C.BNEZ -> bne rs1', x0, offset
            int32_t offset = ((ins >> 3) & 6) | ((ins << 3) & 0x18) | ((ins << 1) & 0x60) |
                            ((ins >> 2) & 0x180) | ((int32_t)(ins << 19) >> 23);
            uint32_t imm_12_10_5 = (offset >> 5) & 0x1fff;
            uint32_t imm_4_1_11 = (offset & 0x1f) | ((offset & 0x800) >> 5);
            return 0x00001063 | (imm_4_1_11 << 7) | (rs1_prime << 15) | (imm_12_10_5 << 20);
          }
        }
        break;
        
      case 2: // C2 quadrant
        switch (funct3) {
          case 0: { // C.SLLI -> slli rd, rd, shamt
            uint32_t shamt = ((ins >> 2) & 0x1f) | ((ins >> 7) & 0x20);
            return 0x00001013 | (rd << 7) | (rd << 15) | (shamt << 20);
          }
          case 2: { // C.LWSP -> lw rd, offset(x2)
            uint32_t offset = ((ins >> 4) & 0x1c) | ((ins << 4) & 0xc0) | ((ins >> 7) & 0x20);
            return 0x00002003 | (rd << 7) | (2 << 15) | (offset << 20);
          }
          case 4: {
            uint32_t bit12 = (ins >> 12) & 1;
            if (bit12 == 0) {
              if (rs2 == 0) { // C.JR -> jalr x0, 0(rs1)
                return 0x00000067 | (rs1 << 15);
              } else { // C.MV -> add rd, x0, rs2
                return 0x00000033 | (rd << 7) | (rs2 << 20);
              }
            } else {
              if (rd == 0 && rs2 == 0) return 0x00100073; // C.EBREAK -> ebreak
              if (rs2 == 0) { // C.JALR -> jalr x1, 0(rs1)
                return 0x00000067 | (1 << 7) | (rs1 << 15);
              } else { // C.ADD -> add rd, rd, rs2
                return 0x00000033 | (rd << 7) | (rd << 15) | (rs2 << 20);
              }
            }
          }
          case 6: { // C.SWSP -> sw rs2, offset(x2)
            uint32_t offset = ((ins >> 9) & 0x3c) | ((ins >> 7) & 0xc0);
            uint32_t imm_11_5 = (offset >> 5) & 0x7f;
            uint32_t imm_4_0 = offset & 0x1f;
            return 0x00002023 | (imm_4_0 << 7) | (2 << 15) | (rs2 << 20) | (imm_11_5 << 25);
          }
        }
        break;
    }
    return 0; // Invalid compressed instruction
  }

  // ─── Main execution (one instruction) ──────────────────────────────────────
  void step() {
    // Fetch instruction
    uint16_t ins16 = fetch16(pc);
    bool compressed = (ins16 & 3) != 3;
    uint32_t ins;
    
    if (compressed) {
      ins = ins16;
    } else {
      ins = fetch32(pc);
    }

    // Save PC for trace output
    uint32_t trace_pc = pc;
    uint32_t trace_ins = ins;
    
    // Expand compressed instruction if needed
    uint32_t expanded_ins = ins;
    if (compressed) {
      expanded_ins = expand_compressed(ins16);
      if (expanded_ins == 0) {
        std::cerr << "Invalid compressed instruction 0x" << std::hex << ins16 
                  << " at PC 0x" << pc << "\n";
        std::exit(1);
      }
    }

    // Decode fields
    uint32_t opc = expanded_ins & 0x7f;
    uint32_t rd  = (expanded_ins >> 7) & 0x1f;
    uint32_t f3  = (expanded_ins >> 12) & 7;
    uint32_t rs1 = (expanded_ins >> 15) & 0x1f;
    uint32_t rs2 = (expanded_ins >> 20) & 0x1f;
    uint32_t f7  = expanded_ins >> 25;

    auto imm_i = [&]{ return sx(expanded_ins>>20,12); };
    auto imm_u = [&]{ return expanded_ins & 0xfffff000u; };
    auto imm_s = [&]{ return sx(((expanded_ins>>7)&0x1f)|((expanded_ins>>20)&0xfe0),12); };
    auto imm_b = [&]{
      uint32_t v = ((expanded_ins>>7)&0x1e)|((expanded_ins>>20)&0x7e0)|((expanded_ins<<4)&0x800)|((expanded_ins>>19)&0x1000);
      return sx(v,13);
    };
    auto imm_j = [&]{
      uint32_t v = ((expanded_ins>>21)&0x3ff)<<1;
      v |= ((expanded_ins>>20)&1)<<11;
      v |= ((expanded_ins>>12)&0xff)<<12;
      v |= (expanded_ins>>31)<<20;
      return sx(v,21);
    };

    // Update PC
    uint32_t next_pc = pc + (compressed ? 2 : 4);

    // Execute
    switch (opc) {
    case 0x37: x[rd] = imm_u();           pc = next_pc; break;          // LUI
    case 0x17: x[rd] = pc + imm_u();      pc = next_pc; break;          // AUIPC
    case 0x6f: { uint32_t t = next_pc; pc += imm_j(); if(rd) x[rd] = t; } break; // JAL
    case 0x67: { uint32_t t = next_pc; pc = (x[rs1] + imm_i()) & ~1u; if(rd) x[rd] = t; } break; // JALR
    case 0x63: {
      bool take = false;
      switch (f3) {
        case 0: take = x[rs1] == x[rs2]; break; // BEQ
        case 1: take = x[rs1] != x[rs2]; break; // BNE
        case 4: take = (int32_t)x[rs1] <  (int32_t)x[rs2]; break; // BLT
        case 5: take = (int32_t)x[rs1] >= (int32_t)x[rs2]; break; // BGE
        case 6: take = x[rs1] <  x[rs2]; break; // BLTU
        case 7: take = x[rs1] >= x[rs2]; break; // BGEU
      }
      pc = take ? pc + imm_b() : next_pc;
    } break;
    case 0x03: {
      uint32_t addr = x[rs1] + imm_i();
      switch (f3) {
        case 0: x[rd] = (int8_t) mem[addr]; break;                       // LB
        case 1: x[rd] = (int16_t)(mem[addr] | mem[addr+1]<<8); break;    // LH
        case 2: x[rd] = fetch32(addr); break;                            // LW
        case 4: x[rd] = mem[addr]; break;                                // LBU
        case 5: x[rd] = mem[addr] | mem[addr+1]<<8; break;               // LHU
      }
      pc = next_pc;
    } break;
    case 0x23: {
      uint32_t addr = x[rs1] + imm_s();
      switch (f3) {
        case 0: mem[addr] = x[rs2]; break;                               // SB
        case 1: mem[addr] = x[rs2]; mem[addr+1] = x[rs2] >> 8; break;    // SH
        case 2: store32(addr, x[rs2]); break;                            // SW
      }
      pc = next_pc;
    } break;
    case 0x13: {
      uint32_t imm = imm_i();
      switch (f3) {
        case 0: x[rd] = x[rs1] + imm; break;                             // ADDI
        case 2: x[rd] = (int32_t)x[rs1] <  (int32_t)imm; break;          // SLTI
        case 3: x[rd] = x[rs1] <  imm; break;                            // SLTIU
        case 4: x[rd] = x[rs1] ^ imm; break;                             // XORI
        case 6: x[rd] = x[rs1] | imm; break;                             // ORI
        case 7: x[rd] = x[rs1] & imm; break;                             // ANDI
        case 1: x[rd] = x[rs1] << (imm & 0x1f); break;                   // SLLI
        case 5: x[rd] = (imm>>10) ? (int32_t)x[rs1] >> (imm & 0x1f)
                                  : x[rs1] >> (imm & 0x1f); break;      // SRLI/SRAI
      }
      pc = next_pc;
    } break;
    case 0x33: {
      if (f7 == 1) {  // M extension instructions
        switch (f3) {
          case 0: x[rd] = x[rs1] * x[rs2]; break;                        // MUL
          case 1: {                                                       // MULH
            int64_t a = (int32_t)x[rs1];
            int64_t b = (int32_t)x[rs2];
            x[rd] = (a * b) >> 32;
          } break;
          case 2: {                                                       // MULHSU
            int64_t a = (int32_t)x[rs1];
            uint64_t b = x[rs2];
            x[rd] = (a * b) >> 32;
          } break;
          case 3: {                                                       // MULHU
            uint64_t a = x[rs1];
            uint64_t b = x[rs2];
            x[rd] = (a * b) >> 32;
          } break;
          case 4: {                                                       // DIV
            int32_t a = x[rs1];
            int32_t b = x[rs2];
            if (b == 0) x[rd] = -1;
            else if (a == INT32_MIN && b == -1) x[rd] = INT32_MIN;
            else x[rd] = a / b;
          } break;
          case 5: {                                                       // DIVU
            if (x[rs2] == 0) x[rd] = 0xffffffff;
            else x[rd] = x[rs1] / x[rs2];
          } break;
          case 6: {                                                       // REM
            int32_t a = x[rs1];
            int32_t b = x[rs2];
            if (b == 0) x[rd] = a;
            else if (a == INT32_MIN && b == -1) x[rd] = 0;
            else x[rd] = a % b;
          } break;
          case 7: {                                                       // REMU
            if (x[rs2] == 0) x[rd] = x[rs1];
            else x[rd] = x[rs1] % x[rs2];
          } break;
        }
      } else {  // Base RV32I instructions
        switch (f3) {
          case 0: x[rd] = f7 ? x[rs1] - x[rs2] : x[rs1] + x[rs2]; break; // SUB/ADD
          case 1: x[rd] = x[rs1] << (x[rs2] & 0x1f); break;              // SLL
          case 2: x[rd] = (int32_t)x[rs1] <  (int32_t)x[rs2]; break;     // SLT
          case 3: x[rd] = x[rs1] <  x[rs2]; break;                       // SLTU
          case 4: x[rd] = x[rs1] ^ x[rs2]; break;                        // XOR
          case 5: x[rd] = f7 ? (int32_t)x[rs1] >> (x[rs2] & 0x1f)
                              : x[rs1] >> (x[rs2] & 0x1f); break;        // SRA/SRL
          case 6: x[rd] = x[rs1] | x[rs2]; break;                        // OR
          case 7: x[rd] = x[rs1] & x[rs2]; break;                        // AND
        }
      }
      pc = next_pc;
    } break;
    case 0x2f: { // A extension (atomic operations)
      if (f3 == 2) { // Word atomics
        uint32_t addr = x[rs1];
        uint32_t f5 = expanded_ins >> 27;
        switch (f5) {
          case 0x02: { // LR.W
            x[rd] = fetch32(addr);
            reservation_addr = addr;
            has_reservation = true;
          } break;
          case 0x03: { // SC.W
            if (has_reservation && reservation_addr == addr) {
              store32(addr, x[rs2]);
              x[rd] = 0; // success
              has_reservation = false;
            } else {
              x[rd] = 1; // failure
            }
          } break;
          case 0x01: { // AMOSWAP.W
            uint32_t old = fetch32(addr);
            store32(addr, x[rs2]);
            x[rd] = old;
          } break;
          case 0x00: { // AMOADD.W
            uint32_t old = fetch32(addr);
            store32(addr, old + x[rs2]);
            x[rd] = old;
          } break;
          case 0x04: { // AMOXOR.W
            uint32_t old = fetch32(addr);
            store32(addr, old ^ x[rs2]);
            x[rd] = old;
          } break;
          case 0x0c: { // AMOAND.W
            uint32_t old = fetch32(addr);
            store32(addr, old & x[rs2]);
            x[rd] = old;
          } break;
          case 0x08: { // AMOOR.W
            uint32_t old = fetch32(addr);
            store32(addr, old | x[rs2]);
            x[rd] = old;
          } break;
          case 0x10: { // AMOMIN.W
            int32_t old = fetch32(addr);
            int32_t val = x[rs2];
            store32(addr, (old < val) ? old : val);
            x[rd] = old;
          } break;
          case 0x14: { // AMOMAX.W
            int32_t old = fetch32(addr);
            int32_t val = x[rs2];
            store32(addr, (old > val) ? old : val);
            x[rd] = old;
          } break;
          case 0x18: { // AMOMINU.W
            uint32_t old = fetch32(addr);
            uint32_t val = x[rs2];
            store32(addr, (old < val) ? old : val);
            x[rd] = old;
          } break;
          case 0x1c: { // AMOMAXU.W
            uint32_t old = fetch32(addr);
            uint32_t val = x[rs2];
            store32(addr, (old > val) ? old : val);
            x[rd] = old;
          } break;
        }
      }
      pc = next_pc;
    } break;
    case 0x0f: { // FENCE instructions
      if (f3 == 1) {
        // FENCE.I - flush instruction cache (no-op for us)
      }
      pc = next_pc;
    } break;
    case 0x73: {
      if (f3 == 0 && rs1 == 0 && rd == 0) {
        if ((expanded_ins >> 20) == 1) {
          // EBREAK
          std::cout << "\nEBREAK at cycle " << cycles << "\n";
        } else {
          // ECALL
          std::cout << "\nECALL reached at cycle " << cycles << "\n";
        }
        std::exit(0);
      }
      pc = next_pc;
    } break;
    default: 
      std::cerr << "Unknown opcode 0x" << std::hex << (int)opc << "\n"; 
      std::exit(1);
    }

    x[0] = 0;   // x0 is hard-wired to zero

    // ─── Trace printout ────────────────────────────────────────────────────
    std::cout << "\n[cycle " << std::dec << cycles << "] pc=0x"
              << std::hex << std::setw(8) << std::setfill('0') << trace_pc
              << " ins=0x" << std::setw(compressed ? 4 : 8) << trace_ins 
              << "  " << disasm(trace_ins, compressed)
              << "\n" << regs_str();

    ++cycles;
  }
};

// -----------------------------------------------------------------------------
// Driver
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " program.bin\n";
    return 1;
  }
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<uint8_t> bin((std::istreambuf_iterator<char>(f)), {});

  CPU cpu(1 << 20);                     // 1 MiB of RAM
  std::copy(bin.begin(), bin.end(), cpu.mem.begin());

  while (true) cpu.step();              // run forever (ECALL exits)
}