////////////////////////////////////////////////////////////////////////////////
// File     : fat32_shared.h
// Date     : 27/07/2015
// Author   : Clément Guérin
// Copyright (c) UPMC-LIP6
////////////////////////////////////////////////////////////////////////////////

#ifndef _FAT32_SHARED
#define _FAT32_SHARED

/********************************************************************************
  This struct is used by _fat_file_info().
********************************************************************************/

typedef struct fat_file_info_s
{
    unsigned int size;      // size in bytes
    unsigned int offset;    // offset in bytes
    unsigned int is_dir;    // is the file a directory
}   fat_file_info_t;

/********************************************************************************
  This struct is used by _fat_readdir(). It describes a directory entry.
********************************************************************************/

typedef struct fat_dirent_s
{
    unsigned int cluster;   // cluster index
    unsigned int size;      // size in bytes
    unsigned int is_dir;    // is the entry a directory
    char name[36];          // entry name
}   fat_dirent_t;

/********************************************************************************
  _fat_open() flags.
********************************************************************************/

#define O_RDONLY                0x01
#define O_TRUNC                 0x10
#define O_CREATE                0x20

/********************************************************************************
  _fat_lseek() flags.
********************************************************************************/

#define SEEK_SET                0
#define SEEK_CUR                1

/********************************************************************************
  Error codes map.
********************************************************************************/

#define GIET_FAT32_OK                   (  0)
#define GIET_FAT32_NOT_INITIALIZED      (- 1)
#define GIET_FAT32_INVALID_BOOT_SECTOR  (- 2)
#define GIET_FAT32_IO_ERROR             (- 3)
#define GIET_FAT32_FILE_NOT_FOUND       (- 4)
#define GIET_FAT32_INVALID_FD           (- 5)
#define GIET_FAT32_NAME_TOO_LONG        (- 6)
#define GIET_FAT32_TOO_MANY_OPEN_FILES  (- 7)
#define GIET_FAT32_NOT_OPEN             (- 8)
#define GIET_FAT32_IS_OPEN              (- 9)
#define GIET_FAT32_READ_ONLY            (-10)
#define GIET_FAT32_NO_FREE_SPACE        (-11)
#define GIET_FAT32_INVALID_ARG          (-12)
#define GIET_FAT32_NOT_A_DIRECTORY      (-13)
#define GIET_FAT32_IS_DIRECTORY         (-14)
#define GIET_FAT32_DIRECTORY_NOT_EMPTY  (-15)
#define GIET_FAT32_MOVE_INTO_SUBDIR     (-16)
#define GIET_FAT32_FILE_EXISTS          (-17)
#define GIET_FAT32_NO_MORE_ENTRIES      (-18)
#define GIET_FAT32_BUFFER_TOO_SMALL     (-19)

#endif // _FAT32_SHARED

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

