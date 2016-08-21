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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <3ds.h>
#include "svchax.h"

vu32 gpu_regs[10];
vu16* NTRCARD_MCNT = 0x1EC64000;
vu32* NTRCARD_ROMCNT = 0x1EC64004;
vu64* NTRCARD_CMD = 0x1EC64008;
vu64* NTRCARD_FIFO = 0x1EC6401C;

uint32_t wrapper_adr = -1;
uint32_t map_memory = -1;
uint32_t pxi_base = -1;

u32 PXI_BASE  = -1; //virtual address of 0x10163000
u32 wrapperAdr = -1; //svc6D_GetDebugThreadParam
u32 mapMemoryAdr = -1; //MapMemory method

#define FCRAM_BASE 0xE0000000
#define ARM9_PAYLOAD_BASE 0x3F00000

extern Result svcMapMemory(u32 handle, u32 v_address, u32 size_in_pages, u32 is_io);
u32 g_backdoorResult = 0xDEADBABE;
u8* a9buffer = NULL;
u32 a9bufferSize = 0;

#define GenerateArmBLX(a,b) GenerateArmBranch(a,b,0xfa000000)
#define GenerateArmBL(a,b) GenerateArmBranch(a,b,0xeb000000)
#define GenerateArmB(a,b) GenerateArmBranch(a,b,0xea000000)
const u32 GenerateArmBranch(const u32 aSource,const u32 aTarget,const u32 base)
{
	u32 result=0;
	s32 diff=aTarget-aSource-8;
	if(diff<=33554428&&diff>=-33554432&&(diff&3)==0)
	{
		result=base|((diff>>2)&0xffffff);
	}
	else
	{
		printf("invalid arm branch\n");
	}
	return result;
}

void flush_dcache(void) {
	asm volatile (
	"MOV     R0, #0\n\t"
	"MCR     p15, 0, R0,c7,c14, 0\n\t"
	"MCR     p15, 0, R0,c7,c10, 4\n\t"
	);
}

void flush_icache(void) {
	asm volatile (
	"MOV     R0, #0\n\t"
	"MCR     p15, 0, R0,c7,c5, 0\n\t"
	"MCR     p15, 0, R0,c7,c5, 4\n\t"
	"MCR     p15, 0, R0,c7,c5, 6\n\t"
	"MCR     p15, 0, R0,c7,c10, 4\n\t"
	);
}


s32 UpdateFramebufferInfo()
{
	__asm__ volatile("cpsid aif");

	*(vu32*)(FCRAM_BASE + 0x0) = 0;
	*(vu32*)(FCRAM_BASE + 0x100) = 0x20000104;
	*(vu8*)(FCRAM_BASE + 0x104) = '/';
	*(vu8*)(FCRAM_BASE + 0x105) = 's';
	*(vu8*)(FCRAM_BASE + 0x106) = 'y';
	*(vu8*)(FCRAM_BASE + 0x107) = 's';
	*(vu8*)(FCRAM_BASE + 0x108) = 0;

	// framebuffers
	*(vu32 *)(FCRAM_BASE + 0x3FFFE00) = gpu_regs[0]; // framebuffer 1 top left
	*(vu32 *)(FCRAM_BASE + 0x3FFFE04) = gpu_regs[1]; // framebuffer 2 top left
	*(vu32 *)(FCRAM_BASE + 0x3FFFE08) = gpu_regs[2]; // framebuffer 1 top right
	*(vu32 *)(FCRAM_BASE + 0x3FFFE0C) = gpu_regs[3]; // framebuffer 2 top right
	*(vu32 *)(FCRAM_BASE + 0x3FFFE10) = gpu_regs[4]; // framebuffer 1 bottom
	*(vu32 *)(FCRAM_BASE + 0x3FFFE14) = gpu_regs[5]; // framebuffer 2 bottom
	*(vu32 *)(FCRAM_BASE + 0x3FFFE18) = gpu_regs[6]; // framebuffer select top
	*(vu32 *)(FCRAM_BASE + 0x3FFFE1C) = gpu_regs[7]; // framebuffer select bottom

	flush_dcache();
	flush_icache();

	memcpy((void*)(ARM9_PAYLOAD_BASE + FCRAM_BASE), a9buffer, a9bufferSize);

	flush_dcache();
	flush_icache();

	return 0;
}

s32 dump_chunk_wrapper()
{
	__asm__ volatile("cpsid aif");
	//ffff9000/ffff9004
	u32 kprocess = *(vu32*)0xffff9004;
	u32 kthread = kprocess+0x1C;
	g_backdoorResult = kthread;

	// patch to bl to map thread
	*(vu32*)(wrapperAdr - 0x1FF80000) = 0xE58D4000; //str r4,[sp]
	*(vu32*)(wrapperAdr + 4 - 0x1FF80000) = 0xE1A00000; //nop
	*(vu32*)(wrapperAdr + 8 - 0x1FF80000) = 0xE1A00000; //nop
	*(vu32*)(wrapperAdr + 12 - 0x1FF80000) = GenerateArmBL(wrapperAdr + 12,mapMemoryAdr); //bl MapMemory
	*(vu32*)(wrapperAdr + 16 - 0x1FF80000) = 0xE1A00000; //nop
	*(vu32*)(wrapperAdr + 20 - 0x1FF80000) = 0xE1A00000; //nop
	*(vu32*)(wrapperAdr + 24 - 0x1FF80000) = 0xE1A00000; //nop

	UpdateFramebufferInfo();

	return 0;
}


u32* pxi_send(u32 pxi_id, u32* buf, int no_recv) {
	static u32 recv_buf[0x40];

	u32   n = (buf[0] & 0x3f) + ((buf[0]>>6) & 0x3f);
	vu8*  PXI_SYNC3  = PXI_BASE+3;
	vu32* PXI_CNT    = PXI_BASE+4;
	vu32* PXI_SEND   = PXI_BASE+8;
	vu32* PXI_RECV   = PXI_BASE+12;

	u32 tmp_irqflag = (*PXI_CNT>>10) & 1;
	*PXI_CNT &= ~(1<<10);
	*PXI_CNT &= ~(1<<2);

	while(*PXI_CNT & 2);
	*PXI_SEND   = pxi_id;
	*PXI_SYNC3 |= 0x40;

	while(*PXI_CNT & 2);
	*PXI_SEND   = buf[0];

	u32 i;
	for(i=0; i<n; i++) {
		while(*PXI_CNT & 2);
		*PXI_SEND = buf[i+1];
	}

	if(no_recv)
		return NULL;

	while(*PXI_CNT & 0x100);
	if(*PXI_RECV != pxi_id)
		return NULL;

	while(*PXI_CNT & 0x100);
	u32 hdr = recv_buf[0] = *PXI_RECV;
	n = (hdr & 0x3f) + ((hdr>>6) & 0x3f);

	for(i=0; i<n; i++) {
		while(*PXI_CNT & 0x100);
		recv_buf[i+1] = *PXI_RECV;
	}

	return recv_buf;
}

s32 SendPXISSSSS()
{
	UpdateFramebufferInfo();

	u32 fs_open_archive_cmd[] = {
		0x1200C2,			//OpenArchive
		0x00000009,			//Archive idcode (SDMC)
		0x1,				//LowPath.Type
		0x1,				//LowPath.Size
		0x106,				//(LowPath.Size<<8) | 6
		0x20000100			//LowPath.Data pointer
	};

	u32 *resp = pxi_send(4, fs_open_archive_cmd, 0); // FS

	u32 fs_open_file_cmd[] = {
		0x101C2,			//OpenFile
		0x0,				//Transaction (usually 0)
		resp[2],			//Archive handle lower word
		resp[3],			//Archive handle upper word
		0x3,				//LowPath.Type
		0x3,				//LowPath.Size
		0x7,				//Open flags (1=R, 2=W, 3=RW, 4=C, 5=RC, 6=WC, 7=RWC)
		0x0,				//Attributes (usually 0)
		0x306,				//(LowPath.Size<<8) | 6
		0x20000100			//LowPath.Data pointer to pointer
	};

	resp = pxi_send(4, fs_open_file_cmd, 0); // FS

	u32 am_reload_dbs_cmd[] = {
		0x480040,			//ReloadDBS
		0x1,				//Mediatype (0=NAND, 1=SD, 2=Gamecard) only media type "SD" is allowed.
	};
	resp = pxi_send(7, am_reload_dbs_cmd, 0); // FS

	return (s32)resp;
}

/*s32 ResetCart()
{
	__asm__ volatile("cpsid aif");

	u32 fs_card_slot_poweroff[] = {
		0x280000,			//CardSlotPowerOff
	};

	u32 *resp = pxi_send(4, fs_card_slot_poweroff, 0); // FS

	u32 fs_card_slot_poweron[] = {
		0x270000,			//CardSlotPowerOn
	};

	return (s32)pxi_send(4, fs_card_slot_poweron, 1); // FS
}

s32 PowerOnCart()
{
	__asm__ volatile("cpsid aif");

	u32 fs_card_slot_poweron[] = {
		0x270000,			//CardSlotPowerOn
	};

	return (s32)pxi_send(1, fs_card_slot_poweron, 0); // FS
}
*/
s32 find_version_specific_addresses()
{
	__asm__ volatile("cpsid aif");

	for(uint32_t *ii = (uint32_t *)0xDFF80000; ii < (uint32_t *)0xE0000000; ++ii)
	{
		//svc6D_GetDebugThreadParam
		//00 50 2D E9                 STMFD   SP!, {R12,LR}
		//10 D0 4D E2                 SUB     SP, SP, #0x10
		//00 00 8D E5                 STR     R0, [SP]        ; this is the address we want
		//04 10 8D E2                 ADD     R1, SP, #4
		//08 00 8D E2                 ADD     R0, SP, #8
		if(wrapperAdr == -1 && 0xE92D5000 == *(ii) && 0xE24DD010 == *(ii + 1) && 0xE58d0000 == *(ii + 2) && 0xE28D1004 == *(ii + 3) && 0xE28D0008 == *(ii + 4))
		{
			wrapperAdr = ((uint8_t*)ii - 0xDFF80000) + 8;
			if(wrapperAdr < 0x20000)
				wrapperAdr += 0xfff00000; // 0xDFF80000;
		}
		//F0 40 2D E9                 STMFD   SP!, {R4-R7,LR}
		//14 D0 4D E2                 SUB     SP, SP, #0x14
		//03 40 A0 E1                 MOV     R4, R3
		//7C C0 9F E5                 LDR     R12, =0x1F600000
		//28 50 9D E5                 LDR     R5, [SP,#0x28]
		else if(mapMemoryAdr == -1 && 0xE92D40F0 == *(ii) && 0xE24DD014 == *(ii + 1) && 0xE1A04003 == *(ii + 2) && 0xE59FC000 == (*(ii + 3)&~0xFFF) && 0xE59D5028 == *(ii + 4))
		{
			mapMemoryAdr = (uint8_t*)ii - 0xDFF80000;

			//Dont need this on 3ds
			if(mapMemoryAdr < 0x20000)
				mapMemoryAdr += 0xfff00000; // 0xDFF80000;
		}
		//FF 5F 2D E9                 STMFD           SP!, {R0-R12,LR}
		//1F 04 51 E3                 CMP             R1, #0x1F000000
		//01 40 A0 E1                 MOV             R4, R1
		//02 50 A0 E1                 MOV             R5, R2
		//00 80 A0 E1                 MOV             R8, R0
		//05 26 81 E0                 ADD             R2, R1, R5,LSL#12
		else if(mapMemoryAdr == -1 && 0xE92D5FFF == *(ii) && 0xE351041F == *(ii + 1) && 0xE1A04001 == *(ii + 2) && 0xE1A05002 == *(ii + 3) && 0xE1A08000 == *(ii + 4) && 0xE0812605 == *(ii + 5))
		{
			mapMemoryAdr = (uint8_t*)ii - 0xDFF80000;

			//Dont need this on 3ds
			if(mapMemoryAdr < 0x20000)
				mapMemoryAdr += 0xfff00000; // 0xDFF80000;
		}
		//79 05 20 E1                 BKPT    0x59
		else if(PXI_BASE == -1 && 0xE1200579 == *(ii))
		{
			for(int jj = 0; jj < 2; jj++)
			{
				uint32_t *adr = ii + jj;
				uint32_t opcode = *(adr);

				//LDR R?, =data_area_start
				if((opcode & 0xFFFF0000) == 0xE59F0000)
				{
					uint32_t adr2 = (uint32_t)((uint8_t*)adr + ((opcode&0xFFF) + 8));
					PXI_BASE = (*(uint32_t *)adr2) + 0x10;
					PXI_BASE = *(uint32_t*)PXI_BASE;
				}
			}
		}
		//80 00 08 F1                 CPSIE   I
		//30 02 9F E5                 LDR     R0, =g_kernel_devmode
		//00 00 D0 E5                 LDRB    R0, [R0]
		//00 00 50 E3                 CMP     R0, #0
		else if(PXI_BASE == -1 && 0xF1080080 == *(ii) && 0xE59F0000 == (*(ii + 1)&~0xFFF) && 0xE5D00000 == *(ii + 2) && 0xE3500000 == *(ii + 3))
		{
			uint32_t adr2 = *(uint32_t*)((uint8_t*)ii + 4 + ((*(ii + 1)&0xFFF) + 8)) + 6;
			PXI_BASE = *(uint32_t*)(adr2);
		}
	}
	return (wrapperAdr != -1) && (mapMemoryAdr != -1) && (PXI_BASE != -1);
}

void LoadArm9Payload()
{
	FILE *file = fopen("sdmc:/load.bin","rb");
	if (file == NULL)
	{
		printf("Error load.bin doesn't exist.");
		while(1);
	}

	// seek to end of file
	fseek(file,0,SEEK_END);

	// file pointer tells us the size
	a9bufferSize = (u32)ftell(file);

	// seek back to start
	fseek(file,0,SEEK_SET);

	//allocate a buffer
	a9buffer=malloc(a9bufferSize);
	if(!a9buffer)
	{
		printf("Error failed to alloc a9buffer");
		while(1);
	}

	//read contents !
	off_t bytesRead = fread(a9buffer,1,a9bufferSize,file);

	//close the file because we like being nice and tidy
	fclose(file);
}

int main(int argc, char** argv)
{
	gfxInitDefault(); //makes displaying to screen easier

	//Initialize console on top screen. Using NULL as the second argument tells the console library to use the internal console structure as current one
	consoleInit(GFX_TOP, NULL);

	printf("Hello World!\n");

	LoadArm9Payload();

	svchax_init(true);
	if(!__ctr_svchax || !__ctr_svchax_srv) {
		printf("Failed to acquire arm11 kernel access.\n");
		goto exit;
	}

	u32* resp = svcBackdoor(find_version_specific_addresses);
	printf("wrapperAdr   : %08x\n",wrapperAdr);
	printf("mapMemoryAdr : %08x\n",mapMemoryAdr);
	printf("PXI_BASE     : %08x\n",PXI_BASE);

	printf("backdoor returned %08lx\n", (svcBackdoor(dump_chunk_wrapper), g_backdoorResult));

	Result r = svcMapMemory(g_backdoorResult, 0x1EC00000, 0x300, 1);
	printf("%08X\n", r);

	u32 old_romcnt = -1;
	int ctr = 0;
	int shall_trigger_fs = 0;

	static int do_overflow = 1; //Do this automatically
	bool card_status = 0;

	//consoleClear();

	/*u32* resp = svcBackdoor(ResetCart);

	(u32*)printf("FS Response:\n%08x %08x %08x %08x\n",
				resp[0], resp[1], resp[2], resp[3]);*/

	/*Result ret = FSUSER_CardSlotPowerOff(&card_status);
	printf("card_status = %d (ret=%08X)\n",card_status,ret);

	ret = FSUSER_CardSlotGetCardIFPowerStatus(&card_status);
	printf("power_status = %d (ret=%08X)\n",card_status,ret);*/

	while(1)
	{
		int has_changed = 0;

		if(shall_trigger_fs==1) {
			//printf("shall_trigger_fs\n");

			gfxSetScreenFormat(GFX_TOP,GSP_BGR8_OES);
			gfxSetScreenFormat(GFX_BOTTOM,GSP_BGR8_OES);
			gfxSetDoubleBuffering(GFX_TOP,0);
			gfxSetDoubleBuffering(GFX_BOTTOM,0);
			gfxSwapBuffersGpu();
			gspWaitForVBlank();

			svcBackdoor(SendPXISSSSS);

			has_changed = 1;
			shall_trigger_fs = 0;
		}

		if(ctr++ == 0x2000) {
			ctr = 0;

			if(!aptMainLoop())
				break;

			//Scan all the inputs. This should be done once for each frame
			hidScanInput();

			//hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
			//u32 kDown = hidKeysDown();

			if(keysHeld() & KEY_START) break; // break in order to return to hbmenu

			if(keysHeld() & KEY_Y)
			{
				do_overflow = 1;
				printf("Do Overflow\n");
			}

			if(keysHeld() & KEY_X)
			{
				static int once = 0;

				//if(!once)
				{
					printf("Triggering FS..\n");
					has_changed = 1;
					shall_trigger_fs = 1;
					once = 1;
				}
			}

		}

		u32 romcnt = *NTRCARD_ROMCNT;
		if(old_romcnt != romcnt) {
			if(shall_trigger_fs > 1) shall_trigger_fs--;

			if(do_overflow && ((romcnt & 0x1fff) == 0x1fff)) {
				//printf("BEFORE %08X\n", *NTRCARD_ROMCNT);

				*NTRCARD_ROMCNT = ((6 << 24) & 0x7FFFFFF) | 0x883F1FFF;
				u32 temp = *NTRCARD_ROMCNT;

				printf("Overflow! %08X EXPECTED %08X\n", temp, ((6 << 24) & 0x7FFFFFF) | 0x883F1FFF);
				has_changed = 1;
				shall_trigger_fs = 5;
			}

			old_romcnt = romcnt;

			printf("%08X\n", *(vu32 *)0x1EC64004);
			//printf("%08X %08X\n", *(vu32 *)0x1EC64008, *(vu32 *)0x1EC6400C);
		}

		if(has_changed)
		{
			// Flush and swap framebuffers
			gfxFlushBuffers();
			gfxSwapBuffers();

			//Wait for VBlank
			gspWaitForVBlank();

			GSPGPU_ReadHWRegs(0x400468, (u32*)&gpu_regs[0], 8); // framebuffer 1 top left & framebuffer 2 top left
			GSPGPU_ReadHWRegs(0x400494, (u32*)&gpu_regs[2], 8); // framebuffer 1 top right & framebuffer 2 top right
			GSPGPU_ReadHWRegs(0x400568, (u32*)&gpu_regs[4], 8); // framebuffer 1 bottom & framebuffer 2 bottom
			GSPGPU_ReadHWRegs(0x400478, (u32*)&gpu_regs[6], 4); // framebuffer select top
			GSPGPU_ReadHWRegs(0x400578, (u32*)&gpu_regs[7], 4); // framebuffer select bottom

			svcBackdoor(UpdateFramebufferInfo);
		}
	}

	//closing all services even more so
exit:
	gfxExit();
	return 0;
}
