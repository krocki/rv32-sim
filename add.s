.global func
.text

_func:
addi t3, x0, 1
addi t4, x0, 2
add t5, t3, t4
add t6, t5, t5
halt
