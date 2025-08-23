// Memory Subsystem Interface for rv32ima.cc
// This allows pluggable memory implementations (basic RAM, MMIO+SDL, etc)

#ifndef MEMORY_SUBSYSTEM_H
#define MEMORY_SUBSYSTEM_H

#include <cstdint>
#include <vector>

// Abstract memory subsystem interface
class MemorySubsystem {
public:
    virtual ~MemorySubsystem() = default;
    
    // Basic memory operations
    virtual uint32_t fetch32(uint32_t addr) = 0;
    virtual void store32(uint32_t addr, uint32_t value) = 0;
    
    virtual uint16_t fetch16(uint32_t addr) = 0;
    virtual void store16(uint32_t addr, uint16_t value) = 0;
    
    virtual uint8_t fetch8(uint32_t addr) = 0;
    virtual void store8(uint32_t addr, uint8_t value) = 0;
    
    // Load binary into memory
    virtual bool load_binary(const uint8_t* data, size_t size, uint32_t load_addr = 0) = 0;
    
    // Optional: periodic updates (for display refresh, etc)
    virtual void update(uint64_t cycles) {}
    
    // Optional: check if we should quit (SDL window closed, etc)
    virtual bool should_quit() { return false; }
    
    // Get memory size
    virtual size_t size() const = 0;
};

// Basic RAM-only implementation
class BasicMemory : public MemorySubsystem {
private:
    std::vector<uint8_t> mem;
    
public:
    explicit BasicMemory(size_t mem_size) : mem(mem_size, 0) {}
    
    uint32_t fetch32(uint32_t addr) override {
        if (addr + 3 >= mem.size()) return 0;
        return mem[addr] | (mem[addr+1]<<8) | (mem[addr+2]<<16) | (mem[addr+3]<<24);
    }
    
    void store32(uint32_t addr, uint32_t v) override {
        if (addr + 3 >= mem.size()) return;
        mem[addr]   = v;
        mem[addr+1] = v >> 8;
        mem[addr+2] = v >> 16;
        mem[addr+3] = v >> 24;
    }
    
    uint16_t fetch16(uint32_t addr) override {
        if (addr + 1 >= mem.size()) return 0;
        return mem[addr] | (mem[addr+1]<<8);
    }
    
    void store16(uint32_t addr, uint16_t v) override {
        if (addr + 1 >= mem.size()) return;
        mem[addr]   = v;
        mem[addr+1] = v >> 8;
    }
    
    uint8_t fetch8(uint32_t addr) override {
        if (addr >= mem.size()) return 0;
        return mem[addr];
    }
    
    void store8(uint32_t addr, uint8_t v) override {
        if (addr >= mem.size()) return;
        mem[addr] = v;
    }
    
    bool load_binary(const uint8_t* data, size_t size, uint32_t load_addr = 0) override {
        if (load_addr + size > mem.size()) return false;
        std::memcpy(mem.data() + load_addr, data, size);
        return true;
    }
    
    size_t size() const override { return mem.size(); }
};

#endif // MEMORY_SUBSYSTEM_H