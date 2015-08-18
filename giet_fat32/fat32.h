////////////////////////////////////////////////////////////////////////////////
// File     : fat32.h 
// Date     : 01/09/2013
// Author   : Alain Greiner / Marco Jankovic
// Copyright (c) UPMC-LIP6
////////////////////////////////////////////////////////////////////////////////

#ifndef _FAT32_H
#define _FAT32_H

#include "fat32_shared.h"
#include "giet_config.h"
#include "kernel_locks.h"

/*************** Partition Boot Sector Format **********************************/
//                                     offset |  length
#define BS_JMPBOOT                          0 ,  3
#define BS_OEMNAME                          3 ,  8
#define BPB_BYTSPERSEC                     11 ,  2
#define BPB_SECPERCLUS                     13 ,  1
#define BPB_RSVDSECCNT                     14 ,  2
#define BPB_NUMFATS                        16 ,  1
#define BPB_ROOTENTCNT                     17 ,  2
#define BPB_TOTSEC16                       19 ,  2
#define BPB_MEDIA                          21 ,  1
#define BPB_FATSZ16                        22 ,  2
#define BPB_SECPERTRK                      24 ,  2
#define BPB_NUMHEADS                       26 ,  2
#define BPB_HIDDSEC                        28 ,  4
#define BPB_TOTSEC32                       32 ,  4
#define BPB_PARTITION_TABLE               446 , 64 

// FAT 32
#define BPB_FAT32_FATSZ32                  36 ,  4
#define BPB_FAT32_EXTFLAGS                 40 ,  2
#define BPB_FAT32_FSVER                    42 ,  2
#define BPB_FAT32_ROOTCLUS                 44 ,  4
#define BPB_FAT32_FSINFO                   48 ,  2
#define BPB_FAT32_BKBOOTSEC                50 ,  2
#define BS_FAT32_DRVNUM                    64 ,  1
#define BS_FAT32_BOOTSIG                   66 ,  1
#define BS_FAT32_VOLID                     67 ,  4
#define BS_FAT32_VOLLAB                    71 , 11
#define BS_FAT32_FILSYSTYPE                82 ,  8

// Partitions
#define FIRST_PARTITION_ACTIVE            446 ,  8
#define FIRST_PARTITION_BEGIN_LBA         454 ,  4
#define FIRST_PARTITION_SIZE              458 ,  4 
#define SECOND_PARTITION_ACTIVE           462 ,  8
#define SECOND_PARTITION_BEGIN_LBA        470 ,  4
#define SECOND_PARTITION_SIZE             474 ,  4
#define THIRD_PARTITION_ACTIVE            478 ,  8
#define THIRD_PARTITION_BEGIN_LBA         486 ,  4
#define THIRD_PARTITION_SIZE              490 ,  4
#define FOURTH_PARTITION_ACTIVE           494 ,  8
#define FOURTH_PARTITION_BEGIN_LBA        502 ,  4
#define FOURTH_PARTITION_SIZE             506 ,  4    
/*******************************************************************************/

#define MBR_SIGNATURE_POSITION            510 , 2
#define MBR_SIGNATURE_VALUE               0xAA55  

/************** FAT_FS_INFO SECTOR  ********************************************/
#define FS_SIGNATURE_VALUE_1              0x52526141
#define FS_SIGNATURE_VALUE_2              0x72724161
#define FS_SIGNATURE_VALUE_3              0x000055AA  
#define FS_SIGNATURE_POSITION_1           0   , 4  
#define FS_SIGNATURE_POSITION_2           484 , 4
#define FS_SIGNATURE_POSITION_3           508 , 4  
#define FS_FREE_CLUSTERS                  488 , 4
#define FS_FREE_CLUSTER_HINT              492 , 4
/*******************************************************************************/

#define DIR_ENTRY_SIZE          32
                   
#define NAME_MAX_SIZE           31

/******* Directory Entry Structure (32 bytes) **********************************/
//                            offset | length
#define DIR_NAME                   0 , 11   // dir_entry name
#define DIR_ATTR                  11 ,  1   // attributes
#define DIR_NTRES                 12 ,  1   // reserved for the OS        
#define DIR_CRT_TIMES_TENTH       13 ,  1 
#define DIR_FST_CLUS_HI           20 ,  2   // cluster index 16 MSB bits
#define DIR_WRT_TIME              22 ,  2   // time of last write
#define DIR_WRT_DATE              24 ,  2   // date of last write
#define DIR_FST_CLUS_LO           26 ,  2   // cluster index 16 LSB bit
#define DIR_FILE_SIZE             28 ,  4   // file size (up to 4 giga bytes)
/*******************************************************************************/

/******* LFN Directory Entry Structure  (32 bytes) *****************************/
//                            offset | length
#define LDIR_ORD                   0 ,  1   // Sequence number (from 0x01 to 0x0f)    
#define LDIR_NAME_1                1 , 10   // name broken into 3 parts 
#define LDIR_ATTR                 11 ,  1   // attributes (must be 0x0F) 
#define LDIR_TYPE                 12 ,  1   // directory type (must be 0x00)
#define LDIR_CHKSUM               13 ,  1   // checksum of name in short dir  
#define LDIR_NAME_2               14 , 12 
#define LDIR_RSVD                 26 ,  2   // artifact of previous fat (must be 0)
#define LDIR_NAME_3               28 ,  4   
/*******************************************************************************/

/***********************  DIR_ATTR values  (attributes) ************************/
#define ATTR_READ_ONLY            0x01
#define ATTR_HIDDEN               0x02
#define ATTR_SYSTEM               0x04
#define ATTR_VOLUME_ID            0x08
#define ATTR_DIRECTORY            0x10
#define ATTR_ARCHIVE              0x20
#define ATTR_LONG_NAME_MASK       0x0f      // READ_ONLY|HIDDEN|SYSTEM|VOLUME_ID
/*******************************************************************************/

/********************* DIR_ORD special values **********************************/
#define FREE_ENTRY                0xE5     // this entry is free in the directory
#define NO_MORE_ENTRY             0x00     // no more entry in the directory
/*******************************************************************************/

/******************** CLuster Index Special Values *****************************/
#define FREE_CLUSTER              0x00000000
#define RESERVED_CLUSTER          0x00000001
#define BAD_CLUSTER               0x0FFFFFF7
#define END_OF_CHAIN_CLUSTER_MIN  0x0ffffff8
#define END_OF_CHAIN_CLUSTER_MAX  0x0fffffff
/*******************************************************************************/

#define FAT_INITIALIZED         0xBABEF00D

/********************************************************************************
  This struct defines a non terminal node in a 64-tree implementing a File-Cache 
  associated to an open file, or the Fat-Cache, associated to the FAT itself.
  Each node has 64 children, and we use the void* type, because the children
  can be non-terminal (fat_cache_node_t) or terminal (fat_cache_cluster_t).
********************************************************************************/

typedef struct fat_cache_node_s
{
    void*              children[64];           // pointers on the 64 children
}   fat_cache_node_t;


/********************************************************************************
  This struct defines a cluster descriptor, that is a leaf cell in a 64-tree.
  Each cluster descriptor contains a pointer on a 4K bytes buffer, and the
  lba on block device.
********************************************************************************/

typedef struct fat_cache_desc_s
{
    unsigned int       lba;                      // cluster lba on block device
    unsigned int       dirty;                    // modified if non zero
    unsigned char*     buffer;                   // pointer on the 4 Kbytes buffer
}   fat_cache_desc_t;


/********************************************************************************
  This struct defines a file/directory inode / size = 64 bytes
********************************************************************************/

typedef struct fat_inode_s
{
    struct fat_inode_s*  parent;                 // parent directory inode
    struct fat_inode_s*  next;                   // next inode in same directory
    struct fat_inode_s*  child;                  // first children inode (dir only)
    fat_cache_node_t*    cache;                  // pointer on the file_cache root
    unsigned int         cluster;                // first cluster index in FAT
    unsigned int         size;                   // number of bytes (file only)
    unsigned int         count;                  // number open if file / 0 if dir
    unsigned short       dentry;                 // directory entry index in parent
    unsigned char        levels;                 // number of levels in file_cache
    unsigned char        is_dir;                 // directory if non zero
    char                 name[32];               // file  directory name
}   fat_inode_t;

/********************************************************************************
  This struct defines a file descriptor (handler) / size = 16 bytes
********************************************************************************/

typedef struct fat_file_desc_s
{
    unsigned int         seek;                   // current offset value (bytes)
    fat_inode_t*         inode;                  // pointer on inode
    char                 allocated;              // file descriptor allocated
    char                 read_only;              // write protected
    char                 reserved[6];            // reserved
}   fat_file_desc_t;

/********************************************************************************
  This struct defines a FAT32 File system descriptor
 *******************************************************************************/

typedef struct fat_desc_s
{
    unsigned char       block_buffer[512];       // one block buffer (for FS_INFO)
    fat_file_desc_t     fd[GIET_OPEN_FILES_MAX]; // file descriptors array
    spin_lock_t         fat_lock;                // global lock protecting FAT
    fat_inode_t*        inode_tree_root;         // Inode-Tree root pointer
    fat_cache_node_t*   fat_cache_root;          // Fat_Cache root pointer
    unsigned int        fat_cache_levels;        // number of levels in Fat-Cache
    unsigned int        block_buffer_lba;        // lba of block in block_buffer
    unsigned int        initialized;             // 0xBABEF00D when FAT initialized
    unsigned int        sector_size;             // must be 512
    unsigned int        cluster_size;            // must be 4096 
    unsigned int        fat_lba;                 // lba of first FAT sector
    unsigned int        fat_sectors;             // number of sectors in FAT region
    unsigned int        data_lba;                // lba of first DATA sector  
    unsigned int        data_sectors;            // number of sectors inf DATA region 
    unsigned int        fs_info_lba;             // lba of fs_info
    unsigned int        first_free_cluster;      // free cluster with smallest index
    unsigned int        free_clusters_number;    // total number of free clusters
}   fat_desc_t;


/*********************** Extern Functions  *************************************/

extern int _fat_init();         

extern int _fat_open( char*        pathname,               // path from root
                      unsigned int flags );                // O_CREATE/O_RDONLY

extern int _fat_close( unsigned int fd_id );               // file descriptor

extern int _fat_file_info( unsigned int     fd_id,         // file descriptor
                           fat_file_info_t* info );        // file info struct

extern int _fat_read( unsigned int fd_id,                  // file descriptor  
                      void*        buffer,                 // destination buffer
                      unsigned int count ); 	           // number of bytes

extern int _fat_write( unsigned int fd_id,                 // file descriptor 
                       void*        buffer,		           // source buffer
                       unsigned int count );               // number of bytes

extern int _fat_lseek( unsigned int fd_id,                 // file descriptor
                       unsigned int offset,                // new offset value
                       unsigned int whence );              // command type

extern int _fat_remove( char*        pathname,             // path from root
                        unsigned int should_be_dir );      // for checking

extern int _fat_rename( char*  old_path,                   // path from root
                        char*  new_path );                 // path from root

extern int _fat_mkdir( char* pathname );                   // path from root

extern int _fat_opendir( char* pathname );                 // path from root

extern int _fat_closedir( unsigned int fd_id );            // file descriptor

extern int _fat_readdir( unsigned int  fd_id,              // file descriptor
                         fat_dirent_t* entry );            // directory entry

extern int _fat_load_no_cache( char*        pathname,      // path from root
                               unsigned int buffer_vbase,  // buffer base 
                               unsigned int buffer_size ); // buffer size

/*******************************************************************************/


#endif


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

