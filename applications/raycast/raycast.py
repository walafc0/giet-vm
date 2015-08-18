#!/usr/bin/env python

from mapping import *

######################################################################################
#   file   : raycast.py  
#   date   : july 2015
#   author : Alain Greiner
#######################################################################################
#  This file describes the mapping of the multi-threaded "raycast" application 
#  on a multi-clusters, multi-processors architecture.
#  The mapping of tasks on processors is the following:
#    - one "main" task on (0,0,0)
#    - one "render" task per processor but (0,0,0)
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
####################################################################################

######################
def extend( mapping ):

    x_size    = mapping.x_size
    y_size    = mapping.y_size
    nprocs    = mapping.nprocs

    # define vsegs base & size
    code_base  = 0x10000000
    code_size  = 0x00010000     # 64 Kbytes 
    
    data_base  = 0x20000000
    data_size  = 0x00040000     # 256 Kbytes 

    stack_base = 0x40000000 
    stack_size = 0x00200000     # 2 Mbytes 

    heap_base  = 0x60000000 
    heap_size  = 0x00400000     # 4 Mbytes

    # create vspace
    vspace = mapping.addVspace( name = 'raycast', startname = 'raycast_data', active = False )
    
    # data vseg : shared / cluster[0][0]
    mapping.addVseg( vspace, 'raycast_data', data_base , data_size, 
                     'C_WU', vtype = 'ELF', x = 0, y = 0, pseg = 'RAM', 
                     binpath = 'bin/raycast/appli.elf',
                     local = False )

    # code vsegs : local (one copy in each cluster)
    for x in xrange (x_size):
        for y in xrange (y_size):
            cluster_id = (x * y_size) + y
            if ( mapping.clusters[cluster_id].procs ):
                mapping.addVseg( vspace, 'raycast_code_%d_%d' %(x,y), 
                                 code_base , code_size,
                                 'CXWU', vtype = 'ELF', x = x, y = y, pseg = 'RAM', 
                                 binpath = 'bin/raycast/appli.elf',
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
                    mapping.addVseg( vspace, 'raycast_stack_%d_%d_%d' % (x,y,p), 
                                     base, size, 'C_WU', vtype = 'BUFFER', 
                                     x = x , y = y , pseg = 'RAM',
                                     local = True, big = True )   

    # heap vsegs : shared (one per cluster) 
    for x in xrange (x_size):
        for y in xrange (y_size):
            cluster_id = (x * y_size) + y
            if ( mapping.clusters[cluster_id].procs ):
                size  = heap_size
                base  = heap_base + (cluster_id * size)
                mapping.addVseg( vspace, 'raycast_heap_%d_%d' %(x,y), base , size, 
                                 'C_WU', vtype = 'HEAP', x = x, y = y, pseg = 'RAM', 
                                 local = False )

    # distributed tasks / one task per processor
    for x in xrange (x_size):
        for y in xrange (y_size):
            cluster_id = (x * y_size) + y
            if ( mapping.clusters[cluster_id].procs ):
                for p in xrange( nprocs ):
                    trdid = (((x * y_size) + y) * nprocs) + p
                    if  ( x == 0 and y == 0 and p == 0 ):       # main task
                        task_index = 1
                        task_name  = 'main_%d_%d_%d' %(x,y,p)
                    else:                                       # render task
                        task_index = 0
                        task_name  = 'render_%d_%d_%d' % (x,y,p)

                    mapping.addTask( vspace, task_name, trdid, x, y, p,
                                     'raycast_stack_%d_%d_%d' % (x,y,p), 
                                     'raycast_heap_%d_%d' % (x,y),
                                     task_index )

    # extend mapping name
    mapping.name += '_raycast'

    return vspace  # useful for test
            
################################ test ######################################################

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

