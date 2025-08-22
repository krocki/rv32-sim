#include <cstdint>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iterator>
#include <cassert>
#include <climits>
#include <chrono>
#include <queue>
#include <cstring>

// -----------------------------------------------------------------------------
// RV32IMA simulator with MMIO support for running Linux/DOOM
// Based on rv32ima.cc with memory-mapped I/O extensions
// -----------------------------------------------------------------------------

// MMIO address map
#define MMIO_BASE       0x10000000
#define CLINT_BASE      0x10000000
#define CLINT_MTIME     (CLINT_BASE + 0xBFF8)
#define CLINT_MTIMECMP  (CLINT_BASE + 0x4000)
#define UART_BASE       0x11000000
#define FRAMEBUFFER_BASE 0x11100000
#define INPUT_BASE      0x11200000
#define SYSCON_BASE     0x11300000

struct CPU {
  // Core state
  uint32_t pc = 0x80000000;  // Linux typically starts here
  uint32_t x[32]{};          
  uint64_t cycles = 0;
  std::vector<uint8_t> mem;  
  
  // Atomic extension state
  bool has_reservation = false;
  uint32_t reservation_addr = 0;
  
  // CSR state
  uint32_t csr[4096]{};  
  
  // Trace mode
  bool trace_enabled = false;
  
  // MMIO devices
  uint64_t mtime_start;
  std::queue<uint8_t> uart_rx_queue;
  uint8_t framebuffer[320 * 200];
  uint8_t palette[256 * 3];
  uint32_t keyboard_state = 0;
  
  explicit CPU(size_t mem_size, bool trace = false) 
    : mem(mem_size), trace_enabled(trace) {
    mtime_start = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
    memset(framebuffer, 0, sizeof(framebuffer));
    memset(palette, 0, sizeof(palette));
  }

  uint64_t get_time_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  // Memory access with MMIO support
  uint32_t fetch32(uint32_t addr) {
    // Check for MMIO read
    if (addr >= MMIO_BASE && addr < 0x12000000) {
      return mmio_read(addr);
    }
    
    if (addr + 3 >= mem.size()) {
      if (trace_enabled) {
        std::cerr << "fetch32: addr=0x" << std::hex << addr 
                  << " out of bounds" << std::endl;
      }
      return 0;
    }
    return mem[addr] | mem[addr+1]<<8 | mem[addr+2]<<16 | mem[addr+3]<<24;
  }

  void store32(uint32_t addr, uint32_t v) {
    // Check for MMIO write
    if (addr >= MMIO_BASE && addr < 0x12000000) {
      mmio_write(addr, v, 4);
      return;
    }
    
    if (addr + 3 >= mem.size()) {
      if (trace_enabled) {
        std::cerr << "store32: addr=0x" << std::hex << addr 
                  << " out of bounds" << std::endl;
      }
      return;
    }
    mem[addr]   = v;
    mem[addr+1] = v >> 8;
    mem[addr+2] = v >> 16;
    mem[addr+3] = v >> 24;
  }
  
  void store16(uint32_t addr, uint16_t v) {
    if (addr >= MMIO_BASE && addr < 0x12000000) {
      mmio_write(addr, v, 2);
      return;
    }
    
    if (addr + 1 >= mem.size()) return;
    mem[addr] = v;
    mem[addr+1] = v >> 8;
  }
  
  void store8(uint32_t addr, uint8_t v) {
    if (addr >= MMIO_BASE && addr < 0x12000000) {
      mmio_write(addr, v, 1);
      return;
    }
    
    if (addr >= mem.size()) return;
    mem[addr] = v;
  }

  uint32_t mmio_read(uint32_t addr) {
    // CLINT timer
    if (addr == CLINT_MTIME) {
      return (get_time_us() - mtime_start) & 0xFFFFFFFF;
    }
    if (addr == CLINT_MTIME + 4) {
      return ((get_time_us() - mtime_start) >> 32) & 0xFFFFFFFF;
    }
    
    // UART
    if (addr == UART_BASE) {
      // UART data register
      if (!uart_rx_queue.empty()) {
        uint8_t data = uart_rx_queue.front();
        uart_rx_queue.pop();
        return data;
      }
      return 0;
    }
    if (addr == UART_BASE + 5) {
      // UART Line Status Register (LSR)
      // Bit 0: Data Ready (DR)
      // Bit 5: Transmitter Empty (THRE)
      return 0x60 | (!uart_rx_queue.empty() ? 1 : 0);
    }
    
    // Input devices
    if (addr == INPUT_BASE) {
      return keyboard_state;
    }
    
    // System control
    if (addr == SYSCON_BASE) {
      return 0x5241524D; // "MRAM" magic value
    }
    
    return 0;
  }

  void mmio_write(uint32_t addr, uint32_t value, int size) {
    // UART
    if (addr == UART_BASE) {
      // Output character
      putchar(value & 0xFF);
      fflush(stdout);
      return;
    }
    
    // Framebuffer write (simplified - not implementing for basic version)
    if (addr >= FRAMEBUFFER_BASE && addr < FRAMEBUFFER_BASE + sizeof(framebuffer)) {
      uint32_t offset = addr - FRAMEBUFFER_BASE;
      if (size == 4 && offset + 3 < sizeof(framebuffer)) {
        framebuffer[offset] = value & 0xFF;
        framebuffer[offset + 1] = (value >> 8) & 0xFF;
        framebuffer[offset + 2] = (value >> 16) & 0xFF;
        framebuffer[offset + 3] = (value >> 24) & 0xFF;
      } else if (size == 2 && offset + 1 < sizeof(framebuffer)) {
        framebuffer[offset] = value & 0xFF;
        framebuffer[offset + 1] = (value >> 8) & 0xFF;
      } else if (size == 1 && offset < sizeof(framebuffer)) {
        framebuffer[offset] = value & 0xFF;
      }
      return;
    }
    
    // System control - shutdown
    if (addr == SYSCON_BASE) {
      if (value == 0x5555) {
        if (trace_enabled) {
          std::cout << "System shutdown requested" << std::endl;
        }
        exit(0);
      }
    }
  }
  
  // Enhanced syscall handling for Linux
  void handle_syscall() {
    uint32_t syscall_num = x[17];  // a7
    
    switch (syscall_num) {
      case 93: {  // Exit
        uint32_t exit_code = x[10];
        if (trace_enabled) {
          std::cout << "Program exited with code " << exit_code << std::endl;
        }
        exit(exit_code);
      }
      
      case 64: {  // Write
        uint32_t fd = x[10];
        uint32_t buf = x[11];
        uint32_t count = x[12];
        
        if (fd == 1 || fd == 2) {
          for (uint32_t i = 0; i < count && buf + i < mem.size(); i++) {
            std::cout << (char)mem[buf + i];
          }
          std::cout.flush();
          x[10] = count;
        } else {
          x[10] = -1;
        }
        break;
      }
      
      case 63: { // Read
        uint32_t fd = x[10];
        uint32_t buf = x[11];
        uint32_t count = x[12];
        
        if (fd == 0) { // stdin
          uint32_t i;
          for (i = 0; i < count && buf + i < mem.size(); i++) {
            int ch = getchar();
            if (ch == EOF) break;
            mem[buf + i] = ch;
            if (ch == '\n') {
              i++;
              break;
            }
          }
          x[10] = i;
        } else {
          x[10] = -1;
        }
        break;
      }
      
      case 214: { // brk (memory allocation)
        static uint32_t heap_end = 0x84000000;  // Start heap at 64MB mark
        uint32_t new_brk = x[10];
        if (new_brk == 0) {
          x[10] = heap_end;
        } else if (new_brk > heap_end && new_brk < 0x90000000) {
          heap_end = new_brk;
          x[10] = heap_end;
        } else {
          x[10] = heap_end;
        }
        break;
      }
      
      case 222: { // mmap2
        uint32_t addr = x[10];
        uint32_t length = x[11];
        uint32_t prot = x[12];
        uint32_t flags = x[13];
        uint32_t fd = x[14];
        uint32_t offset = x[15];
        
        // Simplified mmap - just allocate from high memory
        static uint32_t mmap_base = 0x70000000;
        
        if (length == 0) {
          x[10] = -1;
        } else {
          x[10] = mmap_base;
          mmap_base -= (length + 0xFFF) & ~0xFFF;
        }
        break;
      }
      
      case 403: { // clock_gettime64
        uint32_t clk_id = x[10];
        uint32_t timespec_ptr = x[11];
        uint64_t us = get_time_us();
        store32(timespec_ptr, us / 1000000);         // seconds
        store32(timespec_ptr + 4, 0);                // high seconds
        store32(timespec_ptr + 8, (us % 1000000) * 1000); // nanoseconds
        x[10] = 0;
        break;
      }
      
      default:
        if (trace_enabled) {
          std::cerr << "Unhandled syscall: " << syscall_num << std::endl;
        }
        x[10] = -38;  // ENOSYS
    }
  }
  
  // CSR operations
  uint32_t read_csr(uint32_t addr) {
    switch (addr) {
      case 0xC00:  // cycle
      case 0xB00:  // mcycle
        return cycles & 0xFFFFFFFF;
      case 0xC80:  // cycleh
      case 0xB80:  // mcycleh
        return (cycles >> 32) & 0xFFFFFFFF;
      case 0xC01:  // time
        return (get_time_us() - mtime_start) & 0xFFFFFFFF;
      case 0xC81:  // timeh
        return ((get_time_us() - mtime_start) >> 32) & 0xFFFFFFFF;
      case 0xC02:  // instret
      case 0xB02:  // minstret
        return cycles & 0xFFFFFFFF;
      case 0xC82:  // instreth
      case 0xB82:  // minstreth
        return (cycles >> 32) & 0xFFFFFFFF;
      default:
        return csr[addr];
    }
  }
  
  void write_csr(uint32_t addr, uint32_t value) {
    switch (addr) {
      case 0xC00:
      case 0xC80:
      case 0xC01:
      case 0xC81:
      case 0xC02:
      case 0xC82:
        break;  // Read-only
      default:
        csr[addr] = value;
        break;
    }
  }

  // Instruction decoder (simplified from main version)
  std::string decode_ins(uint32_t ins) {
    if (!trace_enabled) return "";
    
    std::stringstream ss;
    uint32_t opc = ins & 0x7f;
    uint32_t rd  = (ins >> 7) & 0x1f;
    uint32_t f3  = (ins >> 12) & 0x7;
    uint32_t rs1 = (ins >> 15) & 0x1f;
    uint32_t rs2 = (ins >> 20) & 0x1f;
    
    // Basic decode for trace mode
    switch (opc) {
      case 0x37: ss << "lui"; break;
      case 0x17: ss << "auipc"; break;
      case 0x6f: ss << "jal"; break;
      case 0x67: ss << "jalr"; break;
      case 0x63: ss << "branch"; break;
      case 0x03: ss << "load"; break;
      case 0x23: ss << "store"; break;
      case 0x13: ss << "alu-imm"; break;
      case 0x33: ss << "alu"; break;
      case 0x73: ss << "system"; break;
      case 0x2f: ss << "atomic"; break;
      default: ss << "unknown"; break;
    }
    return ss.str();
  }

  // Main execution step
  void step() {
    uint32_t ins = fetch32(pc);
    
    if (trace_enabled) {
      std::cout << "[cycle " << cycles << "] pc=0x" << std::hex << std::setw(8) 
                << std::setfill('0') << pc << " ins=0x" << std::setw(8) 
                << std::setfill('0') << ins << "  " << decode_ins(ins) << "\n";
    }

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

    uint32_t next_pc = pc + 4;

    switch (opc) {
    case 0x37: x[rd] = imm_u();           pc = next_pc; break;          // LUI
    case 0x17: x[rd] = pc + imm_u();      pc = next_pc; break;          // AUIPC
    case 0x6f: { uint32_t t = next_pc; pc += imm_j(); if(rd) x[rd] = t; } break; // JAL
    case 0x67: { uint32_t t = next_pc; pc = (x[rs1] + imm_i()) & ~1u; if(rd) x[rd] = t; } break; // JALR
    case 0x63: {
      bool take = false;
      switch (f3) {
        case 0: take = x[rs1] == x[rs2]; break;
        case 1: take = x[rs1] != x[rs2]; break;
        case 4: take = (int32_t)x[rs1] <  (int32_t)x[rs2]; break;
        case 5: take = (int32_t)x[rs1] >= (int32_t)x[rs2]; break;
        case 6: take = x[rs1] <  x[rs2]; break;
        case 7: take = x[rs1] >= x[rs2]; break;
      }
      pc = take ? pc + imm_b() : next_pc;
    } break;
    case 0x03: {
      uint32_t addr = x[rs1] + imm_i();
      switch (f3) {
        case 0: x[rd] = (int8_t)(addr >= MMIO_BASE ? mmio_read(addr) : mem[addr]); break; // LB
        case 1: x[rd] = (int16_t)(addr >= MMIO_BASE ? mmio_read(addr) : 
                        (mem[addr] | mem[addr+1]<<8)); break;    // LH
        case 2: x[rd] = fetch32(addr); break;                    // LW
        case 4: x[rd] = addr >= MMIO_BASE ? mmio_read(addr) : mem[addr]; break; // LBU
        case 5: x[rd] = addr >= MMIO_BASE ? mmio_read(addr) : 
                        (mem[addr] | mem[addr+1]<<8); break;     // LHU
      }
      pc = next_pc;
    } break;
    case 0x23: {
      uint32_t addr = x[rs1] + imm_s();
      switch (f3) {
        case 0: store8(addr, x[rs2]); break;                     // SB
        case 1: store16(addr, x[rs2]); break;                    // SH
        case 2: store32(addr, x[rs2]); break;                    // SW
      }
      pc = next_pc;
    } break;
    case 0x13: {
      if (f3 == 1 || f3 == 5) {
        uint32_t shamt = rs2;
        if (f3 == 1) x[rd] = x[rs1] << shamt;                    // SLLI
        else x[rd] = f7 ? int32_t(x[rs1]) >> shamt : x[rs1] >> shamt; // SRAI/SRLI
      } else {
        int32_t imm = imm_i();
        switch (f3) {
          case 0: x[rd] = x[rs1] + imm; break;                   // ADDI
          case 2: x[rd] = (int32_t)x[rs1] < imm; break;          // SLTI
          case 3: x[rd] = x[rs1] < (uint32_t)imm; break;         // SLTIU
          case 4: x[rd] = x[rs1] ^ imm; break;                   // XORI
          case 6: x[rd] = x[rs1] | imm; break;                   // ORI
          case 7: x[rd] = x[rs1] & imm; break;                   // ANDI
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
          case 0: x[rd] = prod; break;                           // MUL
          case 1: x[rd] = prod >> 32; break;                     // MULH
          case 2: x[rd] = mxprod >> 32; break;                   // MULHSU
          case 3: x[rd] = uprod >> 32; break;                    // MULHU
          case 4: x[rd] = b ? a / b : -1; break;                 // DIV
          case 5: x[rd] = b ? ua / ub : UINT_MAX; break;         // DIVU
          case 6: x[rd] = b ? a % b : a; break;                  // REM
          case 7: x[rd] = b ? ua % ub : ua; break;               // REMU
        }
      } else {
        switch (f3) {
          case 0: x[rd] = f7 ? x[rs1] - x[rs2] : x[rs1] + x[rs2]; break;
          case 1: x[rd] = x[rs1] << (x[rs2] & 0x1f); break;
          case 2: x[rd] = (int32_t)x[rs1] < (int32_t)x[rs2]; break;
          case 3: x[rd] = x[rs1] < x[rs2]; break;
          case 4: x[rd] = x[rs1] ^ x[rs2]; break;
          case 5: x[rd] = f7 ? (int32_t)x[rs1] >> (x[rs2] & 0x1f)
                              : x[rs1] >> (x[rs2] & 0x1f); break;
          case 6: x[rd] = x[rs1] | x[rs2]; break;
          case 7: x[rd] = x[rs1] & x[rs2]; break;
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
        uint32_t src = (f3 & 4) ? rs1 : x[rs1];
        
        switch (f3 & 3) {
          case 1: new_val = src; break;
          case 2: new_val = old_val | src; break;
          case 3: new_val = old_val & ~src; break;
        }
        
        if (rd != 0) x[rd] = old_val;
        if ((f3 & 3) == 1 || rs1 != 0) write_csr(csr_addr, new_val);
        pc = next_pc;
      }
    } break;
    case 0x2f: { // A extension
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
            x[rd] = 0;
            has_reservation = false;
          } else {
            x[rd] = 1;
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
        std::cerr << "Unhandled opcode " << std::hex << opc 
                  << " at PC " << pc << std::endl;
      }
      exit(1);
    }

    x[0] = 0;
    cycles++;
  }
};

// Driver
int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " [--trace] program.bin\n";
    std::cerr << "\nThis version includes MMIO support for running Linux/DOOM\n";
    return 1;
  }
  
  bool trace = false;
  std::string filename;
  
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

  // Use larger memory for Linux (128MB)
  CPU cpu(128 << 20, trace);
  
  // Load binary at Linux start address (0x80000000 maps to offset 0)
  if (bin.size() > cpu.mem.size()) {
    std::cerr << "Error: Binary too large for memory" << std::endl;
    return 1;
  }
  std::copy(bin.begin(), bin.end(), cpu.mem.begin());

  // Set up initial state for Linux boot
  cpu.pc = 0x80000000;  // Linux entry point
  cpu.x[10] = 0;         // Hart ID in a0
  cpu.x[11] = 0x82000000; // DTB pointer in a1 (device tree blob)

  std::cout << "Starting RV32IMA emulator with MMIO..." << std::endl;
  std::cout << "Memory: " << (cpu.mem.size() >> 20) << "MB" << std::endl;
  std::cout << "Entry: 0x" << std::hex << cpu.pc << std::endl;
  
  while (true) cpu.step();
}