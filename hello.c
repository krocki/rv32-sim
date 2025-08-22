#include <stdint.h>

// Simple system calls
#define SYS_WRITE 64
#define SYS_EXIT  93

// System call interface
long syscall(long number, long arg1, long arg2, long arg3) {
    register long a7 asm("a7") = number;
    register long a0 asm("a0") = arg1;
    register long a1 asm("a1") = arg2;
    register long a2 asm("a2") = arg3;
    
    asm volatile("ecall" : "+r"(a0) : "r"(a7), "r"(a1), "r"(a2) : "memory");
    return a0;
}

void write_string(const char* str) {
    int len = 0;
    while (str[len]) len++;  // strlen
    syscall(SYS_WRITE, 1, (long)str, len);  // write(stdout, str, len)
}

int main() {
    write_string("Hello, RISC-V World!\n");
    syscall(SYS_EXIT, 0, 0, 0);  // exit(0)
    return 0;
}