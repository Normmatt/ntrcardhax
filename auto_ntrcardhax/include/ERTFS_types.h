/*****************************************************************************
*
* EBS - RTFS (Real Time File Manager)
*
* Copyright EBS, 2007
* All rights reserved.
*
****************************************************************************/

#pragma once

//standard types
typedef unsigned char byte;     /* Don't change */
typedef unsigned short word;    /* Don't change */
typedef unsigned long dword;    /* Don't change */
#define BOOLEAN int             /* Don't change */

#if (!defined(TRUE))
#define TRUE  1                 /* Don't change */
#endif
#if (!defined(FALSE))
#define FALSE 0                 /* Don't change */
#endif

/* Structure passed from the blk_dev layer to RTFS_DEVI_device_mount() when new media comes on line */
typedef struct rtfs_devi_media_parms
{
    void  *devhandle;                   /* Handle Rtfs will pass to device_io() and other functions. devhandle is opaque to rtfs */
#define DEVICE_REMOVE_EVENT		0x01
    dword mount_flags;
    dword access_semaphore;	            /* Access semaphore for the device. */
    dword media_size_sectors;           /* Total number of addressable sectors on the media */
    dword numheads;                     /* cylinder, head, sector representation of the media. */
    dword numcyl;                       /* Note: must be valid FAT HCN values. max cyl = 1023, max heads == 255, max sectors = 63 */
    dword secptrk;
    dword sector_size_bytes;            /* Sector size in bytes: 512, 2048, etc */
    dword eraseblock_size_sectors;      /* Sectors per erase block. Set to zero for media without erase blocks */
    int   is_write_protect;             /* Set to one if the media is write protected */
    byte  *device_sector_buffer_base;
    void  *device_sector_buffer;
    dword device_sector_buffer_size;

    int   unit_number;			/* which instance of this device */
    int   device_type;			/* Used by blk dev driver layer. device mount sets it, volume mount may use it to configure buffering */

    int(*device_io)(void  *devhandle, void * pdrive, dword sector, void  *buffer, dword count, BOOLEAN reading);
    int(*device_erase)(void  *devhandle, void *pdrive, dword start_sector, dword nsectors);
    int(*device_ioctl)(void  *devhandle, void *pdrive, int opcode, int iArgs, void *vargs);
    int(*device_configure_media)(struct rtfs_media_insert_args *pmedia_parms, struct rtfs_media_resource_reply *media_config_block, int sector_buffer_required);
    int(*device_configure_volume)(struct rtfs_volume_resource_request *prequest_block, struct rtfs_volume_resource_reply *preply_block);
} RTFS_DEVI_MEDIA_PARMS;

/* Block buffer */
typedef struct blkbuff {
    struct blkbuff *pnext;  /* Used to navigate free and populated lists */
    struct blkbuff *pprev;  /* the populated list is double linked. */
                            /* Free list is not */
    struct blkbuff *pnext2; /* Each hash table entry starts a chain of these */
#define DIRBLOCK_FREE           0
#define DIRBLOCK_ALLOCATED      1
#define DIRBLOCK_UNCOMMITTED    2
    int  block_state;
    int  use_count;
    struct ddrive *pdrive;  /* Used during IO */
    struct ddrive *pdrive_owner; /* Used to distinguish scratch allocation from common pool or device */
    dword blockno;
    dword    data_size_bytes;     /* Size of the data at pointer */
    byte  *data;
} BLKBUFF;

/* contain location information for a directory */
typedef struct dirblk {
    dword  my_frstblock;      /* First block in this directory */
    dword  my_block;          /* Current block number */
#if (INCLUDE_EXFAT)	 /* Exfat dirblk extensions */
    dword  my_exNOFATCHAINfirstcluster;    /* ExFat - if non-zero we are scanning a contiguous region with no FAT chain */
    dword  my_exNOFATCHAINlastcluster;     /* ExFat - if non-zero we are scanning a contiguous region with no FAT chain */
#endif
    int     my_index;         /* dirent number in my block   */
} DIRBLK;

/* Object used to find a dirent on a disk and its parent's */
typedef struct drobj {
    struct ddrive  *pdrive;
    struct finode  *finode;
    DIRBLK  blkinfo;
    BOOLEAN isroot;      /* True if this is the root */
    BOOLEAN is_free;     /* True if on the free list */
    BLKBUFF *pblkbuff;
} DROBJ;

// HEREHERE - reduceme and clear current drive for all users if ejected
/* User structure management */
typedef struct rtfs_system_user
{
    dword         task_handle;     /* Task this is for */
    int           rtfs_errno;       /* current errno value for the task */
#if (INCLUDE_DEBUG_VERBOSE_ERRNO)
    char          *rtfs_errno_caller; /* If diagnostics enabled File name */
    long          rtfs_errno_line_number; /* If diagnostics enabled File line number */
#endif

    int           dfltdrv;          /* Default drive to use if no drive specified  1-26 == a-z */
    void *        plcwd;            /* current working directory, allocated at init time to hold NDRIVES pointers */
#if (INCLUDE_EXFATORFAT64)		/* ExFat cwd strings per drive (13 K) */
    word		  cwd_string[26][EMAXPATH_CHARS];
#endif
} RTFS_SYSTEM_USER;

/* Block buffer context */
typedef struct blkbuffcntxt {
    dword   stat_cache_hits;
    dword   stat_cache_misses;
    struct blkbuff *ppopulated_blocks; /* uses pnext/pprev */
    struct blkbuff *pfree_blocks;      /* uses pnext */
    struct blkbuff *assigned_free_buffers;
#if (INCLUDE_DEBUG_LEAK_CHECKING)
    struct blkbuff *pscratch_blocks;      /* uses pnext */
#endif
    int     num_blocks;
    int     num_free;
    int     scratch_alloc_count;
    int     low_water;
    int     num_alloc_failures;
#define BLOCK_HASHSIZE 16
#define BLOCK_HASHMASK 0xf
    struct blkbuff *blk_hash_tbl[BLOCK_HASHSIZE];  /* uses pnext2 */
} BLKBUFFCNTXT;

/* A placeholder vector must be provided by the device driver if it registers a function to poll for device changes.  */
typedef struct rtfs_devi_poll_request_vector {
    struct rtfs_devi_poll_request_vector *pnext;
    void(*poll_device_ready)(void);
} RTFS_DEVI_POLL_REQUEST_VECTOR;

/* Configuration structure. Must be filled in by the user.
see rtfscfg.c */
typedef struct rtfs_cfg {
    int  dynamically_allocated;
    /* Configuration values */
    int cfg_NDRIVES;                    /* The number of drives to support */
    int cfg_NBLKBUFFS;                  /* The number of block buffers */
    int cfg_NUSERFILES;                 /* The number of user files */
    int cfg_NDROBJS;                    /* The number of directory objects */
    int cfg_NFINODES;                   /* The number of directory inodes */
    int cfg_NUM_USERS;                  /* The number of users to support */
    int cfg_NREGIONS;                   /* The number of region management objects to support */
    dword   region_buffers_free;
    dword   region_buffers_low_water;
#if (INCLUDE_RTFS_PROPLUS)  /* config structure: ProPlus specific element*/
    int cfg_NFINODES_UEX;                /* The number of combined extended and extended 64 directory inodes */
#endif
                                         /* Core that must be provided by the user */
    struct ddrive   *mem_drive_pool;           /* Point at cfg_NDRIVES * sizeof(DDRIVE) bytes*/
    RTFS_DEVI_MEDIA_PARMS *mem_mediaparms_pool; /* Point at cfg_NDRIVES * sizeof(RTFS_DEVI_MEDIA_PARMS) bytes*/
    BLKBUFF  *mem_block_pool;               /* Point at cfg_NBLKBUFFS * sizeof(BLKBUFF) bytes*/
    byte     *mem_block_data;               /* Point at NBLKBUFFS*RTFS_CFG_DEFAULT_BLOCK_SIZE bytes */
    struct pc_file  *mem_file_pool;            /* Point at cfg_USERFILES * sizeof(PC_FILE) bytes*/
    struct finode   *mem_finode_pool;          /* Point at cfg_NFINODE * sizeof(FINODE) bytes*/
    DROBJ    *mem_drobj_pool;           /* Point at cfg_NDROBJ * sizeof(DROBJ) bytes*/
    struct region_fragment *mem_region_pool;    /* Point at cfg_NREGIONS * sizeof(REGION_FRAGMENT) bytes*/
    RTFS_SYSTEM_USER *rtfs_user_table;      	/* Point at cfg_NUM_USERS * sizeof(RTFS_SYSTEM_USER) bytes*/
    void **           rtfs_user_cwd_pointers;   /* Point at cfg_NUM_USERS * cfg_NDRIVES * sizeof(void *) bytes*/

    struct region_fragment *mem_region_freelist;



#if (INCLUDE_RTFS_PROPLUS)  /* config structure: ProPlus specific element*/
    FINODE_EXTENSION_MEMORY *mem_finode_uex_pool;  /* Point at cfg_NFINODES_UEX * sizeof(FINODE_EXTENSION_MEMORY) bytes*/
    FINODE_EXTENSION_MEMORY *mem_finode_uex_freelist;
#endif

    dword  rtfs_exclusive_semaphore;  /* Used by Rtfs to run single threaded so buffers may be shared */
#if (INCLUDE_FAILSAFE_RUNTIME)
    struct rtfs_failsafe_cfg *pfailsafe;		/* Zero unless pc_failsafe_init() was called */
    dword shared_restore_transfer_buffer_size;
    byte  *shared_restore_transfer_buffer;
    byte  *shared_restore_transfer_buffer_base;
#endif


    dword shared_user_buffer_size;
    byte *shared_user_buffer_base;
    byte *shared_user_buffer;

    /* These pointers are internal no user setup is needed */
    BLKBUFFCNTXT buffcntxt;             /* Systemwide shared buffer pool */
    struct finode   *inoroot;          /* Begining of inode pool */
    struct finode   *mem_finode_freelist;
    DROBJ    *mem_drobj_freelist;
    struct ddrive   *mem_drive_freelist;
    struct ddrive   *drno_to_dr_map[26];
    dword  userlist_semaphore;  /* Used by ERTFS for accessing the user structure list */
    dword  critical_semaphore;  /* Used by ERTFS for critical sections */
    dword  mountlist_semaphore; /* Used by ERTFS for accessing the mount list */
                                /* Note: cfg_NDRIVES semaphores are allocated and assigned to the individual
                                drive structure within routine pc_ertfs_init() */
                                /* This value is set in pc_rtfs_init(). It is the drive number of the
                                lowest (0-25) == A: - Z:. valid drive identifier in the system.
                                If the user does not set a default drive, this value will be used. */
    int    default_drive_id;
    /* Counter used to uniquely identify a drive mount. Each time an open
    succeeds this value is incremented and stored in the drive structure.
    it is used by gfirst gnext et al to ensure that the drive was not
    closed and remounted between calls */
    int drive_opencounter;
    dword rtfs_driver_errno;   /* device driver can set driver specific errno value */
    RTFS_DEVI_POLL_REQUEST_VECTOR *device_poll_list;	/* Functions registered on this are called every time the API is entered to check for status change.  */
                                                        /* Private callback functions that overrides calls to the user's callback handler */
    void(*test_drive_async_complete_cb)(int driveno, int operation, int success);

#if (1) /* ||INCLUDE_V_1_0_DEVICES == 1) */
    dword 	floppy_semaphore;
    dword 	floppy_signal;
    dword 	ide_semaphore;
#endif

} RTFS_CFG;
