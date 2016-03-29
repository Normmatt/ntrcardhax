#pragma once
#include <stdint.h>

typedef struct
{
    uint32_t offset;
    uint32_t address;
    uint32_t size;
    uint32_t type;
    uint8_t hash[0x20];
} firmEntry;

typedef struct
{
    char magic[8];
    uint32_t arm11Start;
    uint32_t arm9Start;
    uint8_t reserved[0x30];
    firmEntry entry[4];
    uint8_t signature[0x100];
} firmHeader;

typedef struct
{
    firmHeader *header;
    uint8_t* data;
    uint32_t size;
} firmCtx;

int firmOpen(firmCtx *ctx, char* filename);
uint8_t* firmGetEntryData(firmCtx *ctx, uint8_t index);
uint32_t firmGetEntrySize(firmCtx *ctx, uint8_t index);
uint8_t* firmGetData(firmCtx *ctx);
uint32_t firmGetSize(firmCtx *ctx);