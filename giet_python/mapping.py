#!/usr/bin/env python

import sys

###################################################################################
#   file   : giet_mapping.py
#   date   : april 2014
#   author : Alain Greiner
###################################################################################
#  This file contains the classes required to define a mapping for the GIET_VM.
# - A 'Mapping' contains a set of 'Cluster'   (hardware architecture)
#                        a set of 'Vseg'      (kernel glogals virtual segments)
#                        a set of 'Vspace'    (one or several user applications)
# - A 'Cluster' contains a set of 'Pseg'      (physical segments in cluster)
#                        a set of 'Proc'      (processors in cluster)
#                        a set of 'Periph'    (peripherals in cluster)
# - A 'Vspace' contains  a set of 'Vseg'      (user virtual segments)
#                        a set of 'Task'      (user parallel tasks)
# - A 'Periph' contains  a set of 'Irq'       (only for XCU and PIC types )
###################################################################################
# Implementation Note
# The objects used to describe a mapping are distributed in the PYTHON structure:
# For example the psegs set is split in several subsets (one subset per cluster),
# or the tasks set is split in several subsets (one subset per vspace), etc...
# In the C binary data structure used by the giet_vm, all objects of same type
# are stored in a linear array (one single array for all psegs for example).
# For all objects, we compute and store in the PYTHON object  a "global index"
# corresponding to the index in this global array, and this index can be used as
# a pseudo-pointer to identify a specific object of a given type.
###################################################################################

###################################################################################
# Various constants 
###################################################################################

PADDR_WIDTH       = 40            # number of bits for physical address
X_WIDTH           = 4             # number of bits encoding x coordinate
Y_WIDTH           = 4             # number of bits encoding y coordinate
P_WIDTH           = 4             # number of bits encoding local proc_id
VPN_ANTI_MASK     = 0x00000FFF    # mask vaddr to get offset in small page 
BPN_MASK          = 0xFFE00000    # mask vaddr to get the BPN in big page
PERI_INCREMENT    = 0x10000       # virtual address increment for replicated vsegs
RESET_ADDRESS     = 0xBFC00000    # Processor wired boot_address
MAPPING_SIGNATURE = 0xDACE2014    # Magic number indicating a valid C BLOB

###################################################################################
# These lists must be consistent with values defined in
# mapping_info.h / xml_driver.c /xml_parser.c
###################################################################################
PERIPHTYPES =    [
                  'CMA',
                  'DMA',
                  'FBF',
                  'IOB',
                  'IOC',
                  'MMC',
                  'MWR',
                  'NIC',
                  'ROM',
                  'SIM',
                  'TIM',
                  'TTY',
                  'XCU',
                  'PIC',
                  'DROM',
                 ]

IOCSUBTYPES =    [
                  'BDV',
                  'HBA',
                  'SDC',
                  'SPI',
                 ]

MWRSUBTYPES =    [
                  'GCD',
                  'DCT',
                  'CPY',
                 ]
   
###################################################################################
# These lists must be consistent with values defined in 
# irq_handler.c / irq_handler.h / xml_driver.c / xml_parser.c
###################################################################################
IRQTYPES =       [
                  'HWI',
                  'WTI',
                  'PTI',
                 ]

ISRTYPES =       [
                  'ISR_DEFAULT',
                  'ISR_TICK',
                  'ISR_TTY_RX',
                  'ISR_TTY_TX',
                  'ISR_BDV',
                  'ISR_TIMER',
                  'ISR_WAKUP',
                  'ISR_NIC_RX',
                  'ISR_NIC_TX',
                  'ISR_CMA',
                  'ISR_MMC',
                  'ISR_DMA',
                  'ISR_SDC',
                  'ISR_MWR',
                  'ISR_HBA',
                  'ISR_SPI',
                 ]

VSEGTYPES =      [
                  'ELF',
                  'BLOB',
                  'PTAB',
                  'PERI',
                  'BUFFER',
                  'SCHED',      
                  'HEAP',
                 ]

VSEGMODES =      [
                  '____',
                  '___U',
                  '__W_',
                  '__WU',
                  '_X__',
                  '_X_U',
                  '_XW_',
                  '_XWU',
                  'C___',
                  'C__U',
                  'C_W_',
                  'C_WU',
                  'CX__',
                  'CX_U',
                  'CXW_',
                  'CXWU',
                 ]

PSEGTYPES =      [
                  'RAM',
                  'PERI',
                 ]

###################################################################################
class Mapping( object ):
###################################################################################
    def __init__( self,
                  name,                            # mapping name 
                  x_size,                          # number of clusters in a row
                  y_size,                          # number of clusters in a column
                  nprocs,                          # max number of processors per cluster
                  x_width        = X_WIDTH,        # number of bits encoding x coordinate
                  y_width        = Y_WIDTH,        # number of bits encoding y coordinate
                  p_width        = P_WIDTH,        # number of bits encoding lpid
                  paddr_width    = PADDR_WIDTH,    # number of bits for physical address
                  coherence      = 1,              # hardware cache coherence if non-zero
                  irq_per_proc   = 1,              # number or IRQs from XCU to processor 
                  use_ramdisk    = False,          # use ramdisk when true
                  x_io           = 0,              # cluster_io x coordinate
                  y_io           = 0,              # cluster_io y coordinate
                  peri_increment = PERI_INCREMENT, # address increment for globals
                  reset_address  = RESET_ADDRESS,  # Processor wired boot_address
                  ram_base       = 0,              # RAM physical base in cluster[0,0]
                  ram_size       = 0 ):            # RAM size per cluster (bytes)

        assert ( x_size <= (1<<X_WIDTH) )
        assert ( y_size <= (1<<Y_WIDTH) )
        assert ( nprocs <= (1<<P_WIDTH) )

        self.signature      = MAPPING_SIGNATURE
        self.name           = name
        self.name           = name
        self.paddr_width    = paddr_width
        self.coherence      = coherence
        self.x_size         = x_size
        self.y_size         = y_size
        self.nprocs         = nprocs
        self.x_width        = x_width
        self.y_width        = y_width
        self.p_width        = p_width
        self.irq_per_proc   = irq_per_proc
        self.use_ramdisk    = use_ramdisk
        self.x_io           = x_io
        self.y_io           = y_io
        self.peri_increment = peri_increment
        self.reset_address  = reset_address
        self.ram_base       = ram_base
        self.ram_size       = ram_size

        self.total_vspaces  = 0
        self.total_globals  = 0
        self.total_psegs    = 0
        self.total_vsegs    = 0
        self.total_tasks    = 0
        self.total_procs    = 0
        self.total_irqs     = 0
        self.total_periphs  = 0

        self.clusters       = []
        self.globs          = []
        self.vspaces        = []

        for x in xrange( self.x_size ):
            for y in xrange( self.y_size ):
                cluster = Cluster( x , y )
                cluster.index = (x * self.y_size) + y
                self.clusters.append( cluster )

        return

    ##########################    add a ram pseg in a cluster
    def addRam( self,
                name,                  # pseg name
                base,                  # pseg base address
                size ):                # pseg length (bytes)

        # computes cluster index and coordinates from the base address
        paddr_lsb_width = self.paddr_width - self.x_width - self.y_width
        cluster_xy = base >> paddr_lsb_width
        x          = cluster_xy >> (self.y_width);
        y          = cluster_xy & ((1 << self.y_width) - 1)
        cluster_id = (x * self.y_size) + y

        assert (base & VPN_ANTI_MASK) == 0

        assert (x < self.x_size) and (y < self.y_size)

        assert ( (base & ((1<<paddr_lsb_width)-1)) == self.ram_base )

        assert ( size == self.ram_size )

        # add one pseg in the mapping
        pseg = Pseg( name, base, size, x, y, 'RAM' )
        self.clusters[cluster_id].psegs.append( pseg )
        pseg.index = self.total_psegs
        self.total_psegs += 1

        return pseg

    ##########################   add a peripheral and the associated pseg in a cluster
    def addPeriph( self,
                   name,               # associated pseg name
                   base,               # associated pseg base address
                   size,               # associated pseg length (bytes)
                   ptype,              # peripheral type
                   subtype  = 'NONE',  # peripheral subtype
                   channels = 1,       # number of channels
                   arg0     = 0,       # optional argument (semantic depends on ptype)
                   arg1     = 0,       # optional argument (semantic depends on ptype)
                   arg2     = 0,       # optional argument (semantic depends on ptype)
                   arg3     = 0 ):     # optional argument (semantic depends on ptype)

        # computes cluster index and coordinates from the base address
        cluster_xy = base >> (self.paddr_width - self.x_width - self.y_width)
        x          = cluster_xy >> (self.y_width);
        y          = cluster_xy & ((1 << self.y_width) - 1)
        cluster_id = (x * self.y_size) + y

        assert (x < self.x_size) and (y < self.y_size)

        assert (base & VPN_ANTI_MASK) == 0

        assert ptype in PERIPHTYPES

        if (ptype == 'IOC'): assert subtype in IOCSUBTYPES
        if (ptype == 'MWR'): assert subtype in MWRSUBTYPES

        # add one pseg into mapping
        pseg = Pseg( name, base, size, x, y, 'PERI' )
        self.clusters[cluster_id].psegs.append( pseg )
        pseg.index = self.total_psegs
        self.total_psegs += 1

        # add one periph into mapping
        periph = Periph( pseg, ptype, subtype, channels, arg0, arg1, arg2, arg3 )
        self.clusters[cluster_id].periphs.append( periph )
        periph.index = self.total_periphs
        self.total_periphs += 1

        return periph

    ################################   add an IRQ in a peripheral
    def addIrq( self,
                periph,                # peripheral containing IRQ (PIC or XCU)
                index,                 # peripheral input port index
                src,                   # interrupt source peripheral
                isrtype,               # ISR type
                channel = 0 ):         # channel for multi-channels ISR

        assert isrtype in ISRTYPES

        assert index < 32

        # add one irq into mapping
        irq = Irq( 'HWI', index , isrtype, channel )
        periph.irqs.append( irq )
        irq.index = self.total_irqs
        self.total_irqs += 1

        # pointer from the source to the interrupt controller peripheral
        if src.irq_ctrl == None: src.irq_ctrl = periph
        if src.irq_ctrl != periph:
            print '[genmap error] in addIrq():'
            print '    two different interrupt controller for the same peripheral'
            sys.exit(1)

        return irq

    ##########################    add a processor in a cluster
    def addProc( self,
                 x,                    # cluster x coordinate
                 y,                    # cluster y coordinate
                 lpid ):               # processor local index

        assert (x < self.x_size) and (y < self.y_size)

        cluster_id = (x * self.y_size) + y

        # add one proc into mapping
        proc = Processor( x, y, lpid )
        self.clusters[cluster_id].procs.append( proc )
        proc.index = self.total_procs
        self.total_procs += 1

        return proc

    ############################    add one global vseg into mapping 
    def addGlobal( self, 
                   name,               # vseg name
                   vbase,              # virtual base address
                   length,             # vseg length (bytes)
                   mode,               # CXWU flags
                   vtype,              # vseg type
                   x,                  # destination x coordinate
                   y,                  # destination y coordinate
                   pseg,               # destination pseg name
                   identity = False,   # identity mapping required if true
                   local    = False,   # only mapped in local PTAB if true
                   big      = False,   # to be mapped in a big physical page
                   binpath  = '' ):    # pathname for binary code if required

        # two global vsegs must not overlap if they have different names
        for prev in self.globs:
            if ( ((prev.vbase + prev.length) > vbase ) and 
                 ((vbase + length) > prev.vbase) and
                 (prev.name[0:15] != name[0:15]) ):
                print '[genmap error] in addGlobal()'
                print '    global vseg %s overlap %s' % (name, prev.name)
                print '    %s : base = %x / size = %x' %(name, vbase, length)
                print '    %s : base = %x / size = %x' %(prev.name, prev.vbase, prev.length)
                sys.exit(1)

        # add one vseg into mapping
        vseg = Vseg( name, vbase, length, mode, vtype, x, y, pseg, 
                     identity = identity, local = local, big = big, binpath = binpath )

        self.globs.append( vseg )
        self.total_globals += 1
        vseg.index = self.total_vsegs
        self.total_vsegs += 1

        return

    ################################    add a vspace into mapping
    def addVspace( self,
                   name,                # vspace name
                   startname,           # name of vseg containing start_vector
                   active = False ):    # default value is not active at boot

        # add one vspace into mapping
        vspace = Vspace( name, startname, active )
        self.vspaces.append( vspace )
        vspace.index = self.total_vspaces
        self.total_vspaces += 1

        return vspace

    #################################   add a private vseg in a vspace
    def addVseg( self,
                 vspace,                # vspace containing the vseg
                 name,                  # vseg name
                 vbase,                 # virtual base address
                 length,                # vseg length (bytes)
                 mode,                  # CXWU flags
                 vtype,                 # vseg type
                 x,                     # destination x coordinate
                 y,                     # destination y coordinate
                 pseg,                  # destination pseg name
                 local    = False,      # only mapped in local PTAB if true
                 big      = False,      # to be mapped in a big physical page
                 binpath  = '' ):       # pathname for binary code

        assert mode in VSEGMODES

        assert vtype in VSEGTYPES

        assert (x < self.x_size) and (y < self.y_size)

        # add one vseg into mapping
        vseg = Vseg( name, vbase, length, mode, vtype, x, y, pseg, 
                     identity = False, local = local, big = big, binpath = binpath )
        vspace.vsegs.append( vseg )
        vseg.index = self.total_vsegs
        self.total_vsegs += 1

        return vseg

    ################################    add a task in a vspace
    def addTask( self,
                 vspace,                # vspace containing task
                 name,                  # task name
                 trdid,                 # task index in vspace
                 x,                     # destination x coordinate
                 y,                     # destination y coordinate
                 lpid,                  # destination processor local index
                 stackname,             # name of vseg containing stack
                 heapname,              # name of vseg containing heap
                 startid ):             # index in start_vector

        assert (x < self.x_size) and (y < self.y_size)
        assert lpid < self.nprocs

        # add one task into mapping
        task = Task( name, trdid, x, y, lpid, stackname, heapname, startid )
        vspace.tasks.append( task )
        task.index = self.total_tasks
        self.total_tasks += 1

        return task

    #################################
    def str2bytes( self, nbytes, s ):    # string => nbytes_packed byte array

        byte_stream = bytearray()
        length = len( s )
        if length < (nbytes - 1):
            for b in s:
                byte_stream.append( b )
            for x in xrange(nbytes-length):
                byte_stream.append( '\0' )
        else:
            print '[genmap error] in str2bytes()'
            print '    string %s too long' % s
            sys.exit(1)

        return byte_stream

    ###################################
    def int2bytes( self, nbytes, val ):    # integer => nbytes litle endian byte array

        byte_stream = bytearray()
        for n in xrange( nbytes ):
            byte_stream.append( (val >> (n<<3)) & 0xFF )

        return byte_stream

    ################
    def xml( self ):    # compute string for map.xml file generation

        s = '<?xml version="1.0"?>\n\n'
        s += '<mapping_info signature    = "0x%x"\n' % (self.signature)
        s += '              name         = "%s"\n'   % (self.name)
        s += '              x_size       = "%d"\n'   % (self.x_size)
        s += '              y_size       = "%d"\n'   % (self.y_size)
        s += '              x_width      = "%d"\n'   % (self.x_width)
        s += '              y_width      = "%d"\n'   % (self.y_width)
        s += '              irq_per_proc = "%d"\n'   % (self.irq_per_proc)
        s += '              use_ramdisk  = "%d"\n'   % (self.use_ramdisk)
        s += '              x_io         = "%d"\n'   % (self.x_io)
        s += '              y_io         = "%d" >\n' % (self.y_io)
        s += '\n'

        s += '    <clusterset>\n'
        for x in xrange ( self.x_size ):
            for y in xrange ( self.y_size ):
                cluster_id = (x * self.y_size) + y
                s += self.clusters[cluster_id].xml()
        s += '    </clusterset>\n'
        s += '\n'

        s += '    <globalset>\n'
        for vseg in self.globs: s += vseg.xml()
        s += '    </globalset>\n'
        s += '\n'

        s += '    <vspaceset>\n'
        for vspace in self.vspaces: s += vspace.xml()
        s += '    </vspaceset>\n'

        s += '</mapping_info>\n'
        return s

    ##########################
    def cbin( self, verbose ):     # C binary structure for map.bin file generation 

        byte_stream = bytearray()

        # header
        byte_stream += self.int2bytes(4,  self.signature)
        byte_stream += self.int2bytes(4,  self.x_size)
        byte_stream += self.int2bytes(4,  self.y_size)
        byte_stream += self.int2bytes(4,  self.x_width)
        byte_stream += self.int2bytes(4,  self.y_width)
        byte_stream += self.int2bytes(4,  self.x_io)
        byte_stream += self.int2bytes(4,  self.y_io)
        byte_stream += self.int2bytes(4,  self.irq_per_proc)
        byte_stream += self.int2bytes(4,  self.use_ramdisk)
        byte_stream += self.int2bytes(4,  self.total_globals)
        byte_stream += self.int2bytes(4,  self.total_vspaces)
        byte_stream += self.int2bytes(4,  self.total_psegs)
        byte_stream += self.int2bytes(4,  self.total_vsegs)
        byte_stream += self.int2bytes(4,  self.total_tasks)
        byte_stream += self.int2bytes(4,  self.total_procs)
        byte_stream += self.int2bytes(4,  self.total_irqs)
        byte_stream += self.int2bytes(4,  self.total_periphs)
        byte_stream += self.str2bytes(64, self.name)

        if ( verbose ):
            print '\n'
            print 'name          = %s' % self.name
            print 'signature     = %x' % self.signature
            print 'x_size        = %d' % self.x_size
            print 'y_size        = %d' % self.y_size
            print 'x_width       = %d' % self.x_width
            print 'y_width       = %d' % self.y_width
            print 'x_io          = %d' % self.x_io
            print 'y_io          = %d' % self.y_io
            print 'irq_per_proc  = %d' % self.irq_per_proc
            print 'use_ramdisk   = %d' % self.use_ramdisk
            print 'total_globals = %d' % self.total_globals
            print 'total_psegs   = %d' % self.total_psegs
            print 'total_vsegs   = %d' % self.total_vsegs
            print 'total_tasks   = %d' % self.total_tasks
            print 'total_procs   = %d' % self.total_procs
            print 'total_irqs    = %d' % self.total_irqs
            print 'total_periphs = %d' % self.total_periphs
            print '\n'

        # clusters array
        index = 0
        for cluster in self.clusters:
            byte_stream += cluster.cbin( self, verbose, index )
            index += 1

        if ( verbose ): print '\n'

        # psegs array
        index = 0
        for cluster in self.clusters:
            for pseg in cluster.psegs:
                byte_stream += pseg.cbin( self, verbose, index, cluster )
                index += 1

        if ( verbose ): print '\n'

        # vspaces array
        index = 0
        for vspace in self.vspaces:
            byte_stream += vspace.cbin( self, verbose, index )
            index += 1

        if ( verbose ): print '\n'

        # vsegs array
        index = 0
        for vseg in self.globs:
            byte_stream += vseg.cbin( self, verbose, index )
            index += 1
        for vspace in self.vspaces:
            for vseg in vspace.vsegs:
                byte_stream += vseg.cbin( self, verbose, index )
                index += 1

        if ( verbose ): print '\n'

        # tasks array
        index = 0
        for vspace in self.vspaces:
            for task in vspace.tasks:
                byte_stream += task.cbin( self, verbose, index, vspace )
                index += 1

        if ( verbose ): print '\n'

        # procs array
        index = 0
        for cluster in self.clusters:
            for proc in cluster.procs:
                byte_stream += proc.cbin( self, verbose, index )
                index += 1

        if ( verbose ): print '\n'

        # irqs array
        index = 0
        for cluster in self.clusters:
            for periph in cluster.periphs:
                for irq in periph.irqs:
                    byte_stream += irq.cbin( self, verbose, index )
                    index += 1

        if ( verbose ): print '\n'

        # periphs array
        index = 0
        for cluster in self.clusters:
            for periph in cluster.periphs:
                byte_stream += periph.cbin( self, verbose, index )
                index += 1

        return byte_stream
    # end of cbin()

    #######################################################################
    def giet_vsegs( self ):      # compute string for giet_vsegs.ld file
                                 # required by giet_vm compilation

        # search the vsegs required for the giet_vsegs.ld
        boot_code_found      = False
        boot_data_found      = False
        kernel_uncdata_found = False
        kernel_data_found    = False
        kernel_code_found    = False
        kernel_init_found    = False
        for vseg in self.globs:

            if ( vseg.name[0:13] == 'seg_boot_code' ):
                boot_code_vbase      = vseg.vbase
                boot_code_size       = vseg.length
                boot_code_found      = True

            if ( vseg.name[0:13] == 'seg_boot_data' ):
                boot_data_vbase      = vseg.vbase
                boot_data_size       = vseg.length
                boot_data_found      = True

            if ( vseg.name[0:15] == 'seg_kernel_data' ):
                kernel_data_vbase    = vseg.vbase
                kernel_data_size     = vseg.length
                kernel_data_found    = True

            if ( vseg.name[0:15] == 'seg_kernel_code' ):
                kernel_code_vbase    = vseg.vbase
                kernel_code_size     = vseg.length
                kernel_code_found    = True

            if ( vseg.name[0:15] == 'seg_kernel_init' ):
                kernel_init_vbase    = vseg.vbase
                kernel_init_size     = vseg.length
                kernel_init_found    = True

        # check if all required vsegs have been found
        if ( boot_code_found      == False ):
             print '[genmap error] in giet_vsegs()'
             print '    seg_boot_code vseg missing'
             sys.exit()

        if ( boot_data_found      == False ):
             print '[genmap error] in giet_vsegs()'
             print '    seg_boot_data vseg missing'
             sys.exit()

        if ( kernel_data_found    == False ):
             print '[genmap error] in giet_vsegs()'
             print '    seg_kernel_data vseg missing'
             sys.exit()

        if ( kernel_code_found    == False ):
             print '[genmap error] in giet_vsegs()'
             print '    seg_kernel_code vseg missing'
             sys.exit()

        if ( kernel_init_found    == False ):
             print '[genmap error] in giet_vsegs()'
             print '    seg_kernel_init vseg missing'
             sys.exit()

        # build string
        s =  '/* Generated by genmap for %s */\n'  % self.name
        s += '\n'

        s += 'boot_code_vbase      = 0x%x;\n'   % boot_code_vbase
        s += 'boot_code_size       = 0x%x;\n'   % boot_code_size
        s += '\n'
        s += 'boot_data_vbase      = 0x%x;\n'   % boot_data_vbase
        s += 'boot_data_size       = 0x%x;\n'   % boot_data_size
        s += '\n'
        s += 'kernel_code_vbase    = 0x%x;\n'   % kernel_code_vbase
        s += 'kernel_code_size     = 0x%x;\n'   % kernel_code_size
        s += '\n'
        s += 'kernel_data_vbase    = 0x%x;\n'   % kernel_data_vbase
        s += 'kernel_data_size     = 0x%x;\n'   % kernel_data_size
        s += '\n'
        s += 'kernel_init_vbase    = 0x%x;\n'   % kernel_init_vbase
        s += 'kernel_init_size     = 0x%x;\n'   % kernel_init_size
        s += '\n'

        return s

    ######################################################################
    def hard_config( self ):     # compute string for hard_config.h file 
                                 # required by
                                 # - top.cpp compilation
                                 # - giet_vm compilation
                                 # - tsar_preloader compilation

        nb_total_procs = 0

        # for each peripheral type, define default values
        # for pbase address, size, number of components, and channels
        nb_cma       = 0
        cma_channels = 0
        seg_cma_base = 0xFFFFFFFF
        seg_cma_size = 0

        nb_dma       = 0
        dma_channels = 0
        seg_dma_base = 0xFFFFFFFF
        seg_dma_size = 0

        nb_fbf       = 0
        fbf_channels = 0
        seg_fbf_base = 0xFFFFFFFF
        seg_fbf_size = 0
        fbf_arg0     = 0
        fbf_arg1     = 0

        nb_iob       = 0
        iob_channels = 0
        seg_iob_base = 0xFFFFFFFF
        seg_iob_size = 0

        nb_ioc       = 0
        ioc_channels = 0
        seg_ioc_base = 0xFFFFFFFF
        seg_ioc_size = 0
        use_ioc_bdv  = False
        use_ioc_sdc  = False
        use_ioc_hba  = False
        use_ioc_spi  = False

        nb_mmc       = 0
        mmc_channels = 0
        seg_mmc_base = 0xFFFFFFFF
        seg_mmc_size = 0

        nb_mwr       = 0
        mwr_channels = 0
        seg_mwr_base = 0xFFFFFFFF
        seg_mwr_size = 0
        mwr_arg0     = 0
        mwr_arg1     = 0
        mwr_arg2     = 0
        mwr_arg3     = 0
        use_mwr_gcd  = False
        use_mwr_dct  = False
        use_mwr_cpy  = False

        nb_nic       = 0
        nic_channels = 0
        seg_nic_base = 0xFFFFFFFF
        seg_nic_size = 0

        nb_pic       = 0
        pic_channels = 0
        seg_pic_base = 0xFFFFFFFF
        seg_pic_size = 0

        nb_rom       = 0
        rom_channels = 0
        seg_rom_base = 0xFFFFFFFF
        seg_rom_size = 0

        nb_sim       = 0
        sim_channels = 0
        seg_sim_base = 0xFFFFFFFF
        seg_sim_size = 0

        nb_tim       = 0
        tim_channels = 0
        seg_tim_base = 0xFFFFFFFF
        seg_tim_size = 0

        nb_tty       = 0
        tty_channels = 0
        seg_tty_base = 0xFFFFFFFF
        seg_tty_size = 0

        nb_xcu       = 0
        xcu_channels = 0
        seg_xcu_base = 0xFFFFFFFF
        seg_xcu_size = 0
        xcu_arg0     = 0

        nb_drom       = 0
        drom_channels = 0
        seg_drom_base = 0xFFFFFFFF
        seg_drom_size = 0

        # get peripherals attributes
        for cluster in self.clusters:
            for periph in cluster.periphs:
                if   ( periph.ptype == 'CMA' ):
                    seg_cma_base = periph.pseg.base & 0xFFFFFFFF
                    seg_cma_size = periph.pseg.size
                    cma_channels = periph.channels
                    nb_cma +=1

                elif ( periph.ptype == 'DMA' ):
                    seg_dma_base = periph.pseg.base & 0xFFFFFFFF
                    seg_dma_size = periph.pseg.size
                    dma_channels = periph.channels
                    nb_dma +=1

                elif ( periph.ptype == 'FBF' ):
                    seg_fbf_base = periph.pseg.base & 0xFFFFFFFF
                    seg_fbf_size = periph.pseg.size
                    fbf_channels = periph.channels
                    fbf_arg0     = periph.arg0
                    fbf_arg1     = periph.arg1
                    nb_fbf +=1

                elif ( periph.ptype == 'IOB' ):
                    seg_iob_base = periph.pseg.base & 0xFFFFFFFF
                    seg_iob_size = periph.pseg.size
                    iob_channels = periph.channels
                    nb_iob +=1

                elif ( periph.ptype == 'IOC' ):
                    seg_ioc_base = periph.pseg.base & 0xFFFFFFFF
                    seg_ioc_size = periph.pseg.size
                    ioc_channels = periph.channels
                    nb_ioc += 1
                    if ( periph.subtype == 'BDV' ): use_ioc_bdv = True
                    if ( periph.subtype == 'HBA' ): use_ioc_hba = True
                    if ( periph.subtype == 'SDC' ): use_ioc_sdc = True
                    if ( periph.subtype == 'SPI' ): use_ioc_spi = True

                elif ( periph.ptype == 'MMC' ):
                    seg_mmc_base = periph.pseg.base & 0xFFFFFFFF
                    seg_mmc_size = periph.pseg.size
                    mmc_channels = periph.channels
                    nb_mmc +=1

                elif ( periph.ptype == 'MWR' ):
                    seg_mwr_base = periph.pseg.base & 0xFFFFFFFF
                    seg_mwr_size = periph.pseg.size
                    mwr_channels = periph.channels
                    mwr_arg0     = periph.arg0
                    mwr_arg1     = periph.arg1
                    mwr_arg2     = periph.arg2
                    mwr_arg3     = periph.arg3
                    nb_mwr +=1
                    if ( periph.subtype == 'GCD' ): use_mwr_gcd = True
                    if ( periph.subtype == 'DCT' ): use_mwr_dct = True
                    if ( periph.subtype == 'CPY' ): use_mwr_cpy = True

                elif ( periph.ptype == 'ROM' ):
                    seg_rom_base = periph.pseg.base & 0xFFFFFFFF
                    seg_rom_size = periph.pseg.size
                    rom_channels = periph.channels
                    nb_rom +=1

                elif ( periph.ptype == 'DROM' ):
                    seg_drom_base = periph.pseg.base & 0xFFFFFFFF
                    seg_drom_size = periph.pseg.size
                    drom_channels = periph.channels
                    nb_drom +=1

                elif ( periph.ptype == 'SIM' ):
                    seg_sim_base = periph.pseg.base & 0xFFFFFFFF
                    seg_sim_size = periph.pseg.size
                    sim_channels = periph.channels
                    nb_sim +=1

                elif ( periph.ptype == 'NIC' ):
                    seg_nic_base = periph.pseg.base & 0xFFFFFFFF
                    seg_nic_size = periph.pseg.size
                    nic_channels = periph.channels
                    nb_nic +=1

                elif ( periph.ptype == 'PIC' ):
                    seg_pic_base = periph.pseg.base & 0xFFFFFFFF
                    seg_pic_size = periph.pseg.size
                    pic_channels = periph.channels
                    nb_pic +=1

                elif ( periph.ptype == 'TIM' ):
                    seg_tim_base = periph.pseg.base & 0xFFFFFFFF
                    seg_tim_size = periph.pseg.size
                    tim_channels = periph.channels
                    nb_tim +=1

                elif ( periph.ptype == 'TTY' ):
                    seg_tty_base = periph.pseg.base & 0xFFFFFFFF
                    seg_tty_size = periph.pseg.size
                    tty_channels = periph.channels
                    nb_tty +=1

                elif ( periph.ptype == 'XCU' ):
                    seg_xcu_base = periph.pseg.base & 0xFFFFFFFF
                    seg_xcu_size = periph.pseg.size
                    xcu_channels = periph.channels
                    xcu_arg0     = periph.arg0
                    xcu_arg1     = periph.arg1
                    xcu_arg2     = periph.arg2
                    nb_xcu +=1

        # no more than two access to external peripherals
        assert ( nb_fbf <= 2 )
        assert ( nb_cma <= 2 )
        assert ( nb_ioc <= 2 )
        assert ( nb_nic <= 2 )
        assert ( nb_tim <= 2 )
        assert ( nb_tty <= 2 )
        assert ( nb_pic <= 2 )

        # one and only one type of IOC controller
        nb_ioc_types = 0
        if use_ioc_hba:       nb_ioc_types += 1
        if use_ioc_bdv:       nb_ioc_types += 1
        if use_ioc_sdc:       nb_ioc_types += 1
        if use_ioc_spi:       nb_ioc_types += 1
        if self.use_ramdisk:  nb_ioc_types += 1
        assert ( nb_ioc_types == 1 )

        # one and only one type of MWR controller
        nb_mwr_types = 0
        if use_mwr_gcd:       nb_mwr_types += 1
        if use_mwr_dct:       nb_mwr_types += 1
        if use_mwr_cpy:       nb_mwr_types += 1
        if ( nb_mwr > 0 ) : assert ( nb_mwr_types == 1 )

        # Compute total number of processors
        for cluster in self.clusters:
            nb_total_procs += len( cluster.procs )

        # Compute physical addresses for BOOT vsegs
        boot_mapping_found   = False
        boot_code_found      = False
        boot_data_found      = False
        boot_stack_found     = False

        for vseg in self.globs:
            if ( vseg.name == 'seg_boot_mapping' ):
                boot_mapping_base       = vseg.vbase
                boot_mapping_size       = vseg.length
                boot_mapping_identity   = vseg.identity
                boot_mapping_found      = True

            if ( vseg.name == 'seg_boot_code' ):
                boot_code_base          = vseg.vbase
                boot_code_size          = vseg.length
                boot_code_identity      = vseg.identity
                boot_code_found         = True

            if ( vseg.name == 'seg_boot_data' ):
                boot_data_base          = vseg.vbase
                boot_data_size          = vseg.length
                boot_data_identity      = vseg.identity
                boot_data_found         = True

            if ( vseg.name == 'seg_boot_stack' ):
                boot_stack_base         = vseg.vbase
                boot_stack_size         = vseg.length
                boot_stack_identity     = vseg.identity
                boot_stack_found        = True

        # check that BOOT vsegs are found and identity mapping
        if ( (boot_mapping_found == False) or (boot_mapping_identity == False) ):
             print '[genmap error] in hard_config()'
             print '    seg_boot_mapping missing or not identity mapping'
             sys.exit()

        if ( (boot_code_found == False) or (boot_code_identity == False) ):
             print '[genmap error] in hard_config()'
             print '    seg_boot_code missing or not identity mapping'
             sys.exit()

        if ( (boot_data_found == False) or (boot_data_identity == False) ):
             print '[genmap error] in hard_config()'
             print '    seg_boot_data missing or not identity mapping'
             sys.exit()

        if ( (boot_stack_found == False) or (boot_stack_identity == False) ):
             print '[genmap error] in giet_vsegs()'
             print '    seg_boot_stask missing or not identity mapping'
             sys.exit()

        # Search RAMDISK global vseg if required
        seg_rdk_base =  0xFFFFFFFF
        seg_rdk_size =  0
        seg_rdk_found = False

        if self.use_ramdisk:
            for vseg in self.globs:
                if ( vseg.name == 'seg_ramdisk' ):
                    seg_rdk_base  = vseg.vbase
                    seg_rdk_size  = vseg.length
                    seg_rdk_found = True

            if ( seg_rdk_found == False ):
                print 'Error in hard_config() "seg_ramdisk" not found'
                sys.exit(1)

        # build string
        s =  '/* Generated by genmap for %s */\n'  % self.name
        s += '\n'
        s += '#ifndef HARD_CONFIG_H\n'
        s += '#define HARD_CONFIG_H\n'
        s += '\n'

        s += '/* General platform parameters */\n'
        s += '\n'
        s += '#define X_SIZE                 %d\n'    % self.x_size
        s += '#define Y_SIZE                 %d\n'    % self.y_size
        s += '#define X_WIDTH                %d\n'    % self.x_width
        s += '#define Y_WIDTH                %d\n'    % self.y_width
        s += '#define P_WIDTH                %d\n'    % self.p_width
        s += '#define X_IO                   %d\n'    % self.x_io
        s += '#define Y_IO                   %d\n'    % self.y_io
        s += '#define NB_PROCS_MAX           %d\n'    % self.nprocs
        s += '#define IRQ_PER_PROCESSOR      %d\n'    % self.irq_per_proc
        s += '#define RESET_ADDRESS          0x%x\n'  % self.reset_address
        s += '#define NB_TOTAL_PROCS         %d\n'    % nb_total_procs
        s += '\n'

        s += '/* Peripherals */\n'
        s += '\n'
        s += '#define NB_TTY_CHANNELS        %d\n'    % tty_channels
        s += '#define NB_IOC_CHANNELS        %d\n'    % ioc_channels
        s += '#define NB_NIC_CHANNELS        %d\n'    % nic_channels
        s += '#define NB_CMA_CHANNELS        %d\n'    % cma_channels
        s += '#define NB_TIM_CHANNELS        %d\n'    % tim_channels
        s += '#define NB_DMA_CHANNELS        %d\n'    % dma_channels
        s += '\n'
        s += '#define USE_XCU                %d\n'    % ( nb_xcu != 0 )
        s += '#define USE_DMA                %d\n'    % ( nb_dma != 0 )
        s += '\n'
        s += '#define USE_IOB                %d\n'    % ( nb_iob != 0 )
        s += '#define USE_PIC                %d\n'    % ( nb_pic != 0 )
        s += '#define USE_FBF                %d\n'    % ( nb_fbf != 0 )
        s += '#define USE_NIC                %d\n'    % ( nb_nic != 0 )
        s += '\n'
        s += '#define USE_IOC_BDV            %d\n'    % use_ioc_bdv
        s += '#define USE_IOC_SDC            %d\n'    % use_ioc_sdc
        s += '#define USE_IOC_HBA            %d\n'    % use_ioc_hba
        s += '#define USE_IOC_SPI            %d\n'    % use_ioc_spi
        s += '#define USE_IOC_RDK            %d\n'    % self.use_ramdisk
        s += '\n'
        s += '#define USE_MWR_GCD            %d\n'    % use_mwr_gcd
        s += '#define USE_MWR_DCT            %d\n'    % use_mwr_dct
        s += '#define USE_MWR_CPY            %d\n'    % use_mwr_cpy
        s += '\n'
        s += '#define FBUF_X_SIZE            %d\n'    % fbf_arg0
        s += '#define FBUF_Y_SIZE            %d\n'    % fbf_arg1
        s += '\n'
        s += '#define XCU_NB_HWI             %d\n'    % xcu_arg0
        s += '#define XCU_NB_PTI             %d\n'    % xcu_arg1
        s += '#define XCU_NB_WTI             %d\n'    % xcu_arg2
        s += '#define XCU_NB_OUT             %d\n'    % xcu_channels
        s += '\n'
        s += '#define MWR_TO_COPROC          %d\n'    % mwr_arg0
        s += '#define MWR_FROM_COPROC        %d\n'    % mwr_arg1
        s += '#define MWR_CONFIG             %d\n'    % mwr_arg2
        s += '#define MWR_STATUS             %d\n'    % mwr_arg3
        s += '\n'

        s += '/* base addresses and sizes for physical segments */\n'
        s += '\n'
        s += '#define SEG_RAM_BASE           0x%x\n'  % self.ram_base
        s += '#define SEG_RAM_SIZE           0x%x\n'  % self.ram_size
        s += '\n'
        s += '#define SEG_CMA_BASE           0x%x\n'  % seg_cma_base
        s += '#define SEG_CMA_SIZE           0x%x\n'  % seg_cma_size
        s += '\n'
        s += '#define SEG_DMA_BASE           0x%x\n'  % seg_dma_base
        s += '#define SEG_DMA_SIZE           0x%x\n'  % seg_dma_size
        s += '\n'
        s += '#define SEG_FBF_BASE           0x%x\n'  % seg_fbf_base
        s += '#define SEG_FBF_SIZE           0x%x\n'  % seg_fbf_size
        s += '\n'
        s += '#define SEG_IOB_BASE           0x%x\n'  % seg_iob_base
        s += '#define SEG_IOB_SIZE           0x%x\n'  % seg_iob_size
        s += '\n'
        s += '#define SEG_IOC_BASE           0x%x\n'  % seg_ioc_base
        s += '#define SEG_IOC_SIZE           0x%x\n'  % seg_ioc_size
        s += '\n'
        s += '#define SEG_MMC_BASE           0x%x\n'  % seg_mmc_base
        s += '#define SEG_MMC_SIZE           0x%x\n'  % seg_mmc_size
        s += '\n'
        s += '#define SEG_MWR_BASE           0x%x\n'  % seg_mwr_base
        s += '#define SEG_MWR_SIZE           0x%x\n'  % seg_mwr_size
        s += '\n'
        s += '#define SEG_ROM_BASE           0x%x\n'  % seg_rom_base
        s += '#define SEG_ROM_SIZE           0x%x\n'  % seg_rom_size
        s += '\n'
        s += '#define SEG_SIM_BASE           0x%x\n'  % seg_sim_base
        s += '#define SEG_SIM_SIZE           0x%x\n'  % seg_sim_size
        s += '\n'
        s += '#define SEG_NIC_BASE           0x%x\n'  % seg_nic_base
        s += '#define SEG_NIC_SIZE           0x%x\n'  % seg_nic_size
        s += '\n'
        s += '#define SEG_PIC_BASE           0x%x\n'  % seg_pic_base
        s += '#define SEG_PIC_SIZE           0x%x\n'  % seg_pic_size
        s += '\n'
        s += '#define SEG_TIM_BASE           0x%x\n'  % seg_tim_base
        s += '#define SEG_TIM_SIZE           0x%x\n'  % seg_tim_size
        s += '\n'
        s += '#define SEG_TTY_BASE           0x%x\n'  % seg_tty_base
        s += '#define SEG_TTY_SIZE           0x%x\n'  % seg_tty_size
        s += '\n'
        s += '#define SEG_XCU_BASE           0x%x\n'  % seg_xcu_base
        s += '#define SEG_XCU_SIZE           0x%x\n'  % seg_xcu_size
        s += '\n'
        s += '#define SEG_RDK_BASE           0x%x\n'  % seg_rdk_base
        s += '#define SEG_RDK_SIZE           0x%x\n'  % seg_rdk_size
        s += '\n'
        s += '#define SEG_DROM_BASE          0x%x\n'  % seg_drom_base
        s += '#define SEG_DROM_SIZE          0x%x\n'  % seg_drom_size
        s += '\n'
        s += '#define PERI_CLUSTER_INCREMENT 0x%x\n'  % self.peri_increment
        s += '\n'

        s += '/* physical base addresses for identity mapped vsegs */\n'
        s += '/* used by the GietVM OS                             */\n'
        s += '\n'
        s += '#define SEG_BOOT_MAPPING_BASE  0x%x\n'  % boot_mapping_base
        s += '#define SEG_BOOT_MAPPING_SIZE  0x%x\n'  % boot_mapping_size
        s += '\n'
        s += '#define SEG_BOOT_CODE_BASE     0x%x\n'  % boot_code_base
        s += '#define SEG_BOOT_CODE_SIZE     0x%x\n'  % boot_code_size
        s += '\n'
        s += '#define SEG_BOOT_DATA_BASE     0x%x\n'  % boot_data_base
        s += '#define SEG_BOOT_DATA_SIZE     0x%x\n'  % boot_data_size
        s += '\n'
        s += '#define SEG_BOOT_STACK_BASE    0x%x\n'  % boot_stack_base
        s += '#define SEG_BOOT_STACK_SIZE    0x%x\n'  % boot_stack_size
        s += '#endif\n'

        return s

    # end of hard_config()

    #################################################################
    def linux_dts( self ):     # compute string for linux.dts file
                               # used for linux configuration
        # header
        s =  '/dts-v1/;\n'
        s += '\n'
        s += '/{\n'
        s += '  compatible = "tsar,%s";\n' % self.name
        s += '  #address-cells = <2>;\n'               # physical address on 64 bits
        s += '  #size-cells    = <1>;\n'               # segment size on 32 bits
        s += '  model = "%s";\n' % self.name
        s += '\n'

        # linux globals arguments
        s += '  chosen {\n'
        s += '    linux,stdout-path = &tty;\n'
        s += '    bootargs = "console=tty0 console=ttyVTTY0 earlyprintk";\n'
        s += '  };\n\n'

        # cpus (for each cluster)
        s += '  cpus {\n'
        s += '    #address-cells = <1>;\n'
        s += '    #size-cells    = <0>;\n'

        for cluster in self.clusters:
            for proc in cluster.procs:
                x       = cluster.x
                y       = cluster.y
                l       = proc.lpid
                proc_id = (((x << self.y_width) + y) << self.p_width) + l
                s += '    cpu@%d_%d_%d {\n' %(x,y,l)
                s += '      device_type = "cpu";\n'
                s += '      compatible = "soclib,mips32el";\n'
                s += '      reg = <0x%x>;\n' % proc_id
                s += '    };\n'
                s += '\n'

        s += '  };\n\n'

        # devices (ram or peripheral) are grouped per cluster
        # the "compatible" attribute links a peripheral device
        # to one or several drivers identified by ("major","minor")

        chosen_tty = False
        for cluster in self.clusters:
            x               = cluster.x
            y               = cluster.y
            found_xcu       = False
            found_pic       = False

            s += '  /*** cluster[%d,%d] ***/\n\n' % (x,y)

            # scan all psegs to find RAM in current cluster
            for pseg in cluster.psegs:
                if ( pseg.segtype == 'RAM' ):
                    msb  = pseg.base >> 32
                    lsb  = pseg.base & 0xFFFFFFFF
                    size = pseg.size

                    s += '  %s@0x%x {\n' % (pseg.name, pseg.base)
                    s += '    device_type = "memory";\n'
                    s += '    reg = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)
                    s += '  };\n\n'

            # scan all periphs to find XCU or PIC in current cluster
            for periph in cluster.periphs:
                msb     = periph.pseg.base >> 32
                lsb     = periph.pseg.base & 0xFFFFFFFF
                size    = periph.pseg.size

                # search XCU (can be replicated)
                if ( (periph.ptype == 'XCU') ):
                    found_xcu     = True

                    s += '  %s@0x%x {\n' % (periph.pseg.name, periph.pseg.base)
                    s += '    compatible = "soclib,vci_xicu","soclib,vci_xicu_timer";\n'
                    s += '    interrupt-controller;\n'
                    s += '    #interrupt-cells = <1>;\n'
                    s += '    clocks = <&freq>;\n'         # XCU contains a timer
                    s += '    reg = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)
                    s += '  };\n\n'

                # search PIC (non replicated)
                if ( periph.ptype == 'PIC' ):
                    found_pic     = True

                    s += '  %s@0x%x {\n' % (periph.pseg.name, periph.pseg.base)
                    s += '    compatible = "soclib,vci_iopic";\n'
                    s += '    interrupt-controller;\n'
                    s += '    #interrupt-cells = <1>;\n'
                    s += '    reg = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)
                    s += '  };\n\n'

            # we need one interrupt controler in any cluster containing peripherals
            if ( (found_xcu == False) and
                 (found_pic == False) and
                 (len(cluster.periphs) > 0) ):
                print '[genmap error] in linux_dts()'
                print '    No XCU/PIC in cluster(%d,%d)' % (x,y)
                sys.exit(1)

            # scan all periphs to find TTY and IOC in current cluster
            for periph in cluster.periphs:
                msb     = periph.pseg.base >> 32
                lsb     = periph.pseg.base & 0xFFFFFFFF
                size    = periph.pseg.size

                irq_ctrl = periph.irq_ctrl
                if irq_ctrl != None:
                    irq_ctrl_name = '%s@0x%x' % (irq_ctrl.pseg.name, irq_ctrl.pseg.base)

                # search TTY (non replicated)
                if periph.ptype == 'TTY':
                    assert irq_ctrl != None

                    # get HWI index to XCU or PIC (only TTY0 is used by Linux)
                    hwi_id = 0xFFFFFFFF
                    for irq in irq_ctrl.irqs:
                        if ( (irq.isrtype == 'ISR_TTY_RX') and (irq.channel == 0) ):
                            hwi_id = irq.srcid

                    if ( hwi_id == 0xFFFFFFFF ):
                        print '[genmap error] in linux.dts()'
                        print '    IRQ_TTY_RX not found'
                        sys.exit(1)

                    if chosen_tty == False:
                        chosen_tty = True
                        s += '  tty:\n'

                    s += '  %s@0x%x {\n' % (periph.pseg.name, periph.pseg.base)
                    s += '    compatible = "soclib,vci_multi_tty";\n'
                    s += '    interrupt-parent = <&{/%s}>;\n' % (irq_ctrl_name)
                    s += '    interrupts = <%d>;\n' % hwi_id
                    s += '    reg = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)
                    s += '  };\n\n'


                # search IOC (non replicated)
                elif ( periph.ptype == 'IOC' ):
                    assert irq_ctrl != None

                    if ( periph.subtype == 'BDV' ):

                        # get irq line index associated to bdv
                        hwi_id = 0xFFFFFFFF
                        for irq in irq_ctrl.irqs:
                            if ( irq.isrtype == 'ISR_BDV' ): hwi_id = irq.srcid

                        if ( hwi_id == 0xFFFFFFFF ):
                            print '[genmap error] in linux.dts()'
                            print '    ISR_BDV not found'
                            sys.exit(1)

                        s += '  %s@0x%x {\n' % (periph.pseg.name, periph.pseg.base)
                        s += '    compatible = "tsar,vci_block_device";\n'
                        s += '    interrupt-parent = <&{/%s}>;\n' % (irq_ctrl_name)
                        s += '    interrupts = <%d>;\n' % hwi_id
                        s += '    reg = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)
                        s += '  };\n\n'

                    else:
                        print '[genmap warning] in linux_dts() : '
                        print ' %s' % (periph.subtype),
                        print 'peripheral not supported by LINUX'

                # XCU or PIC have been already parsed
                elif ( periph.ptype == 'XCU' ) or ( periph.ptype == 'PIC' ):
                    pass

                # other peripherals
                else:
                    print '[genmap warning] in linux_dts() : '
                    print ' %s peripheral not supported by LINUX' % (periph.ptype)

        # clocks
        s += '  clocks {\n'
        s += '    freq: freq@50MHZ {\n'
        s += '      #clock-cells = <0>;\n'
        s += '      compatible = "fixed-clock";\n'
        s += '      clock-frequency = <50000000>;\n'
        s += '    };\n'
        s += '  };\n\n'
        s += '  cpuclk {\n'
        s += '    compatible = "soclib,mips32_clksrc";\n'
        s += '    clocks = <&freq>;\n'
        s += '  };\n'
        s += '};\n'

        return s
        # end linux_dts()


    #################################################################
    def netbsd_dts( self ):    # compute string for netbsd.dts file
                               # used for netbsd configuration
        # header
        s =  '/dts-v1/;\n'
        s += '\n'
        s += '/{\n'
        s += '  #address-cells = <2>;\n'
        s += '  #size-cells    = <1>;\n'

        # cpus (for each cluster)
        s += '  cpus {\n'
        s += '    #address-cells = <1>;\n'
        s += '    #size-cells    = <0>;\n'

        for cluster in self.clusters:
            for proc in cluster.procs:
                proc_id = (((cluster.x << self.y_width) + cluster.y)
                              << self.p_width) + proc.lpid

                s += '    Mips,32@0x%x {\n'                % proc_id
                s += '      device_type = "cpu";\n'
                s += '      icudev_type = "cpu:mips";\n'
                s += '      name        = "Mips,32";\n'
                s += '      reg         = <0x%x>;\n'     % proc_id
                s += '    };\n'
                s += '\n'

        s += '  };\n'

        # physical memory banks (for each cluster)
        for cluster in self.clusters:
            for pseg in cluster.psegs:

                if ( pseg.segtype == 'RAM' ):
                    msb  = pseg.base >> 32
                    lsb  = pseg.base & 0xFFFFFFFF
                    size = pseg.size

                    s += '  %s@0x%x {\n' % (pseg.name, pseg.base)
                    s += '    cached      = <1>;\n'
                    s += '    device_type = "memory";\n'
                    s += '    reg         = <0x%x  0x%x  0x%x>;\n' % (msb,lsb,size)
                    s += '  };\n'

        # peripherals (for each cluster)
        for cluster in self.clusters:
            x = cluster.x
            y = cluster.y

            # research XCU or PIC component
            found_xcu = False
            found_pic = False
            for periph in cluster.periphs:
                if ( (periph.ptype == 'XCU') ):
                    found_xcu = True
                    xcu = periph
                    msb  = periph.pseg.base >> 32
                    lsb  = periph.pseg.base & 0xFFFFFFFF
                    size = periph.pseg.size

                    s += '  %s@0x%x {\n'  % (periph.pseg.name, periph.pseg.base)
                    s += '    device_type = "soclib:xicu:root";\n'
                    s += '    reg         = <0x%x  0x%x  0x%x>;\n' % (msb,lsb,size)
                    s += '    input_lines = <%d>;\n'    % periph.arg0
                    s += '    ipis        = <%d>;\n'    % periph.arg1
                    s += '    timers      = <%d>;\n'    % periph.arg2

                    output_id = 0            # output index from XCU
                    for lpid in xrange ( len(cluster.procs) ):        # destination processor index
                        for itid in xrange ( self.irq_per_proc ):     # input irq index on processor
                            cluster_xy = (cluster.x << self.y_width) + cluster.y
                            proc_id    = (cluster_xy << self.p_width) + lpid
                            s += '    out@%d {\n' % output_id
                            s += '      device_type = "soclib:xicu:filter";\n'
                            s += '      irq = <&{/cpus/Mips,32@0x%x} %d>;\n' % (proc_id, itid)
                            s += '      output_line = <%d>;\n' % output_id
                            s += '      parent = <&{/%s@0x%x}>;\n' % (periph.pseg.name, periph.pseg.base)
                            s += '    };\n'

                            output_id += 1

                    s += '  };\n'

                if ( periph.ptype == 'PIC' ):
                    found_pic = True
                    pic  = periph
                    msb  = periph.pseg.base >> 32
                    lsb  = periph.pseg.base & 0xFFFFFFFF
                    size = periph.pseg.size

                    s += '  %s@0x%x {\n'  % (periph.pseg.name, periph.pseg.base)
                    s += '    device_type = "soclib:pic:root";\n'
                    s += '    reg         = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)
                    s += '    input_lines = <%d>;\n' % periph.channels
                    s += '  };\n'

            # at least one interrupt controller
            if ( (found_xcu == False) and (found_pic == False) and (len(cluster.periphs) > 0) ):
                print '[genmap error] in netbsd_dts()'
                print '    No XCU/PIC in cluster(%d,%d)' % (x,y)
                sys.exit(1)

            # get all others peripherals in cluster
            for periph in cluster.periphs:
                msb  = periph.pseg.base >> 32
                lsb  = periph.pseg.base & 0xFFFFFFFF
                size = periph.pseg.size

                irq_ctrl = periph.irq_ctrl
                if irq_ctrl != None:
                    irq_ctrl_name = '%s@0x%x' % (irq_ctrl.pseg.name, irq_ctrl.pseg.base)

                # XCU or PIC have been already parsed
                if ( periph.ptype == 'XCU' ) or ( periph.ptype == 'PIC' ):
                    pass

                # research DMA component
                elif ( periph.ptype == 'DMA' ):

                    s += '  %s@0x%x {\n'  % (periph.pseg.name, periph.pseg.base)
                    s += '    device_type = "soclib:dma";\n'
                    s += '    reg = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)
                    s += '    channel_count = <%d>;\n' % periph.channels

                    # multi-channels : get HWI index (to XCU) for each channel
                    for channel in xrange( periph.channels ):
                        hwi_id = 0xFFFFFFFF
                        for irq in xcu.irqs:
                            if ( (irq.isrtype == 'ISR_DMA') and
                                 (irq.channel == channel) ):
                                hwi_id = irq.srcid

                        if ( hwi_id == 0xFFFFFFFF ):
                            print '[genmap error] in netbsd.dts()'
                            print '    ISR_DMA channel %d not found' % channel
                            sys.exit(1)

                        name = '%s@0x%x' % (xcu.pseg.name, xcu.pseg.base)
                        s += '    irq@%d{\n' % channel
                        s += '      device_type = "soclib:periph:irq";\n'
                        s += '      output_line = <%d>;\n' % channel
                        s += '      irq = <&{/%s}  %d>;\n' % (name, hwi_id)
                        s += '      parent = <&{/%s@0x%x}>;\n' % (periph.pseg.name, periph.pseg.base)
                        s += '    };\n'

                    s += '  };\n'

                # research MMC component
                elif ( periph.ptype == 'MMC' ):

                    # get irq line index associated to MMC in XCU
                    irq_in = 0xFFFFFFFF
                    for irq in xcu.irqs:
                        if ( irq.isrtype == 'ISR_MMC' ): irq_in = irq.srcid

                    if ( irq_in == 0xFFFFFFFF ):
                        print '[genmap error] in netbsd.dts()'
                        print '    ISR_MMC not found'
                        sys.exit(1)

                    s += '  %s@0x%x {\n'  % (periph.pseg.name, periph.pseg.base)
                    s += '    device_type = "soclib:mmc";\n'
                    s += '    irq = <&{/%s} %d>;\n' % (irq_ctrl_name, irq_in)
                    s += '    reg = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)
                    s += '  };\n'

                # research FBF component
                elif ( periph.ptype == 'FBF' ):

                    s += '  %s@0x%x {\n' % (periph.pseg.name, periph.pseg.base)
                    s += '    device_type = "soclib:framebuffer";\n'
                    s += '    mode        = <32>;\n'            # bits par pixel
                    s += '    width       = <%d>;\n'    % periph.arg0
                    s += '    height      = <%d>;\n'    % periph.arg1
                    s += '    reg         = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)
                    s += '  };\n'

                # research IOC component
                elif ( periph.ptype == 'IOC' ):

                    if   ( periph.subtype == 'BDV' ):

                        # get irq line index associated to bdv
                        irq_in = 0xFFFFFFFF
                        for irq in irq_ctrl.irqs:
                            if ( irq.isrtype == 'ISR_BDV' ): irq_in = irq.srcid
                        if ( irq_in == 0xFFFFFFFF ):
                            print '[genmap error] in netbsd.dts()'
                            print '    ISR_BDV not found'
                            sys.exit(1)

                        s += '  %s@0x%x {\n' % (periph.pseg.name, periph.pseg.base)
                        s += '    device_type = "soclib:blockdevice";\n'
                        s += '    irq = <&{/%s} %d>;\n' % (irq_ctrl_name, irq_in)
                        s += '    reg = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)
                        s += '  };\n'

                    elif ( periph.subtype == 'HBA' ):

                        print '[genmap error] in netbsd_dts()'
                        print '    HBA peripheral not supported by NetBSD'

                    elif ( periph.subtype == 'SDC' ):

                        # get irq line index associated to sdc
                        irq_in = 0xFFFFFFFF
                        for irq in irq_ctrl.irqs:
                            if ( irq.isrtype == 'ISR_SDC' ): irq_in = irq.srcid
                        if ( irq_in == 0xFFFFFFFF ):
                            print '[genmap error] in netbsd.dts()'
                            print '    ISR_SDC not found'
                            sys.exit(1)

                        s += '  %s@0x%x {\n'  % (periph.pseg.name, periph.pseg.base)
                        s += '    device_type = "soclib:sdc";\n'
                        s += '    irq = <&{/%s} %d>;\n' % (irq_ctrl_name, irq_in)
                        s += '    reg = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)
                        s += '  };\n'

                # research ROM component
                elif ( periph.ptype == 'ROM' ) or ( periph.ptype == 'DROM' ):

                    s += '  %s@0x%x {\n' % (periph.pseg.name, periph.pseg.base)
                    s += '    device_type = "rom";\n'
                    s += '    cached = <1>;\n'
                    s += '    reg = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)
                    s += '  };\n'

                # research SIM component
                elif ( periph.ptype == 'SIM' ):

                    s += '  %s@0x%x {\n'  % (periph.pseg.name, periph.pseg.base)
                    s += '    device_type = "soclib:simhelper";\n'
                    s += '    reg         = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)
                    s += '  };\n'

                # research TTY component
                elif ( periph.ptype == 'TTY' ):

                    s += '  %s@0x%x {\n' % (periph.pseg.name, periph.pseg.base)
                    s += '    device_type = "soclib:tty";\n'
                    s += '    channel_count = < %d >;\n' % periph.channels
                    s += '    reg = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)

                    # multi-channels : get HWI index (to XCU or PIC) for each channel
                    for channel in xrange( periph.channels ):
                        hwi_id = 0xFFFFFFFF
                        for irq in irq_ctrl.irqs:
                            if ( (irq.isrtype == 'ISR_TTY_RX') and (irq.channel == channel) ):
                                hwi_id = irq.srcid
                        if ( hwi_id == 0xFFFFFFFF ):
                            print '[genmap error] in netbsd.dts()'
                            print '    ISR_TTY_RX channel %d not found' % channel
                            sys.exit(1)

                        name = '%s' % (irq_ctrl_name)
                        s += '    irq@%d{\n' % channel
                        s += '      device_type = "soclib:periph:irq";\n'
                        s += '      output_line = <%d>;\n' % channel
                        s += '      irq = <&{/%s}  %d>;\n' % (name, hwi_id)
                        s += '      parent = <&{/%s@0x%x}>;\n' % (periph.pseg.name, periph.pseg.base)
                        s += '    };\n'

                    s += '  };\n'

                # research IOB component
                elif ( periph.ptype == 'IOB' ):

                    s += '  %s@0x%x {\n'  % (periph.pseg.name, periph.pseg.base)
                    s += '    device_type = "soclib:iob";\n'
                    s += '    reg         = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)
                    s += '  };\n'

                # research NIC component
                elif ( periph.ptype == 'NIC' ):

                    s += '  %s@0x%x {\n'  % (periph.pseg.name, periph.pseg.base)
                    s += '    device_type   = "soclib:nic";\n'
                    s += '    reg           = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)
                    s += '    channel_count = < %d >;\n' % periph.channels

                    # multi-channels : get HWI index (to XCU or PIC) for RX & TX IRQs
                    # RX IRQ : (2*channel) / TX IRQs : (2*channel + 1)
                    for channel in xrange( periph.channels ):
                        hwi_id = 0xFFFFFFFF
                        for irq in irq_ctrl.irqs:
                            if ( (irq.isrtype == 'ISR_NIC_RX') and (irq.channel == channel) ):
                                hwi_id = irq.srcid
                        if ( hwi_id == 0xFFFFFFFF ):
                            print '[genmap error] in netbsd.dts()'
                            print '    ISR_NIC_RX channel %d not found' % channel
                            sys.exit(1)

                        name = '%s' % (irq_ctrl_name)
                        s += '    irq_rx@%d{\n' % channel
                        s += '      device_type = "soclib:periph:irq";\n'
                        s += '      output_line = <%d>;\n' % (2*channel)
                        s += '      irq         = <&{/%s}  %d>;\n' % (name, hwi_id)
                        s += '      parent      = <&{/%s@0x%x}>;\n' % (periph.pseg.name, periph.pseg.base)
                        s += '    };\n'

                        hwi_id = 0xFFFFFFFF
                        for irq in irq_ctrl.irqs:
                            if ( (irq.isrtype == 'ISR_NIC_TX') and (irq.channel == channel) ):
                                hwi_id = irq.srcid
                        if ( hwi_id == 0xFFFFFFFF ):
                            print '[genmap error] in netbsd.dts()'
                            print '    ISR_NIC_TX channel %d not found' % channel
                            sys.exit(1)

                        name = '%s' % (irq_ctrl_name)
                        s += '    irq_tx@%d{\n' % channel
                        s += '      device_type = "soclib:periph:irq";\n'
                        s += '      output_line = <%d>;\n' % (2*channel + 1)
                        s += '      irq         = <&{/%s}  %d>;\n' % (name, hwi_id)
                        s += '      parent      = <&{/%s@0x%x}>;\n' % (periph.pseg.name, periph.pseg.base)
                        s += '    };\n'

                    s += '  };\n'

                # research CMA component
                elif ( periph.ptype == 'CMA' ):

                    s += '  %s@0x%x {\n'  % (periph.pseg.name, periph.pseg.base)
                    s += '    device_type   = "soclib:cma";\n'
                    s += '    reg           = <0x%x  0x%x  0x%x>;\n' % (msb, lsb, size)
                    s += '    channel_count = < %d >;\n' % periph.channels

                    # multi-channels : get HWI index (to XCU or PIC) for each channel
                    for channel in xrange( periph.channels ):
                        hwi_id = 0xFFFFFFFF
                        for irq in irq_ctrl.irqs:
                            if ( (irq.isrtype == 'ISR_CMA') and (irq.channel == channel) ):
                                hwi_id = irq.srcid

                        if ( hwi_id == 0xFFFFFFFF ):
                            print '[genmap error] in netbsd.dts()'
                            print '    ISR_CMA channel %d not found' % channel
                            sys.exit(1)

                        name = '%s' % (irq_ctrl_name)
                        s += '    irq@%d{\n' % channel
                        s += '      device_type = "soclib:periph:irq";\n'
                        s += '      output_line = <%d>;\n' % channel
                        s += '      irq = <&{/%s}  %d>;\n' % (name, hwi_id)
                        s += '      parent = <&{/%s@0x%x}>;\n' % (periph.pseg.name, periph.pseg.base)
                        s += '    };\n'

                    s += '  };\n'

                else:

                    print '[genmap error] in netbsd_dts()'
                    print '    %s peripheral not supported by NetBSD' % periph.ptype


        # topology
        s += '\n'
        s += '  topology {\n'
        s += '    #address-cells = <2>;\n'
        s += '    #size-cells = <0>;\n'
        for cluster in self.clusters:
            s += '    cluster@%d,%d {\n' % (cluster.x, cluster.y)
            s += '      reg     = <%d %d>;\n' % (cluster.x, cluster.y)
            s += '      devices = <\n'

            offset = ((cluster.x << self.y_width) + cluster.y) << self.p_width
            for proc in cluster.procs:
                s += '                &{/cpus/Mips,32@0x%x}\n' % (offset + proc.lpid)
            for periph in cluster.periphs:
                s += '                &{/%s@0x%x}\n' % (periph.pseg.name, periph.pseg.base)
            for pseg in cluster.psegs:
                if ( pseg.segtype == 'RAM' ):
                    s += '                &{/%s@0x%x}\n' % (pseg.name, pseg.base)

            s += '                >;\n'
            s += '    };\n'
        s += '  };\n'
        s += '};\n'

        return s
        # end netbsd_dts()

    ######################################################################
    def almos_arch( self ):    # compute string for arch.info file 
                               # used for almos configuration
        # header
        s =  '# arch.info file generated by genmap for %s\n' % self.name
        s += '\n'
        s += '[HEADER]\n'
        s += '        REVISION=1\n'
        s += '        ARCH=%s\n'            % "SOCLIB-TSAR"
        s += '        XMAX=%d\n'            % self.x_size
        s += '        YMAX=%d\n'            % self.y_size
        s += '        CPU_NR=%d\n'          % self.nprocs
        s += '        BSCPU=%d\n'           % 0
        s += '        BSCPU_ARCH_ID=%d\n'   % 0
        s += '        BSTTY=%s\n'           % "0xF4000000"
        s += '        BSDMA=%s\n'           % "0xF8000000"
        s += '        IOPIC_CLUSTER=%d\n'   % 1
        s += '\n'

        # clusters
        cluster_id = 0
        for cluster in self.clusters:

            ram = None
            nb_cpus = len( cluster.procs )
            nb_devs = len( cluster.periphs )

            # search a RAM
            for pseg in cluster.psegs:
                if ( pseg.segtype == 'RAM' ):
                    ram     = pseg
                    #nb_devs += 1

            # search XCU to get IRQs indexes if cluster contains peripherals
            if ( len( cluster.periphs ) != 0 ):
                tty_irq_id = 16
                bdv_irq_id = 8
                dma_irq_id = 8

                for periph in cluster.periphs:
                    if ( periph.ptype == 'XCU' ):
                        # scan irqs
                        for irq in periph.irqs:
                            if (irq.isrtype=='ISR_TTY_RX'): tty_irq_id = irq.srcid
                            if (irq.isrtype=='ISR_BDV'   ): bdv_irq_id = irq.srcid
                            if (irq.isrtype=='ISR_DMA'   ): dma_irq_id = irq.srcid

            # Build the cluster description
            s += '[CLUSTER]\n'
            s += '         CID=%d\n'        % cluster_id
            s += '         ARCH_CID=0x%x\n' % ((cluster.x<<self.y_width)+cluster.y)
            s += '         CPU_NR=%d\n'     % nb_cpus
            s += '         DEV_NR=%d\n'     % nb_devs


            # Handling RAM when cluster contain a RAM
            if (ram != None ):
                base  = ram.base
                size  = ram.size
                irqid = -1
                s += '         DEVID=RAM'
                s += '  BASE=0x%x  SIZE=0x%x  IRQ=-1\n' % (base,size)

            # Handling peripherals
            for periph in cluster.periphs:
                base  = periph.pseg.base
                size  = periph.pseg.size

                if   ( periph.ptype == 'XCU' ):

                    s += '         DEVID=XICU'
                    s += '  BASE=0x%x  SIZE=0x%x  IRQ=-1\n' %(base,size)

                elif ( (periph.ptype == 'TTY')
                       and (tty_irq_id != None) ):

                    s += '         DEVID=TTY'
                    s += '  BASE=0x%x  SIZE=0x%x  IRQ=%d\n' %(base,size,tty_irq_id)

                elif ( (periph.ptype == 'DMA')
                       and (dma_irq_id != None) ):

                    s += '         DEVID=DMA'
                    s += '  BASE=0x%x  SIZE=0x%x  IRQ=%d\n' %(base,size,dma_irq_id)

                elif ( periph.ptype == 'FBF' ):

                    s += '         DEVID=FB'
                    s += '  BASE=0x%x  SIZE=0x%x  IRQ=-1\n' %(base,size )

                elif ( (periph.ptype == 'IOC') and (periph.subtype == 'BDV')
                       and (bdv_irq_id != None) ):

                    s += '         DEVID=BLKDEV'
                    s += '  BASE=0x%x  SIZE=0x%x  IRQ=%d\n' %(base,size,bdv_irq_id)

                elif ( periph.ptype == 'PIC' ):

                    s += '         DEVID=IOPIC'
                    s += '  BASE=0x%x  SIZE=0x%x  IRQ=-1\n' %(base,size)

                else:
                    print '# Warning from almos_archinfo() in cluster[%d,%d]' \
                          % (cluster.x, cluster.y)
                    print '# peripheral type %s/%s not supported yet\n' \
                          % ( periph.ptype, periph.subtype )

            cluster_id += 1

        return s

    # end of almos_archinfo()








###################################################################################
class Cluster ( object ):
###################################################################################
    def __init__( self,
                  x,
                  y ):

        self.index       = 0           # global index (set by Mapping constructor)
        self.x           = x           # x coordinate
        self.y           = y           # y coordinate
        self.psegs       = []          # filled by addRam() or addPeriph()
        self.procs       = []          # filled by addProc()
        self.periphs     = []          # filled by addPeriph()

        return

    ################
    def xml( self ):  # xml for a cluster

        s = '        <cluster x="%d" y="%d" >\n' % (self.x, self.y)
        for pseg in self.psegs:   s += pseg.xml()
        for proc in self.procs:   s += proc.xml()
        for peri in self.periphs: s += peri.xml()
        s += '        </cluster>\n'

        return s

    #############################################
    def cbin( self, mapping, verbose, expected ):  # C binary structure for Cluster

        if ( verbose ):
            print '*** cbin for cluster [%d,%d]' % (self.x, self.y)

        # check index
        if (self.index != expected):
            print '[genmap error] in Cluster.cbin()'
            print '    cluster global index = %d / expected = %d' \
                       % (self.index,expected)
            sys.exit(1)

        # compute global index for first pseg
        if ( len(self.psegs) > 0 ):
            pseg_id = self.psegs[0].index
        else:
            pseg_id = 0

        # compute global index for first proc
        if ( len(self.procs) > 0 ):
            proc_id = self.procs[0].index
        else:
            proc_id = 0

        # compute global index for first periph
        if ( len(self.periphs) > 0 ):
            periph_id = self.periphs[0].index
        else:
            periph_id = 0

        byte_stream = bytearray()
        byte_stream += mapping.int2bytes(4,self.x)            # x coordinate
        byte_stream += mapping.int2bytes(4,self.y)            # x coordinate
        byte_stream += mapping.int2bytes(4,len(self.psegs))   # psegs in cluster
        byte_stream += mapping.int2bytes(4,pseg_id )          # global index
        byte_stream += mapping.int2bytes(4,len(self.procs))   # procs in cluster
        byte_stream += mapping.int2bytes(4,proc_id )          # global index
        byte_stream += mapping.int2bytes(4,len(self.periphs)) # periphs in cluster
        byte_stream += mapping.int2bytes(4, periph_id )       # global index

        if ( verbose ):
            print 'nb_psegs   = %d' %  len( self.psegs )
            print 'pseg_id    = %d' %  pseg_id
            print 'nb_procs   = %d' %  len( self.procs )
            print 'proc_id    = %d' %  proc_id
            print 'nb_periphs = %d' %  len( self.periphs )
            print 'periph_id  = %d' %  periph_id

        return byte_stream

##################################################################################
class Vspace( object ):
##################################################################################
    def __init__( self,
                  name,
                  startname,
                  active ):

        self.index     = 0              # global index ( set by addVspace() )
        self.name      = name           # vspace name
        self.startname = startname      # name of vseg containing the start_vector
        self.active    = active         # active at boot if true
        self.vsegs     = []
        self.tasks     = []

        return

    ################
    def xml( self ):   # xml for one vspace

        s =  '        <vspace name="%s" startname="%s" active="%d" >\n' \
                            %(self.name , self.startname , self.active)
        for vseg in self.vsegs: s += vseg.xml()
        for task in self.tasks: s += task.xml()
        s += '        </vspace>\n'

        return s

    #############################################
    def cbin( self, mapping, verbose, expected ):   # C binary for Vspace

        if ( verbose ):
            print '*** cbin for vspace %s' % (self.name)

        # check index
        if (self.index != expected):
            print '[genmap error] in Vspace.cbin()'
            print '    vspace global index = %d / expected = %d' \
                        %(self.index,expected)
            sys.exit(1)

        # compute global index for vseg containing start_vector
        vseg_start_id = 0xFFFFFFFF
        for vseg in self.vsegs:
            if ( vseg.name == self.startname ): vseg_start_id = vseg.index

        if ( vseg_start_id == 0xFFFFFFFF ):
            print '[genmap error] in Vspace.cbin()'
            print '    startname %s not found for vspace %s' \
                        %(self.startname,self.name)
            sys.exit(1)

        # compute first vseg and first task global index
        first_vseg_id = self.vsegs[0].index
        first_task_id = self.tasks[0].index

        # compute number of tasks and number of vsegs
        nb_vsegs = len( self.vsegs )
        nb_tasks = len( self.tasks )

        byte_stream = bytearray()
        byte_stream += mapping.str2bytes(32,self.name)         # vspace name
        byte_stream += mapping.int2bytes(4, vseg_start_id)     # vseg start_vector
        byte_stream += mapping.int2bytes(4, nb_vsegs)          # number of vsegs
        byte_stream += mapping.int2bytes(4, nb_tasks)          # number of tasks
        byte_stream += mapping.int2bytes(4, first_vseg_id)     # global index
        byte_stream += mapping.int2bytes(4, first_task_id)     # global index
        byte_stream += mapping.int2bytes(4, self.active)       # always active if non zero

        if ( verbose ):
            print 'start_id   = %d' %  vseg_start_id
            print 'nb_vsegs   = %d' %  nb_vsegs
            print 'nb_tasks   = %d' %  nb_tasks
            print 'vseg_id    = %d' %  first_vseg_id
            print 'task_id    = %d' %  first_task_id
            print 'active     = %d' %  self.active

        return byte_stream

##################################################################################
class Task( object ):
##################################################################################
    def __init__( self,
                  name,
                  trdid,
                  x,
                  y,
                  p,
                  stackname,
                  heapname,
                  startid ):

        self.index     = 0             # global index value set by addTask()
        self.name      = name          # tsk name
        self.trdid     = trdid         # task index (unique in vspace)
        self.x         = x             # cluster x coordinate
        self.y         = y             # cluster y coordinate
        self.p         = p             # processor local index
        self.stackname = stackname     # name of vseg containing the stack
        self.heapname  = heapname      # name of vseg containing the heap
        self.startid   = startid       # index in start_vector
        return

    ######################################
    def xml( self ):    # xml for one task

        s =  '            <task name="%s"' % self.name
        s += ' trdid="%d"'                 % self.trdid
        s += ' x="%d"'                     % self.x
        s += ' y="%d"'                     % self.y
        s += ' p="%d"'                     % self.p
        s += '\n                 '
        s += ' stackname="%s"'             % self.stackname
        s += ' heapname="%s"'              % self.heapname
        s += ' startid="%d"'               % self.startid
        s += ' />\n'

        return s

    ##########################################################################
    def cbin( self, mapping, verbose, expected, vspace ):  # C binary for Task

        if ( verbose ):
            print '*** cbin for task %s in vspace %s' \
                     % (self.name, vspace.name)

        # check index
        if (self.index != expected):
            print '[genmap error] in Task.cbin()'
            print '    task global index = %d / expected = %d' \
                        %(self.index,expected)
            sys.exit(1)

        # compute cluster global index
        cluster_id = (self.x * mapping.y_size) + self.y

        # compute vseg index for stack
        vseg_stack_id = 0xFFFFFFFF
        for vseg in vspace.vsegs:
            if ( vseg.name == self.stackname ): vseg_stack_id = vseg.index

        if ( vseg_stack_id == 0xFFFFFFFF ):
            print '[genmap error] in Task.cbin()'
            print '    stackname %s not found for task %s in vspace %s' \
                  % ( self.stackname, self.name, vspace.name )
            sys.exit(1)

        # compute vseg index for heap
        if ( self.heapname == '' ):
            vseg_heap_id = 0
        else:
            vseg_heap_id = 0xFFFFFFFF
            for vseg in vspace.vsegs:
                if ( vseg.name == self.heapname ): vseg_heap_id = vseg.index

            if ( vseg_heap_id == 0xFFFFFFFF ):
                print '[genmap error] in Task.cbin()'
                print '    heapname %s not found for task %s in vspace %s' \
                      % ( self.heapname, self.name, vspace.name )
                sys.exit(1)

        byte_stream = bytearray()
        byte_stream += mapping.str2bytes(32,self.name)     # task name in vspace
        byte_stream += mapping.int2bytes(4, cluster_id)    # cluster global index
        byte_stream += mapping.int2bytes(4, self.p)        # processor local index 
        byte_stream += mapping.int2bytes(4, self.trdid)    # thread index in vspace
        byte_stream += mapping.int2bytes(4, vseg_stack_id) # stack vseg local index
        byte_stream += mapping.int2bytes(4, vseg_heap_id)  # heap vseg local index
        byte_stream += mapping.int2bytes(4, self.startid)  # index in start vector
        byte_stream += mapping.int2bytes(4 ,0)             # ltid (dynamically computed)

        if ( verbose ):
            print 'clusterid  = %d' %  cluster_id
            print 'lpid       = %d' %  self.p
            print 'trdid      = %d' %  self.trdid
            print 'stackid    = %d' %  vseg_stack_id
            print 'heapid     = %d' %  vseg_heap_id
            print 'startid    = %d' %  self.startid

        return byte_stream

##################################################################################
class Vseg( object ):
##################################################################################
    def __init__( self,
                  name,
                  vbase,
                  length,
                  mode,
                  vtype,
                  x,
                  y,
                  pseg,
                  identity = False,
                  local    = False,
                  big      = False,
                  binpath  = '' ):

        assert (vbase & 0xFFFFFFFF) == vbase

        assert (length & 0xFFFFFFFF) == length

        assert mode in VSEGMODES

        assert vtype in VSEGTYPES

        assert (vtype != 'ELF') or (binpath != '')

        self.index    = 0                   # global index ( set by addVseg() )
        self.name     = name                # vseg name (unique in vspace)
        self.vbase    = vbase               # virtual base address in vspace
        self.length   = length              # vseg length (bytes)
        self.vtype    = vtype               # vseg type (defined in VSEGTYPES)
        self.mode     = mode                # CXWU access rights
        self.x        = x                   # x coordinate of destination cluster
        self.y        = y                   # y coordinate of destination cluster
        self.psegname = pseg                # name of pseg in destination cluster
        self.identity = identity            # identity mapping required
        self.local    = local               # only mapped in local PTAB when true
        self.big      = big                 # to be mapped in a big physical page
        self.binpath  = binpath             # pathname for binary file (ELF or BLOB)

        return

    ##################################
    def xml( self ):  # xml for a vseg

        s =  '            <vseg name="%s"' %(self.name)
        s += ' vbase="0x%x"'               %(self.vbase)
        s += ' length="0x%x"'              %(self.length)
        s += ' type="%s"'                  %(self.vtype)
        s += ' mode="%s"'                  %(self.mode)
        s += '\n                 '
        s += ' x="%d"'                     %(self.x)
        s += ' y="%d"'                     %(self.y)
        s += ' psegname="%s"'              %(self.psegname)
        if ( self.identity ):       s += ' ident="1"'
        if ( self.local ):          s += ' local="1"'
        if ( self.big ):            s += ' big="1"'
        if ( self.binpath != '' ):  s += ' binpath="%s"' %(self.binpath)
        s += ' />\n'

        return s

    #####################################################################
    def cbin( self, mapping, verbose, expected ):    # C binary for Vseg

        if ( verbose ):
            print '*** cbin for vseg[%d] %s' % (self.index, self.name)

        # check index
        if (self.index != expected):
            print '[genmap error] in Vseg.cbin()'
            print '    vseg global index = %d / expected = %d' \
                  % (self.index, expected )
            sys.exit(1)

        # compute pseg_id
        pseg_id = 0xFFFFFFFF
        cluster_id = (self.x * mapping.y_size) + self.y
        cluster = mapping.clusters[cluster_id]
        for pseg in cluster.psegs:
            if (self.psegname == pseg.name):
                pseg_id = pseg.index
        if (pseg_id == 0xFFFFFFFF):
            print '[genmap error] in Vseg.cbin() : '
            print '    psegname %s not found for vseg %s in cluster %d' \
                  % ( self.psegname, self.name, cluster_id )
            sys.exit(1)

        # compute numerical value for mode
        mode_id = 0xFFFFFFFF
        for x in xrange( len(VSEGMODES) ):
            if ( self.mode == VSEGMODES[x] ):
                mode_id = x
        if ( mode_id == 0xFFFFFFFF ):
            print '[genmap error] in Vseg.cbin() : '
            print '    undefined vseg mode %s' % self.mode
            sys.exit(1)

        # compute numerical value for vtype
        vtype_id = 0xFFFFFFFF
        for x in xrange( len(VSEGTYPES) ):
            if ( self.vtype == VSEGTYPES[x] ):
                vtype_id = x
        if ( vtype_id == 0xFFFFFFFF ):
            print '[genmap error] in Vseg.cbin()'
            print '    undefined vseg type %s' % self.vtype
            sys.exit(1)

        byte_stream = bytearray()
        byte_stream += mapping.str2bytes(32,self.name )     # vseg name
        byte_stream += mapping.str2bytes(64,self.binpath )  # binpath
        byte_stream += mapping.int2bytes(4, self.vbase )    # virtual base address
        byte_stream += mapping.int2bytes(8, 0 )             # physical base address
        byte_stream += mapping.int2bytes(4, self.length )   # vseg size (bytes)
        byte_stream += mapping.int2bytes(4, pseg_id )       # pseg global index
        byte_stream += mapping.int2bytes(4, mode_id )       # CXWU flags
        byte_stream += mapping.int2bytes(4, vtype_id )      # vseg type
        byte_stream += mapping.int2bytes(1, 0 )             # mapped when non zero
        byte_stream += mapping.int2bytes(1, self.identity ) # identity mapping 
        byte_stream += mapping.int2bytes(1, self.local )    # only in local PTAB 
        byte_stream += mapping.int2bytes(1,  self.big )     # to be mapped in BPP 

        if ( verbose ):
            print 'binpath    = %s' %  self.binpath
            print 'vbase      = %x' %  self.vbase
            print 'pbase      = 0'
            print 'length     = %x' %  self.length
            print 'pseg_id    = %d' %  pseg_id
            print 'mode       = %d' %  mode_id
            print 'type       = %d' %  vtype_id
            print 'mapped     = 0'
            print 'ident      = %d' %  self.identity
            print 'local      = %d' %  self.local
            print 'big        = %d' %  self.big

        return byte_stream

##################################################################################
class Processor ( object ):
##################################################################################
    def __init__( self,
                  x,
                  y,
                  lpid ):

        self.index    = 0      # global index ( set by addProc() )
        self.x        = x      # x cluster coordinate
        self.y        = y      # y cluster coordinate
        self.lpid     = lpid   # processor local index

        return

    ########################################
    def xml( self ):   # xml for a processor
        return '            <proc index="%d" />\n' % (self.lpid)

    ####################################################################
    def cbin( self, mapping, verbose, expected ):    # C binary for Proc

        if ( verbose ):
            print '*** cbin for proc %d in cluster (%d,%d)' \
                  % (self.lpid, self.x, self.y)

        # check index
        if (self.index != expected):
            print '[genmap error] in Proc.cbin()'
            print '    proc global index = %d / expected = %d' \
                       % (self.index,expected)
            sys.exit(1)

        byte_stream = bytearray()
        byte_stream += mapping.int2bytes( 4 , self.lpid )       # local index

        return byte_stream

##################################################################################
class Pseg ( object ):
##################################################################################
    def __init__( self,
                  name,
                  base,
                  size,
                  x,
                  y,
                  segtype ):

        assert( segtype in PSEGTYPES )

        self.index    = 0       # global index ( set by addPseg() )
        self.name     = name    # pseg name (unique in cluster)
        self.base     = base    # physical base address
        self.size     = size    # segment size (bytes)
        self.x        = x       # cluster x coordinate
        self.y        = y       # cluster y coordinate
        self.segtype  = segtype # RAM / PERI (defined in mapping_info.h)

        return

    ###################################
    def xml( self ):   # xml for a pseg

        s = '            <pseg name="%s" type="%s" base="0x%x" length="0x%x" />\n' \
                         % (self.name, self.segtype, self.base, self.size)
        return s

    ###########################################################################
    def cbin( self, mapping, verbose, expected, cluster ):  # C binary for Pseg

        if ( verbose ):
            print '*** cbin for pseg[%d] %s in cluster[%d,%d]' \
                  % (self.index, self.name, cluster.x, cluster.y)

        # check index
        if (self.index != expected):
            print '[genmap error] in Pseg.cbin()'
            print '    pseg global index = %d / expected = %d' \
                       % (self.index,expected)
            sys.exit(1)

        # compute numerical value for segtype
        segtype_int = 0xFFFFFFFF
        for x in xrange( len(PSEGTYPES) ):
            if ( self.segtype == PSEGTYPES[x] ): segtype_int = x

        if ( segtype_int == 0xFFFFFFFF ):
            print '[genmap error] in Pseg.cbin()'
            print '    undefined segment type %s' % self.segtype
            sys.exit(1)

        byte_stream = bytearray()
        byte_stream += mapping.str2bytes(32,self.name)     # pseg name
        byte_stream += mapping.int2bytes(8 ,self.base)     # physical base address
        byte_stream += mapping.int2bytes(8 ,self.size)     # segment length
        byte_stream += mapping.int2bytes(4 ,segtype_int)   # segment type
        byte_stream += mapping.int2bytes(4 ,cluster.index) # cluster global index
        byte_stream += mapping.int2bytes(4 ,0)             # linked list of vsegs

        if ( verbose ):
            print 'pbase      = %x' %  self.base
            print 'size       = %x' %  self.size
            print 'type       = %s' %  self.segtype

        return byte_stream

##################################################################################
class Periph ( object ):
##################################################################################
    def __init__( self,
                  pseg,               # associated pseg
                  ptype,              # peripheral type
                  subtype  = 'NONE',  # peripheral subtype
                  channels = 1,       # for multi-channels peripherals
                  arg0     = 0,       # optional (semantic depends on ptype)
                  arg1     = 0,       # optional (semantic depends on ptype)
                  arg2     = 0,       # optional (semantic depends on ptype)
                  arg3     = 0 ):     # optional (semantic depends on ptype)

        self.index    = 0            # global index ( set by addPeriph() )
        self.channels = channels
        self.ptype    = ptype
        self.subtype  = subtype
        self.arg0     = arg0
        self.arg1     = arg1
        self.arg2     = arg2
        self.arg3     = arg3
        self.pseg     = pseg
        self.irqs     = []
        self.irq_ctrl = None         # interrupt controller peripheral
        return

    ######################################
    def xml( self ):    # xml for a periph

        s =  '            <periph type="%s"' % self.ptype
        s += ' subtype="%s"'                 % self.subtype
        s += ' psegname="%s"'                % self.pseg.name
        s += ' channels="%d"'                % self.channels
        s += ' arg0="%d"'                    % self.arg0
        s += ' arg1="%d"'                    % self.arg1
        s += ' arg2="%d"'                    % self.arg2
        s += ' arg3="%d"'                    % self.arg3
        if ( (self.ptype == 'PIC') or (self.ptype == 'XCU') ):
            s += ' >\n'
            for irq in self.irqs: s += irq.xml()
            s += '            </periph>\n'
        else:
            s += ' />\n'
        return s

    ######################################################################
    def cbin( self, mapping, verbose, expected ):    # C binary for Periph

        if ( verbose ):
            print '*** cbin for periph %s in cluster [%d,%d]' \
                  % (self.ptype, self.pseg.x, self.pseg.y)

        # check index
        if (self.index != expected):
            print '[genmap error] in Periph.cbin()'
            print '    periph global index = %d / expected = %d' \
                       % (self.index,expected)
            sys.exit(1)

        # compute pseg global index
        pseg_id = self.pseg.index

        # compute first irq global index
        if ( len(self.irqs) > 0 ):
            irq_id = self.irqs[0].index
        else:
            irq_id = 0

        # compute numerical value for ptype
        ptype_id = 0xFFFFFFFF
        for x in xrange( len(PERIPHTYPES) ):
            if ( self.ptype == PERIPHTYPES[x] ):  ptype_id = x

        if ( ptype_id == 0xFFFFFFFF ):
            print '[genmap error] in Periph.cbin()'
            print '    undefined peripheral type %s' % self.ptype
            sys.exit(1)

        # compute numerical value for subtype
        subtype_id = 0xFFFFFFFF
        if (self.ptype == 'IOC'):
            for x in xrange( len(IOCSUBTYPES) ):
                if ( self.subtype == IOCSUBTYPES[x] ):  subtype_id = x
        if (self.ptype == 'MWR'):
            for x in xrange( len(MWRSUBTYPES) ):
                if ( self.subtype == MWRSUBTYPES[x] ):  subtype_id = x
        
        byte_stream = bytearray()
        byte_stream += mapping.int2bytes(4,ptype_id)       # peripheral type
        byte_stream += mapping.int2bytes(4,subtype_id)     # peripheral subtype
        byte_stream += mapping.int2bytes(4,pseg_id)        # pseg global index
        byte_stream += mapping.int2bytes(4,self.channels)  # number of channels
        byte_stream += mapping.int2bytes(4,self.arg0)      # optionnal arg0
        byte_stream += mapping.int2bytes(4,self.arg1)      # optionnal arg1
        byte_stream += mapping.int2bytes(4,self.arg2)      # optionnal arg2
        byte_stream += mapping.int2bytes(4,self.arg3)      # optionnal arg3
        byte_stream += mapping.int2bytes(4,len(self.irqs)) # number of input irqs
        byte_stream += mapping.int2bytes( 4 , irq_id )     # global index

        if ( verbose ):
            print 'ptype      = %d' %  ptype_id
            print 'subtype    = %d' %  subtype_id
            print 'pseg_id    = %d' %  pseg_id
            print 'nb_irqs    = %d' %  len( self.irqs )
            print 'irq_id     = %d' %  irq_id
        return byte_stream

##################################################################################
class Irq ( object ):
##################################################################################
    def __init__( self,
                  irqtype,         # input IRQ type : HWI / WTI / PTI (for XCU only)
                  srcid,           # input IRQ index (for XCU or PIC)
                  isrtype,         # Type of ISR to be executed
                  channel = 0 ):   # channel index for multi-channel ISR

        assert irqtype in IRQTYPES
        assert isrtype in ISRTYPES
        assert srcid < 32

        self.index   = 0        # global index ( set by addIrq() )
        self.irqtype = irqtype  # IRQ type
        self.srcid   = srcid    # source IRQ index
        self.isrtype = isrtype  # ISR type
        self.channel = channel  # channel index (for multi-channels ISR)
        return

    ################################
    def xml( self ):   # xml for Irq

        s = '                <irq srctype="%s" srcid="%d" isr="%s" channel="%d" />\n' \
                % ( self.irqtype, self.srcid, self.isrtype, self.channel )
        return s

    ####################################################################
    def cbin( self, mapping, verbose, expected ):     # C binary for Irq

        if ( verbose ):
            print '*** cbin for irq[%d]' % (self.index)

        # check index
        if (self.index != expected):
            print '[genmap error] in Irq.cbin()'
            print '    irq global index = %d / expected = %d' \
                       % (self.index,expected)
            sys.exit(1)

        # compute numerical value for irqtype
        irqtype_id = 0xFFFFFFFF
        for x in xrange( len(IRQTYPES) ):
            if ( self.irqtype == IRQTYPES[x] ): irqtype_id = x

        if ( irqtype_id == 0xFFFFFFFF ):
            print '[genmap error] in Irq.cbin()'
            print '    undefined irqtype %s' % self.irqtype
            sys.exit(1)

        # compute numerical value for isrtype
        isrtype_id = 0xFFFFFFFF
        for x in xrange( len(ISRTYPES) ):
            if ( self.isrtype == ISRTYPES[x] ): isrtype_id = x

        if ( isrtype_id == 0xFFFFFFFF ):
            print '[genmap error] in Irq.cbin()' 
            print '    undefined isrtype %s' % self.isrtype
            sys.exit(1)

        byte_stream = bytearray()
        byte_stream += mapping.int2bytes( 4,  irqtype_id )
        byte_stream += mapping.int2bytes( 4,  self.srcid )
        byte_stream += mapping.int2bytes( 4,  isrtype_id )
        byte_stream += mapping.int2bytes( 4,  self.channel )
        byte_stream += mapping.int2bytes( 4,  0 )
        byte_stream += mapping.int2bytes( 4,  0 )

        if ( verbose ):
            print 'irqtype    = %s' %  self.irqtype
            print 'srcid      = %d' %  self.srcid
            print 'isrtype    = %s' %  self.isrtype
            print 'channel    = %d' %  self.channel

        return byte_stream

# Local Variables:
# tab-width: 4;
# c-basic-offset: 4;
# c-file-offsets:((innamespace . 0)(inline-open . 0));
# indent-tabs-mode: nil;
# End:
#
# vim: filetype=python:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

