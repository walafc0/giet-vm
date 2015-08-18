//////////////////////////////////////////////////////////////////////////////////
// Date     : 01/06/2015
// Authors  : Alain Greiner
// Copyright (c) UPMC-LIP6
//////////////////////////////////////////////////////////////////////////////////
// The fat32.h and fat32.c files define a library of access functions
// to a FAT32 disk on a block device. It is intended to be used by both 
// the boot code and the kernel code.
//////////////////////////////////////////////////////////////////////////////////
// Implementation notes:
// 1. the "lba" (Logical Block Address) is the physical sector index on
//    the block device. The physical sector size is supposed to be 512 bytes.
// 2. the "cluster" variable is actually a cluster index. A cluster contains
//    8 sectors (4K bytes) and the cluster index is a 32 bits word.
// 3. Each file or directory referenced by the software is represented
//    by an "inode". The set of "inodes" is organised as a tree, that is 
//    a sub-tree of the complete file system existing on the block device.
// 4. A given file can be referenced by several software tasks, and each task
//    will use a private handler, called a "file descriptor", allocated by the OS
//    when the task open the file, that is organised as an indexed array.
// 5. This FAT32 library implements (N+1) caches : one private "File_ Cache" 
//    for each referenced file or directory, and a specific "Fat_Cache" for 
//    the FAT itself. Each cache contain a variable number of clusters that are
//    dynamically allocated when they are accessed, and organised as a 64-Tree.
//////////////////////////////////////////////////////////////////////////////////
// General Debug Policy:
// The global variable GIET_DEBUG_FAT is defined in the giet_config.h file.
// The debug is activated if (proctime > GIET_DEBUG_FAT) && (GIET_DEBUG_FAT != 0)
// The GIET_DEBUG_FAT bit 0 defines the level of debug:
//    if   (GIET_DEBUG_FAT & 0x1)    => detailed debug 
//    else                           => external functions only
//////////////////////////////////////////////////////////////////////////////////

#include <giet_config.h>
#include <hard_config.h>
#include <fat32.h>
#include <utils.h>
#include <vmem.h>
#include <kernel_malloc.h>
#include <bdv_driver.h>
#include <hba_driver.h>
#include <sdc_driver.h>
#include <rdk_driver.h>
#include <mmc_driver.h>
#include <tty0.h>

//////////////////////////////////////////////////////////////////////////////////
//               Global variables 
//////////////////////////////////////////////////////////////////////////////////

// Fat-Descriptor
__attribute__((section(".kdata")))
fat_desc_t _fat __attribute__((aligned(64))); 

// buffer used by boot code as a simple cache when scanning FAT
__attribute__((section(".kdata")))
unsigned char  _fat_buffer_fat[4096] __attribute__((aligned(64)));

// buffer used by boot code as a simple cache when scanning a directory in DATA region
__attribute__((section(".kdata")))
unsigned char  _fat_buffer_data[4096] __attribute__((aligned(64)));

// lba of cluster in fat_buffer_fat
__attribute__((section(".kdata")))
unsigned int   _fat_buffer_fat_lba;

// lba of cluster in fat_buffer_data 
__attribute__((section(".kdata")))
unsigned int   _fat_buffer_data_lba;

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//                  Static functions declaration
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////
// This debug function displays the content of a 512 bytes buffer "buf",
// with an identifier defined by the "string" and "block_id" arguments.
///////////////////////////////////////////////////////////////////////////////////

#if GIET_DEBUG_FAT
static void _display_one_block( unsigned char* buf,
                                char*          string,
                                unsigned int   block_id );
#endif

//////////////////////////////////////////////////////////////////////////////////
// This debug function displays the FAT descriptor. 
//////////////////////////////////////////////////////////////////////////////////

#if GIET_DEBUG_FAT
static void _display_fat_descriptor();
#endif

/////////////////////////////////////////////////////////////////////////////////
// This debug function displays the sequence of clusters allocated to a 
// file (or directory) identified by the "inode" argument.
/////////////////////////////////////////////////////////////////////////////////

#if GIET_DEBUG_FAT
static void _display_clusters_list( fat_inode_t* inode );
#endif

/////////////////////////////////////////////////////////////////////////////////
// The following function transfers one or several blocks between the device 
// and a memory buffer identified by a virtual address.
// It computes the memory buffer physical address, and calls the proper 
// IOC driver depending on the subtype (BDV / HBA / SDC / SPI / RDK). 
// The use_irq argument allows to activate the descheduling mode,
// if it supported by the IOC driver subtype
// It returns 0 on success.
// It returns -1 on error.
/////////////////////////////////////////////////////////////////////////////////

static int _fat_ioc_access( unsigned int use_irq,
                            unsigned int to_mem,
                            unsigned int lba,
                            unsigned int buf_vaddr,
                            unsigned int count );

//////////////////////////////////////////////////////////////////////////////////
// The following function returns in the "desc" argument a pointer on a buffer 
// descriptor contained in a File_Cache, or in the Fat_Cache. 
// The searched buffer is idenfified by the "inode" and "cluster_id" arguments.
// If the "inode" pointer is not NULL, the searched cache is a File-Cache.
// If the "inode" pointer is NULL, the searched cache is the Fat-Cache, 
// The "cluster_id" argument is the buffer index in the file (or in the FAT). 
// In case of miss, it allocate a 4 Kbytes buffer and a cluster descriptor 
// from the local kernel heap, and calls the _fat_ioc_access() function to load 
// the missing cluster from the block device.
// It returns 0 on success.
// It returns 1 on error.
//////////////////////////////////////////////////////////////////////////////////

static unsigned int _get_buffer_from_cache( fat_inode_t*        inode,
                                            unsigned int        cluster_id,
                                            fat_cache_desc_t**  desc );

////////////////////////////////////////////////////////////////////////////////
// This function extract a (partial) name from a LFN directory entry.
////////////////////////////////////////////////////////////////////////////////

static void _get_name_from_long( unsigned char* buffer, 
                                 char*          name );

////////////////////////////////////////////////////////////////////////////////
// The following function extract a name from a NORMAL directory entry.
////////////////////////////////////////////////////////////////////////////////

static void _get_name_from_short( unsigned char* buffer,
                                  char*          name );

//////////////////////////////////////////////////////////////////////////////////
// This function returns the number of levels of a File-Cache (or Fat-Cache)
// from the size of the file (or FAT).
//////////////////////////////////////////////////////////////////////////////////

static inline unsigned int _get_levels_from_size( unsigned int size );

///////////////////////////////////////////////////////////////////////////////////
// The following function analyses the "pathname" argument, from the character 
// defined by the "nb_read" argument.
// It copies the found name in the "name" buffer (without '/'), 
// and updates the "nb_read" argument.
// It returns 0 on success.
// It returns 1 if one name length > NAME_MAX_SIZE characters
///////////////////////////////////////////////////////////////////////////////////

static unsigned int _get_name_from_path( char*          pathname,
                                         char*          name,
                                         unsigned int*  nb_read );

////////////////////////////////////////////////////////////////////////////////
// The following function scan the "pathname" argument, and copies in the
// "name" buffer the last name in path (leaf name).
// It returns 0 on success.
// It returns 1 if one name length > NAME_MAX_SIZE characters
////////////////////////////////////////////////////////////////////////////////
static unsigned int _get_last_name( char*   pathname,
                                    char*   name );

//////////////////////////////////////////////////////////////////////////////////
// The following function accesses the Fat-Cache and returns in the "value"
// argument the content of the FAT slot identified by the "cluster" argument.
// It loads the missing cluster from block device into cache in case of miss.
// It returns 0 on success.
// It returns 1 on error.
//////////////////////////////////////////////////////////////////////////////////

static unsigned int _get_fat_entry( unsigned int  cluster,
                                    unsigned int* value );

//////////////////////////////////////////////////////////////////////////////////
// The following function writes a new "value" in the Fat-Cache, in the slot 
// identified by the "cluster" argument.  
// It loads the missing cluster from block device into cache in case of miss.
// It returns 0 on success,
// It returns 1 on error.
//////////////////////////////////////////////////////////////////////////////////

static unsigned int _set_fat_entry( unsigned int  cluster,
                                    unsigned int  value );

//////////////////////////////////////////////////////////////////////////////////
// The following function introduces the inode identified by the "child" argument,
// as a new child of the "parent" inode in the Inode-Tree.
// All checking are supposed to be done by the caller.
// Nor the File-Cache, neither the block device are modified.
//////////////////////////////////////////////////////////////////////////////////

static void _add_inode_in_tree( fat_inode_t*  child,
                                fat_inode_t*  parent );

//////////////////////////////////////////////////////////////////////////////////
// The following function removes one inode identified by the "inode" argument 
// from the Inode-Tree. All checking are supposed to be done by the caller.
// Nor the File-Cache, neither the block device are modified.
//////////////////////////////////////////////////////////////////////////////////

static void _remove_inode_from_tree( fat_inode_t* inode );

//////////////////////////////////////////////////////////////////////////////////
// This recursive function scan one File-Cache (or Fat-Cache) from root to leaves,
// to writes all dirty clusters to block device, and reset the dirty bits.
// The cache is identified by the "root" an "levels" arguments.
// The "string" argument is only used for debug : inode name or Fat.
// It returns 0 on success.
// It returns 1 on error.
//////////////////////////////////////////////////////////////////////////////////

static unsigned int _update_device_from_cache( unsigned int      levels,
                                               fat_cache_node_t* root,
                                               char*             string );

//////////////////////////////////////////////////////////////////////////////////
// The following function accesses directly the FS_INFO block on the block device, 
// to update the "first_free_cluster" and "free_clusters_number" values,
// using only the Fat-Descriptor single block buffer.
// It return 0 on success.
// It return 1 on error.
//////////////////////////////////////////////////////////////////////////////////

static unsigned int _update_fs_info();

//////////////////////////////////////////////////////////////////////////////
// The following function read a data field (from one to four bytes) 
// from an unsigned char[] buffer, taking endianness into account. 
// The analysed field is defined by the "offset" and "size" arguments.
//////////////////////////////////////////////////////////////////////////////

static unsigned int _read_entry( unsigned int    offset,
                                 unsigned int    size,
                                 unsigned char*  buffer,
                                 unsigned int    little_indian );

//////////////////////////////////////////////////////////////////////////////////
// The following function returns the lba of first sector in DATA region 
// from the cluster index. The cluster index must be larger than 2. 
//////////////////////////////////////////////////////////////////////////////////

static unsigned int _cluster_to_lba( unsigned int cluster );

//////////////////////////////////////////////////////////////////////////////////
// The following function returns in the "nb_entries" argument the number of files
// (or directories) contained in a directory identified by the "inode " pointer.
// It returns 0 on success.
// It returns 1 on error.
//////////////////////////////////////////////////////////////////////////////////

static unsigned int _get_nb_entries( fat_inode_t*   inode,
                                     unsigned int*  nb_entries );

//////////////////////////////////////////////////////////////////////////////////
// The following function search in the directory identified by the "parent"
// inode pointer a child (file or directory) identified by its "name".
// It returns in the "inode" argument the searched child inode pointer.
// If the searched name is not found in the Inode-Tree, the function accesses
// the "file_cache" associated to the parent directory.
// If the child exists on block device, the Inode-Tree is updated, and
// a success code is returned.
// If the file/dir does not exist on block device, a error code is returned.
// It returns 0 if inode found.
// It returns 1 if inode not found.
// It returns 2 on error in cache access.
//////////////////////////////////////////////////////////////////////////////////

static unsigned int _get_child_from_parent( fat_inode_t*   parent,
                                            char*          name,
                                            fat_inode_t**  inode ); 

/////////////////////////////////////////////////////////////////////////////////
// For a file (or a directory) identified by the "pathname" argument, the
// following function returns in the "inode" argument the inode pointer 
// associated to the searched file (or directory), with code (0).
// If the searched file (or directory) is not found, but the parent directory 
// is found, it returns in the "inode" argument the pointer on the parent inode,
// with code (1).  Finally, code (2) and code (3) are error codes.
// Both the Inode-Tree and the involved Cache-Files are updated from the block
// device in case of miss on one inode during the search in path.
// Neither the Fat-Cache, nor the block device are updated.
// It returns 0 if searched inode found
// It returns 1 if searched inode not found but parent directory found
// It returns 2 if searched inode not found and parent directory not found
// It returns 3 if one name too long
/////////////////////////////////////////////////////////////////////////////////

static unsigned int _get_inode_from_path( char*          pathname,
                                          fat_inode_t**  inode );

//////////////////////////////////////////////////////////////////////////////////
// The following function checks if node "a" is an ancestor of inode "b".
// It returns 0 on failure.
// It returns 1 otherwise.
//////////////////////////////////////////////////////////////////////////////////

static unsigned int _is_ancestor( fat_inode_t* a,
                                  fat_inode_t* b);

//////////////////////////////////////////////////////////////////////////////////
// This function computes the length and the number of LFN entries required
// to store a node name in the "length" and "nb_lfn" arguments.
// Short name (less than 13 characters) require 1 LFN entry.
// Medium names (from 14 to 26 characters require 2 LFN entries.
// Large names (up to 31 characters) require 3 LFN entries. 
// It returns 0 on success.
// It returns 1 if length larger than 31 characters.
//////////////////////////////////////////////////////////////////////////////////

static unsigned int _check_name_length( char* name,
                                        unsigned int* length,
                                        unsigned int* nb_lfn );

//////////////////////////////////////////////////////////////////////////////////
// For a node identified by the "inode" argument, this function updates the
// "size" and "cluster" values in the entry of the parent directory File-Cache.
// It set the dirty bit in the modified buffer of the parent directory File-Cache.
//////////////////////////////////////////////////////////////////////////////////

static unsigned int _update_dir_entry( fat_inode_t*  inode );

//////////////////////////////////////////////////////////////////////////////////
// The following function add new "child" in Cache-File of "parent" directory.
// It accesses the File_Cache associated to the parent directory, and scan the
// clusters allocated to this directory to find the NO_MORE entry.
// This entry will be the first modified entry in the directory.
// Regarding the name storage, it uses LFN entries for all names. 
// Therefore, it writes 1, 2, or 3 LFN entries (depending on the child name
// actual length, it writes one NORMAL entry, and writes the new NOMORE entry.
// It updates the dentry field in the child inode.
// It set the dirty bit for all modified File-Cache buffers.
// The block device is not modified by this function.
//////////////////////////////////////////////////////////////////////////////////

static unsigned int _add_dir_entry( fat_inode_t* child,
                                    fat_inode_t* parent );

//////////////////////////////////////////////////////////////////////////////////
// The following function invalidates all dir_entries associated to the "inode"
// argument from its parent directory.
// It set the dirty bit for all modified buffers in parent directory Cache-File.
// The inode itself is not modified by this function.
// The block device is not modified by this function.
//////////////////////////////////////////////////////////////////////////////////

static unsigned int _remove_dir_entry( fat_inode_t*  inode );

//////////////////////////////////////////////////////////////////////////////////
// The following function add the special entries "." and ".." in the File-Cache
// of the directory identified by the "child" argument.
// The parent directory is defined by the "parent" argument.
// The child directory File-Cache is supposed to be empty.
// We use two NORMAL entries for these "." and ".." entries.
// The block device is not modified by this function.
//////////////////////////////////////////////////////////////////////////////////

static void _add_special_directories( fat_inode_t* child,
                                      fat_inode_t* parent );

//////////////////////////////////////////////////////////////////////////////////
// The following function releases all clusters allocated to a file or directory,
// from the cluster index defined by the "cluster" argument, until the end 
// of the FAT linked list.
// It calls _get_fat_entry() and _set_fat_entry() functions to scan the FAT,
// and to update the clusters chaining.
// The FAT region on block device is updated.
// It returns 0 on success.
// It returns 1 on error.
//////////////////////////////////////////////////////////////////////////////////

static unsigned int _clusters_release( unsigned int cluster );

//////////////////////////////////////////////////////////////////////////////////
// This function allocate "nb_clusters_more" new clusters to a file (or directory)
// identified by the "inode" pointer. It allocates also the associated buffers
// and buffer descriptors in the Cache-File.
// It calls _get_fat_entry() and _set_fat_entry() functions to update the
// clusters chaining in the Cache-Fat. The FAT region on block device is updated.
// It returns 0 on success.
// It returns 1 on error.
//////////////////////////////////////////////////////////////////////////////////

static unsigned int _clusters_allocate( fat_inode_t* inode, 
                                        unsigned int nb_clusters_current,
                                        unsigned int nb_clusters_more );

//////////////////////////////////////////////////////////////////////////////////
// This recursive function scans one File-Cache (or Fat-Cache) from root to
// leaves. All memory allocated for 4KB buffers, and buffer descriptors (in
// leaves) is released, along with the 64-Tree structure (root node is kept).
// The cache is identified by the "root" and "levels" arguments.
// It should not contain any dirty clusters.
//////////////////////////////////////////////////////////////////////////////////

static void _release_cache_memory( fat_cache_node_t*  root,
                                   unsigned int       levels );

//////////////////////////////////////////////////////////////////////////////////
// The following function allocates and initializes a new Fat-Cache node.
// Its first child can be specified (used when adding a cache level).
// The Fat-Cache is initialized as empty: all children set to NULL.
// It returns a pointer to a new Fat-Cache node.
//////////////////////////////////////////////////////////////////////////////////

static fat_cache_node_t* _allocate_one_cache_node( fat_cache_node_t* first_child );

//////////////////////////////////////////////////////////////////////////////////
// The following function allocates and initializes a new inode, 
// using the values defined by the arguments.
// If the "cache_allocate" argument is true ans empty cache is allocated.
// It returns a pointer on the new inode.
//////////////////////////////////////////////////////////////////////////////////

static fat_inode_t* _allocate_one_inode( char*        name,
                                         unsigned int is_dir,
                                         unsigned int cluster,
                                         unsigned int size,
                                         unsigned int count,
                                         unsigned int dentry,
                                         unsigned int cache_allocate );

//////////////////////////////////////////////////////////////////////////////////
// The following function allocates one 4 Kbytes buffer and associated cluster 
// descriptor for the file (or directory) identified by the "inode" argument,
// and updates the Cache_File slot identified by the "cluster_id" argument. 
// The File-Cache slot must be empty.
// It updates the cluster descriptor, using the "cluster" argument, that is 
// the cluster index in FAT.  The cluster descriptor dirty field is set.
// It traverse the 64-tree Cache-file from top to bottom to find the last level.
//////////////////////////////////////////////////////////////////////////////////

static void _allocate_one_buffer( fat_inode_t*    inode,
                                  unsigned int    cluster_id,
                                  unsigned int    cluster );

//////////////////////////////////////////////////////////////////////////////////
// The following function allocates one free cluster from the FAT "heap" of free 
// clusters, and returns the cluster index in the "cluster" argument.
// It updates the FAT slot, and the two FAT global variables: first_free_cluster,
// and free_clusters_number.
// It returns 0 on success.
// It returns 1 on error.
//////////////////////////////////////////////////////////////////////////////////

static unsigned int _allocate_one_cluster( unsigned int*  cluster );

/////////////////////////////////////////////////////////////////////////////
// This function remove from the file system a file or a directory 
// identified by the "inode" argument. 
// The remove condition must be checked by the caller.
// The relevant lock(s) must have been taken by te caller.
// It returns 0 on success.
// It returns 1 on error.
/////////////////////////////////////////////////////////////////////////////

static unsigned int _remove_node_from_fs( fat_inode_t* inode );

/////////////////////////////////////////////////////////////////////////////
// This function return the cluster index and the size for a file 
// identified by the "pathname" argument, scanning directly the block
// device DATA region.
// It is intended to be called only by the _fat_load_no_cache() function, 
// it does not use the dynamically allocated File Caches, but uses only
// the 4 Kbytes _fat_buffer_data. 
// It returns 0 on success.
// It returns 1 on error.
/////////////////////////////////////////////////////////////////////////////

static unsigned int _file_info_no_cache( char*          pathname,
                                         unsigned int*  file_cluster,
                                         unsigned int*  file_size );

/////////////////////////////////////////////////////////////////////////////
// This function scan directly the FAT region on the block device, 
// and returns in the "next" argument the value stored in the fat slot 
// identified by the "cluster" argument.
// It is intended to be called only by the _fat_load_no_cache() function,
// as it does not use the dynamically allocated Fat-Cache, but uses only 
// the 4 Kbytes _fat_buffer_fat.
// It returns 0 on success.
// It returns 1 on error.
/////////////////////////////////////////////////////////////////////////////

static unsigned int _next_cluster_no_cache( unsigned int   cluster,
                                            unsigned int*  next );


//////////////////////////////////////////////////////////////////////////////////
// The following functions return the length or the size of a FAT field, 
// identified by an (offset,length) mnemonic defined in the fat32.h file.
//////////////////////////////////////////////////////////////////////////////////

static inline int get_length( int offset , int length ) { return length; }

static inline int get_offset( int offset , int length ) { return offset; }





//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//                  Static functions definition
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

#if GIET_DEBUG_FAT
///////////////////////////////////////////////////
static void _display_one_block( unsigned char* buf,
                                char*          string,
                                unsigned int   block_id )
{
    unsigned int line;
    unsigned int word;

    _printf("\n***  <%s>  block %x  ***********************************\n",
            string , block_id );

    for ( line = 0 ; line < 16 ; line++ )
    {
        // display line index 
        _printf("%x : ", line );

        // display 8*4 bytes hexa
        for ( word=0 ; word<8 ; word++ )
        {
            unsigned int byte  = (line<<5) + (word<<2);
            unsigned int hexa  = (buf[byte  ]<<24) |
                                 (buf[byte+1]<<16) |
                                 (buf[byte+2]<< 8) |
                                 (buf[byte+3]      );
            _printf(" %X |", hexa );
        }
        _printf("\n");
    }
    _printf("*******************************************************************\n"); 
} // end _display_one_block()  
#endif



#if GIET_DEBUG_FAT
/////////////////////////////////////
static void _display_fat_descriptor()
{
    _printf("\n###############  FAT DESCRIPTOR  ################################"  
            "\nFAT initialized                  %x"
            "\nBlock Size  (bytes)              %x"
            "\nCluster Size  (bytes)            %x"
            "\nFAT region first lba             %x"
            "\nFAT region size (blocks)         %x"
            "\nDATA region first lba            %x"
            "\nDATA region size (blocks)        %x"
            "\nNumber of free clusters          %x"
            "\nFirst free cluster index         %x" 
            "\nFat_cache_levels                 %d" 
            "\n#################################################################\n",
            _fat.initialized,
            _fat.sector_size,
            _fat.cluster_size,
            _fat.fat_lba,
            _fat.fat_sectors,
            _fat.data_lba,
            _fat.data_sectors,
            _fat.free_clusters_number,
            _fat.first_free_cluster,
            _fat.fat_cache_levels );

} // end _display_fat_descriptor()
#endif



#if GIET_DEBUG_FAT
////////////////////////////////////////////////////////
static void _display_clusters_list( fat_inode_t* inode )
{
    _printf("\n**************** clusters for <%s> ***********************\n", inode->name );
    unsigned int next;
    unsigned int n       = 0;
    unsigned int current = inode->cluster;
    while( (current < END_OF_CHAIN_CLUSTER_MIN) && (n < 1024) )
    {
        _get_fat_entry( current , &next );
        _printf(" > %X", current );
        n++;
        if ( (n & 0x7) == 0 ) _printf("\n");
        current = next;
    }
    _printf("\n");
}  // end _display_clusters_list()
#endif



/////////////////////////////////////////////////////////////////////////////////
static int _fat_ioc_access( unsigned int use_irq,       // descheduling if non zero
                            unsigned int to_mem,        // read / write
                            unsigned int lba,           // first sector on device
                            unsigned int buf_vaddr,     // memory buffer vaddr
                            unsigned int count )        // number of sectors
{
    // compute memory buffer physical address
    unsigned int       flags;         // for _v2p_translate
    unsigned long long buf_paddr;     // buffer physical address 

    if ( ((_get_mmu_mode() & 0x4) == 0 ) || USE_IOC_RDK )  // identity
    { 
        buf_paddr = (unsigned long long)buf_vaddr;
    }
    else                                // V2P translation required
    {
        buf_paddr = _v2p_translate( buf_vaddr , &flags );
    }

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_ioc_access(): enters at cycle %d\n"
        "  to_mem = %d / vaddr = %x / paddr = %l / sectors = %d / lba = %x\n",
        _get_proctime(), to_mem, buf_vaddr, buf_paddr, count, lba );
#endif


#if GIET_NO_HARD_CC     // L1 cache inval (virtual addresses)
    if ( to_mem ) _dcache_buf_invalidate( buf_vaddr, count<<9 );
#endif


#if   ( USE_IOC_BDV )   // call the proper driver
    return( _bdv_access( use_irq , to_mem , lba , buf_paddr , count ) ); 
#elif ( USE_IOC_HBA )
    return( _hba_access( use_irq , to_mem , lba , buf_paddr , count ) );
#elif ( USE_IOC_SDC )
    return( _sdc_access( use_irq , to_mem , lba , buf_paddr , count ) );
#elif ( USE_IOC_SPI )
    return( _spi_access( use_irq , to_mem , lba , buf_paddr , count ) );
#elif ( USE_IOC_RDK )
    return( _rdk_access( use_irq , to_mem , lba , buf_paddr , count ) );
#else
    _printf("\n[FAT ERROR] _fat_ioc_access(): no IOC driver\n");
    _exit();
#endif

}  // end _fat_ioc_access()




/////////////////////////////////////////////////////////////////////
static inline unsigned int _get_levels_from_size( unsigned int size )
{ 
    if      ( size <= (1<<18) ) return 1;     // 64 clusters == 256 Kbytes
    else if ( size <= (1<<24) ) return 2;     // 64 * 64 clusters => 16 Mbytes
    else if ( size <= (1<<30) ) return 3;     // 64 * 64 * 64 cluster => 1 Gbytes
    else                        return 4;     // 64 * 64 * 64 * 64 clusters 
}



////////////////////////////////////////////////////////
static unsigned int _read_entry( unsigned int    offset,
                                 unsigned int    size,
                                 unsigned char*  buffer,
                                 unsigned int    little_endian )
{
    unsigned int n;
    unsigned int res  = 0;

    if ( little_endian)
    {
        for( n = size ; n > 0 ; n-- ) res = (res<<8) | buffer[offset+n-1];
    }
    else
    {
        for( n = 0 ; n < size ; n++ ) res = (res<<8) | buffer[offset+n];
    }
    return res;

}  // end _read_entry



//////////////////////////////////////////////////////////////////
static inline unsigned int _cluster_to_lba( unsigned int cluster )       
{
    if ( cluster < 2 )
    { 
        _printf("\n[FAT ERROR] _cluster_to_lba(): cluster smaller than 2\n");
        _exit();
    }

   return  ((cluster - 2) << 3) + _fat.data_lba;
}


//////////////////////////////////////////////////////
static inline unsigned char _to_lower(unsigned char c)
{
   if (c >= 'A' && c <= 'Z') return (c | 0x20);
   else                      return c;
}


//////////////////////////////////////////////////////
static inline unsigned char _to_upper(unsigned char c)
{
   if (c >= 'a' && c <= 'z') return (c & ~(0x20));
   else                      return c;
}



///////////////////////////////////////////////////////////////////////////
static unsigned int _get_name_from_path( char*          pathname,  // input
                                         char*          name,      // output
                                         unsigned int*  nb_read )  // input & output	
{
    // skip leading "/" character
    if ( pathname[*nb_read] == '/' ) *nb_read = *nb_read + 1;

    // initialises current indexes
    unsigned int i = *nb_read;
    unsigned int j = 0;
   
    while ( (pathname[i] != '/') && (pathname[i] != 0) )
    {
        name[j++] = pathname[i++];    
        if ( j > NAME_MAX_SIZE ) return 1;
    }

    // set end of string
    name[j] = 0;

    // skip trailing "/" character
    if ( pathname[i] == '/' ) *nb_read += j+1;
    else                      *nb_read += j;

    return 0;
}



////////////////////////////////////////////////////////////////////
static unsigned int _get_last_name( char*   pathname,       // input
                                    char*   name )          // output
{
    unsigned int nb_read = 0;      
    while ( pathname[nb_read] != 0 )
    {
        if ( _get_name_from_path( pathname, name, &nb_read ) ) return 1;
    }

    return 0;
}   // end _get_last_name()



////////////////////////////////////////////////////////////////////////////////
static void _get_name_from_short( unsigned char* buffer,  // input:  SFN dir_entry
                                  char*          name )   // output: name
{
    unsigned int i;
    unsigned int j = 0;

    // get name
    for ( i = 0; i < 8 && buffer[i] != ' '; i++ )
    {
        name[j] = _to_lower( buffer[i] );
        j++;
    }

    // get extension
    for ( i = 8; i < 8 + 3 && buffer[i] != ' '; i++ )
    {
        // we entered the loop so there is an extension. add the dot
        if ( i == 8 )
        {
            name[j] = '.';
            j++;
        }

        name[j] = _to_lower( buffer[i] );
        j++;
    }

    name[j] = '\0';
}

///////////////////////////////////////////////////////////////////////////////
static void _get_name_from_long( unsigned char*  buffer, // input : LFN dir_entry
                                 char*           name )  // output : name
{
    unsigned int   name_offset         = 0;
    unsigned int   buffer_offset       = get_length(LDIR_ORD);
    unsigned int   l_name_1            = get_length(LDIR_NAME_1);
    unsigned int   l_name_2            = get_length(LDIR_NAME_2);
    unsigned int   l_name_3            = get_length(LDIR_NAME_3);
    unsigned int   l_attr              = get_length(LDIR_ATTR);
    unsigned int   l_type              = get_length(LDIR_TYPE);
    unsigned int   l_chksum            = get_length(LDIR_CHKSUM);
    unsigned int   l_rsvd              = get_length(LDIR_RSVD);

    unsigned int j            = 0;
    unsigned int eof          = 0;

    while ( (buffer_offset != DIR_ENTRY_SIZE)  && (!eof) )
    {
        while (j != l_name_1 && !eof )
        {
            if ( (buffer[buffer_offset] == 0x00) || 
                 (buffer[buffer_offset] == 0xFF) )
            {
                eof = 1;
                continue;
            }
            name[name_offset] = buffer[buffer_offset];
            buffer_offset += 2;
            j += 2;
            name_offset++;
        }

        buffer_offset += (l_attr + l_type + l_chksum);
        j = 0;

        while (j != l_name_2 && !eof )
        {
            if ( (buffer[buffer_offset] == 0x00) || 
                 (buffer[buffer_offset] == 0xFF) )
            {
                eof = 1;
                continue;
            }
            name[name_offset] = buffer[buffer_offset];
            buffer_offset += 2;
            j += 2;
            name_offset++;
        }

        buffer_offset += l_rsvd;
        j = 0;

        while (j != l_name_3 && !eof )
        {
            if ( (buffer[buffer_offset] == 0x00) || 
                 (buffer[buffer_offset] == 0xFF) )
            {
                eof = 1;
                continue;
            }
            name[name_offset] = buffer[buffer_offset];
            buffer_offset += 2;
            j += 2;
            name_offset++;
        }
    }
    name[name_offset] = 0;
} // end get_name_from_long()



////////////////////////////////////////////////////////////
static fat_cache_node_t* _allocate_one_cache_node( fat_cache_node_t* first_child )
{
    fat_cache_node_t* cnode;
    unsigned int i;

    cnode = _malloc( sizeof(fat_cache_node_t) );

    cnode->children[0] = first_child;
    for ( i = 1 ; i < 64 ; i++ )
        cnode->children[i] = NULL;

    return cnode;
}   // end _allocate_one_cache_node()



////////////////////////////////////////////////////////////
static fat_inode_t* _allocate_one_inode( char*        name,
                                         unsigned int is_dir,
                                         unsigned int cluster,
                                         unsigned int size, 
                                         unsigned int count,
                                         unsigned int dentry,
                                         unsigned int cache_allocate )
{
    fat_inode_t* new_inode  = _malloc( sizeof(fat_inode_t) );

    new_inode->parent   = NULL;                 // set by _add_inode_in_tree()
    new_inode->next     = NULL;                 // set by _add_inode_in_tree()
    new_inode->child    = NULL;                 // set by _add_inode_in_tree()
    new_inode->cluster  = cluster;
    new_inode->size     = size; 
    new_inode->cache    = NULL;
    new_inode->levels   = 0;
    new_inode->count    = count;
    new_inode->is_dir   = (is_dir != 0);
    new_inode->dentry   = dentry;             

    _strcpy( new_inode->name , name );  

    if ( cache_allocate )
    {
        new_inode->cache    = _allocate_one_cache_node( NULL );
        new_inode->levels   = _get_levels_from_size( size );
    }

    return new_inode;
}   // end _allocate_one_inode()




////////////////////////////////////////////////////
static void _add_inode_in_tree( fat_inode_t*  child,
                                fat_inode_t*  parent )
{
    child->parent = parent;
    child->next   = parent->child;
    parent->child = child;
}   // end _add_inode-in_tree()




//////////////////////////////////////////////////////////
static void _remove_inode_from_tree( fat_inode_t*  inode )
{
    fat_inode_t*  current;
    fat_inode_t*  prev = inode->parent->child;

    if ( inode == prev )  // removed inode is first in its linked list
    {
        inode->parent->child = inode->next;
    }
    else                  // removed inode is not the first
    {
        for( current = prev->next ; current ; current = current->next )
        {
            if ( current == inode )
            {
                prev->next = current->next;
            }
            prev = current;
        }    
    }    
}  // end _delete_one_inode()




//////////////////////////////////////////////////////////////////////
static unsigned int _get_buffer_from_cache( fat_inode_t*        inode,
                                            unsigned int        cluster_id,
                                            fat_cache_desc_t**  desc )
{
    // get cache pointer and levels
    fat_cache_node_t*   node;         // pointer on a 64-tree node
    unsigned int        level;        // cache level

    if ( inode == NULL )   // searched cache is the Fat-Cache
    {
        node   = _fat.fat_cache_root;
        level  = _fat.fat_cache_levels;

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_buffer_from_cache(): enters in FAT-Cache"
        " for cluster_id = %d\n", cluster_id );
#endif
    }
    else                   // searched cache is a File-Cache
    {
        // add cache levels if needed
        while ( _get_levels_from_size( (cluster_id + 1) * 4096 ) > inode->levels )
        {
#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_buffer_from_cache(): adding a File-Cache level\n" );
#endif

            inode->cache = _allocate_one_cache_node( inode->cache );
            inode->levels++;
        }

        node   = inode->cache;
        level  = inode->levels;

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_buffer_from_cache(): enters in File-Cache <%s>"
        " for cluster_id = %d\n", inode->name , cluster_id );
#endif
    }

    // search the 64-tree cache from top to bottom 
    while ( level )
    {
        // compute child index at each level
        unsigned int index = (cluster_id >> (6*(level-1))) & 0x3F;

        if ( level == 1 )        // last level => children are cluster descriptors
        {
            fat_cache_desc_t* pdesc = (fat_cache_desc_t*)node->children[index];

            if ( pdesc == NULL )      // miss 
            {
                // get missing cluster index lba
                unsigned int lba;
                unsigned int next;
                unsigned int current = inode->cluster;
                unsigned int count   = cluster_id;

                if ( inode == NULL )      // searched cache is the Fat-Cache
                {

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_buffer_from_cache(): miss in FAT-Cache for cluster_id %d\n",
        cluster_id );
#endif
                    lba = _fat.fat_lba + (cluster_id << 3);
                }
                else                      // searched cache is a File-Cache
                {

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_buffer_from_cache(): miss in File-Cache <%s> "
        "for cluster_id %d\n", inode->name, cluster_id );
#endif
                    while ( count )
                    { 
                        if ( _get_fat_entry( current , &next ) ) return 1;
                        current = next;
                        count--;
                    }
                    lba = _cluster_to_lba( current );
                }

                // allocate 4K buffer
                void* buf = _malloc( 4096 );

                // load one cluster (8 blocks) from block device
                if ( _fat_ioc_access( 1,         // descheduling
                                      1,         // to memory
                                      lba,
                                      (unsigned int)buf,
                                      8 ) )
                {
                    _free( buf );
                    _printf("\n[FAT ERROR] _get_buffer_from_cache()"
                            ": cannot access block device for lba = %x\n", lba );
                    return 1;
                }

                // allocate buffer descriptor
                pdesc          = _malloc( sizeof(fat_cache_desc_t) );
                pdesc->lba     = lba;
                pdesc->buffer  = buf;
                pdesc->dirty   = 0;
                node->children[index] = pdesc;

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_buffer_from_cache(): buffer loaded from device"
        " at vaddr = %x\n", (unsigned int)buf );
#endif
            }

            // return pdesc pointer
            *desc = pdesc;

            // prepare next iteration
            level--;
        }
        else                      // not last level => children are 64-tree nodes
        {
            fat_cache_node_t* child = (fat_cache_node_t*)node->children[index];
            if ( child == NULL )  // miss 
            {
                // allocate a cache node if miss
                child = _allocate_one_cache_node( NULL );
                node->children[index] = child;    
            }

            // prepare next iteration
            node = child;
            level--;
        }
    } // end while

    return 0;
}  // end _get_buffer_from_cache()




/////////////////////////////////////
static unsigned int _update_fs_info()
{
    // update buffer if miss
    if ( _fat.fs_info_lba != _fat.block_buffer_lba )
    {
        if ( _fat_ioc_access( 1,                 // descheduling
                              1,                 // read
                              _fat.fs_info_lba, 
                              (unsigned int)_fat.block_buffer, 
                              1 ) )              // one block
        {
            _printf("\n[FAT_ERROR] _update_fs_info(): cannot read block\n");
            return 1;
        }
        _fat.block_buffer_lba = _fat.fs_info_lba;
    }

    // update FAT buffer
    unsigned int* ptr;
    ptr  = (unsigned int*)(_fat.block_buffer + get_offset(FS_FREE_CLUSTERS) );
    *ptr = _fat.free_clusters_number;

    ptr  = (unsigned int*)(_fat.block_buffer + get_offset(FS_FREE_CLUSTER_HINT) );
    *ptr = _fat.first_free_cluster;
   
    // write bloc to FAT
    if ( _fat_ioc_access( 1,                // descheduling
                          0,                // write
                          _fat.fs_info_lba,
                          (unsigned int)_fat.block_buffer, 
                          1 ) )             // one block
    {
        _printf("\n[FAT_ERROR] _update_fs_info(): cannot write block\n");
        return 1;
    }

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _update_fs_info(): nb_free = %x / first_free = %x\n",
        _fat.free_clusters_number , _fat.first_free_cluster );
#endif

    return 0;
}  // end _update_fs_info()



/////////////////////////////////////////////////////////////////
static inline unsigned int _get_fat_entry( unsigned int  cluster,
                                           unsigned int* value )
{
    // compute cluster_id & entry_id in FAT
    // a FAT cluster is an array of 1024 unsigned int entries
    unsigned int       cluster_id = cluster >> 10;       
    unsigned int       entry_id   = cluster & 0x3FF;

    // get pointer on the relevant cluster descriptor in FAT cache
    fat_cache_desc_t*  pdesc;
    unsigned int*      buffer;
    if ( _get_buffer_from_cache( NULL,               // Fat-Cache
                                 cluster_id,
                                 &pdesc ) ) return 1;

    // get value from FAT slot
    buffer = (unsigned int*)pdesc->buffer;
    *value = buffer[entry_id];

    return 0;
}  // end _get_fat_entry()



////////////////////////////////////////////////////////////////
static inline unsigned int _set_fat_entry( unsigned int cluster, 
                                           unsigned int value  )
{
    // compute cluster_id & entry_id in FAT
    // a FAT cluster is an array of 1024 unsigned int entries
    unsigned int cluster_id = cluster >> 10;
    unsigned int entry_id   = cluster & 0x3FF;

    // get pointer on the relevant cluster descriptor in FAT cache
    fat_cache_desc_t*  pdesc;
    unsigned int*      buffer; 
    if ( _get_buffer_from_cache( NULL,               // Fat-Cache
                                 cluster_id,
                                 &pdesc ) )  return 1;           

    // set value into FAT slot
    buffer           = (unsigned int*)pdesc->buffer;
    buffer[entry_id] = value;
    pdesc->dirty     = 1;

    return 0;
} // end _set_fat_entry()



//////////////////////////////////////////////////////
static void _allocate_one_buffer( fat_inode_t*  inode,
                                  unsigned int  cluster_id,
                                  unsigned int  cluster )
{
    // add cache levels if needed
    while ( _get_levels_from_size( (cluster_id + 1) * 4096 ) > inode->levels )
    {
#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _allocate_one_buffer(): adding a cache level\n" );
#endif

        inode->cache = _allocate_one_cache_node( inode->cache );
        inode->levels++;
    }

    // search the 64-tree cache from top to bottom 
    fat_cache_node_t*  node   = inode->cache;
    unsigned int       level;

    for ( level = inode->levels; level != 0; level-- )
    {
        // compute child index
        unsigned int index = (cluster_id >> (6*(level-1))) & 0x3F;

        if ( level == 1 )        // last level => children are cluster descriptors
        {
            fat_cache_desc_t* pdesc = (fat_cache_desc_t*)node->children[index];
            if ( pdesc != NULL )      // slot not empty!!!
            {
                _printf("\n[FAT ERROR] allocate_one buffer(): slot not empty "
                        "in File-Cache <%s> / cluster_id = %d\n", inode->name , cluster_id );
                _exit();
            }

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _allocate_one_buffer(): buffer allocated to <%s> for cluster_id %d\n",
        inode->name, cluster_id );
#endif

            // allocate buffer descriptor
            pdesc = _malloc( sizeof(fat_cache_desc_t) );
            pdesc->lba     = _cluster_to_lba( cluster );
            pdesc->buffer  = _malloc( 4096 );
            pdesc->dirty   = 1;
            node->children[index] = pdesc;
        }
        else                      // not last level => children are 64-tree nodes
        {
            fat_cache_node_t* child = (fat_cache_node_t*)node->children[index];
            if ( child == NULL )  // miss 
            {
                // allocate a cache node if miss
                child = _allocate_one_cache_node( NULL );
                node->children[index] = child;    
            }

            // prepare next iteration
            node  = child;
        }
    } // end for
} // end _allocate_one_buffer




///////////////////////////////////////////////////////////////////
static unsigned int _allocate_one_cluster( unsigned int*  cluster )  
{
    unsigned int nb_free = _fat.free_clusters_number;
    unsigned int free    = _fat.first_free_cluster;

    // scan FAT to get next free cluster index
    unsigned int current = free;
    unsigned int found   = 0;
    unsigned int max     = (_fat.data_sectors >> 3);
    unsigned int value;
    do
    {
        // increment current
        current++;

        // get FAT entry indexed by current
        if ( _get_fat_entry( current , &value ) ) return 1;
        // test if free
        if ( value == FREE_CLUSTER ) found = 1;
    }
    while ( (current < max) && (found == 0) );
        
    // check found  
    if ( found == 0 )
    {
        _printf("\n[FAT_ERROR] _allocate_one_cluster(): unconsistent FAT state");
        return 1;
    }

    // update allocated FAT slot
    if ( _set_fat_entry( free , END_OF_CHAIN_CLUSTER_MAX ) ) return 1;

    // update FAT descriptor global variables
    _fat.free_clusters_number = nb_free - 1;
    _fat.first_free_cluster   = current;

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _allocate_one_cluster(): cluster = %x / first_free = %x\n",
        free , current );
#endif

    // returns free cluster index
    *cluster = free;
    return 0;

}  // end _allocate_one_cluster()




//////////////////////////////////////////////////////////////////////////
static unsigned int _update_device_from_cache( unsigned int        levels,
                                               fat_cache_node_t*   root,
                                               char*               string )
{
    unsigned int index;
    unsigned int ret = 0;

    if ( levels == 1 )  // last level => children are cluster descriptors
    {
        for( index = 0 ; index < 64 ; index++ )
        { 
            fat_cache_desc_t* pdesc = root->children[index];
            if ( pdesc != NULL )
            { 
                // update cluster on device if dirty
                if ( pdesc->dirty )
                {
                    if ( _fat_ioc_access( 1,           // descheduling
                                          0,           // to block device
                                          pdesc->lba,
                                          (unsigned int)pdesc->buffer,
                                          8 ) )
                    {
                        _printf("\n[FAT_ERROR] _update_device from_cache(): "
                                " cannot access lba = %x\n", pdesc->lba );
                        ret = 1;
                    }
                    else
                    {
                        pdesc->dirty = 0;

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _update_device_from_cache(): cluster_id = %d for <%s>\n",
        index , string );
#endif

                    }
                }
            }
        }
    }
    else               // not the last level = recursive call on each children
    {
        for( index = 0 ; index < 64 ; index++ )
        { 
            fat_cache_node_t* pnode = root->children[index];
            if ( pnode != NULL )
            {
                if ( _update_device_from_cache( levels - 1,
                                                root->children[index],
                                                string ) ) ret = 1;
            }    
        }
    }
    return ret;
}  // end _update_device_from_cache()



///////////////////////////////////////////////////////////////////
static void _release_cache_memory( fat_cache_node_t*  root,
                                   unsigned int       levels )
{
    unsigned int i;

    if ( levels == 1 )  // last level => children are cluster descriptors
    {
        for( i = 0 ; i < 64 ; i++ )
        { 
            fat_cache_desc_t* pdesc = root->children[i];

            if ( pdesc != NULL )
            { 
                // check dirty
                if ( pdesc->dirty )
                    _printf("\n[FAT ERROR] _release_cache_memory(): dirty cluster\n");

                _free( pdesc->buffer );
                _free( pdesc );
                root->children[i] = NULL;
            }
        }
    }
    else               // not the last level = recursive call on each children
    {
        for( i = 0 ; i < 64 ; i++ )
        { 
            fat_cache_node_t* cnode = root->children[i];

            if ( cnode != NULL )
            {
                _release_cache_memory( cnode, levels - 1 );
                _free( cnode );
                root->children[i] = NULL;
            }
        }
    }
}  // end _release_cache_memory()





/////////////////////////////////////////////////////////////
static unsigned int _clusters_allocate( fat_inode_t* inode, 
                                        unsigned int nb_current_clusters,
                                        unsigned int nb_required_clusters )
{
    // Check if FAT contains enough free clusters
    if ( nb_required_clusters > _fat.free_clusters_number )
    {
        _printf("\n[FAT ERROR] _clusters_allocate(): required_clusters = %d"
                " / free_clusters = %d\n", nb_required_clusters , _fat.free_clusters_number );
        return 1;
    }

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _clusters_allocate(): enters for <%s> / nb_current_clusters = %d "
        "/ nb_required_clusters = %d\n", 
        inode->name , nb_current_clusters , nb_required_clusters );
#endif
 
    // compute last allocated cluster index when (nb_current_clusters > 0)
    unsigned int current = inode->cluster;
    unsigned int next;
    unsigned int last;
    if ( nb_current_clusters )   // clusters allocated => search last
    {    
        while ( current < END_OF_CHAIN_CLUSTER_MIN )
        {
            // get next cluster
            if ( _get_fat_entry( current , &next ) )  return 1;
            last    = current;
            current = next;
        }
    }  

    // Loop on the new clusters to be allocated
    // if (nb_current_clusters == 0) the first new cluster index must
    //                               be written in inode->cluster field 
    // if (nb_current_clusters >  0) the first new cluster index must
    //                               be written in FAT[last]
    unsigned int      cluster_id;
    unsigned int      new;
    for ( cluster_id = nb_current_clusters ; 
          cluster_id < (nb_current_clusters + nb_required_clusters) ; 
          cluster_id ++ )
    {
        // allocate one cluster on block device
        if ( _allocate_one_cluster( &new ) ) return 1;

        // allocate one 4K buffer to File-Cache
        _allocate_one_buffer( inode,
                              cluster_id,
                              new );

        if ( cluster_id == 0 )  // update inode 
        {
            inode->cluster = new;
        }
        else                    // update FAT
        {
            if ( _set_fat_entry( last , new ) ) return 1;
        }

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _clusters_allocate(): done for cluster_id = %d / cluster = %x\n",
        cluster_id , new );
#endif

        // update loop variables
        last = new;

    } // end for loop

    // update FAT : last slot should contain END_OF_CHAIN_CLUSTER_MAX
    if ( _set_fat_entry( last , END_OF_CHAIN_CLUSTER_MAX ) )  return 1;

    // update the FAT on block device
    if ( _update_device_from_cache( _fat.fat_cache_levels,
                                    _fat.fat_cache_root,
                                    "FAT" ) )              return 1;
    return 0;
}  // end _clusters_allocate()



//////////////////////////////////////////////////////////////
static unsigned int _clusters_release( unsigned int cluster )
{
    // scan the FAT
    unsigned int current = cluster;
    unsigned int next;
    do
    { 
        // get next_cluster index
        if ( _get_fat_entry( current , &next ) )  return 1;

        // release current_cluster
        if ( _set_fat_entry( current , FREE_CLUSTER ) )   return 1;

        // update first_free_cluster and free_clusters_number in FAT descriptor
        _fat.free_clusters_number = _fat.free_clusters_number + 1;
        if ( _fat.first_free_cluster > current ) _fat.first_free_cluster = current;

        // update loop variable
        current = next;
    }
    while ( next < END_OF_CHAIN_CLUSTER_MIN );

    // update the FAT on block device
    if ( _update_device_from_cache( _fat.fat_cache_levels,
                                    _fat.fat_cache_root,
                                    "FAT" ) )                return 1;
    return 0;
}  // end _clusters_release()



///////////////////////////////////////////////////////////
static void _add_special_directories( fat_inode_t*  child, 
                                      fat_inode_t*  parent )
{
    // get first File-Cache buffer for child
    fat_cache_desc_t*   pdesc  = (fat_cache_desc_t*)child->cache->children[0];
    unsigned char*      entry;

    unsigned int i;
    unsigned int cluster;
    unsigned int size;

    // set "." entry (32 bytes)
    cluster = child->cluster;
    size    = child->size;
    entry   = pdesc->buffer;
    
    for ( i = 0 ; i < 32 ; i++ )
    {
        if      (i == 0 )     entry[i] = 0x2E;          // SFN
        else if (i <  11)     entry[i] = 0x20;          // SFN
        else if (i == 11)     entry[i] = 0x10;          // ATTR == dir
        else if (i == 20)     entry[i] = cluster>>16;   // cluster.B2
        else if (i == 21)     entry[i] = cluster>>24;   // cluster.B3
        else if (i == 26)     entry[i] = cluster>>0;    // cluster.B0
        else if (i == 27)     entry[i] = cluster>>8;    // cluster.B1
        else if (i == 28)     entry[i] = size>>0;       // size.B0
        else if (i == 29)     entry[i] = size>>8;       // size.B1
        else if (i == 30)     entry[i] = size>>16;      // size.B2
        else if (i == 31)     entry[i] = size>>24;      // size.B3
        else                  entry[i] = 0x00;
    }

    // set ".." entry (32 bytes)
    cluster = parent->cluster;
    size    = parent->size;
    entry   = pdesc->buffer + 32;

    for ( i = 0 ; i < 32 ; i++ )
    {
        if      (i <  2 )     entry[i] = 0x2E;          // SFN
        else if (i <  11)     entry[i] = 0x20;          // SFN
        else if (i == 11)     entry[i] = 0x10;          // ATTR == dir
        else if (i == 20)     entry[i] = cluster>>16;   // cluster.B2
        else if (i == 21)     entry[i] = cluster>>24;   // cluster.B3
        else if (i == 26)     entry[i] = cluster>>0;    // cluster.B0
        else if (i == 27)     entry[i] = cluster>>8;    // cluster.B1
        else if (i == 28)     entry[i] = size>>0;       // size.B0
        else if (i == 29)     entry[i] = size>>8;       // size.B1
        else if (i == 30)     entry[i] = size>>16;      // size.B2
        else if (i == 31)     entry[i] = size>>24;      // size.B3
        else                  entry[i] = 0x00;
    }
}  // end _add_special_directories



////////////////////////////////////////////////////////////
static unsigned int _is_ancestor( fat_inode_t* a,
                                  fat_inode_t* b )
{
    while ( b )
    {
        if ( a == b )
            return 1;

        b = b->parent;
    }

    return 0;
} // _is_ancestor()



////////////////////////////////////////////////////////////
static unsigned int _check_name_length( char* name,
                                        unsigned int* length,
                                        unsigned int* nb_lfn )
{
    unsigned int len = _strlen( name );
    if      ( len <= 13 )
    {
        *length  = len;
        *nb_lfn  = 1;
        return 0;
    }
    else if ( len <= 26 )
    {
        *length  = len;
        *nb_lfn  = 2;
        return 0;
    }
    else if ( len <= 31 )
    {
        *length  = len;
        *nb_lfn  = 3;
        return 0;
    }
    else
    {
        return 1;
    }
}  // _check_name_length()




///////////////////////////////////////////////////////////
static unsigned int _get_nb_entries( fat_inode_t*   inode,
                                     unsigned int*  nb_entries )
{
    // scan directory until "end of directory" with two embedded loops:
    // - scan the clusters allocated to this directory
    // - scan the entries to find NO_MORE_ENTRY
    fat_cache_desc_t*  pdesc;                      // pointer on buffer descriptor
    unsigned char*     buffer;                     // 4 Kbytes buffer (one cluster)
    unsigned int       ord;                        // ORD field in directory entry
    unsigned int       attr;                       // ATTR field in directory entry
    unsigned int       cluster_id = 0;             // cluster index in directory
    unsigned int       offset     = 0;             // position in scanned buffer
    unsigned int       found      = 0;             // NO_MORE_ENTRY found
    unsigned int       count      = 0;             // number of valid NORMAL entries

    // loop on clusters allocated to directory
    while ( found == 0 )
    {
        // get one 4 Kytes buffer from File_Cache  
        if ( _get_buffer_from_cache( inode,
                                     cluster_id,
                                     &pdesc ) )   return 1;
        buffer = pdesc->buffer;
        
        // loop on directory entries in buffer
        while ( (offset < 4096) && (found == 0) )
        {
            attr = _read_entry( DIR_ATTR , buffer + offset , 0 );   
            ord  = _read_entry( LDIR_ORD , buffer + offset , 0 );

            if ( ord == NO_MORE_ENTRY )
            {
                found = 1;
            }  
            else if ( ord == FREE_ENTRY )             // free entry => skip
            {
                offset = offset + 32;
            }
            else if ( attr == ATTR_LONG_NAME_MASK )   // LFN entry => skip
            {
                offset = offset + 32;
            }
            else                                      // NORMAL entry
            {
                offset = offset + 32;
                count++;
            }
        }  // end loop on directory entries

        cluster_id++;
        offset = 0;

    }  // end loop on clusters

    // return nb_entries
    *nb_entries = count;
    
    return 0;
}  // end dir_not_empty()



///////////////////////////////////////////////////////////
static unsigned int _add_dir_entry( fat_inode_t*   child,
                                    fat_inode_t*   parent )
{
    // get child attributes
    unsigned int      is_dir  = child->is_dir;     
    unsigned int      size    = child->size;
    unsigned int      cluster = child->cluster;

    // compute number of required entries to store child->name
    // - Short name (less than 13 characters) require 3 entries: 
    //   one LFN entry / one NORMAL entry / one NO_MORE_ENTRY entry.
    // - Longer names (up to 31 characters) can require 4 or 5 entries:
    //   2 or 3 LFN entries / one NORMAL entry / one NO_MORE entry.
    unsigned int length;
    unsigned int nb_lfn;
    if ( _check_name_length( child->name, 
                             &length,
                             &nb_lfn ) )  return 1;

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _add_dir_entry(): try to add <%s> in <%s> / nb_lfn = %d\n", 
        child->name , parent->name, nb_lfn );
#endif

    // Find end of directory : two embedded loops:
    // - scan the clusters allocated to this directory
    // - scan the entries to find NO_MORE_ENTRY
    fat_cache_desc_t*  pdesc;                      // pointer on buffer descriptor
    unsigned char*     buffer;                     // 4 Kbytes buffer (one cluster)
    unsigned int       cluster_id = 0;             // cluster index in directory
    unsigned int       offset     = 0;             // position in scanned buffer
    unsigned int       found      = 0;             // NO_MORE_ENTRY found

    // loop on clusters allocated to directory
    while ( found == 0 )
    {
        // get one 4 Kytes buffer from File_Cache  
        if ( _get_buffer_from_cache( parent,
                                     cluster_id,
                                     &pdesc ) )   return 1;

        buffer = pdesc->buffer;
        
        // loop on directory entries in buffer
        while ( (offset < 4096) && (found == 0) )
        {
            if ( _read_entry( LDIR_ORD , buffer + offset , 0 ) == NO_MORE_ENTRY )
            {
                found        = 1;
                pdesc->dirty = 1;
            }  
            else
            {
                offset = offset + 32;
            }
        }  // end loop on entries
        if ( found == 0 )
        {
            cluster_id++;
            offset = 0;
        }
    }  // end loop on clusters

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _add_dir_entry(): get NO_MORE directory entry : "
        " buffer = %x / offset = %x / cluster_id = %d\n",
        (unsigned int)buffer , offset , cluster_id );
#endif

    // enter FSM :
    // The new child requires to write 3, 4, or 5 directory entries.
    // To actually register the new child, we use a 5 steps FSM
    // (one state per entry to be written), that is traversed as:
    //    LFN3 -> LFN2 -> LFN1 -> NORMAL -> NOMORE 
    // The buffer and first directory entry to be  written are identified 
    // by the variables : buffer / cluster_id / offset 

    unsigned char* name  = (unsigned char*)child->name;

    unsigned int step;          // FSM state

    if      ( nb_lfn == 1 ) step = 3;
    else if ( nb_lfn == 2 ) step = 4;
    else if ( nb_lfn == 3 ) step = 5;
    
    unsigned int   i;           // byte index in 32 bytes directory
    unsigned int   c;           // character index in name
    unsigned char* entry;       // buffer + offset;

    while ( step )   
    {
        // get another buffer if required
        if ( offset >= 4096 )  // new buffer required
        {
            if ( _get_buffer_from_cache( parent,
                                         cluster_id + 1,
                                         &pdesc ) )      return 1;
            buffer       = pdesc->buffer;
            pdesc->dirty = 1;
            offset       = 0;
        }

        // compute directory entry address
        entry = buffer + offset;

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _add_dir_entry(): FSM step = %d /"
        " offset = %x / nb_lfn = %d\n", step, offset, nb_lfn );
#endif

        // write one 32 bytes directory entry per iteration
        switch ( step )
        {
            case 5:   // write LFN3 entry
            {
                c = 26;
                // scan the 32 bytes in dir_entry
                for ( i = 0 ; i < 32 ; i++ )
                {
                    if (i == 0)
                    {
                        if ( nb_lfn == 3) entry[i] = 0x43;
                        else              entry[i] = 0x03;
                    }
                    else if ( ( ((i >= 1 ) && (i<=10) && ((i&1)==1))   ||
                                ((i >= 14) && (i<=25) && ((i&1)==0))   ||
                                ((i >= 28) && (i<=31) && ((i&1)==0)) ) &&
                              ( c < length ) )
                    {
                                          entry[i] = name[c];
                                          c++;
                    }
                    else if (i == 11)     entry[i] = 0x0F;
                    else if (i == 12)     entry[i] = 0xCA;
                    else                  entry[i] = 0x00;
                }
                step--;
                break;
            }
            case 4:   // write LFN2 entry  
            {
                c = 13;
                // scan the 32 bytes in dir_entry
                for ( i = 0 ; i < 32 ; i++ )
                {
                    if (i == 0)
                    {
                        if ( nb_lfn == 2) entry[i] = 0x42;
                        else              entry[i] = 0x02;
                    }
                    else if ( ( ((i >= 1 ) && (i<=10) && ((i&1)==1))   ||
                                ((i >= 14) && (i<=25) && ((i&1)==0))   ||
                                ((i >= 28) && (i<=31) && ((i&1)==0)) ) &&
                              ( c < length ) )
                    {
                                          entry[i] = name[c];
                                          c++;
                    }
                    else if (i == 11)     entry[i] = 0x0F;
                    else if (i == 12)     entry[i] = 0xCA;
                    else                  entry[i] = 0x00;
                }
                step--;
                break;
            }
            case 3:   // Write LFN1 entry   
            {
                c = 0;
                // scan the 32 bytes in dir_entry
                for ( i = 0 ; i < 32 ; i++ )
                {
                    if (i == 0)
                    {
                        if ( nb_lfn == 1) entry[i] = 0x41;
                        else              entry[i] = 0x01;
                    }
                    else if ( ( ((i >= 1 ) && (i<=10) && ((i&1)==1))   ||
                                ((i >= 14) && (i<=25) && ((i&1)==0))   ||
                                ((i >= 28) && (i<=31) && ((i&1)==0)) ) &&
                              ( c < length ) )
                    {
                                          entry[i] = name[c];
                                          c++;
                    }
                    else if (i == 11)     entry[i] = 0x0F;
                    else if (i == 12)     entry[i] = 0xCA;
                    else                  entry[i] = 0x00;
                }
                step--;
                break;
            }
            case 2:   // write NORMAL entry     
            {
                c = 0;
                // scan the 32 bytes in dir_entry
                for ( i = 0 ; i < 32 ; i++ )
                {
                    if      ( (i < 8) && (c < length) )             // SFN
                    {
                                          entry[i] = _to_upper( name[c] );
                                          c++;
                    }
                    else if (i <  11)     entry[i] = 0x20;          // EXT
                    else if (i == 11)                               // ATTR
                    {
                        if (is_dir)       entry[i] = 0x10;
                        else              entry[i] = 0x20;
                    }
                    else if (i == 20)     entry[i] = cluster>>16;   // cluster.B2
                    else if (i == 21)     entry[i] = cluster>>24;   // cluster.B3
                    else if (i == 26)     entry[i] = cluster>>0;    // cluster.B0
                    else if (i == 27)     entry[i] = cluster>>8;    // cluster.B1
                    else if (i == 28)     entry[i] = size>>0;       // size.B0
                    else if (i == 29)     entry[i] = size>>8;       // size.B1
                    else if (i == 30)     entry[i] = size>>16;      // size.B2
                    else if (i == 31)     entry[i] = size>>24;      // size.B3
                    else                  entry[i] = 0x00;
                }

                // update the dentry field in child inode
                child->dentry = ((cluster_id<<12) + offset)>>5;

                step--;
                break;
            }
            case 1:   // write NOMORE entry  
            {
                step--;
                entry [0] = 0x00;
                break;
            }
        } // end switch step
        offset += 32;
    } // exit while => exit FSM    

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
{
    _printf("\n[DEBUG FAT] _add_dir_entry(): <%s> successfully added in <%s>\n",
            child->name , parent->name );
}
#endif

    return 0;        
} // end _add_dir_entry



////////////////////////////////////////////////////////////
static unsigned int _remove_dir_entry( fat_inode_t*  inode )
{
    // compute number of LFN entries
    unsigned int length;
    unsigned int nb_lfn;
    if ( _check_name_length( inode->name, 
                             &length,
                             &nb_lfn ) )  return 1;

    // get cluster_id and offset in parent directory cache
    unsigned int  dentry     = inode->dentry;
    unsigned int  cluster_id = dentry >> 7;
    unsigned int  offset     = (dentry & 0x7F)<<5;

    // get buffer from parent directory cache
    unsigned char*     buffer;
    fat_cache_desc_t*  pdesc;

    if ( _get_buffer_from_cache( inode->parent,
                                 cluster_id,
                                 &pdesc ) ) return 1;
    buffer       = pdesc->buffer;
    pdesc->dirty = 1;

    // invalidate NORMAL entry in directory cache
    buffer[offset] = 0xE5;

    // invalidate LFN entries
    while ( nb_lfn )
    {
        if (offset == 0)  // we must load buffer for (cluster_id - 1)
        {
            if ( cluster_id == 0 )
                break;

            if ( _get_buffer_from_cache( inode->parent,
                                         cluster_id - 1,
                                         &pdesc ) )   return 1;
            buffer       = pdesc->buffer;
            pdesc->dirty = 1;
            offset       = 4096;
        }

        offset = offset - 32;

        // check for LFN entry
        if ( _read_entry( DIR_ATTR , buffer + offset , 0 ) != ATTR_LONG_NAME_MASK )
            break;

        // invalidate LFN entry
        buffer[offset] = 0xE5;

        nb_lfn--;
    }     
         
    return 0;
}  // end _remove_dir_entry




////////////////////////////////////////////////////////////
static unsigned int _update_dir_entry( fat_inode_t*  inode )
{  
    // get Cache-File buffer containing the parent directory entry
    // 128 directories entries in one 4 Kbytes buffer
    fat_cache_desc_t*  pdesc;
    unsigned char*     buffer;   
    unsigned int       cluster_id = inode->dentry>>7;
    unsigned int       offset     = (inode->dentry & 0x7F)<<5;

    if ( _get_buffer_from_cache( inode->parent,
                                 cluster_id,
                                 &pdesc ) )    return 1;
    buffer       = pdesc->buffer;
    pdesc->dirty = 1;

    // update size field
    buffer[offset + 28] = inode->size>>0;       // size.B0 
    buffer[offset + 29] = inode->size>>8;       // size.B1 
    buffer[offset + 30] = inode->size>>16;      // size.B2 
    buffer[offset + 31] = inode->size>>24;      // size.B3 

    // update cluster field
    buffer[offset + 26] = inode->cluster>>0;    // cluster.B0 
    buffer[offset + 27] = inode->cluster>>8;    // cluster.B1 
    buffer[offset + 20] = inode->cluster>>16;   // cluster.B2 
    buffer[offset + 21] = inode->cluster>>24;   // cluster.B3 
    
    return 0;
} // end _update_dir_entry()




//////////////////////////////////////////////////////////////////
static unsigned int _get_child_from_parent( fat_inode_t*   parent,
                                            char*          name, 
                                            fat_inode_t**  inode )
{
    fat_inode_t*   current;

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_child_from_parent(): search <%s> in directory <%s>\n",
        name , parent->name );
#endif
    
    // scan inodes in the parent directory 
    for ( current = parent->child ; current ; current = current->next )
    {
        if ( _strcmp( name , current->name ) == 0 )
        {

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_child_from_parent(): found inode <%s> in directory <%s>\n", 
        name , parent->name );
#endif
            *inode = current;
            return 0;           // name found
        }
    }

    // not found in Inode-Tree => access the parent directory 
    // file_cache.  Two embedded loops:
    // - scan the clusters allocated to this directory
    // - scan the directory entries in each 4 Kbytes buffer

    unsigned char*    buffer;           // pointer on one cache buffer
    char              cname[32];        // buffer for one full entry name 
    char              lfn1[16];         // buffer for one partial name
    char              lfn2[16];         // buffer for one partial name
    char              lfn3[16];         // buffer for one partial name
    unsigned int      size;             // searched file/dir size (bytes) 
    unsigned int      cluster;          // searched file/dir cluster index
    unsigned int      is_dir;           // searched file/dir type
    unsigned int      attr;             // directory entry ATTR field
    unsigned int      ord;              // directory entry ORD field
    unsigned int      lfn = 0;          // LFN entries number
    unsigned int      dentry;           // directory entry index 
    unsigned int      offset     = 0;   // byte offset in buffer
    unsigned int      cluster_id = 0;   // cluster index in directory
    int               found      = 0;   // not found (0) / name found (1) / end of dir (-1)

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_child_from_parent(): does not found inode <%s>"
        " in directory <%s> => search in cache\n", name , parent->name );
#endif

    // scan the clusters allocated to parent directory
    while ( found == 0 )
    {
        // get one 4 Kytes buffer from parent File_Cache  
        fat_cache_desc_t*  pdesc;
        if ( _get_buffer_from_cache( parent,
                                     cluster_id,
                                     &pdesc ) )    return 2;
        buffer = pdesc->buffer;

        // scan this buffer until end of directory, end of buffer, or name found
        while( (offset < 4096) && (found == 0) )
        {

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_child_from_parent(): scan buffer %d for <%s>\n",
        cluster_id , name );
#endif
            attr = _read_entry( DIR_ATTR , buffer + offset , 0 );   
            ord  = _read_entry( LDIR_ORD , buffer + offset , 0 );

            if (ord == NO_MORE_ENTRY)                 // no more entry in directory => break
            {
                found = -1;
            }
            else if ( ord == FREE_ENTRY )             // free entry => skip
            {
                offset = offset + 32;
            }
            else if ( attr == ATTR_LONG_NAME_MASK )   // LFN entry => get partial name
            {
                unsigned int seq = ord & 0x3;
                lfn = (seq > lfn) ? seq : lfn;   
                if      ( seq == 1 ) _get_name_from_long( buffer + offset, lfn1 );
                else if ( seq == 2 ) _get_name_from_long( buffer + offset, lfn2 );
                else if ( seq == 3 ) _get_name_from_long( buffer + offset, lfn3 );
                offset = offset + 32;
            }
            else                                 // NORMAL entry
            {
                // build the extracted name
                if      ( lfn == 0 )
                {
                    _get_name_from_short( buffer + offset , cname );
                }
                else if ( lfn == 1 )
                {
                    _strcpy( cname      , lfn1 );
                }   
                else if ( lfn == 2 ) 
                {
                    _strcpy( cname      , lfn1 );
                    _strcpy( cname + 13 , lfn2 );
                }
                else if ( lfn == 3 ) 
                {
                    _strcpy( cname      , lfn1 );
                    _strcpy( cname + 13 , lfn2 );
                    _strcpy( cname + 26 , lfn3 );
                }
                    
                // test if extracted name == searched name
                if ( _strcmp( name , cname ) == 0 )
                {
                    cluster = (_read_entry( DIR_FST_CLUS_HI , buffer + offset , 1 ) << 16) |
                              (_read_entry( DIR_FST_CLUS_LO , buffer + offset , 1 )      ) ;
                    dentry  = ((cluster_id<<12) + offset)>>5;
                    is_dir  = ((attr & ATTR_DIRECTORY) == ATTR_DIRECTORY);
                    size    = _read_entry( DIR_FILE_SIZE , buffer + offset , 1 );
                    found   = 1;
                }
                offset = offset + 32;
                lfn    = 0;
            }
        }  // end loop on directory entries
        cluster_id++;
        offset = 0;
    }  // end loop on buffers

    if ( found == -1 )  // found end of directory in parent directory 
    {

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_child_from_parent(): found end of directory in <%s>\n",
        parent->name );
#endif
        *inode = NULL;
        return 1;
    }
    else               // found searched name in parent directory
    {
        // allocate a new inode and an empty Cache-File
        *inode = _allocate_one_inode( name,
                                      is_dir,
                                      cluster,
                                      size,
                                      0,             // count
                                      dentry,
                                      1 );           // cache_allocate

        // introduce it in Inode-Tree
        _add_inode_in_tree( *inode , parent );

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_child_from_parent(): found <%s> on device\n", name );
#endif
        return 0;
    }
}  // end _get_child_from_parent()




//////////////////////////////////////////////////////////////////
static unsigned int _get_inode_from_path( char*          pathname,
                                          fat_inode_t**  inode )
{
    char                 name[32];         // buffer for one name in pathname
    unsigned int         nb_read;	       // number of characters written in name[] 
    fat_inode_t*         parent;           // parent inode
    fat_inode_t*         child;            // child inode
    unsigned int         last;             // while exit condition 
    unsigned int         code;             // return value

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_inode_from_path(): enters for path <%s>\n", pathname );
#endif

    // handle root directory case
    if ( _strcmp( pathname , "/" ) == 0 )
    {

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_inode_from_path(): found root inode for <%s>\n", 
        pathname );
#endif
        *inode  = _fat.inode_tree_root;
        return 0;
    }

    // If the pathname is not "/", we traverse the inode tree from the root.
    // We use _get_name_from_path() to scan pathname and extract inode names.
    // We use _get_child_from_parent() to scan each directory in the path. 

    last       = 0;
    nb_read    = 0;                      // number of characters analysed in path
    parent     = _fat.inode_tree_root;   // Inode-Tree root 
    
    while ( !last )
    {
        // get searched file/dir name
        if ( _get_name_from_path( pathname, name, &nb_read ) )
        {
            return 3;   // error : name too long
        }

        // compute last iteration condition
        last = (pathname[nb_read] == 0);

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_inode_from_path(): got name <%s>\n", name );
#endif

        if ( _strcmp( name, ".." ) == 0)
        {
            // found special name "..", try to go up
            code = 0;
            if ( parent->parent )
                child = parent->parent;
            else
                child = parent;
        }
        else if ( _strcmp( name, "." ) == 0 )
        {
            // found special name ".", stay on the same level
            code = 0;
            child = parent;
        }
        else
        {
            // get child inode from parent directory
            code = _get_child_from_parent( parent,
                                           name,
                                           &child );

            // we need to find the child inode for all non terminal names
            if ( (code == 2) || ((code == 1 ) && !last) )
            {

    #if (GIET_DEBUG_FAT & 1)
    if ( _get_proctime() > GIET_DEBUG_FAT )
    _printf("\n[DEBUG FAT] _get_inode_from_path(): neither parent, nor child found for <%s>\n",
            pathname );
    #endif
                return 2;  // error : parent inode not found
            }
        }

        // update parent if not the last iteration
        if ( !last )
            parent = child;
    } // end while

    // returns inode pointer
    if (code == 0 )
    {

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_inode_from_path(): found inode for <%s>\n", 
        pathname );
#endif
        *inode  = child;
    }
    else
    {

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _get_inode_from_path(): found only parent inode for <%s>\n",
        pathname );
#endif
        *inode  = parent;
    }

    return code;                 // can be 0 (found) or 1 (not found)

}  // end _get_inode_from_path() 




//////////////////////////////////////////////////////////////
static unsigned int _remove_node_from_fs( fat_inode_t* inode )
{
    // check for root node
    if ( !inode->parent ) return 1;

    // remove entry in parent directory
    if ( _remove_dir_entry( inode ) ) return 1;

    // update parent directory on device
    if ( _update_device_from_cache( inode->parent->levels,
                                    inode->parent->cache,
                                    inode->parent->name ) ) return 1;

    // release clusters allocated to file/dir in DATA region
    if ( _clusters_release( inode->cluster ) ) return 1;

    // release File-Cache
    _release_cache_memory( inode->cache, inode->levels );
    _free ( inode->cache );

    // remove inode from Inode-Tree
    _remove_inode_from_tree( inode );

    // release inode
    _free ( inode );

    return 0;
}  // end _remove_node_from_fs()


//////////////////////////////////////////////////////////////////
static unsigned int _next_cluster_no_cache( unsigned int   cluster,
                                            unsigned int*  next )
{
    // compute cluster_id and slot_id 
    // each cluster contains 1024 slots (4 bytes per slot)
    unsigned int cluster_id  = cluster >> 10;
    unsigned int slot_id     = cluster & 0x3FF;

    // compute lba of cluster identified by cluster_id
    unsigned int lba = _fat.fat_lba + (cluster_id << 3);

    // get cluster containing the adressed FAT slot in FAT buffer
    if ( _fat_buffer_fat_lba != lba )
    {
        if ( _fat_ioc_access( 0,         // no descheduling
                              1,         // read
                              lba,
                              (unsigned int)_fat_buffer_fat,
                              8 ) )
        {
            _printf("\n[FAT ERROR] _next_cluster_no_cache(): "
                    "cannot load lba = %x into fat_buffer\n", lba );
            return 1;
        }

        _fat_buffer_fat_lba = lba;
    }

    // return next cluster index
    unsigned int* buf = (unsigned int*)_fat_buffer_fat;
    *next = buf[slot_id];
    return 0;
   
}  // end _next_cluster_no_cache()




/////////////////////////////////////////////////////////////////
static unsigned int _file_info_no_cache( char*          pathname,
                                         unsigned int*  file_cluster,
                                         unsigned int*  file_size )
{
    
#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _file_info_no_cache(): enters for path <%s>\n", pathname );
#endif

    char            name[32];             // buffer for one name in the analysed pathname
    char            lfn1[16];             // buffer for a partial name in LFN entry
    char            lfn2[16];             // buffer for a partial name in LFN entry
    char            lfn3[16];             // buffer for a partial name in LFN entry
    char            cname[32];            // buffer for a full name in a directory entry
    unsigned int    nb_read;           	  // number of characters analysed in path 
    unsigned int    parent_cluster;       // cluster index for the parent directory 
    unsigned int    child_cluster = 0;    // cluster index for the searched file/dir
    unsigned int    child_size = 0;       // size of the searched file/dir
    unsigned int    child_is_dir;         // type of the searched file/dir
    unsigned int    offset;               // offset in a 4 Kbytes buffer
    unsigned int    ord;                  // ORD field in a directory entry
    unsigned int    attr;                 // ATTR field in a directory entry
    unsigned int    lfn = 0;              // number of lfn entries
    unsigned char*  buf;                  // pointer on a 4 Kbytes buffer 
    unsigned int    found;                // name found in current directory entry

    // Three embedded loops: 
    // - scan pathname to extract file/dir names, 
    // - for each name, scan the clusters of the parent directory
    // - for each cluster, scan the 4 Kbytes buffer to find the file/dir name
    // The starting point is the root directory (cluster 2)

    nb_read        = 0;
    parent_cluster = 2; 

    // scan pathname  
    while ( pathname[nb_read] != 0 )    
    {
        // get searched file/dir name
        if ( _get_name_from_path( pathname, name, &nb_read ) ) return 1;

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _file_info_no_cache(): search name <%s>"
        " in cluster %x\n", name , parent_cluster );
#endif
        found  = 0;

        // scan clusters containing the parent directory
        while ( found == 0 )  
        {
            // compute lba 
            unsigned int lba = _cluster_to_lba( parent_cluster );

            // load one cluster of the parent directory into data_buffer
            if ( _fat_buffer_data_lba != lba )
            {
                if ( _fat_ioc_access( 0,         // no descheduling
                                      1,         // read
                                      lba,
                                      (unsigned int)_fat_buffer_data,
                                      8 ) )
                {
                    _printf("\n[FAT ERROR] _file_info_no_cache(): "
                            "cannot load lba = %x into data_buffer\n", lba );
                    return 1;
                }

                _fat_buffer_data_lba = lba;
            }

            offset = 0;

            // scan this 4 Kbytes buffer 
            while ( (offset < 4096) && (found == 0) )
            {
                buf  = _fat_buffer_data + offset;
                attr = _read_entry( DIR_ATTR , buf , 0 );   
                ord  = _read_entry( LDIR_ORD , buf , 0 );

                if (ord == NO_MORE_ENTRY)               // no more entry => break
                {
                    found = 2;
                }
                else if ( ord == FREE_ENTRY )           // free entry => skip
                {
                    offset = offset + 32;
                }
                else if ( attr == ATTR_LONG_NAME_MASK ) // LFN entry => get partial name
                {
                    unsigned int seq = ord & 0x3;
                    lfn = (seq > lfn) ? seq : lfn;   
                    if      ( seq == 1 ) _get_name_from_long( buf, lfn1 );
                    else if ( seq == 2 ) _get_name_from_long( buf, lfn2 );
                    else if ( seq == 3 ) _get_name_from_long( buf, lfn3 );
                    offset = offset + 32;
                }
                else                                    // NORMAL entry
                {
                    // build the full mame for current directory entry
                    if      ( lfn == 0 )
                    {
                        _get_name_from_short( buf , cname );
                    }
                    else if ( lfn == 1 )
                    {
                        _strcpy( cname      , lfn1 );
                    }   
                    else if ( lfn == 2 ) 
                    {
                        _strcpy( cname      , lfn1 );
                        _strcpy( cname + 13 , lfn2 );
                    }
                    else if ( lfn == 3 ) 
                    {
                        _strcpy( cname      , lfn1 );
                        _strcpy( cname + 13 , lfn2 );
                        _strcpy( cname + 26 , lfn3 );
                    }
                    
                    // test if extracted name == searched name
                    if ( _strcmp( name , cname ) == 0 )
                    {
                        child_cluster = (_read_entry( DIR_FST_CLUS_HI , buf , 1 ) << 16) |
                                        (_read_entry( DIR_FST_CLUS_LO , buf , 1 )      ) ;
                        child_is_dir  = ((attr & ATTR_DIRECTORY) == ATTR_DIRECTORY);
                        child_size    = _read_entry( DIR_FILE_SIZE , buf , 1 );
                        found         = 1;
                    }
                    offset = offset + 32;
                    lfn = 0;
                }
            }  // en loop on directory entries
            
            // compute next cluster index
            unsigned int next;
            if ( _next_cluster_no_cache ( parent_cluster , &next ) ) return 1;
            parent_cluster = next;
        } // end loop on clusters 

        if ( found == 2 )  // found end of directory => error
        { 
            _printf("\n[FAT ERROR] _file_info_no_cache(): <%s> not found\n",
                    name );
            return 1;
        }
 
        // check type
        if ( ((pathname[nb_read] == 0) && (child_is_dir != 0)) ||
             ((pathname[nb_read] != 0) && (child_is_dir == 0)) )
        {
            _printf("\n[FAT ERROR] _file_info_no_cache(): illegal type for <%s>\n", name );
            return 1;
        }

        // update parent_cluster for next name
        parent_cluster = child_cluster;

    }  // end loop on names

#if (GIET_DEBUG_FAT & 1)
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _file_info_no_cache(): success for <%s> / "
        "file_size = %x / file_cluster = %x\n", pathname, child_size, child_cluster );
#endif

    // return file cluster and size
    *file_size    = child_size;
    *file_cluster = child_cluster;
    return 0;

}  // end _file_info_no_cache()



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//             Extern functions                                               
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// This function initializes the FAT structures.
// - The Fat-Descriptor is always initialized.
// - The dynamically allocated structures (the Inode-Tre, the Fat_Cache, 
//   and the File-Cache for the root directory) are only allocated
//   and initialized if the "kernel_mode" argument is set.
/////////////////////////////////////////////////////////////////////////////
// Implementation note:
// This function is called twice, by the boot-loader, and by the kernel_init.
// It does not use dynamic memory allocation from the distributed heap.
// It use informations found in the boot sector and FS-INFO sector.
// that are loaded in the Fat-Descriptor temporary block_buffer.
/////////////////////////////////////////////////////////////////////////////
// Returns GIET_FAT32_OK on success.
// Returns a negative value on error:
//   GIET_FAT32_IO_ERROR,
//   GIET_FAT32_INVALID_BOOT_SECTOR
/////////////////////////////////////////////////////////////////////////////
int _fat_init( unsigned int kernel_mode )  
{

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_init(): enters at cycle %d\n", _get_proctime() );
#endif

    // FAT initialisation should be done only once
    if ( _fat.initialized == FAT_INITIALIZED )
    {
        _printf("\n[FAT WARNING] _fat_init(): FAT already initialized\n");
        return GIET_FAT32_OK;
    }

    // load Boot sector (VBR) into FAT buffer
    if ( _fat_ioc_access( 0,                                  // no descheduling
                          1,                                  // read
                          0,                                  // block index
                          (unsigned int)_fat.block_buffer,
                          1 ) )                               // one block 
    {
        _printf("\n[FAT ERROR] _fat_init(): cannot load VBR\n");
        return GIET_FAT32_IO_ERROR;
    }

    _fat.block_buffer_lba = 0;
    
#if GIET_DEBUG_FAT 
if ( _get_proctime() > GIET_DEBUG_FAT )
{
    _printf("\n[DEBUG FAT] _fat_init(): Boot sector loaded\n");
    _display_one_block( _fat.block_buffer, "block device", _fat.block_buffer_lba );
}
#endif

    // checking various FAT32 assuptions from boot sector
    if( _read_entry( BPB_BYTSPERSEC, _fat.block_buffer, 1 ) != 512 )
    {
        _printf("\n[FAT ERROR] _fat_init(): The sector size must be 512 bytes\n");
        return GIET_FAT32_INVALID_BOOT_SECTOR;
    }
    if( _read_entry( BPB_SECPERCLUS, _fat.block_buffer, 1 ) != 8 )
    {
        _printf("\n[FAT ERROR] _fat_init(): The cluster size must be 8 blocks\n");
        return GIET_FAT32_INVALID_BOOT_SECTOR;
    }
    if( _read_entry( BPB_NUMFATS, _fat.block_buffer, 1 ) != 1 )
    {
        _printf("\n[FAT ERROR] _fat_init(): The number of FAT copies in FAT region must be 1\n");
        return GIET_FAT32_INVALID_BOOT_SECTOR;
    }
    if( (_read_entry( BPB_FAT32_FATSZ32, _fat.block_buffer, 1 ) & 0xF) != 0 )
    {
        _printf("\n[FAT ERROR] _fat_init(): The FAT region must be multiple of 16 sectors\n");
        return GIET_FAT32_INVALID_BOOT_SECTOR;
    }
    if( _read_entry( BPB_FAT32_ROOTCLUS, _fat.block_buffer, 1 ) != 2 )
    {
        _printf("\n[FAT ERROR] _fat_init(): The root directory must be at cluster 2\n");
        return GIET_FAT32_INVALID_BOOT_SECTOR;
    }

    // initialise Fat-Descriptor from VBR
    _fat.sector_size         = 512;
    _fat.cluster_size        = 4096;
    _fat.fat_sectors         = _read_entry( BPB_FAT32_FATSZ32 , _fat.block_buffer , 1 );
    _fat.fat_lba             = _read_entry( BPB_RSVDSECCNT , _fat.block_buffer , 1 );
    _fat.data_sectors        = _fat.fat_sectors << 10;
    _fat.data_lba            = _fat.fat_lba + _fat.fat_sectors;
    _fat.fs_info_lba         = _read_entry( BPB_FAT32_FSINFO , _fat.block_buffer , 1 );
    _fat_buffer_fat_lba      = 0xFFFFFFFF;
    _fat_buffer_data_lba     = 0xFFFFFFFF;
    _fat.initialized         = FAT_INITIALIZED;

    // load FS_INFO sector into FAT buffer
    if ( _fat_ioc_access( 0,                                // no descheduling 
                          1,                                // read
                          _fat.fs_info_lba,                 // lba 
                          (unsigned int)_fat.block_buffer,
                          1 ) )                             // one block
    { 
        _printf("\n[FAT ERROR] _fat_init(): cannot load FS_INFO Sector\n"); 
        return GIET_FAT32_IO_ERROR;
    }

    _fat.block_buffer_lba = _fat.fs_info_lba;

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
{
    _printf("\n[DEBUG FAT] _fat_init(): FS-INFO sector loaded\n");
    _display_one_block( _fat.block_buffer, "block device", _fat.block_buffer_lba );
}
#endif

    // initialise Fat-Descriptor from FS_INFO
    _fat.free_clusters_number   = _read_entry( FS_FREE_CLUSTERS    , _fat.block_buffer, 1);
    _fat.first_free_cluster     = _read_entry( FS_FREE_CLUSTER_HINT, _fat.block_buffer, 1);

    // This is done only when the _fat_init() is called in kernel mode

    if ( kernel_mode )
    {
        unsigned int i;

        // create Inode-Tree root
        _fat.inode_tree_root = _allocate_one_inode("/", // dir name
                                                   1,   // directory
                                                   2,   // cluster id
                                                   0,   // no size
                                                   0,   // no children
                                                   0,   // no dentry
                                                   1);  // allocate cache

        // initialize lock
        _spin_lock_init( &_fat.fat_lock );

        // initialize File Descriptor Array
        for( i = 0 ; i < GIET_OPEN_FILES_MAX ; i++ ) _fat.fd[i].allocated = 0;

        // initialize fat_cache root
        _fat.fat_cache_root   = _allocate_one_cache_node( NULL );
        _fat.fat_cache_levels = _get_levels_from_size( _fat.fat_sectors << 9 );
    }  // end if kernel_mode

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_display_fat_descriptor();
#endif

    return GIET_FAT32_OK;
}  // end _fat_init()




///////////////////////////////////////////////////////////////////////////////
// This function implements the giet_fat_open() system call.
// The semantic is similar to the UNIX open() function, but only the O_CREATE
// and O_RDONLY flags are supported. The UNIX access rights are not supported. 
// If the file does not exist in the specified directory, it is created.
// If the specified directory does not exist, an error is returned.
// It allocates a file descriptor to the calling task, for the file identified 
// by "pathname". If several tasks try to open the same file, each task  
// obtains a private file descriptor. 
// A node name (file or directory) cannot be larger than 31 characters.
///////////////////////////////////////////////////////////////////////////////
// Returns a file descriptor index on success.
// Returns a negative value on error:
//   GIET_FAT32_NOT_INITIALIZED,
//   GIET_FAT32_FILE_NOT_FOUND,
//   GIET_FAT32_NAME_TOO_LONG,
//   GIET_FAT32_IO_ERROR,
//   GIET_FAT32_TOO_MANY_OPEN_FILES
///////////////////////////////////////////////////////////////////////////////
int _fat_open( char*        pathname,     // absolute path from root
               unsigned int flags )       // O_CREATE and O_RDONLY
{
    unsigned int         fd_id;            // index in File-Descriptor-Array
    unsigned int         code;             // error code
    fat_inode_t*         inode;            // anonymous inode pointer
    fat_inode_t*         child;            // pointer on searched file inode
    fat_inode_t*         parent;           // pointer on parent directory inode
    
    // get flags
    unsigned int create    = ((flags & O_CREATE) != 0);
    unsigned int read_only = ((flags & O_RDONLY) != 0);
    unsigned int truncate  = ((flags & O_TRUNC)  != 0);

#if GIET_DEBUG_FAT
unsigned int procid  = _get_procid();
unsigned int x       = procid >> (Y_WIDTH + P_WIDTH);
unsigned int y       = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int p       = procid & ((1<<P_WIDTH)-1);
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_open(): P[%d,%d,%d] enters for path <%s> / "
        " create = %d / read_only = %d\n",
        x, y, p, pathname , create , read_only );
#endif

    // checking FAT initialized
    if( _fat.initialized != FAT_INITIALIZED )
    {
        _printf("\n[FAT ERROR] _fat_open(): FAT not initialized\n");
        return GIET_FAT32_NOT_INITIALIZED;
    }

    // takes the lock
    _spin_lock_acquire( &_fat.fat_lock );

    // get inode pointer
    code = _get_inode_from_path( pathname , &inode );

    if ( code == 2 )  
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_open(): path to parent not found"
                " for file <%s>\n", pathname );
        return GIET_FAT32_FILE_NOT_FOUND;
    }
    else if ( code == 3 )  
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_open(): one name in path too long"
                " for file <%s>\n", pathname );
        return GIET_FAT32_NAME_TOO_LONG;
    }
    else if ( (code == 1) && (create == 0) )   
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_open(): file not found"
                " for file <%s>\n", pathname );
        return GIET_FAT32_FILE_NOT_FOUND;
    }
    else if ( (code == 1) && (create != 0) )   // file name not found => create 
    {
        // set parent inode pointer
        parent = inode;

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_open(): P[%d,%d,%d] create a new file <%s>\n",
        x , y , p , pathname );
#endif

        // get new file name / error check already done by _get_inode_from_path()
        char name[32];        
        _get_last_name( pathname , name );

        // allocate a new inode and an empty Cache-File 
        child = _allocate_one_inode( name,
                                     0,                         // not a directory
                                     END_OF_CHAIN_CLUSTER_MAX,  // no cluster allocated
                                     0,                         // size : new file is empty
                                     0,                         // count incremented later
                                     0,                         // dentry set by add_dir_entry
                                     1 );                       // cache_allocate

        // introduce inode into Inode-Tree
        _add_inode_in_tree( child , parent );

        // add an entry in the parent directory Cache_file
        if ( _add_dir_entry( child , parent ) )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_open(): cannot update parent directory"
                    " for file <%s>\n" , pathname );
            return GIET_FAT32_IO_ERROR;
        } 

        // update DATA region on block device for parent directory
        if ( _update_device_from_cache( parent->levels,
                                        parent->cache,
                                        parent->name ) )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_open(): cannot update DATA region "
                    " for parent of file <%s>\n", pathname );
            return GIET_FAT32_IO_ERROR;
        }

        // update FAT region on block device
        if ( _update_device_from_cache( _fat.fat_cache_levels,
                                        _fat.fat_cache_root,
                                        "FAT" ) )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_open(): cannot update FAT region"
                    " for file <%s>\n", pathname );
            return GIET_FAT32_IO_ERROR;
        }

        // update FS_INFO sector
        if ( _update_fs_info() )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_open(): cannot update FS-INFO"
                    " for file <%s>\n", pathname );
            return GIET_FAT32_IO_ERROR;
        }

        // no need to truncate a new file
        truncate = 0;
    }
    else // code == 0
    {
        // set searched file inode pointer
        child = inode;

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_open(): P[%d,%d,%d] found file <%s> on device : inode = %x\n",
        x , y , p , pathname , child );
#endif
    }

    // Search an empty slot in file descriptors array
    fd_id = 0;
    while ( (_fat.fd[fd_id].allocated) != 0 && (fd_id < GIET_OPEN_FILES_MAX) )
    {
        fd_id++;
    }

    // set file descriptor if an empty slot has been found
    if ( fd_id >= GIET_OPEN_FILES_MAX )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_open(): File-Descriptors-Array full\n");
        return GIET_FAT32_TOO_MANY_OPEN_FILES;
    }

    // update file descriptor
    _fat.fd[fd_id].allocated  = 1;
    _fat.fd[fd_id].seek       = 0;
    _fat.fd[fd_id].read_only  = read_only;
    _fat.fd[fd_id].inode      = child;

    // increment the refcount
    child->count = child->count + 1;

    // truncate the file if requested
    if ( truncate && !read_only && !child->is_dir )
    {
        // empty file
        child->size = 0;
        child->levels = _get_levels_from_size( child->size );

        // release File-Cache (keep root node)
        _release_cache_memory( child->cache, child->levels );

        // release clusters allocated to file/dir in DATA region
        if ( _clusters_release( child->cluster ) )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_open(): can't truncate file\n");
            return GIET_FAT32_IO_ERROR;
        }

        // update parent directory entry (size and cluster index)
        if ( _update_dir_entry( child ) )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_open(): can't truncate file\n");
            return GIET_FAT32_IO_ERROR;
        }
    }

    // releases the lock
    _spin_lock_release( &_fat.fat_lock );

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_open(): P[%d,%d,%d] got fd = %d for <%s> / "
        "read_only = %d\n",
        x , y , p , fd_id , pathname , read_only );
#endif
    return fd_id;
} // end _fat_open()




/////////////////////////////////////////////////////////////////////////////////
// This function implements the "giet_fat_close()" system call.
// It decrements the inode reference count, and release the fd_id entry
// in the file descriptors array.
// If the reference count is zero, it writes all dirty clusters on block device, 
// and releases the memory allocated to the File_Cache.
/////////////////////////////////////////////////////////////////////////////////
// Returns GIET_FAT32_OK on success.
// Returns a negative value on error:
//   GIET_FAT32_NOT_INITIALIZED,
//   GIET_FAT32_INVALID_FD,
//   GIET_FAT32_NOT_OPEN,
//   GIET_FAT32_IO_ERROR
/////////////////////////////////////////////////////////////////////////////////
int _fat_close( unsigned int fd_id )
{
    // checking FAT initialized
    if( _fat.initialized != FAT_INITIALIZED )
    {
        _printf("\n[FAT ERROR] _fat_close(): FAT not initialized\n");
        return GIET_FAT32_NOT_INITIALIZED;
    }

    if( (fd_id >= GIET_OPEN_FILES_MAX) )
    {
        _printf("\n[FAT ERROR] _fat_close(): illegal file descriptor index\n");
        return GIET_FAT32_INVALID_FD;
    } 

    // takes lock
    _spin_lock_acquire( &_fat.fat_lock );

    if( _fat.fd[fd_id].allocated == 0 )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_close(): file not open\n");
        return GIET_FAT32_NOT_OPEN;
    }

    // get the inode pointer 
    fat_inode_t*  inode = _fat.fd[fd_id].inode;

    // decrement reference count
    inode->count = inode->count - 1;
    
#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[FAT DEBUG] _fat_close() for file <%s> : refcount = %d\n",
        inode->name , inode->count );
#endif

    // update block device and release File-Cache if no more references
    if ( inode->count == 0 )
    {
        // update all dirty clusters for closed file
        if ( _update_device_from_cache( inode->levels, 
                                        inode->cache,
                                        inode->name ) ) 
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_close(): cannot write dirty clusters "
                    "for file <%s>\n", inode->name );
            return GIET_FAT32_IO_ERROR;
        }

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[FAT DEBUG] _fat_close() update device for file <%s>\n", inode->name );
#endif

        // update directory dirty clusters for parent directory
        if ( inode->parent &&
             _update_device_from_cache( inode->parent->levels,
                                        inode->parent->cache,
                                        inode->parent->name ) )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_close(): cannot write dirty clusters "
                    "for directory <%s>\n", inode->parent->name );
            return GIET_FAT32_IO_ERROR;
        }

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[FAT DEBUG] _fat_close() update device for parent directory <%s>\n",
        inode->parent->name );
#endif

        // release memory allocated to File-Cache (keep cache root node)
        _release_cache_memory( inode->cache, inode->levels );

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[FAT DEBUG] _fat_close() release memory for File-Cache <%s>\n",
        inode->name );
#endif

    }

    // release fd_id entry in file descriptor array
    _fat.fd[fd_id].allocated = 0;

    // release lock
    _spin_lock_release( &_fat.fat_lock );

    return GIET_FAT32_OK;
} // end fat_close()




/////////////////////////////////////////////////////////////////////////////////
// This function implements the giet_fat_file_info() system call.
// It returns the size, the current offset and the directory info for a file
// identified by the "fd_id" argument.
/////////////////////////////////////////////////////////////////////////////////
// Returns GIET_FAT32_OK on success.
// Returns a negative value on error:
//   GIET_FAT32_NOT_INITIALIZED,
//   GIET_FAT32_INVALID_FD,
//   GIET_FAT32_NOT_OPEN
/////////////////////////////////////////////////////////////////////////////////
int _fat_file_info( unsigned int     fd_id,
                    fat_file_info_t* info )
{
    if ( _fat.initialized != FAT_INITIALIZED )
    {
        _printf("\n[FAT ERROR] _fat_file_info(): FAT not initialized\n");
        return GIET_FAT32_NOT_INITIALIZED;
    }

    if ( fd_id >= GIET_OPEN_FILES_MAX )
    {
        _printf("\n[FAT ERROR] _fat_file_info(): illegal file descriptor index\n");
        return GIET_FAT32_INVALID_FD;
    } 

    if ( _fat.fd[fd_id].allocated == 0 )
    {
        _printf("\n[FAT ERROR] _fat_file_info(): file not open\n");
        return GIET_FAT32_NOT_OPEN;
    }

    info->size   = _fat.fd[fd_id].inode->size;
    info->offset = _fat.fd[fd_id].seek;
    info->is_dir = _fat.fd[fd_id].inode->is_dir;

    return GIET_FAT32_OK;
} // end _fat_file_info()




/////////////////////////////////////////////////////////////////////////////////
// The following function implements the "giet_fat_read()" system call.
// It transfers "count" bytes from the File_Cache associated to the file
// identified by "fd_id", to the user "buffer", from the current file offset.
// In case of miss in the File_Cache, it loads all involved clusters into cache.
/////////////////////////////////////////////////////////////////////////////////
// Returns the number of bytes actually transfered on success.
// Returns 0 if EOF is encountered (offset + count > file_size). 
// Returns a negative value on error:
//   GIET_FAT32_NOT_INITIALIZED,
//   GIET_FAT32_INVALID_FD,
//   GIET_FAT32_NOT_OPEN,
//   GIET_FAT32_IO_ERROR
/////////////////////////////////////////////////////////////////////////////////
int _fat_read( unsigned int fd_id,     // file descriptor index
               void*        buffer,    // destination buffer
               unsigned int count )    // number of bytes to read
{
    // checking FAT initialized
    if( _fat.initialized != FAT_INITIALIZED )
    {
        _printf("\n[FAT ERROR] _fat_write(): FAT not initialized\n");
        return GIET_FAT32_NOT_INITIALIZED;
    }

    // check fd_id overflow
    if ( fd_id >= GIET_OPEN_FILES_MAX )
    {
        _printf("\n[FAT ERROR] _fat_read(): illegal file descriptor\n");
        return GIET_FAT32_INVALID_FD;
    }

    // check file is open
    if ( _fat.fd[fd_id].allocated == 0 )
    {
        _printf("\n[FAT ERROR] _fat_read(): file not open\n");
        return GIET_FAT32_NOT_OPEN;
    }

    // takes lock
    _spin_lock_acquire( &_fat.fat_lock );
           
    // get file inode pointer and offset
    fat_inode_t* inode  = _fat.fd[fd_id].inode;
    unsigned int seek   = _fat.fd[fd_id].seek;

    // check count & seek versus file size
    if ( count + seek > inode->size && !inode->is_dir )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_read(): file too small"
                " / seek = %x / count = %x / file_size = %x\n",
                seek , count , inode->size );
        return 0;
    }

    // compute first_cluster_id and first_byte_to_move 
    unsigned int first_cluster_id   = seek >> 12;
    unsigned int first_byte_to_move = seek & 0xFFF;   

    // compute last_cluster and last_byte_to_move 
    unsigned int last_cluster_id   = (seek + count - 1) >> 12;   
    unsigned int last_byte_to_move = (seek + count - 1) & 0xFFF;

#if GIET_DEBUG_FAT
unsigned int procid  = _get_procid();
unsigned int x       = procid >> (Y_WIDTH + P_WIDTH);
unsigned int y       = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int p       = procid & ((1<<P_WIDTH)-1);
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_read(): P[%d,%d,%d] enters for file <%s> "
        " / bytes = %x / offset = %x\n"
        "first_cluster_id = %x / first_byte_to_move = %x"
        " / last_cluster_id = %x / last_byte_to_move = %x\n",
        x , y , p , inode->name , count , seek ,
        first_cluster_id , first_byte_to_move , last_cluster_id , last_byte_to_move );
#endif

    // loop on all cluster covering the requested transfer
    unsigned int cluster_id;
    unsigned int done = 0;
    for ( cluster_id = first_cluster_id ; cluster_id <= last_cluster_id ; cluster_id++ )
    {
        // get pointer on the cluster_id buffer in cache 
        unsigned char*     cbuf;
        fat_cache_desc_t*  pdesc;
        if ( _get_buffer_from_cache( inode, 
                                     cluster_id,
                                     &pdesc ) )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_read(): cannot load file <%s>\n",
                    inode->name );
            return GIET_FAT32_IO_ERROR;
        }
        cbuf = pdesc->buffer;

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_read(): P[%d,%d,%d] moves cluster_id %d from Cache-File <%s>\n",
        x , y , p , cluster_id, inode->name );
#endif

        // compute memcpy arguments
        unsigned char*  source;
        unsigned int    nbytes;
        unsigned char*  dest = (unsigned char*)buffer + done;
        if ( (cluster_id == first_cluster_id) && (cluster_id == last_cluster_id) )
        {
            source = cbuf + first_byte_to_move; 
            nbytes = last_byte_to_move - first_byte_to_move + 1;
        }
        else if ( cluster_id == first_cluster_id )
        {
            source = cbuf + first_byte_to_move; 
            nbytes = 4096 - first_byte_to_move;
        }
        else if ( cluster_id == last_cluster_id )
        {
            source = cbuf; 
            nbytes = last_byte_to_move + 1;
        }
        else  // not first / not last
        {
            source = cbuf; 
            nbytes = 4096;
        }

        // move data
        memcpy( dest , source , nbytes );
        done = done + nbytes;
    }

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_read(): P[%d,%d,%d] loaded file <%s> from Cache-File\n",
        x , y , p , inode->name );
#endif

    // update seek
    _fat.fd[fd_id].seek += done;

    // release lock
    _spin_lock_release( &_fat.fat_lock );

    return done;
} // end _fat_read()




/////////////////////////////////////////////////////////////////////////////////
// The following function implements the "giet_fat_write()" system call.
// It transfers "count" bytes to the fat_cache associated to the file
// identified by "fd_id", from the user "buffer", using the current file offset.
// It increases the file size and allocate new clusters if (count + offset) 
// is larger than the current file size. Then it loads and updates all 
// involved clusters in the cache.
/////////////////////////////////////////////////////////////////////////////////
// Returns number of bytes actually written on success.
// Returns a negative value on error:
//   GIET_FAT32_NOT_INITIALIZED,
//   GIET_FAT32_INVALID_FD,
//   GIET_FAT32_NOT_OPEN,
//   GIET_FAT32_READ_ONLY,
//   GIET_FAT32_NO_FREE_SPACE,
//   GIET_FAT32_IO_ERROR
/////////////////////////////////////////////////////////////////////////////////
int _fat_write( unsigned int fd_id,    // file descriptor index
                void*        buffer,   // source buffer
                unsigned int count )   // number of bytes to write
{
    // checking FAT initialized
    if( _fat.initialized != FAT_INITIALIZED )
    {
        _printf("\n[FAT ERROR] _fat_write(): FAT not initialized\n");
        return GIET_FAT32_NOT_INITIALIZED;
    }

    // takes lock
    _spin_lock_acquire( &_fat.fat_lock );
           
    // check fd_id overflow
    if ( fd_id >= GIET_OPEN_FILES_MAX )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_write(): illegal file descriptor\n");
        return GIET_FAT32_INVALID_FD;
    }

    // check file open
    if ( _fat.fd[fd_id].allocated == 0 )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_write(): file not open\n" );
        return GIET_FAT32_NOT_OPEN;
    }

    // check file writable
    if ( _fat.fd[fd_id].read_only )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_write(): file <%s> is read-only\n",
                _fat.fd[fd_id].inode->name );
        return GIET_FAT32_READ_ONLY;
    }

    // get file inode pointer and seek 
    fat_inode_t* inode  = _fat.fd[fd_id].inode;
    unsigned int seek   = _fat.fd[fd_id].seek;

#if GIET_DEBUG_FAT
unsigned int procid  = _get_procid();
unsigned int x       = procid >> (Y_WIDTH + P_WIDTH);
unsigned int y       = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int p       = procid & ((1<<P_WIDTH)-1);
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_write(): P[%d,%d,%d] enters for file <%s> "
        " / bytes = %x / seek = %x\n",
        x , y , p , inode->name , count , seek );
#endif

    // chek if file size must be incremented
    // and allocate new clusters from FAT if required
    unsigned int old_size = inode->size;
    unsigned int new_size = seek + count;
    if ( new_size > old_size )
    {
        // update size in inode
        inode->size = new_size;
 
        // compute current and required numbers of clusters
        unsigned old_clusters = old_size >> 12;
        if ( old_size & 0xFFF ) old_clusters++;

        unsigned new_clusters = new_size >> 12;
        if ( new_size & 0xFFF ) new_clusters++;

        // allocate new clusters from FAT if required
        if ( new_clusters > old_clusters )
        {

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_write(): P[%d,%d,%d] allocates new clusters for file <%s>"
        " / current = %d / required = %d\n",
        x , y , p , inode->name , old_clusters , new_clusters );
#endif
            // allocate missing clusters
            if ( _clusters_allocate( inode,
                                     old_clusters,
                                     new_clusters - old_clusters ) )
            {
                _spin_lock_release( &_fat.fat_lock );
                _printf("\n[FAT ERROR] _fat_write(): no free clusters"
                        " for file <%s>\n", _fat.fd[fd_id].inode->name );
                return GIET_FAT32_NO_FREE_SPACE;
            }
        }
         
        // update parent directory entry (size and cluster index)
        if ( _update_dir_entry( inode ) )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_write(): cannot update parent directory entry"
                    " for file <%s>\n", _fat.fd[fd_id].inode->name );
            return GIET_FAT32_IO_ERROR;
        }
            

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_write(): P[%d,%d,%d] updates size for file <%s> / size = %x\n",
        x , y , p , inode->name , (new_size - old_size) );
#endif

    }

    // compute first_cluster_id and first_byte_to_move 
    unsigned int first_cluster_id   = seek >> 12;
    unsigned int first_byte_to_move = seek & 0xFFF;   

    // compute last_cluster and last_byte_to_move 
    unsigned int last_cluster_id   = (seek + count - 1) >> 12;   
    unsigned int last_byte_to_move = (seek + count - 1) & 0xFFF;

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_write(): P[%d,%d,%d] starts loop on clusters for file <%s>\n"
        "  first_cluster_id = %d / first_byte_to_move = %x"
        " / last_cluster_id = %d / last_byte_to_move = %x\n",
        x , y , p , inode->name ,
        first_cluster_id , first_byte_to_move , last_cluster_id , last_byte_to_move );
#endif

    // loop on all clusters covering the requested transfer
    unsigned int cluster_id;
    unsigned int done = 0;
    for ( cluster_id = first_cluster_id ; cluster_id <= last_cluster_id ; cluster_id++ )
    {
        // get pointer on one 4K buffer in File-Cache 
        unsigned char*     cbuf;
        fat_cache_desc_t*  pdesc;
        if ( _get_buffer_from_cache( inode,   
                                     cluster_id,  
                                     &pdesc ) )   
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_write(): cannot load file <%s>\n",
                    inode->name );
            return GIET_FAT32_IO_ERROR;
        }
        
        cbuf         = pdesc->buffer;
        pdesc->dirty = 1;
   
#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_write(): P[%d,%d,%d] move cluster_id %d to Cache-file <%s>\n",
        x , y , p , cluster_id, inode->name );
#endif

        // compute memcpy arguments
        unsigned char* source = (unsigned char*)buffer + done;
        unsigned char* dest;
        unsigned int   nbytes;
        if ( (cluster_id == first_cluster_id) && (cluster_id == last_cluster_id) )
        {
            dest   = cbuf + first_byte_to_move; 
            nbytes = last_byte_to_move - first_byte_to_move + 1;
        }
        else if ( cluster_id == first_cluster_id )
        {
            dest   = cbuf + first_byte_to_move; 
            nbytes = 4096 - first_byte_to_move;
        }
        else if ( cluster_id == last_cluster_id )
        {
            dest   = cbuf; 
            nbytes = last_byte_to_move + 1;
        }
        else
        {
            dest   = cbuf; 
            nbytes = 4096;
        }

        //move date
        memcpy( dest , source , nbytes ); 
        done = done + nbytes;

    } // end for clusters

    // update seek
    _fat.fd[fd_id].seek += done;

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_write(): P[%d,%d,%d] store file <%s> into Cache-File\n",
        x , y , p , inode->name );
#endif

    // release lock
    _spin_lock_release( &_fat.fat_lock );

    return done;
} // end _fat_write()



/////////////////////////////////////////////////////////////////////////////////
// The following function implements the "giet_fat_lseek()" system call.
// It repositions the seek in the file descriptor "fd_id", according to
// the "seek" and "whence" arguments.
// It has the same semantic as the UNIX lseek() function.
// Accepted values for whence are SEEK_SET and SEEK_CUR.
/////////////////////////////////////////////////////////////////////////////////
// Returns new seek value (in bytes) on success.
// Returns a negative value on error:
//   GIET_FAT32_NOT_INITIALIZED,
//   GIET_FAT32_INVALID_FD,
//   GIET_FAT32_NOT_OPEN,
//   GIET_FAT32_INVALID_ARG
/////////////////////////////////////////////////////////////////////////////////
int _fat_lseek( unsigned int fd_id,
                unsigned int seek,
                unsigned int whence )
{
    // checking FAT initialized
    if( _fat.initialized != FAT_INITIALIZED )
    {
        _printf("\n[FAT ERROR] _fat_lseek(): FAT not initialized\n");
        return GIET_FAT32_NOT_INITIALIZED;
    }

    // check fd_id overflow
    if ( fd_id >= GIET_OPEN_FILES_MAX )
    {
        _printf("\n[FAT ERROR] _fat_lseek(): illegal file descriptor\n");
        return GIET_FAT32_INVALID_FD;
    }

    // takes lock
    _spin_lock_acquire( &_fat.fat_lock );

    // check file open
    if ( _fat.fd[fd_id].allocated == 0 )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_lseek(): file not open\n");
        return GIET_FAT32_NOT_OPEN;
    }

    unsigned int  new_seek;

    // compute new seek
    if      ( whence == SEEK_CUR ) new_seek = _fat.fd[fd_id].seek + seek;
    else if ( whence == SEEK_SET ) new_seek = seek;
    else
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_lseek(): illegal whence value\n");
        return GIET_FAT32_INVALID_ARG;
    }

    // update file descriptor offset 
    _fat.fd[fd_id].seek = new_seek;

#if GIET_DEBUG_FAT
unsigned int procid  = _get_procid();
unsigned int x       = procid >> (Y_WIDTH + P_WIDTH);
unsigned int y       = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int p       = procid & ((1<<P_WIDTH)-1);
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_lseek(): P[%d,%d,%d] set seek = %x for file <%s>\n",
        x , y , p , new_seek , _fat.fd[fd_id].inode->name );
#endif

    // release lock
    _spin_lock_release( &_fat.fat_lock );

    return new_seek;
}  // end _fat_lseek()



/////////////////////////////////////////////////////////////////////////////////
// The following function implements the giet_fat_remove() system call. 
// It deletes the file/directory identified by the "pathname" argument from 
// the file system, if the remove condition is fulfilled (directory empty,
// or file not referenced).
// All clusters allocated in the block device DATA region are released.
// The FAT region is updated on the block device.
// The Inode-Tree is updated.
// The associated File_Cache is released.
// The Fat_Cache is updated.
/////////////////////////////////////////////////////////////////////////////////
// Returns GIET_FAT32_OK on success.
// Returns a negative value on error:
//   GIET_FAT32_NOT_INITIALIZED,
//   GIET_FAT32_FILE_NOT_FOUND,
//   GIET_FAT32_NAME_TOO_LONG,
//   GIET_FAT32_IS_DIRECTORY,
//   GIET_FAT32_NOT_A_DIRECTORY,
//   GIET_FAT32_IS_OPEN,
//   GIET_FAT32_IO_ERROR,
//   GIET_FAT32_DIRECTORY_NOT_EMPTY
/////////////////////////////////////////////////////////////////////////////////
int _fat_remove( char*        pathname,
                 unsigned int should_be_dir )
{
    fat_inode_t*  inode;            // searched file inode pointer

#if GIET_DEBUG_FAT
unsigned int procid  = _get_procid();
unsigned int x       = procid >> (Y_WIDTH + P_WIDTH);
unsigned int y       = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int p       = procid & ((1<<P_WIDTH)-1);
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_remove(): P[%d,%d,%d] enters for path <%s>\n",
        x, y, p, pathname );
#endif

    // checking FAT initialized
    if( _fat.initialized != FAT_INITIALIZED )
    {
        _printf("\n[FAT ERROR] _fat_remove(): FAT not initialized\n");
        return GIET_FAT32_NOT_INITIALIZED;
    }

    // take the lock 
    _spin_lock_acquire( &_fat.fat_lock );

    // get searched file inode 
    unsigned int code = _get_inode_from_path( pathname , &inode );

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_remove(): P[%d,%d,%d] found inode %x for <%s> / code = %d\n",
        x , y , p , (unsigned int)inode , pathname , code );
#endif

    if ( (code == 1) || (code == 2) )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_remove(): file <%s> not found\n", 
                pathname );
        return GIET_FAT32_FILE_NOT_FOUND;
    }
    else if ( code == 3 )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_remove(): name too long in <%s>\n",
                pathname );
        return GIET_FAT32_NAME_TOO_LONG;
    }

    // check inode type
    if ( (inode->is_dir != 0) && (should_be_dir == 0) ) 
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_remove(): <%s> is a directory\n",
                pathname );
        return GIET_FAT32_IS_DIRECTORY;
    }
    if ( (inode->is_dir == 0) && (should_be_dir != 0) )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_remove(): <%s> is not a directory\n", 
                pathname );
        return GIET_FAT32_NOT_A_DIRECTORY;
    }

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_remove(): P[%d,%d,%d] checked inode type for <%s>\n",
        x , y , p , pathname );
#endif
    
    // check references count for a file
    if ( (inode->is_dir == 0) && (inode->count != 0) )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_remove(): file <%s> still referenced\n",
                pathname );
        return GIET_FAT32_IS_OPEN;
    }

    //  check empty for a directory
    if ( inode->is_dir )
    {
        unsigned int entries;
        if ( _get_nb_entries( inode , &entries ) )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_remove(): cannot scan directory <%s>\n", 
                    pathname );
            return GIET_FAT32_IO_ERROR;
        }
        else if ( entries > 2 )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_remove(): directory <%s> not empty\n", 
                    pathname );
            return GIET_FAT32_DIRECTORY_NOT_EMPTY;
        }
    }

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_remove(): P[%d,%d,%d] checked remove condition OK for <%s>\n",
        x , y , p , pathname );
#endif
    
    // remove the file or directory from the file system
    if ( _remove_node_from_fs( inode ) )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_remove(): cannot remove <%s> from FS\n",
                pathname );
        return GIET_FAT32_IO_ERROR;
    }

    // release lock and return success
    _spin_lock_release( &_fat.fat_lock );

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_remove(): P[%d,%d,%d] removed  <%s> from FS\n",
        x, y, p, pathname );
#endif
    
    return GIET_FAT32_OK;
        
}  // end _fat_remove()





/////////////////////////////////////////////////////////////////////////////////
// This function implements the giet_fat_rename() system call.
// It moves an existing file or directory from one node (defined by "old_path"
// argument) to another node (defined by "new_path" argument) in the FS tree. 
// The type (file/directory) and content are not modified.
// If the new_path file/dir exist, it is removed from the file system, but only  
// if the remove condition is respected (directory empty / file not referenced).
// The removed entry is only removed after the new entry is actually created.
/////////////////////////////////////////////////////////////////////////////////
// Returns GIET_FAT32_OK on success.
// Returns a negative value on error:
//   GIET_FAT32_NOT_INITIALIZED,
//   GIET_FAT32_FILE_NOT_FOUND,
//   GIET_FAT32_MOVE_INTO_SUBDIR,
//   GIET_FAT32_IO_ERROR,
//   GIET_FAT32_DIRECTORY_NOT_EMPTY,
//   GIET_FAT32_IS_OPEN
/////////////////////////////////////////////////////////////////////////////////
int _fat_rename( char*  old_path,
                 char*  new_path )
{
    fat_inode_t*  inode;        // anonymous inode pointer
    fat_inode_t*  old;          // inode identified by old_path      => to be deleted
    fat_inode_t*  new;          // inode identified by new_path      => to be created
    fat_inode_t*  old_parent;   // parent inode  in old_path         => to be modified
    fat_inode_t*  new_parent;   // parent inode  in new_path         => to be modified
    fat_inode_t*  to_remove;    // previouly identified by new_path  => to be removed
    unsigned int  code;

#if GIET_DEBUG_FAT
unsigned int procid  = _get_procid();
unsigned int x       = procid >> (Y_WIDTH + P_WIDTH);
unsigned int y       = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int p       = procid & ((1<<P_WIDTH)-1);
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_rename(): P[%d,%d,%d] enters to move <%s> to <%s>\n",
        x , y , p , old_path , new_path );
#endif

    // checking FAT initialized
    if( _fat.initialized != FAT_INITIALIZED )
    {
        _printf("\n[FAT ERROR] _fat_rename(): FAT not initialized\n");
        return GIET_FAT32_NOT_INITIALIZED;
    }

    // take the lock 
    _spin_lock_acquire( &_fat.fat_lock );

    // get "old" and "old_parent" inode pointers
    if ( _get_inode_from_path( old_path , &inode ) )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_rename(): <%s> not found\n", old_path );
        return GIET_FAT32_FILE_NOT_FOUND;
    }
    else
    {
        old        = inode;
        old_parent = inode->parent;
    }

    // get "to_removed" and "new_parent" inode pointers
    code = _get_inode_from_path( new_path , &inode );

    if ( code == 0 )       // new_path inode already exist 
    {
        if ( inode == old )  // the file will replace itself, do nothing
        {
            _spin_lock_release( &_fat.fat_lock );
            return GIET_FAT32_OK;
        }

        to_remove        = inode;
        new_parent       = inode->parent;
    }
    else if ( code == 1 )  // to_remove does not exist but parent exist
    {
        to_remove        = NULL;
        new_parent       = inode;
    }
    else                   // parent directory in new_path not found
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_rename(): <%s> not found\n", new_path );
        return GIET_FAT32_FILE_NOT_FOUND;
    }

    // check for move into own subdirectory
    if ( _is_ancestor( old, new_parent ) )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_rename(): can't move %s into its own subdirectory\n", old_path );
        return GIET_FAT32_MOVE_INTO_SUBDIR;
    }

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
{
if ( to_remove )
_printf("\n[DEBUG FAT] _fat_rename(): old_parent = %s / old = %s / new_parent = %s "
        "/ to_remove = %s\n",
        old_parent->name , old->name , new_parent->name , to_remove->name );
else
_printf("\n[DEBUG FAT] _fat_rename(): old_parent = %s / old = %s / new_parent = %s "
        "/ no remove\n", 
        old_parent->name , old->name , new_parent->name );
}
#endif

    // check remove condition for "to_remove" inode
    if ( to_remove )
    {
        if ( to_remove->is_dir )   // it's a directory
        {
            unsigned int entries;
            if ( _get_nb_entries( to_remove , &entries ) )
            {
                _spin_lock_release( &_fat.fat_lock );
                _printf("\n[FAT ERROR] _fat_rename(): cannot scan directory <%s>\n", 
                        to_remove->name );
                return GIET_FAT32_IO_ERROR;
            }
            else if ( entries > 2 )
            {
                _spin_lock_release( &_fat.fat_lock );
                _printf("\n[FAT ERROR] _fat_rename(): directory <%s> not empty\n", 
                        to_remove->name );
                return GIET_FAT32_DIRECTORY_NOT_EMPTY;
            }
        }
        else                       // it's a file
        {
            if ( to_remove->count ) 
            {
                _spin_lock_release( &_fat.fat_lock );
                _printf("\n[FAT ERROR] _fat_rename(): file <%s> still referenced\n", 
                        to_remove->name );
                return GIET_FAT32_IS_OPEN;
            }
        }
    }

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[FAT DEBUG] _fat_rename(): P[%d,%d,%d] checked remove condition OK\n",
        x , y , p );
#endif

    // get new last name / error checking already done by _get_inode_from_path()
    char  new_name[32];
    _get_last_name( new_path , new_name );

    // allocate "new" inode
    new = _allocate_one_inode( new_name,
                               old->is_dir,
                               old->cluster,
                               old->size,
                               0,              // count
                               0,              // dentry
                               0 );            // no cache_allocate
 
    // give the "old" File-Cache to the "new inode
    new->levels = old->levels;
    new->cache  = old->cache;

    // add "new" to "new_parent" directory File-Cache
    if ( _add_dir_entry( new , new_parent ) )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_rename(): cannot add <%s> into <%s>\n",
                new->name , new_parent->name );
        return GIET_FAT32_IO_ERROR;
    }

    // add "new" to "new_parent" directory in Inode-Tree
    _add_inode_in_tree( new , new_parent );
    
    // updates "new_parent" directory on device
    if ( _update_device_from_cache( new_parent->levels,
                                    new_parent->cache,
                                    new_parent->name ) )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_rename(): cannot update <%s> on device\n",
                    new_parent->name );
        return GIET_FAT32_IO_ERROR;
    }

    // remove "old" from "old_parent" File-Cache 
    if ( _remove_dir_entry( old ) )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_rename(): cannot remove <%s> from <%s>\n",
                old->name , old_parent->name );
        return GIET_FAT32_IO_ERROR;
    }
 
    // remove "old" inode from Inode-Tree
    _remove_inode_from_tree( old );

    // release "old" inode
    _free( old );

    // updates "old_parent" directory on device
    if ( _update_device_from_cache( old_parent->levels,
                                    old_parent->cache,
                                    old_parent->name ) )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_rename(): cannot update <%s> on device\n",
                    old_parent->name );
        return GIET_FAT32_IO_ERROR;
    }

    // remove "to_remove" from File System (if required)
    if ( to_remove )
    {
        if ( _remove_node_from_fs( to_remove ) )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_rename(): cannot remove <%s> from FS\n",
                    to_remove->name );
            return GIET_FAT32_IO_ERROR;
        }
    }

    // release lock
    _spin_lock_release( &_fat.fat_lock );

    return GIET_FAT32_OK;
}  // end _fat_rename()




/////////////////////////////////////////////////////////////////////////////////
// The following function implements the giet_fat_mkdir() system call.
// It creates in file system the directory specified by the "pathname" argument.
// The Inode-Tree is updated.
// One cluster is allocated to the new directory.
// The associated File-Cache is created.
// The FAT region on block device is updated.
// The DATA region on block device is updated.
/////////////////////////////////////////////////////////////////////////////////
// Returns GIET_FAT32_OK on success.
// Returns a negative value on error:
//   GIET_FAT32_NOT_INITIALIZED,
//   GIET_FAT32_FILE_NOT_FOUND,
//   GIET_FAT32_NAME_TOO_LONG,
//   GIET_FAT32_FILE_EXISTS,
//   GIET_FAT32_NO_FREE_SPACE,
//   GIET_FAT32_IO_ERROR
/////////////////////////////////////////////////////////////////////////////////
int _fat_mkdir( char* pathname )
{
    fat_inode_t*         inode;            // anonymous inode pointer
    fat_inode_t*         child;            // searched directory inode pointer
    fat_inode_t*         parent;           // parent directory inode pointer

#if GIET_DEBUG_FAT
unsigned int procid  = _get_procid();
unsigned int x       = procid >> (Y_WIDTH + P_WIDTH);
unsigned int y       = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int p       = procid & ((1<<P_WIDTH)-1);
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_mkdir(): P[%d,%d,%d] enters for path <%s>\n",
        x, y, p, pathname );
#endif

    // checking FAT initialized
    if( _fat.initialized != FAT_INITIALIZED )
    {
        _printf("\n[FAT ERROR] _fat_mkdir(): FAT not initialized\n");
        return GIET_FAT32_NOT_INITIALIZED;
    }

    // takes the lock
    _spin_lock_acquire( &_fat.fat_lock );
    
    // get inode
    unsigned int code = _get_inode_from_path( pathname , &inode );

    if ( code == 2 )  
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_mkdir(): path to parent not found"
                " for directory <%s>\n", pathname );
        return GIET_FAT32_FILE_NOT_FOUND;
    }
    else if ( code == 3 )  
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_mkdir(): one name in path too long"
                " for directory  <%s>\n", pathname );
        return GIET_FAT32_NAME_TOO_LONG;
    }
    else if ( code == 0 )
    {
        _spin_lock_release( &_fat.fat_lock );
        _printf("\n[FAT ERROR] _fat_mkdir(): directory <%s> already exist\n",
                pathname );
        return GIET_FAT32_FILE_EXISTS;
    }
    else if ( code == 1 )   // directory not found => create 
    {
        parent = inode;

#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_mkdir(): P[%d,%d,%d] create new directory <%s>\n",
        x , y , p , pathname );
#endif

        // get directory name / error check already done by _get_inode_from_path()
        char name[32];       
        _get_last_name( pathname , name );

        // allocate one cluster from FAT for the new directory
        unsigned int cluster;
        if ( _allocate_one_cluster( &cluster ) )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_mkdir(): no free cluster"
                    " for directory <%s>\n" , pathname );
            return GIET_FAT32_NO_FREE_SPACE;
        }

        // allocate a new inode and an empty Cache-File 
        child = _allocate_one_inode( name,
                                     1,           // it's a directory
                                     cluster,  
                                     0,           // size not defined
                                     0,           // count
                                     0,           // dentry set by _add_dir_entry()
                                     1 );         // cache_allocate

        // introduce inode in Inode-Tree
        _add_inode_in_tree( child , parent );
 
        // allocate and initialise one 4 Kbytes buffer and associated descriptor
        _allocate_one_buffer( child,
                              0,            // cluster_id,
                              cluster );

        _add_special_directories( child, 
                                  parent );

        // add an entry in the parent directory Cache_file
        if ( _add_dir_entry( child , parent ) )
        { 
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_mkdir(): cannot update parent directory"
                    " for directory <%s>\n" , pathname );
            return GIET_FAT32_IO_ERROR;
        } 

        // update DATA region on block device for parent directory
        if ( _update_device_from_cache( parent->levels,
                                        parent->cache,
                                        parent->name ) )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_mkdir(): cannot update DATA region "
                    " for parent of directory <%s>\n", pathname );
            return GIET_FAT32_IO_ERROR;
        }

        // update FAT region on block device
        if ( _update_device_from_cache( _fat.fat_cache_levels,
                                        _fat.fat_cache_root,
                                        "FAT" ) )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_mkdir(): cannot update FAT region"
                    " for directory <%s>\n", pathname );
            return GIET_FAT32_IO_ERROR;
        }

        // update FS_INFO sector
        if ( _update_fs_info() )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_mkdir(): cannot update FS-INFO"
                    " for directory <%s>\n", pathname );
            return GIET_FAT32_IO_ERROR;
        }

        // update DATA region on block device for the new directory
        if ( _update_device_from_cache( child->levels,   
                                        child->cache,
                                        child->name ) )
        {
            _spin_lock_release( &_fat.fat_lock );
            _printf("\n[FAT ERROR] _fat_mkdir(): cannot update DATA region"
                    " for directory <%s>\n", pathname );
            return GIET_FAT32_IO_ERROR;
        }
    }  // end create directory

    // release lock
    _spin_lock_release( &_fat.fat_lock );

    return GIET_FAT32_OK;
}  // end _fat_mkdir()




///////////////////////////////////////////////////////////////////////////////
// This function implements the giet_fat_opendir() system call.
// The semantic is similar to the UNIX opendir() function.
// If the specified directory does not exist, an error is returned.
// It allocates a file descriptor to the calling task, for the directory
// identified by "pathname". If several tasks try to open the same directory,
// each task obtains a private file descriptor.
// A node name cannot be larger than 31 characters.
///////////////////////////////////////////////////////////////////////////////
// Returns a file descriptor for the directory index on success
// Returns a negative value on error:
//   GIET_FAT32_NOT_INITIALIZED,
//   GIET_FAT32_NAME_TOO_LONG,
//   GIET_FAT32_FILE_NOT_FOUND,
//   GIET_FAT32_TOO_MANY_OPEN_FILES,
//   GIET_FAT32_NOT_A_DIRECTORY
///////////////////////////////////////////////////////////////////////////////
extern int _fat_opendir( char* pathname )
{
    int fd_id = _fat_open( pathname, O_RDONLY );

    if ( fd_id < 0 )
        return fd_id;

    if ( !_fat.fd[fd_id].inode->is_dir )
    {
        _printf("\n[FAT ERROR] _fat_opendir(): <%s> is not a directory\n",
                pathname );
        return GIET_FAT32_NOT_A_DIRECTORY;
    }

    return fd_id;
}




/////////////////////////////////////////////////////////////////////////////////
// This function implements the "giet_fat_closedir()" system call.
// Same behavior as _fat_close(), no check for directory.
/////////////////////////////////////////////////////////////////////////////////
// Returns GIET_FAT32_OK on success.
// Returns a negative value on error:
//   GIET_FAT32_NOT_INITIALIZED,
//   GIET_FAT32_INVALID_FD,
//   GIET_FAT32_NOT_OPEN,
//   GIET_FAT32_IO_ERROR
/////////////////////////////////////////////////////////////////////////////////
extern int _fat_closedir( unsigned int fd_id )
{
    return _fat_close( fd_id );
}




/////////////////////////////////////////////////////////////////////////////////
// This function implements the "giet_fat_readdir()" system call.
// It reads one directory entry from the file descriptor opened by
// "giet_fat_opendir()" and writes its info to the "entry" argument.
// This includes the cluster, size, is_dir and name info for each entry.
/////////////////////////////////////////////////////////////////////////////////
// Returns GIET_FAT32_OK on success.
// Returns a negative value on error:
//   GIET_FAT32_NOT_INITIALIZED,
//   GIET_FAT32_INVALID_FD,
//   GIET_FAT32_NOT_OPEN,
//   GIET_FAT32_NOT_A_DIRECTORY,
//   GIET_FAT32_IO_ERROR,
//   GIET_FAT32_NO_MORE_ENTRIES
/////////////////////////////////////////////////////////////////////////////////
extern int _fat_readdir( unsigned int  fd_id,
                         fat_dirent_t* entry )
{
    unsigned int  lfn   = 0;            // lfn entries count
    unsigned int  attr;                 // ATTR field value
    unsigned int  ord;                  // ORD field value
    char          lfn1[16];             // temporary buffer for string in LFN1
    char          lfn2[16];             // temporary buffer for string in LFN2
    char          lfn3[16];             // temporary buffer for string in LFN3
    unsigned char buf[DIR_ENTRY_SIZE];  // raw entry buffer
    fat_file_info_t info;

    // check for directory
    int ret = _fat_file_info( fd_id, &info );
    if (ret < 0)
    {
        return ret;
    }
    else if ( !info.is_dir )
    {
        _printf("\n[FAT ERROR] _fat_readdir(): not a directory\n" );
        return GIET_FAT32_NOT_A_DIRECTORY;
    }

    while ( 1 )
    {
        if ( _fat_read( fd_id, &buf, sizeof(buf) ) != sizeof(buf) )
        {
            _printf("\n[FAT ERROR] _fat_readdir(): can't read entry\n" );
            return GIET_FAT32_IO_ERROR;
        }

        attr = _read_entry( DIR_ATTR, buf, 0 );
        ord  = _read_entry( LDIR_ORD, buf, 0 );

        if (ord == NO_MORE_ENTRY)               // no more entry in directory => stop
        {
            // seek back to this entry
            _spin_lock_acquire( &_fat.fat_lock );
            _fat.fd[fd_id].seek -= DIR_ENTRY_SIZE;
            _spin_lock_release( &_fat.fat_lock );

            return GIET_FAT32_NO_MORE_ENTRIES;
        }
        else if ( ord == FREE_ENTRY )           // free entry => skip
        {
            continue;
        }
        else if ( attr == ATTR_LONG_NAME_MASK ) // LFN entry => get partial names
        {
            unsigned int seq = ord & 0x3;
            lfn = (seq > lfn) ? seq : lfn;
            if      ( seq == 1 ) _get_name_from_long( buf, lfn1 );
            else if ( seq == 2 ) _get_name_from_long( buf, lfn2 );
            else if ( seq == 3 ) _get_name_from_long( buf, lfn3 );
            continue;
        }
        else                                    // NORMAL entry => stop
        {
            break;
        }
    }

    // TODO handle is_vid
    entry->cluster = (_read_entry( DIR_FST_CLUS_HI, buf, 1 ) << 16) |
                     (_read_entry( DIR_FST_CLUS_LO, buf, 1 )      ) ;
    entry->size    = (_read_entry( DIR_FILE_SIZE  , buf, 1 )      ) ;
    entry->is_dir  = ((attr & ATTR_DIRECTORY) == ATTR_DIRECTORY);

    if      ( lfn == 0 )
    {
        _get_name_from_short( buf, entry->name );
    }
    else if ( lfn == 1 )
    {
        _strcpy( entry->name     , lfn1 );
    }
    else if ( lfn == 2 )
    {
        _strcpy( entry->name     , lfn1 );
        _strcpy( entry->name + 13, lfn2 );
    }
    else if ( lfn == 3 )
    {
        _strcpy( entry->name     , lfn1 );
        _strcpy( entry->name + 13, lfn2 );
        _strcpy( entry->name + 26, lfn3 );
    }

    return GIET_FAT32_OK;
}




///////////////////////////////////////////////////////////////////////////////
// This function loads a file identified by the "pathname" argument into the
// memory buffer defined by the "buffer_vbase" and "buffer_size" arguments.
// It is intended to be called by the boot-loader, as it does not use the
// dynamically allocated FAT structures (Inode-Tree, Fat_Cache or File-Cache,
// File-Descriptor-Array).
// It uses only the 512 bytes buffer defined in the FAT descriptor. 
///////////////////////////////////////////////////////////////////////////////
// Returns GIET_FAT32_OK on success.
// Returns negative value on error:
//   GIET_FAT32_NOT_INITIALIZED
//   GIET_FAT32_FILE_NOT_FOUND
//   GIET_FAT32_BUFFER_TOO_SMALL
//   GIET_FAT32_IO_ERROR
///////////////////////////////////////////////////////////////////////////////
int _fat_load_no_cache( char*        pathname,
                        unsigned int buffer_vbase,  
                        unsigned int buffer_size ) 
{
    // checking FAT initialized
    if( _fat.initialized != FAT_INITIALIZED )
    {
        _printf("\n[FAT ERROR] _fat_load_no_cache(): FAT not initialized\n");
        return GIET_FAT32_NOT_INITIALIZED;
    }

    unsigned int  file_size;
    unsigned int  cluster;

#if GIET_DEBUG_FAT
unsigned int procid  = _get_procid();
unsigned int x       = procid >> (Y_WIDTH + P_WIDTH);
unsigned int y       = (procid >> P_WIDTH) & ((1<<Y_WIDTH)-1);
unsigned int p       = procid & ((1<<P_WIDTH)-1);
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_load_no_cache(): P[%d,%d,%d] enters for file <%s>\n",
        x , y , p , pathname );
#endif

    // get file size, and cluster index in FAT
    if ( _file_info_no_cache( pathname,
                              &cluster,
                              &file_size ) )
    {
        _printf("\n[FAT ERROR] _fat_load_no_cache(): file <%s> not found\n",
        pathname );
        return GIET_FAT32_FILE_NOT_FOUND;
    }

    // check buffer size
    if ( file_size > buffer_size )
    {
        _printf("\n[FAT ERROR] _fat_load_no_cache(): buffer too small : "
                "file_size = %x / buffer_size = %x", file_size , buffer_size );
        return GIET_FAT32_BUFFER_TOO_SMALL;
    }

    // compute total number of clusters to read
    unsigned int nb_clusters = file_size >> 12;
    if ( file_size & 0xFFF ) nb_clusters++;

    // initialise buffer address
    unsigned int dst = buffer_vbase;

    // loop on the clusters containing the file
    while ( nb_clusters > 0 )
    {
        unsigned int lba = _cluster_to_lba( cluster );

        if( _fat_ioc_access( 0,         // no descheduling
                             1,         // read
                             lba, 
                             dst, 
                             8 ) )      // 8 blocks
        {
            _printf("\n[FAT ERROR] _fat_load_no_cache(): cannot load lba %x", lba );
            return GIET_FAT32_IO_ERROR;
        }
         

        // compute next cluster index
        unsigned int next;
        if ( _next_cluster_no_cache( cluster , &next ) )
        {
            _printf("\n[FAT ERROR] _fat_load_no_cache(): cannot get next cluster "
                    " for cluster = %x\n", cluster );
            return GIET_FAT32_IO_ERROR;
        }
        
        // update variables for next iteration
        nb_clusters = nb_clusters - 1;
        dst         = dst + 4096;
        cluster     = next;
    }
         
#if GIET_DEBUG_FAT
if ( _get_proctime() > GIET_DEBUG_FAT )
_printf("\n[DEBUG FAT] _fat_load_no_cache(): P[%d,%d,%d] loaded <%s> at vaddr = %x"
        " / size = %x\n", x , y , p , pathname , buffer_vbase , file_size );
#endif

    return GIET_FAT32_OK;
}  // end _fat_load_no_cache() 



// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

