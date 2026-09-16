#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    unsigned char *rom;
    size_t rom_size;

    uint8_t ewram[256 * 1024];
    uint8_t iwram[32 * 1024];
} Memory;

void memory_init(Memory *memory, unsigned char *rom, size_t rom_size);

uint32_t memory_read32(Memory *memory, uint32_t address);

void memory_write32(Memory *memory, uint32_t address, uint32_t value);

#endif