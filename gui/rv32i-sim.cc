// rv32i_sim.cpp — RV32I core + CLI driver (GUI moved to separate file)
// -----------------------------------------------------------------------------
// Build CLI trace version:
//   g++ -std=c++17 -O2 rv32i_sim.cpp -o rv32i_sim
// -----------------------------------------------------------------------------
// To use the core from another translation unit (e.g. the ImGui GUI), compile
// that file with -DRV32I_LIBRARY so the CLI `main()` here is skipped:
//   g++ -std=c++17 -O2 -DRV32I_LIBRARY -DGUI -o rv32i_gui rv32i_gui.cpp -lSDL2 -lGL -ldl -pthread
// -----------------------------------------------------------------------------

#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iterator>
#include <cassert>
#include <array>
#include <deque>

// ────────────────────────────────────────────────────────────────────────────
// CPU core (unchanged logic from previous version)
// ─────────────────────────────────────────────────────────────────────────────
struct CPU {
  struct Snapshot {
    uint32_t pc;
    std::array<uint32_t, 32> x;
    uint64_t cycles;
  };

  uint32_t pc = 0;
  std::array<uint32_t, 32> x{};
  uint64_t cycles = 0;
  std::vector<uint8_t> mem;
  std::array<bool, 32> reg_changed{};

  struct Trace {
    uint64_t cyc;
    uint32_t pc;
    uint32_t ins;
    std::string txt;
  };

  std::deque<Trace>   trace;
  size_t              trace_max   = 10'000;
  std::deque<Snapshot> history;
  size_t              history_max = 512;

  explicit CPU(size_t mem_sz = 1 << 20)
      : mem(mem_sz) {}

  // — utilities — -----------------------------------------------------------
  static uint32_t sx(uint32_t v, int bits)
  {
    uint32_t m = 1u << (bits - 1);
    return (v ^ m) - m;
  }

  uint32_t fetch32(uint32_t addr) const
  {
    assert(addr + 3 < mem.size());
    return mem[addr] | mem[addr + 1] << 8 | mem[addr + 2] << 16 |
           mem[addr + 3] << 24;
  }

  uint32_t fetch() const { return fetch32(pc); }

  void store32(uint32_t a, uint32_t v)
  {
    assert(a + 3 < mem.size());
    mem[a]     = v & 0xFF;
    mem[a + 1] = v >> 8;
    mem[a + 2] = v >> 16;
    mem[a + 3] = v >> 24;
  }

  // -------------------------------------------------------------------------
  // Disassembler (same as before)
  // -------------------------------------------------------------------------
  std::string disasm(uint32_t ins) const
  {
    uint32_t opc = ins & 0x7F;
    uint32_t rd  = (ins >> 7) & 0x1F;
    uint32_t f3  = (ins >> 12) & 7;
    uint32_t rs1 = (ins >> 15) & 0x1F;
    uint32_t rs2 = (ins >> 20) & 0x1F;
    uint32_t f7  = ins >> 25;

    auto imm_i = [&] { return sx(ins >> 20, 12); };
    auto imm_u = [&] { return ins & 0xFFFFF000u; };
    auto imm_s = [&] {
      return sx(((ins >> 7) & 0x1F) | ((ins >> 20) & 0xFE0), 12);
    };
    auto imm_b = [&] {
      uint32_t v = ((ins >> 7) & 0x1E) | ((ins >> 20) & 0x7E0) |
                   ((ins << 4) & 0x800) | ((ins >> 19) & 0x1000);
      return sx(v, 13);
    };
    auto imm_j = [&] {
      uint32_t v = ((ins >> 21) & 0x3FF) << 1;
      v |= ((ins >> 20) & 1) << 11;
      v |= ((ins >> 12) & 0xFF) << 12;
      v |= (ins >> 31) << 20;
      return sx(v, 21);
    };

    std::ostringstream os;
    switch (opc) {
      case 0x37:
        os << "lui  x" << rd << ",0x" << std::hex << imm_u();
        break;
      case 0x17:
        os << "auipc x" << rd << ",0x" << std::hex << imm_u();
        break;
      case 0x6F:
        os << "jal  x" << rd << "," << std::dec << imm_j();
        break;
      case 0x67:
        os << "jalr x" << rd << ",x" << rs1 << "," << imm_i();
        break;
      case 0x63: {
        static const char* m[] = {"beq",  "bne",   "",     "",
                                   "blt", "bge",   "bltu", "bgeu"};
        os << m[f3] << " x" << rs1 << ",x" << rs2 << "," << imm_b();
      } break;
      case 0x03: {
        static const char* m[] = {"lb", "lh", "lw", "", "lbu", "lhu"};
        os << m[f3] << " x" << rd << "," << imm_i() << "(x" << rs1 << ")";
      } break;
      case 0x23: {
        static const char* m[] = {"sb", "sh", "sw"};
        os << m[f3] << " x" << rs2 << "," << imm_s() << "(x" << rs1 << ")";
      } break;
      case 0x13: {
        if (f3 == 1)
          os << "slli";
        else if (f3 == 5)
          os << ((ins >> 30) ? "srai" : "srli");
        else {
          static const char* m[] = {"addi", "",     "slti",  "sltiu",
                                     "xori", "",     "ori",   "andi"};
          os << m[f3];
        }
        os << " x" << rd << ",x" << rs1 << ",";
        os << ((f3 == 1 || f3 == 5) ? (imm_i() & 31) : imm_i());
      } break;
      case 0x33: {
        if (f3 == 0 && f7)
          os << "sub";
        else if (f3 == 5 && f7)
          os << "sra";
        else {
          static const char* m[] = {"add", "sll", "slt",  "sltu",
                                     "xor", "srl", "or",   "and"};
          os << m[f3];
        }
        os << " x" << rd << ",x" << rs1 << ",x" << rs2;
      } break;
      case 0x0F:
        os << "fence";
        break;
      case 0x73:
        os << "ecall";
        break;
      default:
        os << "illegal";
    }
    return os.str();
  }

  // -------------------------------------------------------------------------
  // One instruction step (logic unchanged)
  // -------------------------------------------------------------------------
  void step();  // implemented after the class for clarity

  // -------------------------------------------------------------------------
  // I/O helpers
  // -------------------------------------------------------------------------
  void clear_changes() { reg_changed.fill(false); }

  void reset()
  {
    pc = 0;
    cycles = 0;
    x.fill(0);
    clear_changes();
    history.clear();
    trace.clear();
  }

  bool load_bin(const std::string& path)
  {
    std::ifstream f(path, std::ios::binary);
    if (!f)
      return false;

    std::vector<uint8_t> bin((std::istreambuf_iterator<char>(f)), {});
    if (bin.size() > mem.size())
      return false;

    std::copy(bin.begin(), bin.end(), mem.begin());
    reset();
    return true;
  }
};

// ----------------------------------------------------------------------------
// CPU::step implementation (identical to previous logic, skipped here for brev.)
// ----------------------------------------------------------------------------

void CPU::step()
{
  // Snapshot for history -----------------------------------------------------
  if (history.size() == history_max)
    history.pop_front();
  history.push_back({pc, x, cycles});

  uint32_t ins = fetch();
  uint32_t opc = ins & 0x7F;
  uint32_t rd  = (ins >> 7) & 0x1F;
  uint32_t f3  = (ins >> 12) & 7;
  uint32_t rs1 = (ins >> 15) & 0x1F;
  uint32_t rs2 = (ins >> 20) & 0x1F;
  uint32_t f7  = ins >> 25;

  auto imm_i = [&] { return sx(ins >> 20, 12); };
  auto imm_u = [&] { return ins & 0xFFFFF000u; };
  auto imm_s = [&] { return sx(((ins >> 7) & 0x1F) | ((ins >> 20) & 0xFE0), 12); };
  auto imm_b = [&] {
    uint32_t v = ((ins >> 7) & 0x1E) | ((ins >> 20) & 0x7E0) |
                 ((ins << 4) & 0x800) | ((ins >> 19) & 0x1000);
    return sx(v, 13);
  };
  auto imm_j = [&] {
    uint32_t v = ((ins >> 21) & 0x3FF) << 1;
    v |= ((ins >> 20) & 1) << 11;
    v |= ((ins >> 12) & 0xFF) << 12;
    v |= (ins >> 31) << 20;
    return sx(v, 21);
  };

  std::array<uint32_t, 32>	prev = x;

  // --- Execute (unchanged from previous version) ---------------------------
  switch (opc) {
    case 0x37:
      x[rd] = imm_u();
      pc += 4;
      break;
    case 0x17:
      x[rd] = pc + imm_u();
      pc += 4;
      break;

      case 0x67:{ uint32_t t=pc+4; pc=(x[rs1]+imm_i())&~1u; if(rd) x[rd]=t; } break;
      case 0x63:{ bool take=false; switch(f3){case 0:take=x[rs1]==x[rs2]; break; case 1:take=x[rs1]!=x[rs2]; break; case 4:take=(int32_t)x[rs1]<(int32_t)x[rs2]; break; case 5:take=(int32_t)x[rs1]>=(int32_t)x[rs2]; break; case 6:take=x[rs1]<x[rs2]; break; case 7:take=x[rs1]>=x[rs2]; break;} pc+=take?imm_b():4;} break;
      case 0x03:{ uint32_t a=x[rs1]+imm_i(); switch(f3){case 0:x[rd]=(int8_t)mem[a]; break; case 1:x[rd]=(int16_t)(mem[a]|mem[a+1]<<8); break; case 2:x[rd]=fetch32(a); break; case 4:x[rd]=mem[a]; break; case 5:x[rd]=mem[a]|mem[a+1]<<8; break;} pc+=4;} break;
      case 0x23:{ uint32_t a=x[rs1]+imm_s(); switch(f3){case 0:mem[a]=x[rs2]; break; case 1:mem[a]=x[rs2]; mem[a+1]=x[rs2]>>8; break; case 2:store32(a,x[rs2]); break;} pc+=4;} break;
      case 0x13:{ uint32_t imm=imm_i(); switch(f3){case 0:x[rd]=x[rs1]+imm; break; case 2:x[rd]=(int32_t)x[rs1]<(int32_t)imm; break; case 3:x[rd]=x[rs1]<imm; break; case 4:x[rd]=x[rs1]^imm; break; case 6:x[rd]=x[rs1]|imm; break; case 7:x[rd]=x[rs1]&imm; break; case 1:x[rd]=x[rs1]<<(imm&0x1F); break; case 5:x[rd]=(imm>>10)?(int32_t)x[rs1]>>(imm&0x1F):x[rs1]>>(imm&0x1F); break;} pc+=4;} break;
      case 0x33:{ switch(f3){case 0:x[rd]=f7?x[rs1]-x[rs2]:x[rs1]+x[rs2]; break; case 1:x[rd]=x[rs1]<<(x[rs2]&0x1F); break; case 2:x[rd]=(int32_t)x[rs1]<(int32_t)x[rs2]; break; case 3:x[rd]=x[rs1]<x[rs2]; break; case 4:x[rd]=x[rs1]^x[rs2]; break; case 5:x[rd]=f7?(int32_t)x[rs1]>>(x[rs2]&0x1F):x[rs1]>>(x[rs2]&0x1F); break; case 6:x[rd]=x[rs1]|x[rs2]; break; case 7:x[rd]=x[rs1]&x[rs2]; break;} pc+=4;} break;
      case 0x0F: pc+=4; break; case 0x73: std::cout<<"ECALL @ cycle "<<cycles<<"\n"; std::exit(0);
      default: std::cerr<<"Illegal opcode 0x"<<std::hex<<opc<<"\n"; std::exit(1);
    }

    x[0]=0;
    for(int i=0;i<32;++i) reg_changed[i]=(x[i]!=prev[i]);

    // push trace entry
    if(trace.size()==trace_max) trace.pop_front();
    trace.push_back({cycles, history.back().pc, ins, disasm(ins)});

    ++cycles;
  }

  bool CPU::step_back(){
    if(history.size()<2) return false;            // need at least one prior snapshot
    history.pop_back();                           // discard current state snapshot
    Snapshot snap = history.back(); history.pop_back();
    pc = snap.pc; x = snap.x; cycles = snap.cycles; clear_changes();
    if(!trace.empty()) trace.pop_back();          // remove current inst from trace
    return true;
  }
};

static const char *REG_NAMES[32]={"zero","ra","sp","gp","tp","t0","t1","t2","s0","s1","a0","a1","a2","a3","a4","a5","a6","a7","s2","s3","s4","s5","s6","s7","s8","s9","s10","s11","t3","t4","t5","t6"};

// ─────────────────────────────────────────────────────────────────────────────
// CLI driver (when compiled without GUI)
// ─────────────────────────────────────────────────────────────────────────────
#ifndef GUI
int main(int argc,char**argv){ if(argc<2){ std::cerr<<"usage: "<<argv[0]<<" program.bin\n"; return 1; } CPU cpu; if(!cpu.load_bin(argv[1])){ std::cerr<<"failed to load "<<argv[1]<<"\n"; return 1;} while(true){ uint32_t ins=cpu.fetch(); cpu.step(); std::cout<<"[cycle "<<std::dec<<cpu.cycles<<"] pc=0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<cpu.pc<<" ins=0x"<<std::setw(8)<<ins<<" "<<cpu.disasm(ins)<<"\n"; for(int i=0;i<32;++i){ if(cpu.reg_changed[i]) std::cout<<"*"; std::cout<<REG_NAMES[i]<<"="<<std::hex<<cpu.x[i]<<((i%8==7)?"\n":" "); } }}
#endif
