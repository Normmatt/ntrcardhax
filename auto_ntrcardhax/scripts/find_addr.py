import sys
import struct

def search(binary, pattern, skip=0):
    pattern_len = len(pattern)
    for idx in xrange(len(binary)):
        if binary[idx : idx + pattern_len] != pattern:
            continue
        return struct.unpack('I', (binary[idx + skip: idx + skip + 4]))[0]

def find_ntrcard_header_address(arm9bin):
    # 7C B5               PUSH    {R2-R6,LR}
    # 2C 4C               LDR     R4, =card_struct
    # 05 00               MOVS    R5, R0
    # 2A 48               LDR     R0, =ntrcard_header
    # 26 00               MOVS    R6, R4
    return search(arm9bin, '\x7c\xb5\x2c\x4c\x05\x00\x2a\x48\x26\x00', 0xb0)

def find_rtfs_cfg_address(arm9bin):
    # 10 B5               PUSH    {R4,LR}
    # 0D 48               LDR     R0, =rtfs_cfg; dest
    # 0D 4C               LDR     R4, =ERTFS_prtfs_cfg
    # FF 22 6D 32         MOVS    R2, #0x16C; len
    # 00 21               MOVS    R1, #0; val
    # 20 60               STR     R0, [R4]
    return search(arm9bin,
                  '\x10\xb5\x0d\x48\x0d\x4c\xff\x22\x6d\x32\x00\x21\x20\x60',
                  0x38)

def find_rtfs_handle_address(arm9bin):
    # 70 B5               PUSH    {R4-R6,LR}
    # 0B 23               MOVS    R3, #0xB
    # 0B 4A               LDR     R2, =rtfs_handle
    # 00 21               MOVS    R1, #0
    # 9B 01               LSLS    R3, R3, #6
    # C4 18               ADDS    R4, R0, R3
    addr = search(arm9bin,
                  '\x70\xb5\x0b\x23\x0b\x4a\x00\x21\x9b\x01\xc4\x18',
                  0x34)
    if addr:
        return addr + 0x10

def hex_or_dead(addr):
    return hex(addr or 0xdeadbabe)

if len(sys.argv) < 2:
    print '%s <native_nand_arm9.bin>' % sys.argv[0]
    raise SystemExit(1)

with open(sys.argv[1], 'rb') as r:
    arm9bin = r.read()
    ntrcard_header_addr = find_ntrcard_header_address(arm9bin)
    rtfs_cfg_addr = find_rtfs_cfg_address(arm9bin)
    rtfs_handle_addr = find_rtfs_handle_address(arm9bin)
    print '#define NTRCARD_HEADER_ADDR %s' % hex_or_dead(ntrcard_header_addr)
    print '#define RTFS_CFG_ADDR       %s' % hex_or_dead(rtfs_cfg_addr)
    print '#define RTFS_HANDLE_ADDR    %s' % hex_or_dead(rtfs_handle_addr)
