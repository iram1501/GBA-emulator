#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include "memory.h"

#define CPSR_N (1u << 31)
#define CPSR_Z (1u << 30)
#define CPSR_C (1u << 29)
#define CPSR_V (1u << 28)

typedef struct {
    uint32_t r[16];
    uint32_t cpsr;
} CPU;

void cpu_init(CPU *cpu);

void cpu_decode_arm(CPU *cpu, uint32_t instruction);

void cpu_step(CPU *cpu, Memory *memory);

void cpu_execute_branch(CPU *cpu, uint32_t instruction);

#endif