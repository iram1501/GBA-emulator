#include "memory.h"
#include <stdio.h>
#include <string.h>

void memory_init (Memory *memory, unsigned char *rom, size_t rom_size) {
    memory->rom = rom;
    memory->rom_size = rom_size;

    memset(memory->ewram, 0, sizeof(memory->ewram));
    memset(memory->iwram, 0, sizeof(memory->iwram));
}

uint32_t memory_read32(Memory *memory, uint32_t address) {
    uint8_t *region;
    uint32_t offset;

    
    if(address >= 0x02000000 && address <= 0x0203FFFC) { // EWRAM
        region = memory->ewram;
        offset = address - 0x02000000;
    } else if(address >= 0x03000000 && address <= 0x03007FFC) { // IWRAM
        region = memory->iwram;
        offset = address - 0x03000000;
    } else if(address >= 0x08000000) { // Game Pak ROM
        offset = address - 0x08000000;

        if((size_t)offset + 3 >= memory->rom_size) {
            printf("ROM read out of bounds: 0x%08X\n", address);
            return 0;
        }

        region = memory->rom;
    } else {
        printf("Unhandled memory read: 0x%08X\n", address);
        return 0;
    }

    return ((uint32_t)region[offset]) | ((uint32_t)region[offset + 1] << 8) |
           ((uint32_t)region[offset + 2] << 16) | ((uint32_t)region[offset + 3] << 24);
}

void memory_write32(Memory *memory, uint32_t address, uint32_t value) {
    uint8_t *region;
    uint32_t offset;

    if(address >= 0x02000000 && address <= 0x0203FFFC) { // EWRAM
        region = memory->ewram;
        offset = address - 0x02000000;
    } else if (address >= 0x03000000 && address <= 0x03007FFC) { // IWRAM
        region = memory->iwram;
        offset = address - 0x03000000;
    } else {
        printf("Unhandled memory write: 0x%08X", address);
        return;
    }

    region[offset]     = value & 0xFF;
    region[offset + 1] = (value >> 8) & 0xFF;
    region[offset + 2] = (value >> 16) & 0xFF;
    region[offset + 3] = (value >> 24) & 0xFF;
}