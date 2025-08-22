// Minimal cycle-accurate RV32I simulator (single-issue, in-order)
// Compile:  g++ -std=c++20 -O2 rv32i_sim.cpp -o rv32i_sim
// Usage:    ./rv32i_sim <raw-binary> [mem_size]
// NOTE: Only the core loop and ISA behaviour are implemented;            
//       program loading / I/O is intentionally kept minimal.

#include <cstdint>
#include <iostream>
#include <fstream>

using u8  = uint8_t;   using  u16 = uint16_t;
using u32 = uint32_t;  using  u64 = uint64_t;
using i8  = int8_t;    using  i16 = int16_t;
using i32 = int32_t;   using  i64 = int64_t;

struct CPU {
  u32 pc = 0;                      // program counter
  u32 x[32]{};                     // integer register file
  u64 cycles = 0;                  // retired cycles
  std::vector<u8> mem;             // flat memory

  explicit CPU(size_t mem_sz = 1<<20) : mem(mem_sz, 0) {}

  /* --- helpers ------------------------------------------------------ */
  template<typename T> T  load(u32 addr) const {
    T v = 0; memcpy(&v, &mem.at(addr), sizeof(T)); return v; }
  template<typename T> void store(u32 addr, T v) {
    memcpy(&mem.at(addr), &v, sizeof(T)); }
  u32 fetch()            { return load<u32>(pc); }
  static i32 sext(u32 val, int bits) { // sign-extend
    int shift = 32 - bits; return int(val << shift) >> shift; }

  /* --- single-cycle execute ---------------------------------------- */
  void step() {
    u32 ins = fetch(); pc += 4; cycles++;
    u32 opc =  ins & 0x7f;
    u32 rd  = (ins >> 7)  & 0x1f;
    u32 f3  = (ins >> 12) & 0x7;
    u32 rs1 = (ins >> 15) & 0x1f;
    u32 rs2 = (ins >> 20) & 0x1f;
    u32 f7  = (ins >> 25);
    auto &R = x;        // alias
    auto imm_i = [&]{ return sext(ins >> 20, 12); };
    auto imm_u = [&]{ return ins & 0xfffff000; };
    auto imm_b = [&]{ return sext(((f7 & 0x40) << 1) | ((R[0] = 0), // clang-format off
                     ((ins >> 7) & 0x1e) | ((ins >> 20) & 0x7e0) | ((ins << 4) & 0x800)), 13); };
    auto imm_s = [&]{ return sext(((ins >> 7) & 0x1f) | ((ins >> 20) & 0xfe0), 12); };
    auto imm_j = [&]{ return sext(((ins >> 20) & 0x7fe) | ((ins >> 9) & 0x800) | (ins & 0xff000) | ((ins >> 11) & 0x100000), 21); };

    switch (opc) {
    case 0x37: R[rd] = imm_u();                             break;               // LUI
    case 0x17: R[rd] = pc - 4 + imm_u();                    break;               // AUIPC
    case 0x6f: { u32 npc = pc - 4 + imm_j(); R[rd] = pc; pc = npc; } break;      // JAL
    case 0x67: { u32 t = pc; pc = (R[rs1] + imm_i()) & ~1u; R[rd] = t; } break;  // JALR
    case 0x63: { bool take = false; int a=R[rs1], b=R[rs2];
      switch (f3) {case 0: take=a==b; break;case 1: take=a!=b; break;
      case 4: take=a<b; break;case 5: take=a>=b; break;
      case 6: take=(u32)a<(u32)b; break;case 7: take=(u32)a>=(u32)b; break;}
      if(take) pc = pc - 4 + imm_b(); } break;                                // BRANCH
    case 0x03: { u32 addr=R[rs1]+imm_i(); switch(f3){
      case 0: R[rd]=sext(load<i8>(addr),8); break; case 1:R[rd]=sext(load<i16>(addr),16);break;
      case 2: R[rd]=load<u32>(addr); break; case 4:R[rd]=load<u8>(addr); break;
      case 5: R[rd]=load<u16>(addr); break; } } break;                        // LOAD
    case 0x23: { u32 addr=R[rs1]+imm_s(); switch(f3){
      case 0: store<u8>(addr,R[rs2]); break; case 1: store<u16>(addr,R[rs2]); break;
      case 2: store<u32>(addr,R[rs2]); break; } } break;                      // STORE
    case 0x13: { u32 imm=imm_i(); switch(f3){
      case 0: R[rd]=R[rs1]+imm; break; case 2: R[rd]=(i32)R[rs1]<(i32)imm; break;
      case 3: R[rd]=R[rs1]<imm; break; case 4: R[rd]=R[rs1]^imm; break;
      case 6: R[rd]=R[rs1]|imm; break; case 7: R[rd]=R[rs1]&imm; break;
      case 1: R[rd]=R[rs1]<< (imm & 0x1f); break; case 5:
          R[rd]=(f7? (i32)R[rs1]>>(imm&0x1f) : R[rs1]>>(imm&0x1f)); }
      } break;                                                               // ALU-IMM
    case 0x33: { switch((f7<<3)|f3){
      case 0x00: R[rd]=R[rs1]+R[rs2]; break;  case 0x20: R[rd]=R[rs1]-R[rs2];break;
      case 0x01: R[rd]=R[rs1]<< (R[rs2]&0x1f); break; case 0x02:R[rd]=(i32)R[rs1]<(i32)R[rs2];break;
      case 0x03: R[rd]=R[rs1]<R[rs2]; break; case 0x04: R[rd]=R[rs1]^R[rs2]; break;
      case 0x05: R[rd]=R[rs1]>>(R[rs2]&0x1f); break; case 0x25:R[rd]=(i32)R[rs1]>>(R[rs2]&0x1f);break;
      case 0x06: R[rd]=R[rs1]|R[rs2]; break; case 0x07: R[rd]=R[rs1]&R[rs2]; break; } } break; // ALU-REG
    case 0x0f: /*FENCE*/ break; case 0x73: if((ins>>20)==0) std::exit(R[17]);   // ECALL
    default: std::cerr << "Illegal opcode 0x"<<std::hex<<opc<<"\n"; std::exit(1);
    }
    R[0] = 0; // x0 hard-wired to zero
  }
};

/* ------------------- tiny front-end ---------------------------------- */
int main(int argc, char* argv[]) {
  if(argc<2){ std::cerr<<"Usage: ./rv32i_sim <raw_bin> [mem_size]\n"; return 1; }
  size_t mem_size = (argc>2? std::stoul(argv[2]) : 1<<20);
  CPU cpu(mem_size);
  std::ifstream f(argv[1], std::ios::binary);
  f.read((char*)cpu.mem.data(), cpu.mem.size());

  while(true) cpu.step();
}

