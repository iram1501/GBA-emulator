#include <stdio.h>
#include "cpu.h"

typedef struct {
    uint32_t value;
    uint32_t carry;
} Operand2Result;

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

static Operand2Result decode_operand2(CPU *cpu, uint32_t operand2, uint32_t i_bit) {
    Operand2Result result;

    result.value = 0;
    result.carry = (cpu->cpsr & CPSR_C) != 0;

    if(i_bit == 1) {
        uint32_t rotate = (operand2 >> 8) & 0xF;
        uint32_t imm = operand2 & 0xFF;
        uint32_t amount = rotate * 2;

        result.value = ror32(imm, amount);

        if(amount != 0) {
            result.carry = (result.value >> 31) & 1;
        }

        return result;
    }

    uint32_t rm = operand2 & 0xF;
    uint32_t shift_by_register = (operand2 >> 4) & 1;
    uint32_t shift_type = (operand2 >> 5) & 0x3;

    uint32_t value = cpu->r[rm];

    if (shift_by_register == 0) {
        uint32_t amount = (operand2 >> 7) & 0x1F;

        switch(shift_type) {
            case 0x0: // LSL
                if(amount == 0) {
                    result.value = value;
                } else {
                    result.carry = (value >> (32 - amount)) & 1;
                    result.value = value << amount;
                }
                break;
            case 0x1: // LSR
                if(amount == 0) {
                    result.carry = (value >> 31) & 1;
                    result.value = 0;
                } else {
                    result.carry = (value >> (amount - 1)) & 1;
                    result.value = value >> amount;
                }
                break;
            case 0x2: // ASR
                if(amount == 0) {
                    result.carry = (value >> 31) & 1;

                    if(value & 0x80000000) {
                        result.value = 0xFFFFFFFF;
                    } else {
                        result.value = 0;
                    }
                } else {
                    result.carry = (value >> (amount - 1)) & 1;
                    result.value = (uint32_t)((int32_t)value >> amount);
                }
                break;
            case 0x3: // ROR / RRX
                if(amount == 0) {
                    uint32_t old_carry = (cpu->cpsr & CPSR_C) != 0;

                    result.carry = value & 1;

                    result.value = (old_carry << 31) | (value >> 1);
                } else {
                    result.value = ror32(value, amount);
                    result.carry = (result.value >> 31) & 1;
                }
                break;
        }

        return result;
    }

    uint32_t rs = (operand2 >> 8) & 0xF;
    uint32_t amount = cpu->r[rs] & 0xFF;

    if(amount == 0) {
        result.value = value;
        return result;
    }

    switch(shift_type) {
        case 0x0: // LSL
            if(amount < 32) {
                result.carry = (value >> (32 - amount)) & 1;
                result.value = value << amount;
            } else if (amount == 32) {
                result.carry = value & 1;
                result.value = 0;
            } else {
                result.carry = 0;
                result.value = 0;
            }
            break;
        case 0x1: // LSR
            if(amount < 32) {
                result.carry = (value >> (amount - 1)) & 1;
                result.value = value >> amount;
            } else if (amount == 32) {
                result.carry = (value >> 31) & 1;
                result.value = 0;
            } else {
                result.carry = 0;
                result.value = 0;
            }
            break;
        case 0x2: // ASR
            if(amount < 32) {
                result.carry = (value >> (amount - 1)) & 1;
                result.value = (uint32_t)((int32_t)value >> amount);
            } else {
                result.carry = (value >> 31) & 1;
                result.value = (value & 0x80000000) ? 0xFFFFFFFF : 0;
            }
            break;
        case 0x3: // ROR
            amount &= 31;
            if(amount == 0) {
                result.value = value;
                result.carry = (value >> 31) & 1;
            } else {
                result.value = ror32(value, amount);
                result.carry = (result.value >> 31) & 1;
            }
            break;
    }
    
    return result;
}

void cpu_decode_arm (CPU *cpu, Memory *memory, uint32_t instruction) {
    uint32_t i_bit   = (instruction >> 25) & 0x1;
    uint32_t opcode  = (instruction >> 21) & 0xF;
    uint32_t s_bit   = (instruction >> 20) & 0x1;
    uint32_t rn      = (instruction >> 16)  & 0xF;
    uint32_t rd      = (instruction >> 12) & 0xF;
    uint32_t operand2 = instruction & 0xFFF;

    Operand2Result op2 = decode_operand2(cpu, operand2, i_bit);
    uint32_t operand2_value = op2.value;

    if(opcode == 0x0) { // AND
        printf("\nExecuting AND\n");

        uint32_t result = cpu->r[rn] & operand2_value;
        cpu->r[rd] = result;

        if(s_bit) {
            update_nz_flags(cpu, result);
            set_flag(cpu, CPSR_C, op2.carry);
        }
    }

    if(opcode == 0x1) { // EOR (XOR)
        printf("\nExecuting EOR\n");

        uint32_t result = cpu->r[rn] ^ operand2_value;
        cpu->r[rd] = result;

        if(s_bit) {
            update_nz_flags(cpu, result);
            set_flag(cpu, CPSR_C, op2.carry);
        }
    }

    if(opcode == 0x2) { // SUB
        printf("\nExecuting SUB\n");

        uint32_t a = cpu->r[rn];
        uint32_t b = operand2_value;

        uint32_t result = a - b;

        cpu->r[rd] = result;

        if(s_bit) {
            update_sub_flags(cpu, a, b, result);
            print_flags(cpu);
        }
    }

    if(opcode == 0x4) { // ADD
        printf("\nExecuting ADD\n");

        uint32_t a = cpu->r[rn];
        uint32_t b = operand2_value;

        uint32_t result = a + b;

        cpu->r[rd] = result;

        if(s_bit) {
            update_add_flags(cpu, a, b, result);
        }
    }

    if(opcode == 0x8) { // TST
        printf("\nExecuting TST\n");

        uint32_t result = cpu->r[rn] & operand2_value;

        update_nz_flags(cpu, result);
        set_flag(cpu, CPSR_C, op2.carry);
    }

    if(opcode == 0xA) { // CMP
        printf("\nExecuting CMP\n");

        uint32_t a = cpu->r[rn];
        uint32_t b = operand2_value;
        uint32_t result = a - b;

        update_sub_flags(cpu, a, b, result);

        print_flags(cpu);
    }

    if(opcode == 0xC) { // ORR
        printf("\nExecuting ORR\n");

        uint32_t result = cpu->r[rn] | operand2_value;
        cpu->r[rd] = result;

        if(s_bit) {
            update_nz_flags(cpu, result);
            set_flag(cpu, CPSR_C, op2.carry);
        }
    }

    if(opcode == 0xD) { // MOV
        printf("\nExecuting MOV\n");

        cpu->r[rd] = operand2_value;

        if(s_bit) {
            update_nz_flags(cpu, operand2_value);
            set_flag(cpu, CPSR_C, op2.carry);

            print_flags(cpu);
        }
    }

    if(opcode == 0xE) { // BIC
        printf("\nExecuting BIC\n");

        uint32_t result = cpu->r[rn] & ~operand2_value;
        cpu->r[rd] = result;

        if(s_bit) {
            update_nz_flags(cpu, result);
            set_flag(cpu, CPSR_C, op2.carry);
        }
    }

    if(opcode == 0XF) { // MVN
        printf("\nExecuting MVN\n");

        uint32_t result = ~operand2_value;
        cpu->r[rd] = result;

        if(s_bit) {
            update_nz_flags(cpu, result);
            set_flag(cpu, CPSR_C, op2.carry);
        }
    }   
}

void cpu_execute_single_transfer(CPU *cpu, Memory *memory, uint32_t instruction) {
    uint32_t i_bit = (instruction >> 25) & 1;
    uint32_t p_bit = (instruction >> 24) & 1;
    uint32_t u_bit = (instruction >> 23) & 1;
    uint32_t b_bit = (instruction >> 22) & 1;
    uint32_t w_bit = (instruction >> 21) & 1;
    uint32_t l_bit = (instruction >> 20) & 1;

    uint32_t rn = (instruction >> 16) & 0xF;
    uint32_t rd = (instruction >> 12) & 0xF;

    uint32_t offset = instruction & 0xFFF;

    if(i_bit != 0 || b_bit != 0) {
        printf("Transfer type not implemented yet\n");
        return;
    }

    uint32_t base = cpu->r[rn];
    uint32_t offset_address;

    if(u_bit) {
        offset_address = base + offset;
    } else {
        offset_address = base - offset;
    }

    uint32_t address;

    if(p_bit) {
        address = offset_address;
    } else {
        address = base;
    }

    if(l_bit == 0) {
        printf("Executing STR\n");

        uint32_t value = cpu->r[rd];

        printf("Address: 0x%08X\n", address);
        printf("Value:   0x%08X", value);

        memory_write32(memory, address, value);
    } else {
        printf("Execiting LDR\n");

        uint32_t value = memory_read32(memory, address);

        cpu->r[rd] = value;

        printf("Address: 0x%08X\n", address);
        printf("Value:   0x%08X", value);
    }

    if(p_bit){
        if(w_bit) {
            cpu->r[rn] = offset_address;
        }
    } else {
        cpu->r[rn] = offset_address;
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
    uint32_t bits_27_26 = (instruction >> 26) & 0x3;

    if(instruction_type  == 0x5) {
        cpu_execute_branch(cpu, instruction);
    } else if (bits_27_26 == 0x1){
        cpu_execute_single_transfer(cpu, memory, instruction);
        cpu->r[15] += 4;
    } else {
        cpu_decode_arm(cpu, memory, instruction);
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

