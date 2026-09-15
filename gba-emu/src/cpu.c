#include <stdio.h>
#include "cpu.h"

void cpu_init (CPU *cpu) {
    for (int i = 0; i < 16; i++) {
        cpu->r[i] = 0;
    }

    cpu->cpsr = 0;

    cpu->r[15] = 0x08000000;
}

static uint32_t ror32(uint32_t value, uint32_t amount) {
    amount &= 31;

    if (amount == 0) {
        return value;
    }

    return (value >> amount) | (value << (32 - amount));
}

static int condition_passed(CPU *cpu, uint32_t cond) {
    int n = (cpu->cpsr & CPSR_N) != 0;
    int z = (cpu->cpsr & CPSR_Z) != 0;
    int c = (cpu->cpsr & CPSR_C) != 0;
    int v = (cpu->cpsr * CPSR_V) != 0;

    switch (cond) {
        case 0x0: return z;                 // EQ
        case 0x1: return !z;                // NE
        case 0x2: return c;                 // CC/HS
        case 0x3: return !c;                // CC/LO
        case 0x4: return n;                 // MI
        case 0x5: return !n;                // PL
        case 0x6: return v;                 // VS
        case 0x7: return !v;                // VC
        case 0x8: return c && !z;           // HI
        case 0x9: return !c || z;           // LS
        case 0xA: return n == v;            // GE
        case 0xB: return n != v;            // LT
        case 0xC: return !z && (n == v);    // GT
        case 0xD: return z || (n != v);     // LE
        case 0xE: return 1;                 // AL
        default: return 0;
    }
}

static void set_flag(CPU *cpu, uint32_t flag, int value) {
    if (value) {
        cpu->cpsr |= flag;
    } else {
        cpu->cpsr &= ~flag;
    }
}

static void update_nz_flags(CPU *cpu, uint32_t result) {
    set_flag(cpu, CPSR_Z, result == 0);
    set_flag(cpu, CPSR_N, (result >> 31) & 1);
}

static void update_sub_flags(CPU *cpu, uint32_t a, uint32_t b, uint32_t result) {
    set_flag(cpu, CPSR_N, (result >> 31) & 1);
    set_flag(cpu, CPSR_Z, result == 0);

    set_flag(cpu, CPSR_C, a >= b);

    uint32_t overflow = ((a ^ b) & (a ^ result)) >> 31;

    set_flag(cpu, CPSR_V, overflow);
}

static void update_add_flags(CPU *cpu, uint32_t a, uint32_t b, uint32_t result) {
    set_flag(cpu, CPSR_N, (result >> 31) & 1);
    set_flag(cpu, CPSR_Z, result == 0);

    set_flag(cpu, CPSR_C, result < a);

    uint32_t overflow = (~(a ^ b) & (a ^ result)) >> 31;

    set_flag(cpu, CPSR_V, overflow);
}

static void print_flags(CPU *cpu) {
    printf("N=%d Z=%d C=%d V=%d\n", (cpu->cpsr & CPSR_N) != 0, (cpu->cpsr & CPSR_Z) != 0,
                                    (cpu->cpsr & CPSR_C) != 0, (cpu->cpsr & CPSR_V) != 0);
}

void cpu_decode_arm (CPU *cpu, uint32_t instruction) {
    uint32_t cond    = (instruction >> 28) & 0xF;
    uint32_t i_bit   = (instruction >> 25) & 0x1;
    uint32_t opcode  = (instruction >> 21) & 0xF;
    uint32_t s_bit   = (instruction >> 20) & 0x1;
    uint32_t rn      = (instruction >> 16)  & 0xF;
    uint32_t rd      = (instruction >> 12) & 0xF;
    uint32_t operand2 = instruction & 0xFFF;
    uint32_t rs      = (operand2 >> 8) & 0xF;

    uint32_t operand2_value;

    if (i_bit == 1) {
        uint32_t rotate = (operand2 >> 8) & 0xF;
        uint32_t imm = operand2 & 0xFF;

        operand2_value = ror32(imm, rotate * 2);
    } else {
        uint32_t rm = operand2 & 0xF;

        uint32_t shift_by_register = (operand2 >> 4) & 0x1;
        uint32_t shift_type = (operand2 >> 5) & 0x3;
        uint32_t shift_amount = (operand2 >> 7) & 0x1F;

        operand2_value = cpu->r[rm];

        printf("\nOperand2 type: Register\n");
        printf("Rm:            R%u\n", rm);
        printf("Shift type:    %u\n", shift_type);
        printf("Shift amount:  %u\n", shift_amount);

        if(shift_by_register == 0) {
            switch(shift_type) {
                case 0x0: // LSL
                    operand2_value = operand2_value << shift_amount;
                    break;
                case 0x1: // LSR
                    if(shift_amount == 0) {
                        operand2_value = 0;
                    } else {
                        operand2_value = operand2_value >> shift_amount;
                    }
                    break;
                case 0x2: // ASR
                    if(shift_amount == 0) {
                        shift_amount = 32;
                    }

                    if(shift_amount >= 32) {
                        if(operand2_value & 0x80000000) {
                            operand2_value = 0xFFFFFFFF;
                        } else {
                            operand2_value = 0;
                        }
                    } else {
                        operand2_value = (uint32_t)((int32_t)operand2_value >> shift_amount);
                    }
                    break;
                case 0x3: // ROR
                    if(shift_amount != 0) {
                        operand2_value = ror32(operand2_value, shift_amount);
                    }
                    break;
            }
        } else {
            uint32_t register_shift_amount = cpu->r[rs] & 0xFF;

            switch(shift_type) {
                case 0x0: // LSL
                    if(register_shift_amount < 32) {
                    operand2_value = operand2_value << register_shift_amount;
                    } else {
                        operand2_value = 0;
                    }
                    break;
                case 0x1: // LSR
                    if(register_shift_amount < 32) {
                        operand2_value = operand2_value >> register_shift_amount;
                    } else {
                        operand2_value = 0;
                    }
                    break;
                case 0x2: // ASR
                    if(register_shift_amount >= 32) {
                        if(operand2_value & 0x80000000) {
                            operand2_value = 0xFFFFFFFF;
                        } else {
                            operand2_value = 0;
                        }
                    } else {
                        operand2_value = (uint32_t)((int32_t)operand2_value >> register_shift_amount);
                    }
                    break;
                case 0x3: // ROR
                    if(register_shift_amount != 0) {
                        operand2_value = ror32(operand2_value, register_shift_amount);
                    }
                    break;
            }
        }
    }

    if (opcode == 0xD) { // MOV
        printf("Executing MOV\n");

        cpu->r[rd] = operand2_value;

        if(s_bit) {
            update_nz_flags(cpu, operand2_value);
        }
    }

    if(opcode == 0x4) { // ADD
        printf("Executing ADD\n");

        uint32_t a = cpu->r[rn];
        uint32_t b = operand2_value;

        uint32_t result = a + b;

        cpu->r[rd] = result;

        if(s_bit) {
            update_add_flags(cpu, a, b, result);
        }
    }

    if(opcode == 0xA) { // CMP
        printf("Executing CMP\n");

        uint32_t a = cpu->r[rn];
        uint32_t b = operand2_value;
        uint32_t result = a - b;

        update_sub_flags(cpu, a, b, result);

        print_flags(cpu);
    }

    if(opcode == 0x2) { // SUB
        printf("Executing SUB\n");

        uint32_t a = cpu->r[rn];
        uint32_t b = operand2_value;

        uint32_t result = a - b;

        cpu->r[rd] = result;

        if(s_bit) {
            update_sub_flags(cpu, a, b, result);
            print_flags(cpu);
        }
    }
}

void cpu_step(CPU *cpu, Memory *memory) {
    uint32_t pc = cpu->r[15];
    uint32_t instruction = memory_read32(memory, pc);

    printf("\nPC: 0x%08X\nInstruction: 0x%08X\n", pc, instruction); 

    uint32_t cond = (instruction >> 28) & 0xF;

    if (!condition_passed(cpu, cond)) {
        printf("Condition failed - instruction skipped\n");
        cpu->r[15] += 4;
        return;
    }

    uint32_t instruction_type = (instruction >> 25) & 0x7;

    if(instruction_type  == 0x5) {
        cpu_execute_branch(cpu, instruction);
    } else {
        cpu_decode_arm(cpu, instruction);
        cpu->r[15] += 4;
    }
}

void cpu_execute_branch (CPU *cpu, uint32_t instruction) {
    int32_t offset = instruction & 0x00FFFFFF;

    if(offset & 0x00800000) {
        offset |= 0xFF000000;
    }

    offset <<= 2;

    uint32_t target = cpu->r[15] + 8 + offset;

    printf("Executing B\n");
    printf("Branch offset: %d\n", offset);
    printf("Branch target: 0x%08X\n", target);

    cpu->r[15] = target;
}

