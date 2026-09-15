#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    unsigned char *rom;
    size_t rom_size;
} Memory;

void memory_init(Memory *memory, unsigned char *rom, size_t rom_size);

uint32_t memory_read32(Memory *memory, uint32_t address);

#endif