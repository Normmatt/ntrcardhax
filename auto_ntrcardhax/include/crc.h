
#ifndef __CRC_H
#define __CRC_H

#include <stdio.h>
#include <inttypes.h>

extern unsigned short crc16tab[];

uint16_t CalcCrc(uint8_t *data, uint32_t length);

#endif	// __CRC_H
