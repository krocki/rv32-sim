# Simple RISC-V assembly test to write to framebuffer
# This will write some colored pixels to test SDL display

.section .text
.globl _start

_start:
    # Set up framebuffer base address in t0
    lui t0, 0x11100    # Load 0x11100000 into t0
    
    # Write some test pixels (32-bit ARGB format)
    
    # Write red pixel at position 0
    li t1, 0xFFFF0000  # Red color (ARGB)
    sw t1, 0(t0)
    
    # Write green pixel at position 1
    li t1, 0xFF00FF00  # Green color  
    sw t1, 4(t0)
    
    # Write blue pixel at position 2
    li t1, 0xFF0000FF  # Blue color
    sw t1, 8(t0)
    
    # Fill first row with white pixels
    li t2, 640         # Width = 640 pixels
    li t1, 0xFFFFFFFF  # White color
    mv t3, t0          # Copy framebuffer address
    
fill_loop:
    beqz t2, done      # If counter is 0, we're done
    sw t1, 0(t3)       # Write white pixel
    addi t3, t3, 4     # Move to next pixel (4 bytes per pixel)
    addi t2, t2, -1    # Decrement counter
    j fill_loop
    
done:
    # Infinite loop to keep program running
loop:
    j loop