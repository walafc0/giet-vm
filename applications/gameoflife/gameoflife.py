#!/usr/bin/env python

from mapping import *

##################################################################################
#   file   : gameoflife.py  
#   date   : february 2015
#   author : Alain Greiner
##################################################################################
#  This file describes the mapping of the multi-threaded "gameoflife" 
#  application on a multi-clusters, multi-processors architecture.
#  This include both the mapping of virtual segments on the clusters,
#  and the mapping of tasks on processors.
#  There is one task per processor.
#  The mapping of virtual segments is the following:
#    - There is one shared data vseg in cluster[0][0]
#    - The code vsegs are replicated on all clusters containing processors.
#    - The stack vsegs are distributed on all clusters containing processors.
#    - The heap vsegs are distributed on all clusters containing processors.
#  This mapping uses 5 platform parameters, (obtained from the "mapping" argument)
#    - x_size    : number of clusters in a row
#    - y_size    : number of clusters in a column
#    - x_width   : number of bits coding x coordinate
#    - y_width   : number of bits coding y coordinate
#    - nprocs    : number of processors per cluster
##################################################################################

######################
def extend( mapping ):

    x_size    = mapping.x_size
    y_size    = mapping.y_size
    nprocs    = mapping.nprocs
    x_width   = mapping.x_width
    y_width   = mapping.y_width

    # define vsegs base & size
    code_base  = 0x10000000
    code_size  = 0x00010000     # 64 Kbytes (replicated in each cluster)
    
    data_base  = 0x20000000
    data_size  = 0x00200000     # 2 Mbytes (non replicated)

    heap_base  = 0x30000000
    heap_size  = 0x00040000     # 256 Kbytes (per cluster)      

    stack_base = 0x40000000 
    stack_size = 0x00200000     # 2 Mbytes (per cluster)

    # create vspace
    vspace = mapping.addVspace( name = 'gameoflife', startname = 'gol_data', active = False )
    
    # data vseg : shared (only in cluster[0,0])
    mapping.addVseg( vspace, 'gol_data', data_base , data_size, 
                     'C_WU', vtype = 'ELF', x = 0, y = 0, pseg = 'RAM', 
                     binpath = 'bin/gameoflife/appli.elf',
                     local = False , big = True )

    # heap vsegs : shared (one per cluster) 
    for x in xrange (x_size):
        for y in xrange (y_size):
            cluster_id = (x * y_size) + y
            if ( mapping.clusters[cluster_id].procs ):
                size  = heap_size
                base  = heap_base + (cluster_id * size)

                mapping.addVseg( vspace, 'gol_heap_%d_%d' %(x,y), base , size, 
                                 'C_WU', vtype = 'HEAP', x = x, y = y, pseg = 'RAM', 
                                 local = False )

    # code vsegs : local (one copy in each cluster)
    for x in xrange (x_size):
        for y in xrange (y_size):
            cluster_id = (x * y_size) + y
            if ( mapping.clusters[cluster_id].procs ):

                mapping.addVseg( vspace, 'gol_code_%d_%d' %(x,y), 
                                 code_base , code_size,
                                 'CXWU', vtype = 'ELF', x = x, y = y, pseg = 'RAM', 
                                 binpath = 'bin/gameoflife/appli.elf',
                                 local = True )

    # stacks vsegs: local (one stack per processor => nprocs stacks per cluster)
    for x in xrange (x_size):
        for y in xrange (y_size):
            cluster_id = (x * y_size) + y
            if ( mapping.clusters[cluster_id].procs ):
                for p in xrange( nprocs ):
                    proc_id = (((x * y_size) + y) * nprocs) + p
                    size    = (stack_size / nprocs) & 0xFFFFF000
                    base    = stack_base + (proc_id * size)

                    mapping.addVseg( vspace, 'gol_stack_%d_%d_%d' % (x,y,p), 
                                     base, size, 'C_WU', vtype = 'BUFFER', 
                                     x = x , y = y , pseg = 'RAM',
                                     local = True, big = True )

    # distributed tasks / one task per processor
    for x in xrange (x_size):
        for y in xrange (y_size):
            cluster_id = (x * y_size) + y
            if ( mapping.clusters[cluster_id].procs ):
                for p in xrange( nprocs ):
                    trdid = (((x * y_size) + y) * nprocs) + p

                    mapping.addTask( vspace, 'gol_%d_%d_%d' % (x,y,p),
                                     trdid, x, y, p,
                                     'gol_stack_%d_%d_%d' % (x,y,p),
                                     'gol_heap_%d_%d' %(x,y) , 0 )  

    # extend mapping name
    mapping.name += '_gol'

    return vspace  # useful for test
            
################################ test ##################################################

if __name__ == '__main__':

    vspace = extend( Mapping( 'test', 2, 2, 4 ) )
    print vspace.xml()


# Local Variables:
# tab-width: 4;
# c-basic-offset: 4;
# c-file-offsets:((innamespace . 0)(inline-open . 0));
# indent-tabs-mode: nil;
# End:
#
# vim: filetype=python:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

