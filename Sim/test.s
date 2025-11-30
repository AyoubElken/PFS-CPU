.org 0x0
start:
    addi x1, x0, 10
    addi x2, x0, 20
    add  x3, x1, x2
loop:
    beq  x3, x0, end
    addi x3, x3, -1
    beq  x0, x0, loop
end:
    nop