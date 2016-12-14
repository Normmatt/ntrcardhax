#ifndef __NAND_ADDR_H__
#define __NAND_ADDR_H__

struct nand_configure {
    uint32_t version;
    uint32_t ntrcard_hader_addr;
    uint32_t rtfs_cfg_addr;
    uint32_t rtfs_handle_addr;
};

#endif
