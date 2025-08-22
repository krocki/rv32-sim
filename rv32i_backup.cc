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
// Tiny *cycle-accurate* RV32I simulator with full trace output
// Every call to CPU::step() executes **one** instruction and then prints:
//   • cycle number and PC
//   • raw 32-bit word, mnemonic & operands (best-effort disassembler)
//   • register file (x0-x31)
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

  // ─── Helpers ───────────────────────────────────────────────────────────────
  uint32_t fetch32(uint32_t addr) const {
    assert(addr + 3 < mem.size());
    return mem[addr] | mem[addr+1]<<8 | mem[addr+2]<<16 | mem[addr+3]<<24;
  }

  uint32_t fetch() const { return fetch32(pc); }

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
         << ":0x" << std::setw(8) << x[i]
         << ((i % 8 == 7) ? "\n" : "  ");
    }
    return os.str();
  }

  std::string disasm(uint32_t ins) const {
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
    case 0x0f: os << "fence"; break;
    case 0x73: os << "ecall"; break;
    default:   os << "illegal"; break;
    }
    return os.str();
  }

  // ─── Main execution (one instruction) ──────────────────────────────────────
  void step() {
    uint32_t ins = fetch();

    // Decode fields (needed for exec & print) --------------------------------
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

    // Execute ---------------------------------------------------------------
    switch (opc) {
    case 0x37: x[rd] = imm_u();           pc += 4; break;               // LUI
    case 0x17: x[rd] = pc + imm_u();      pc += 4; break;               // AUIPC
    case 0x6f: { uint32_t t = pc + 4; pc += imm_j(); if(rd) x[rd] = t; } break; // JAL
    case 0x67: { uint32_t t = pc + 4; pc = (x[rs1] + imm_i()) & ~1u; if(rd) x[rd] = t; } break; // JALR
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
      pc += take ? imm_b() : 4;
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
      pc += 4;
    } break;
    case 0x23: {
      uint32_t addr = x[rs1] + imm_s();
      switch (f3) {
        case 0: mem[addr] = x[rs2]; break;                               // SB
        case 1: mem[addr] = x[rs2]; mem[addr+1] = x[rs2] >> 8; break;    // SH
        case 2: store32(addr, x[rs2]); break;                            // SW
      }
      pc += 4;
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
      pc += 4;
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
      pc += 4;
    } break;
    case 0x2f: { // A extension (atomic operations)
      if (f3 == 2) { // Word atomics
        uint32_t addr = x[rs1];
        uint32_t f5 = ins >> 27;
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
      pc += 4;
    } break;
    case 0x0f: pc += 4; break;                                           // FENCE (noop)
    case 0x73: std::cout << "\nECALL reached at cycle " << cycles << "\n"; std::exit(0);
    default: std::cerr << "Unknown opcode 0x" << std::hex << (int)opc << "\n"; std::exit(1);
    }

    x[0] = 0;   // x0 is hard-wired to zero

    // ─── Trace printout ────────────────────────────────────────────────────
    std::cout << "\n[cycle " << std::dec << cycles << "] pc=0x"
              << std::hex << std::setw(8) << std::setfill('0') << pc
              << " ins=0x" << std::setw(8) << ins << "  " << disasm(ins)
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
