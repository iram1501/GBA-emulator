#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "cpu.h"
#include "memory.h"

uint32_t read32(unsigned char *rom, uint32_t offset) {
    return ((uint32_t)rom[offset]) | ((uint32_t)rom[offset + 1] << 8) | ((uint32_t)rom[offset + 2] << 16) |
           ((uint32_t)rom[offset + 3] << 24);
}


int main (int argc, char *argv[]) {
    printf("GBA emulator\n");

    if (argc != 2) {
        printf("Usage: %s <rom.gba>\n", argv[0]);
        return 1;
    }

    FILE *rom_file = fopen(argv[1], "rb");

    if(rom_file == NULL) {
        printf("Failed to open ROM\n");
        return 1;
    }

    printf("ROM: %s\n", argv[1]);

    fseek(rom_file, 0, SEEK_END);

    long rom_size = ftell(rom_file);

    rewind(rom_file);

    printf("ROM size %ld bytes\n", rom_size);

    unsigned char *rom = malloc(rom_size);

    if(rom == NULL) {
        printf("Failed to allocate memory for ROM\n");
        fclose(rom_file);
        return 1;
    }

    size_t bytes_read = fread(rom, 1, rom_size, rom_file);

    if(bytes_read != rom_size) {
        printf("Failed to read entire ROM\n");
        free(rom);
        fclose(rom_file);
        return 1;
    }

    printf("ROM loaded into memory successfully\n");
    
    CPU cpu;
    cpu_init(&cpu);
    
    Memory memory;
    memory_init(&memory, rom, rom_size);

    for (int i = 0; i < 6; i++) {
        cpu_step(&cpu, &memory);
    }

    printf("\nFinal registers:\n");
    printf("R0 = 0x%08X\n", cpu.r[0]);
    printf("R1 = 0x%08X\n", cpu.r[1]);
    printf("R2 = 0x%08X\n", cpu.r[2]);
    printf("R3 = 0x%08X\n", cpu.r[3]);
    printf("R4 = 0x%08X\n", cpu.r[4]);
    printf("R5 = 0x%08X\n", cpu.r[5]);
    printf("R6 = 0x%08X\n", cpu.r[6]);
    printf("PC = 0x%08X\n", cpu.r[15]);

    free(rom);
    fclose(rom_file);

    return 0;
}