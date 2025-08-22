# Simple test for compressed instructions
.text
.global _start
_start:
    # Test C.LI (load immediate)
    c.li x1, 5        # x1 = 5
    
    # Test C.ADDI 
    c.addi x1, 3      # x1 = x1 + 3 = 8
    
    # Test C.MV (move)
    c.mv x2, x1       # x2 = x1 = 8
    
    # Test C.ADD
    c.add x1, x2      # x1 = x1 + x2 = 16
    
    # Exit
    li a0, 0
    ecall