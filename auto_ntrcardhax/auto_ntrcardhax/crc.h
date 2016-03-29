
#ifndef __CRC_H
#define __CRC_H

#include <cstdio>
#include <cinttypes>

extern unsigned short crc16tab[];

uint16_t CalcCrc(uint8_t *data, uint32_t length);

#endif	// __CRC_H