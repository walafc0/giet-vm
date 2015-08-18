#!/usr/bin/env python

from mapping import *

###################################################################################
#   file   : classif.py 
#   date   : november 2014
#   author : Alain Greiner
###################################################################################
#  This file describes the mapping of the multi-threaded "classif" 
#  application on a multi-clusters, multi-processors architecture.
#  The mapping of tasks on processors is the following:
#    - one "load" task per cluster containing processors, 
#    - one "store" task per cluster containing processors, 
#    - (nprocs-2) "analyse" task per cluster containing processors.
#  The mapping of virtual segments is the following:
#    - There is one shared data vseg in cluster[0][0]
#    - The code vsegs are replicated on all clusters containing processors.
#    - There is one heap vseg per cluster containing processors.
#    - The stacks vsegs are distibuted on all clusters containing processors.
#  This mapping uses 5 platform parameters, (obtained from the "mapping" argument)
#    - x_size    : number of clusters in a row
#    - y_size    : number of clusters in a column
#    - x_width   : number of bits for x field
#    - y_width   : number of bits for y field
#    - nprocs    : number of processors per cluster
#
#  WARNING: The target architecture cannot contain less
#           than 3 processors per cluster.
##################################################################################

######################
def extend( mapping ):

    x_size    = mapping.x_size
    y_size    = mapping.y_size
    nprocs    = mapping.nprocs
    x_width   = mapping.x_width
    y_width   = mapping.y_width

    assert (nprocs >= 3)

    # define vsegs base & size
    code_base  = 0x10000000
    code_size  = 0x00010000     # 64 Kbytes (replicated in each cluster)
    
    data_base  = 0x20000000
    data_size  = 0x00010000     # 64 Kbytes 

    heap_base  = 0x30000000
    heap_size  = 0x00040000     # 256 Kbytes (per cluster)      

    stack_base = 0x40000000 
    stack_size = 0x00200000     # 2 Mbytes (per cluster)

    # create vspace
    vspace = mapping.addVspace( name = 'classif', startname = 'classif_data' )
    
    # data vseg : shared / cluster[0][0]
    mapping.addVseg( vspace, 'classif_data', data_base , data_size, 
                     'C_WU', vtype = 'ELF', x = 0, y = 0, pseg = 'RAM', 
                     binpath = 'bin/classif/appli.elf',
                     local = False )

    # heap vsegs : shared (one per cluster) 
    for x in xrange (x_size):
        for y in xrange (y_size):
            cluster_id = (x * y_size) + y
            if ( mapping.clusters[cluster_id].procs ):
                size  = heap_size
                base  = heap_base + (cluster_id * size)

                mapping.addVseg( vspace, 'classif_heap_%d_%d' %(x,y), base , size, 
                                 'C_WU', vtype = 'HEAP', x = x, y = y, pseg = 'RAM', 
                                 local = False )

    # code vsegs : local (one copy in each cluster)
    for x in xrange (x_size):
        for y in xrange (y_size):
            cluster_id = (x * y_size) + y
            if ( mapping.clusters[cluster_id].procs ):

                mapping.addVseg( vspace, 'classif_code_%d_%d' %(x,y), 
                                 code_base , code_size,
                                 'CXWU', vtype = 'ELF', x = x, y = y, pseg = 'RAM', 
                                 binpath = 'bin/classif/appli.elf',
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

                    mapping.addVseg( vspace, 'classif_stack_%d_%d_%d' % (x,y,p), 
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
                    if  ( p== 0 ):                              # task load
                        task_index = 2
                        task_name  = 'load_%d_%d_%d' %(x,y,p)            
                    elif  ( p== 1 ):                            # task store
                        task_index = 1
                        task_name  = 'store_%d_%d_%d' %(x,y,p)            
                    else :                                      # task analyse
                        task_index = 0
                        task_name  = 'analyse_%d_%d_%d' % (x,y,p)

                    mapping.addTask( vspace, task_name, trdid, x, y, p,
                                     'classif_stack_%d_%d_%d' % (x,y,p), 
                                     'classif_heap_%d_%d' % (x,y),
                                     task_index )

    # extend mapping name
    mapping.name += '_classif'

    return vspace  # useful for test
            
################################ test ############################################

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

