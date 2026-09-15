#include "memory.h"
#include <stdio.h>

void memory_init (Memory *memory, unsigned char *rom, size_t rom_size) {
    memory->rom = rom;
    memory->rom_size = rom_size;
}

uint32_t memory_read32(Memory *memory, uint32_t address) {
    if (address >= 0x08000000 && address <= 0x09FFFFFF) {
        uint32_t offset = address - 0x08000000;

        if(offset + 3 >= memory->rom_size) {
            printf("ROM read out of bound %08X\n", address);
            return 0;
        }

        return ((uint32_t)memory->rom[offset]) | ((uint32_t)memory->rom[offset + 1] << 8) | 
               ((uint32_t)memory->rom[offset + 2] << 16) | ((uint32_t)memory->rom[offset + 3] << 24);
    }

    printf("Unhandled memory read: %08X\n", address);

    return 0;
}