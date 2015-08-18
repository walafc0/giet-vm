#!/usr/bin/env python

from mapping import *

######################################################################################
#   file   : convol.py  (for the convol application)
#   date   : may 2014
#   author : Alain Greiner
#######################################################################################
#  This file describes the mapping of the multi-threaded "convol" 
#  application on a multi-clusters, multi-processors architecture.
#  This include both the mapping of virtual segments on the clusters,
#  and the mapping of tasks on processors.
#  This mapping uses 5 platform parameters, (obtained from the "mapping" argument)
#  - x_size    : number of clusters in a row
#  - y_size    : number of clusters in a column
#  - x_width   : number of bits coding x coordinate
#  - y_width   : number of bits coding y coordinate
#  - procs_max : number of processors per cluster
####################################################################################

######################
def convol( mapping ):

    x_size    = mapping.x_size
    y_size    = mapping.y_size
    procs_max = mapping.procs_max
    x_width   = mapping.x_width
    y_width   = mapping.y_width

    # define vsegs base & size
    code_base  = 0x10000000
    code_size  = 0x00010000     # 64 Kbytes
    
    data_base  = 0x20000000
    data_size  = 0x00010000     # 64 Kbytes

    ptab_base  = 0x30000000
    ptab_size  = 0x00040000     # 256 Kbytes

    heap_base  = 0x40000000     
    heap_size  = 0x01000000     # max 16 Mbytes (for all clusters)

    stack_base = 0x50000000     
    stack_size = 0x02000000     # max 32 Mbytes (for all processors)

    # create Vspace
    vspace = mapping.addVspace( name = 'convol', startname = 'conv_data' )
    
    # non replicated vsegs in cluster[0,0]
    mapping.addVseg( vspace, 'conv_code', code_base , code_size, 'CXWU', vtype = 'ELF', 
                     x = 0, y = 0, pseg = 'RAM', binpath = 'build/convol/convol.elf' )

    mapping.addVseg( vspace, 'conv_data', data_base , data_size, 'C_WU', vtype = 'ELF',
                     x = 0, y = 0, pseg = 'RAM', binpath = 'build/convol/convol.elf' )

    mapping.addVseg( vspace, 'conv_ptab', ptab_base , ptab_size, 'C_WU', vtype = 'PTAB',
                     x = 0, y = 0, pseg = 'RAM', align = 13 )

    # distributed heaps: one heap per cluster : size = 16 Mbytes/NCLUSTERS
    for x in xrange (x_size):
        for y in xrange (y_size):
            
            cluster_id = (x * y_size) + y
            size       = heap_size / (x_size * y_size)
            base       = heap_base + (cluster_id * size)
            mapping.addVseg( vspace, 'conv_heap_%d_%d' % (x,y), base, size,
                             'C_WU', vtype = 'BUFFER', x = x , y = y , pseg = 'RAM' )

    # distributed stacks: one stack per processor : size = 32 Mbytes/(NCLUSTERS*NPROCS)
    for x in xrange (x_size):
        for y in xrange (y_size):
            for p in xrange( procs_max ):

                proc_id = (((x * y_size) + y) * procs_max) + p
                size    = stack_size / (x_size * y_size * procs_max)
                base    = stack_base + (proc_id * size)
                mapping.addVseg( vspace, 'conv_stack_%d_%d_%d' % (x,y,p), base, size,
                                 'C_WU', vtype = 'BUFFER', x = x , y = y , pseg = 'RAM' )
            
    # distributed tasks / one task per processor
    for x in xrange (x_size):
        for y in xrange (y_size):
            for p in xrange( procs_max ):

                trdid = (((x * y_size) + y) * procs_max) + p
                mapping.addTask( vspace, 'sort_%d_%d_%d' % (x,y,p), trdid, x, y, p,
                                 'conv_stack_%d_%d_%d' % (x,y,p),
                                 'conv_heap_%d_%d' % (x,y), 0 )

    # extend mapping name
    mapping.name += '_convol'

    return vspace  # useful for test
            
################################ test ######################################################

if __name__ == '__main__':

    vspace = convol( Mapping( 'test', 2, 2, 4 ) )
    print vspace.xml()


# Local Variables:
# tab-width: 4;
# c-basic-offset: 4;
# c-file-offsets:((innamespace . 0)(inline-open . 0));
# indent-tabs-mode: nil;
# End:
#
# vim: filetype=python:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

