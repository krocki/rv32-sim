#include <cstdint>
#include <vector>
#include <iostream>
#include <fstream>
#include <iterator>
#include <cassert>

struct CPU {
  uint32_t pc = 0;
  uint32_t regs[32]{};
  uint64_t cycles = 0;
  std::vector<uint8_t> mem;

  CPU(size_t mem_size): mem(mem_size) {}

  uint32_t fetch() const {
    assert(pc + 3 < mem.size());
    return mem[pc] | mem[pc+1]<<8 | mem[pc+2]<<16 | mem[pc+3]<<24;
  }

  uint32_t load32(uint32_t addr) const {
    assert(addr + 3 < mem.size());
    return mem[addr] | mem[addr+1]<<8 | mem[addr+2]<<16 | mem[addr+3]<<24;
  }

  void store32(uint32_t addr, uint32_t val) {
    assert(addr + 3 < mem.size());
    mem[addr] = val & 0xff;
    mem[addr+1] = (val>>8) & 0xff;
    mem[addr+2] = (val>>16) & 0xff;
    mem[addr+3] = (val>>24) & 0xff;
  }

  static uint32_t sx(uint32_t v, int bits) {
    uint32_t m = 1u << (bits-1);
    return (v ^ m) - m;
  }

  void step() {
    uint32_t ins = fetch();
    uint32_t opc = ins & 0x7f;
    uint32_t rd  = (ins>>7)&0x1f;
    uint32_t f3  = (ins>>12)&7;
    uint32_t rs1 = (ins>>15)&0x1f;
    uint32_t rs2 = (ins>>20)&0x1f;
    uint32_t f7  = ins>>25;

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

    auto &x = regs;

    printf("cycle=%04llu, opc=0x%X\n", cycles, opc);
    switch(opc){

    case 0x37: x[rd]=imm_u(); pc+=4; break; // LUI
    case 0x17: x[rd]=pc+imm_u(); pc+=4; break; // AUIPC
    case 0x6f:{ uint32_t t=pc+4; pc+=imm_j(); if(rd)x[rd]=t; }break; // JAL
    case 0x67:{ uint32_t t=pc+4; pc=(x[rs1]+imm_i())&~1u; if(rd)x[rd]=t; }break; // JALR
    case 0x63:{ bool take=false; switch(f3){
      case 0: take=x[rs1]==x[rs2]; break; // BEQ
      case 1: take=x[rs1]!=x[rs2]; break; // BNE
      case 4: take=(int32_t)x[rs1]<(int32_t)x[rs2]; break; // BLT
      case 5: take=(int32_t)x[rs1]>=(int32_t)x[rs2]; break; // BGE
      case 6: take=x[rs1]<x[rs2]; break; // BLTU
      case 7: take=x[rs1]>=x[rs2]; break; // BGEU
    } pc+=take?imm_b():4; }break;
    case 0x03:{ uint32_t a=x[rs1]+imm_i(); switch(f3){
      case 0: x[rd]=(int8_t)mem[a]; break; // LB
      case 1: x[rd]=(int16_t)(mem[a]|mem[a+1]<<8); break; // LH
      case 2: x[rd]=load32(a); break; // LW
      case 4: x[rd]=mem[a]; break; // LBU
      case 5: x[rd]=mem[a]|mem[a+1]<<8; break; // LHU
    } pc+=4; }break;
    case 0x23:{ uint32_t a=x[rs1]+imm_s(); switch(f3){
      case 0: mem[a]=x[rs2]; break; // SB
      case 1: mem[a]=x[rs2]; mem[a+1]=x[rs2]>>8; break; // SH
      case 2: store32(a,x[rs2]); break; // SW
    } pc+=4; }break;
    case 0x13:{ uint32_t imm=imm_i(); switch(f3){
      case 0: x[rd]=x[rs1]+imm; break; // ADDI
      case 2: x[rd]=(int32_t)x[rs1]<(int32_t)imm; break; // SLTI
      case 3: x[rd]=x[rs1]<imm; break; // SLTIU
      case 4: x[rd]=x[rs1]^imm; break; // XORI
      case 6: x[rd]=x[rs1]|imm; break; // ORI
      case 7: x[rd]=x[rs1]&imm; break; // ANDI
      case 1: x[rd]=x[rs1]<<(imm&0x1f); break; // SLLI
      case 5: x[rd]=(imm>>10)?(int32_t)x[rs1]>>(imm&0x1f):x[rs1]>>(imm&0x1f); break; // SRLI/SRAI
    } pc+=4; }break;
    case 0x33:{ switch(f3){
      case 0: x[rd]=f7?x[rs1]-x[rs2]:x[rs1]+x[rs2]; break; // SUB/ADD
      case 1: x[rd]=x[rs1]<<(x[rs2]&0x1f); break; // SLL
      case 2: x[rd]=(int32_t)x[rs1]<(int32_t)x[rs2]; break; // SLT
      case 3: x[rd]=x[rs1]<x[rs2]; break; // SLTU
      case 4: x[rd]=x[rs1]^x[rs2]; break; // XOR
      case 5: x[rd]=f7?(int32_t)x[rs1]>>(x[rs2]&0x1f):x[rs1]>>(x[rs2]&0x1f); break; // SRA/SRL
      case 6: x[rd]=x[rs1]|x[rs2]; break; // OR
      case 7: x[rd]=x[rs1]&x[rs2]; break; // AND
    } pc+=4; }break;
    case 0x0f: pc+=4; break; // FENCE (noop)
    case 0x73: std::cout<<"ECALL cycle "<<cycles<<"\n"; std::exit(0);
    default: std::cerr<<"BAD OPC "<<std::hex<<opc<<"\n"; std::exit(1);
    }
    x[0]=0;
    cycles++;
  }
};

int main(int argc,char**argv){
  if(argc<2){ std::cerr<<"usage: "<<argv[0]<<" prog.bin\n"; return 1; }
  std::ifstream f(argv[1],std::ios::binary);
  std::vector<uint8_t> bin((std::istreambuf_iterator<char>(f)),{});
  CPU cpu(1<<20);
  std::copy(bin.begin(),bin.end(),cpu.mem.begin());
  while(true) cpu.step();
}

