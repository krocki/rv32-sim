#include <iostream>
#include <iomanip>

int main() {
    uint16_t ins = 0xa091;
    
    int32_t offset = ((ins >> 3) & 0xe) | ((ins >> 7) & 0x10) | ((ins << 3) & 0x20) |
                    ((ins >> 1) & 0x40) | ((ins << 1) & 0x80) | ((ins >> 2) & 0x300) |
                    ((ins << 9) & 0x400) | ((int32_t)(ins << 20) >> 21);
    uint32_t imm = offset & 0xfffff;
    uint32_t expanded = 0x0000006f | (imm << 12);
    
    std::cout << "ins = 0x" << std::hex << ins << "\n";
    std::cout << "offset = 0x" << std::hex << offset << " (" << std::dec << offset << ")\n";
    std::cout << "expanded = 0x" << std::hex << expanded << "\n";
    
    return 0;
}
