#include <iostream>
#include <iomanip>

static uint32_t sx(uint32_t v, int bits) {  // sign-extend *bits*
    uint32_t m = 1u << (bits - 1);
    return (v ^ m) - m;
}

int main() {
    uint32_t expanded_ins = 0x4a06f;
    
    uint32_t v = ((expanded_ins>>21)&0x3ff)<<1;
    v |= ((expanded_ins>>20)&1)<<11;
    v |= ((expanded_ins>>12)&0xff)<<12;
    v |= (expanded_ins>>31)<<20;
    uint32_t imm = sx(v,21);
    
    std::cout << "expanded_ins = 0x" << std::hex << expanded_ins << "\n";
    std::cout << "v = 0x" << std::hex << v << "\n";
    std::cout << "imm = 0x" << std::hex << imm << " (" << std::dec << (int32_t)imm << ")\n";
    
    return 0;
}
