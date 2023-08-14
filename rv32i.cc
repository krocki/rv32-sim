#include <iostream>
#include <cstdint>
#include <string>
#include <unordered_map>

std::unordered_map<uint8_t, std::string> imm_names = {
  {0x37, "LUI"},
  {0x00, "ADDI"},
};

std::unordered_map<uint8_t, std::string> op_names = {
  {0x00, "ADD"},
};

#define XLEN 32
#define RAM_SIZE 0x10000
#define DEBUG 1
uint8_t ram[RAM_SIZE];

struct RV32I {
  uint32_t reg[32] = {0};
  uint32_t pc = 0;
  uint32_t cycles = 0;
  uint32_t read32(uint8_t *ram) {
    uint8_t* p = ram + pc;
    pc += 4;
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
  };
  void step() {
    if (DEBUG) {
      printf("cycle %6d, pc = 0x%08x, ", cycles, pc);
    }
    uint32_t insn = read32(ram);
    cycles++;
    if (DEBUG) {
      printf("insn = 0x%08x\n", insn);
    }

    // decode
    uint32_t opcode, rd, rs1, rs2, funct3;
    int32_t imm, cond, err;
    uint32_t addr, val=0, val2;

    opcode = insn & 0x7f;
    rd = (insn >> 7) & 0x1f;
    rs1 = (insn >> 15) & 0x1f;
    rs2 = (insn >> 20) & 0x1f;
    funct3 = ((insn >> 12) & 7) | ((insn >> (30 - 3)) & (1 << 3));

    if (DEBUG) {
      printf("opcode: 0x%04x, rd: 0x%04x, rs1: 0x%04x, rs2: 0x%04x, funct3: 0x%04x\n",
        opcode, rd, rs1, rs2, funct3);

      if (opcode == 0x13) { // IMM
        funct3 = (insn >> 12) & 7;
        printf("IMM\n---- opcode 0x%04x funct3 0x%04x\n", opcode, funct3);
        imm = (int32_t)insn >> 20;
        if (imm_names.count(funct3) > 0) {
          std::cout << "opcode " << std::hex << opcode << ", name [" << imm_names[funct3] << "], imm = " << imm << "\n";
        } else {
          std::cout << "opcode not in the table\n";
        }
      }
      if (opcode == 0x33) { // OP
        imm = insn >> 25;
        funct3 = ((insn >> 12) & 7) | ((insn >> (30 - 3)) & (1 << 3));
        printf("IMM 0x%04x\n---- imm 0x%08x, rs1 0%08x, rs2 0x%08x, val 0x%08x, val2 0x%08x\n",
          opcode, imm, rs1, rs2, val, val2);
        val = reg[rs1];
        val2 = reg[rs2];
        if (op_names.count(funct3) > 0) {
          std::cout << "opcode " << std::hex << funct3 << ", name [" << op_names[funct3] << "], imm = " << imm << "\n";
        } else {
          std::cout << "opcode not in the table\n";
        }
      }
    }
  }
};

int main(int argc, char* argv[]) {

  RV32I cpu = {.pc = 0x0};

  // li      t3,1, addi t3, x0, 1
  ((uint32_t *)ram)[0] = 0x00100e13;
  // li      t4,2, addi t4, x0, 2
  ((uint32_t *)ram)[1] = 0x00200e93;
  // add     t5,t3,t4
  ((uint32_t *)ram)[2] = 0x01de0f33;
  //add      t6,t5,t5
  ((uint32_t *)ram)[3] = 0x01ef0fb3;
  for (int i = 0; i < 4; i++) {
    cpu.step();
  }
  return EXIT_SUCCESS;
}
