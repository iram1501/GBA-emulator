.syntax unified
.arm
.global _start

_start:
    mov r0, #42
    mov r1, #0x02000000

    str r0, [r1], #4
    ldr r2, [r1, #-4]

loop:
    b loop