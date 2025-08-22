// RISC-V RV32IMA Core Emulator Header
// Provides common definitions and structures for RV32IMA emulation
// Supports I (base), M (multiply/divide), A (atomic) extensions

#ifndef RV32IMA_CORE_H
#define RV32IMA_CORE_H

#include <cstdint>
#include <vector>
#include <cstring>

// Memory configuration
#define RAM_SIZE (64 * 1024 * 1024)  // 64MB RAM default
#define RAM_BASE 0x80000000

// MMIO addresses
#define MINIRV32_RAM_IMAGE_OFFSET   0x80000000
#define MINIRV32_UART_BASE          0x10000000
#define MINIRV32_DEFAULT_TIMERDIV   0x11E00
#define MINIRV32_MMIO_RANGE_START   0x10000000
#define MINIRV32_MMIO_RANGE_END     0x12000000

// CSR definitions
#define CSR_CYCLE       0xC00
#define CSR_CYCLEH      0xC80
#define CSR_TIME        0xC01
#define CSR_TIMEH       0xC81
#define CSR_INSTRET     0xC02
#define CSR_INSTRETH    0xC82
#define CSR_MSTATUS     0x300
#define CSR_MTVEC       0x305
#define CSR_MIE         0x304
#define CSR_MIP         0x344

// RV32IMA CPU State
struct RV32IMA_State {
    uint32_t pc;
    uint32_t regs[32];
    uint32_t csr[4096];
    uint64_t cyclel;
    uint64_t cycleh;
    uint64_t timerl;
    uint64_t timerh;
    uint64_t timermatchl;
    uint64_t timermatchh;
    uint32_t mscratch;
    uint32_t mtvec;
    uint32_t mie;
    uint32_t mip;
    uint32_t mepc;
    uint32_t mtval;
    uint32_t mcause;
    uint32_t mstatus;
    uint32_t mideleg;
    uint32_t medeleg;
    uint32_t mhartid;
    uint32_t extraflags;
    uint32_t reserved_store;
    uint8_t* ram_image;
    uint32_t ram_size;
    
    // Constructor
    RV32IMA_State(uint32_t ram_amt = RAM_SIZE) : 
        pc(MINIRV32_RAM_IMAGE_OFFSET),
        cyclel(0), cycleh(0),
        timerl(0), timerh(0),
        timermatchl(0), timermatchh(0),
        mscratch(0), mtvec(0), mie(0), mip(0),
        mepc(0), mtval(0), mcause(0), mstatus(0),
        mideleg(0), medeleg(0), mhartid(0),
        extraflags(0), reserved_store(0),
        ram_size(ram_amt) {
        memset(regs, 0, sizeof(regs));
        memset(csr, 0, sizeof(csr));
        ram_image = new uint8_t[ram_size];
        memset(ram_image, 0, ram_size);
    }
    
    ~RV32IMA_State() {
        delete[] ram_image;
    }
};

// Function declarations for core emulator
uint32_t HandleMemoryLoad(RV32IMA_State* state, uint32_t addr, uint32_t size);
int HandleMemoryStore(RV32IMA_State* state, uint32_t addr, uint32_t val, uint32_t size);
int HandleControlStore(RV32IMA_State* state, uint32_t addr, uint32_t val);
uint32_t HandleControlLoad(RV32IMA_State* state, uint32_t addr);
void HandleOtherCSR(RV32IMA_State* state, uint32_t csrno, uint32_t* val);
int IsKBHit();
int ReadKBByte();

// Instruction decode helpers
inline uint32_t GetRd(uint32_t ir) { return (ir >> 7) & 0x1f; }
inline uint32_t GetRs1(uint32_t ir) { return (ir >> 15) & 0x1f; }
inline uint32_t GetRs2(uint32_t ir) { return (ir >> 20) & 0x1f; }
inline uint32_t GetFunct3(uint32_t ir) { return (ir >> 12) & 0x7; }
inline uint32_t GetFunct7(uint32_t ir) { return (ir >> 25) & 0x7f; }

// Sign extension helpers
inline int32_t SignExt(uint32_t val, int bits) {
    uint32_t sign_bit = 1u << (bits - 1);
    return (val ^ sign_bit) - sign_bit;
}

#endif // RV32IMA_CORE_H