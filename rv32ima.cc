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
// RV32IMA simulator with full trace output
// Supports: I (base), M (multiply/divide), A (atomic)
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
  
  // CSR (Control and Status Register) state
  uint32_t csr[4096]{};  // CSR address space
  
  // Trace mode
  bool trace_enabled = false;
  
  explicit CPU(size_t mem_size, bool trace = false) : mem(mem_size), trace_enabled(trace) {}

  // ─── Memory access helpers ─────────────────────────────────────────────────
  uint32_t fetch32(uint32_t addr) const {
    if (addr + 3 >= mem.size()) {
      std::cerr << "fetch32: addr=0x" << std::hex << addr << " mem.size=0x" << mem.size() << std::endl;
      return 0;
    }
    return mem[addr] | mem[addr+1]<<8 | mem[addr+2]<<16 | mem[addr+3]<<24;
  }

  void store32(uint32_t addr, uint32_t v) {
    assert(addr + 3 < mem.size());
    mem[addr]   = v;
    mem[addr+1] = v >> 8;
    mem[addr+2] = v >> 16;
    mem[addr+3] = v >> 24;
  }
  
  // ─── Syscall handling ─────────────────────────────────────────────────────
  void handle_syscall() {
    uint32_t syscall_num = x[17];  // a7
    
    switch (syscall_num) {
      case 93: {  // Exit
        uint32_t exit_code = x[10];  // a0
        if (trace_enabled) {
          std::cout << "Program exited with code " << exit_code << std::endl;
        }
        exit(exit_code);
      }
      case 64: {  // Write
        uint32_t fd = x[10];     // a0
        uint32_t buf = x[11];    // a1
        uint32_t count = x[12];  // a2
        
        if (fd == 1 || fd == 2) {  // stdout or stderr
          for (uint32_t i = 0; i < count && buf + i < mem.size(); i++) {
            std::cout << (char)mem[buf + i];
          }
          std::cout.flush();
          x[10] = count;  // return number of bytes written
        } else {
          x[10] = -1;  // error
        }
        break;
      }
      default:
        if (trace_enabled) {
          std::cerr << "Unhandled syscall: " << syscall_num << std::endl;
        }
        x[10] = -1;  // error
    }
  }
  
  // ─── CSR (Control and Status Register) operations ────────────────────────
  uint32_t read_csr(uint32_t addr) {
    // Special handling for certain CSRs
    switch (addr) {
      case 0xC00:  // cycle (lower 32 bits of cycle counter)
      case 0xB00:  // mcycle
        return cycles & 0xFFFFFFFF;
      case 0xC80:  // cycleh (upper 32 bits of cycle counter)
      case 0xB80:  // mcycleh
        return (cycles >> 32) & 0xFFFFFFFF;
      case 0xC01:  // time (lower 32 bits)
      case 0xC81:  // timeh (upper 32 bits)
        // For simplicity, return cycle count as time
        return (addr == 0xC01) ? (cycles & 0xFFFFFFFF) : ((cycles >> 32) & 0xFFFFFFFF);
      case 0xC02:  // instret (instructions retired, lower 32 bits)
      case 0xB02:  // minstret
        return cycles & 0xFFFFFFFF;  // For simplicity, assume 1 instruction per cycle
      case 0xC82:  // instreth (upper 32 bits)
      case 0xB82:  // minstreth
        return (cycles >> 32) & 0xFFFFFFFF;
      default:
        return csr[addr];
    }
  }
  
  void write_csr(uint32_t addr, uint32_t value) {
    // Some CSRs are read-only
    switch (addr) {
      case 0xC00:  // cycle (read-only)
      case 0xC80:  // cycleh (read-only)
      case 0xC01:  // time (read-only)
      case 0xC81:  // timeh (read-only)
      case 0xC02:  // instret (read-only)
      case 0xC82:  // instreth (read-only)
        // Silently ignore writes to read-only CSRs
        break;
      default:
        csr[addr] = value;
        break;
    }
  }

  // ─── Instruction decoding helpers ──────────────────────────────────────────
  std::string decode_ins(uint32_t ins) {
    std::stringstream ss;
    
    uint32_t opc = ins & 0x7f;
    uint32_t rd  = (ins >> 7) & 0x1f;
    uint32_t f3  = (ins >> 12) & 0x7;
    uint32_t rs1 = (ins >> 15) & 0x1f;
    uint32_t rs2 = (ins >> 20) & 0x1f;
    uint32_t f7  = ins >> 25;

    auto sx = [](uint32_t v, unsigned bits) {
      uint32_t sign = 1u << (bits - 1);
      return int32_t((v ^ sign) - sign);
    };

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

    switch (opc) {
      case 0x37: ss << "lui  x" << rd << ",0x" << std::hex << imm_u(); break;
      case 0x17: ss << "auipc x" << rd << ",0x" << std::hex << imm_u(); break;
      case 0x6f: ss << "jal  x" << rd << "," << std::dec << imm_j(); break;
      case 0x67: ss << "jalr x" << rd << ",x" << rs1 << "," << imm_i(); break;
      case 0x63: {
        const char* names[] = {"beq","bne","?","?","blt","bge","bltu","bgeu"};
        ss << names[f3] << " x" << rs1 << ",x" << rs2 << "," << imm_b();
        break;
      }
      case 0x03: {
        const char* names[] = {"lb","lh","lw","?","lbu","lhu"};
        ss << names[f3] << " x" << rd << "," << imm_i() << "(x" << rs1 << ")";
        break;
      }
      case 0x23: {
        const char* names[] = {"sb","sh","sw"};
        ss << names[f3] << " x" << rs2 << "," << imm_s() << "(x" << rs1 << ")";
        break;
      }
      case 0x13: {
        if (f3 == 1 || f3 == 5) {
          uint32_t shamt = rs2;
          ss << ((f3 == 1) ? "slli" : (f7 ? "srai" : "srli")) << " x" << rd << ",x" << rs1 << "," << shamt;
        } else {
          const char* names[] = {"addi","?","slti","sltiu","xori","?","ori","andi"};
          ss << names[f3] << " x" << rd << ",x" << rs1 << "," << imm_i();
        }
        break;
      }
      case 0x33: {
        if (f7 == 1) { // M extension
          const char* names[] = {"mul","mulh","mulhsu","mulhu","div","divu","rem","remu"};
          ss << names[f3] << " x" << rd << ",x" << rs1 << ",x" << rs2;
        } else {
          const char* names[] = {"add","sll","slt","sltu","xor","srl","or","and"};
          const char* alt[] = {"sub","","","","","sra","",""};
          ss << (f7 && alt[f3][0] ? alt[f3] : names[f3]) << " x" << rd << ",x" << rs1 << ",x" << rs2;
        }
        break;
      }
      case 0x0f: ss << "fence"; break;
      case 0x73: {
        if (f3 == 0) {
          if (ins == 0x73) ss << "ecall";
          else if (ins == 0x100073) ss << "ebreak";
          else ss << "unknown_system";
        } else {
          const char* names[] = {"?","csrrw","csrrs","csrrc","?","csrrwi","csrrsi","csrrci"};
          ss << names[f3] << " x" << rd << ",0x" << std::hex << (ins>>20) << ",x" << rs1;
        }
        break;
      }
      case 0x2f: { // A extension
        uint32_t funct5 = f7 >> 2;
        const char* names[] = {"amoadd","amoswap","lr","sc","amoxor","?","?","?",
                                "amoor","amoand","amomin","amomax","amominu","amomaxu"};
        if (funct5 < 14 && names[funct5][0] != '?') {
          ss << names[funct5] << ".w x" << rd << ",x" << rs2 << ",(x" << rs1 << ")";
        } else {
          ss << "unknown_atomic";
        }
        break;
      }
      default: ss << "unknown"; break;
    }
    return ss.str();
  }

  // ─── Main step function ────────────────────────────────────────────────────
  void step() {
    // Fetch instruction
    uint32_t ins = fetch32(pc);
    
    // Decode and trace if enabled
    if (trace_enabled) {
      std::cout << "[cycle " << cycles << "] pc=0x" << std::hex << std::setw(8) 
                << std::setfill('0') << pc << " ins=0x" << std::setw(8) 
                << std::setfill('0') << ins << "  " << decode_ins(ins) << "\n";

      // Show registers
      for (int i = 0; i < 32; i++) {
        if (i % 8 == 0) std::cout << "x" << std::dec << std::setw(2) 
                                   << std::setfill('0') << i << ":";
        std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') 
                  << x[i] << "  ";
        if (i % 8 == 7) std::cout << "\n";
      }
      std::cout << "\n";
    }

    // Decode instruction fields
    uint32_t opc = ins & 0x7f;
    uint32_t rd  = (ins >> 7) & 0x1f;
    uint32_t f3  = (ins >> 12) & 0x7;
    uint32_t rs1 = (ins >> 15) & 0x1f;
    uint32_t rs2 = (ins >> 20) & 0x1f;
    uint32_t f7  = ins >> 25;

    auto sx = [](uint32_t v, unsigned bits) {
      uint32_t sign = 1u << (bits - 1);
      return int32_t((v ^ sign) - sign);
    };

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

    // Update PC
    uint32_t next_pc = pc + 4;

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
        case 1: mem[addr] = x[rs2]; mem[addr+1] = x[rs2]>>8; break;      // SH
        case 2: store32(addr, x[rs2]); break;                            // SW
      }
      pc = next_pc;
    } break;
    case 0x13: {
      if (f3 == 1 || f3 == 5) {
        uint32_t shamt = rs2;
        if (f3 == 1) x[rd] = x[rs1] << shamt;                            // SLLI
        else x[rd] = f7 ? int32_t(x[rs1]) >> shamt : x[rs1] >> shamt;    // SRAI/SRLI
      } else {
        int32_t imm = imm_i();
        switch (f3) {
          case 0: x[rd] = x[rs1] + imm; break;                           // ADDI
          case 2: x[rd] = (int32_t)x[rs1] < imm; break;                  // SLTI
          case 3: x[rd] = x[rs1] < (uint32_t)imm; break;                 // SLTIU
          case 4: x[rd] = x[rs1] ^ imm; break;                           // XORI
          case 6: x[rd] = x[rs1] | imm; break;                           // ORI
          case 7: x[rd] = x[rs1] & imm; break;                           // ANDI
        }
      }
      pc = next_pc;
    } break;
    case 0x33: {
      if (f7 == 1) { // M extension
        int32_t a = x[rs1], b = x[rs2];
        uint32_t ua = x[rs1], ub = x[rs2];
        int64_t prod = (int64_t)a * b;
        uint64_t uprod = (uint64_t)ua * ub;
        int64_t mxprod = (int64_t)a * (uint64_t)ub;
        switch (f3) {
          case 0: x[rd] = prod; break;                                   // MUL
          case 1: x[rd] = prod >> 32; break;                             // MULH
          case 2: x[rd] = mxprod >> 32; break;                           // MULHSU
          case 3: x[rd] = uprod >> 32; break;                            // MULHU
          case 4: x[rd] = b ? a / b : -1; break;                         // DIV
          case 5: x[rd] = b ? ua / ub : UINT_MAX; break;                 // DIVU
          case 6: x[rd] = b ? a % b : a; break;                          // REM
          case 7: x[rd] = b ? ua % ub : ua; break;                       // REMU
        }
      } else {
        switch (f3) {
          case 0: x[rd] = f7 ? x[rs1] - x[rs2] : x[rs1] + x[rs2]; break; // ADD/SUB
          case 1: x[rd] = x[rs1] << (x[rs2] & 0x1f); break;              // SLL
          case 2: x[rd] = (int32_t)x[rs1] < (int32_t)x[rs2]; break;      // SLT
          case 3: x[rd] = x[rs1] < x[rs2]; break;                        // SLTU
          case 4: x[rd] = x[rs1] ^ x[rs2]; break;                        // XOR
          case 5: x[rd] = f7 ? (int32_t)x[rs1] >> (x[rs2] & 0x1f)        // SRA/SRL
                              : x[rs1] >> (x[rs2] & 0x1f); break;
          case 6: x[rd] = x[rs1] | x[rs2]; break;                        // OR
          case 7: x[rd] = x[rs1] & x[rs2]; break;                        // AND
        }
      }
      pc = next_pc;
    } break;
    case 0x0f: pc = next_pc; break; // FENCE
    case 0x73: {
      if (f3 == 0) {
        if (ins == 0x73) {  // ECALL
          handle_syscall();
          pc = next_pc;
        } else if (ins == 0x100073) {  // EBREAK
          if (trace_enabled) {
            std::cerr << "EBREAK at PC " << std::hex << pc << std::endl;
          }
          exit(1);
        }
      } else {  // CSR instructions
        uint32_t csr_addr = ins >> 20;
        uint32_t old_val = read_csr(csr_addr);
        uint32_t new_val = old_val;
        uint32_t src = (f3 & 4) ? rs1 : x[rs1];  // Immediate vs register
        
        switch (f3 & 3) {
          case 1:  // CSRRW/CSRRWI
            new_val = src;
            break;
          case 2:  // CSRRS/CSRRSI
            new_val = old_val | src;
            break;
          case 3:  // CSRRC/CSRRCI
            new_val = old_val & ~src;
            break;
        }
        
        if (rd != 0) x[rd] = old_val;
        if ((f3 & 3) == 1 || rs1 != 0) write_csr(csr_addr, new_val);
        pc = next_pc;
      }
    } break;
    case 0x2f: { // A extension (atomics)
      uint32_t funct5 = f7 >> 2;
      uint32_t addr = x[rs1];
      
      switch (funct5) {
        case 0: { // AMOADD.W
          uint32_t old_val = fetch32(addr);
          store32(addr, old_val + x[rs2]);
          if (rd) x[rd] = old_val;
          break;
        }
        case 1: { // AMOSWAP.W
          uint32_t old_val = fetch32(addr);
          store32(addr, x[rs2]);
          if (rd) x[rd] = old_val;
          break;
        }
        case 2: { // LR.W
          x[rd] = fetch32(addr);
          has_reservation = true;
          reservation_addr = addr;
          break;
        }
        case 3: { // SC.W
          if (has_reservation && reservation_addr == addr) {
            store32(addr, x[rs2]);
            x[rd] = 0;  // Success
            has_reservation = false;
          } else {
            x[rd] = 1;  // Failure
          }
          break;
        }
        case 4: { // AMOXOR.W
          uint32_t old_val = fetch32(addr);
          store32(addr, old_val ^ x[rs2]);
          if (rd) x[rd] = old_val;
          break;
        }
        case 8: { // AMOOR.W
          uint32_t old_val = fetch32(addr);
          store32(addr, old_val | x[rs2]);
          if (rd) x[rd] = old_val;
          break;
        }
        case 12: { // AMOAND.W
          uint32_t old_val = fetch32(addr);
          store32(addr, old_val & x[rs2]);
          if (rd) x[rd] = old_val;
          break;
        }
        case 16: { // AMOMIN.W
          int32_t old_val = (int32_t)fetch32(addr);
          int32_t new_val = (int32_t)x[rs2];
          store32(addr, (old_val < new_val) ? old_val : new_val);
          if (rd) x[rd] = old_val;
          break;
        }
        case 20: { // AMOMAX.W
          int32_t old_val = (int32_t)fetch32(addr);
          int32_t new_val = (int32_t)x[rs2];
          store32(addr, (old_val > new_val) ? old_val : new_val);
          if (rd) x[rd] = old_val;
          break;
        }
        case 24: { // AMOMINU.W
          uint32_t old_val = fetch32(addr);
          uint32_t new_val = x[rs2];
          store32(addr, (old_val < new_val) ? old_val : new_val);
          if (rd) x[rd] = old_val;
          break;
        }
        case 28: { // AMOMAXU.W
          uint32_t old_val = fetch32(addr);
          uint32_t new_val = x[rs2];
          store32(addr, (old_val > new_val) ? old_val : new_val);
          if (rd) x[rd] = old_val;
          break;
        }
      }
      pc = next_pc;
    } break;
    default:
      if (trace_enabled) {
        std::cerr << "Unhandled opcode " << std::hex << opc << " at PC " << pc << std::endl;
      }
      exit(1);
    }

    x[0] = 0;  // x0 is always zero
    cycles++;
  }
};

// Driver
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " [--trace] program.bin\n";
    return 1;
  }
  
  bool trace = false;
  std::string filename;
  
  // Parse arguments
  if (argc == 3 && std::string(argv[1]) == "--trace") {
    trace = true;
    filename = argv[2];
  } else if (argc == 2) {
    filename = argv[1];
  } else {
    std::cerr << "usage: " << argv[0] << " [--trace] program.bin\n";
    return 1;
  }
  
  std::ifstream f(filename, std::ios::binary);
  if (!f.is_open()) {
    std::cerr << "Error: Cannot open file " << filename << std::endl;
    return 1;
  }
  std::vector<uint8_t> bin((std::istreambuf_iterator<char>(f)), {});

  CPU cpu(2 << 20, trace);              // 2 MiB of RAM
  std::copy(bin.begin(), bin.end(), cpu.mem.begin());

  while (true) cpu.step();              // run forever (ECALL exits)
}