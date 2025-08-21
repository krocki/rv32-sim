# Simple framebuffer test for RV32I
# Draws colored squares to test SDL2 integration

.globl _start
_start:
  # Set up stack pointer
  lui sp, %hi(0x100000)
  addi sp, sp, %lo(0x100000)
  
  # Load framebuffer base address: 0x50000000
  lui t0, 0x50000
  
  # Draw a simple pattern
  li t1, 0          # y counter  
  li t2, 100        # height limit
  
draw_outer_loop:
  li t3, 0          # x counter
  li t4, 100        # width limit
  
draw_inner_loop:
  # Calculate pixel address: FB_BASE + (y * 640 + x) * 4
  li t5, 640
  mul t6, t1, t5    # y * 640
  add t6, t6, t3    # y * 640 + x
  slli t6, t6, 2    # (y * 640 + x) * 4
  add t6, t0, t6    # FB_BASE + offset
  
  # Calculate color: 0xFF000000 | (x * 2) << 16 | (y * 2) << 8 | 0xFF
  slli a0, t3, 1    # x * 2
  slli a0, a0, 16   # (x * 2) << 16
  slli a1, t1, 1    # y * 2
  slli a1, a1, 8    # (y * 2) << 8
  or a0, a0, a1     # combine
  ori a0, a0, 0xFF  # add blue component
  lui a1, 0xFF000   # alpha channel
  or a0, a0, a1     # final color
  
  # Store pixel
  sw a0, 0(t6)
  
  # Increment x
  addi t3, t3, 1
  blt t3, t4, draw_inner_loop
  
  # Increment y  
  addi t1, t1, 1
  blt t1, t2, draw_outer_loop
  
  # Trigger display update: write to FB_CTRL + 0x10
  lui t0, 0x50001
  li t1, 1
  sw t1, 0x10(t0)
  
  # Infinite loop
infinite_loop:
  j infinite_loop