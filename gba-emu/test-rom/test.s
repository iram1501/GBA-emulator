.syntax unified
.arm
.global _start

_start:
    mov r0, #3
    mov r1, #4
    mov r2, #2

    add r3, r0, r1, lsl r2

loop:
    b loop