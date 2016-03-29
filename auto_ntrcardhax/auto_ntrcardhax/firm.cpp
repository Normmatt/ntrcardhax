#include "firm.h"
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

int firmOpen(firmCtx *ctx, char* filename)
{
    FILE* file = fopen(filename, "rb");
    if(file)
    {
        fseek(file, 0, SEEK_END);
        ctx->size = ftell(file);
        fseek(file, 0, SEEK_SET);
        ctx->data = (uint8_t*)malloc(ctx->size);
        fread(ctx->data, 1, ctx->size, file);
        ctx->header = (firmHeader*)ctx->data;
        fclose(file);
        return 0;
    }
    else
        return -1;
}

uint8_t* firmGetEntryData(firmCtx *ctx, uint8_t index)
{
    if(index > 3) return NULL;
    return (ctx->data + ctx->header->entry[index].offset);
}

uint32_t firmGetEntrySize(firmCtx *ctx, uint8_t index)
{
    if(index > 3) return NULL;
    return ctx->header->entry[index].size;
}

uint8_t* firmGetData(firmCtx *ctx)
{
    return ctx->data;
}

uint32_t firmGetSize(firmCtx *ctx)
{
    return ctx->size;
}