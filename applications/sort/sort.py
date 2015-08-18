#!/usr/bin/env python

from mapping import *

####################################################################################
#   file   : sort.py 
#   date   : may 2014
#   author : Alain Greiner
####################################################################################
#  This file describes the mapping of the multi-threaded "sort"
#  application on a multi_clusters, multi-processors architecture.
#  This include both the mapping of virtual segments on the clusters,
#  and the mapping of tasks on processors.
#  This mapping uses 5 platform parameters, (obtained from the "mapping" argument)
#  - x_size    : number of clusters in a row
#  - y_size    : number of clusters in a column
#  - x_width   : number of bits coding x coordinate
#  - y_width   : number of bits coding y coordinate
#  - nprocs    : number of processors per cluster
####################################################################################

######################
def extend( mapping ):

    x_size    = mapping.x_size
    y_size    = mapping.y_size
    nprocs    = mapping.nprocs
    x_width   = mapping.x_width
    y_width   = mapping.y_width

    ntasks    = x_size * y_size * nprocs

    # define vsegs base & size
    code_base  = 0x10000000
    code_size  = 0x00010000     # 64 Kbytes (replicated in each cluster)

    data_base  = 0x20000000
    data_size  = 0x00010000     # 64 Kbyte (non replicated)

    stack_base = 0x40000000
    stack_size = 0x00200000     # 2 Mbytes (per cluster)

    heap_base  = 0x60000000
    heap_size  = 0x00200000     # 2 Mbytes (per cluster)

    # create Vspace
    vspace = mapping.addVspace( name = 'sort', startname = 'sort_data', active = False )

    # data vseg : non local (only in cluster[0,0])
    mapping.addVseg( vspace, 'sort_data', data_base , data_size,
                     'C_WU', vtype = 'ELF', x = 0, y = 0, pseg = 'RAM',
                     binpath = 'bin/sort/appli.elf',
                     local = False )

    # code vsegs : local (one copy per cluster)
    for x in xrange (x_size):
        for y in xrange (y_size):
            cluster_id = (x * y_size) + y
            if ( mapping.clusters[cluster_id].procs ):

                mapping.addVseg( vspace, 'sort_code', code_base , code_size,
                                 'CXWU', vtype = 'ELF', x = x, y = y, pseg = 'RAM',
                                 binpath = 'bin/sort/appli.elf',
                                 local = True )

    # stacks vsegs : local (one stack per task)
    for x in xrange (x_size):
        for y in xrange (y_size):
            cluster_id = (x * y_size) + y
            if ( mapping.clusters[cluster_id].procs ):
                for p in xrange (nprocs):
                    proc_id = (((x * y_size) + y) * nprocs) + p
                    size    = stack_size / nprocs
                    base    = stack_base + (proc_id * size)

                    mapping.addVseg( vspace, 'sort_stack_%d_%d_%d' % (x,y,p), 
                                     base, size, 'C_WU', vtype = 'BUFFER',
                                     x = x, y = y, pseg = 'RAM',
                                     local = True, big = True )

    # heap vsegs : distributed but non local (all tasks can access all heap vsegs)
    for x in xrange (x_size):
        for y in xrange (y_size):
            cluster_id = (x * y_size) + y
            if ( mapping.clusters[cluster_id].procs ):
                size       = heap_size
                base       = heap_base + (cluster_id * size)

                mapping.addVseg( vspace, 'sort_heap_%d_%d' % (x,y), base, size,
                                 'C_WU', vtype = 'BUFFER', x = x, y = y, pseg = 'RAM',
                                 local = False, big = True )

    # distributed tasks / one task per processor
    for x in xrange (x_size):
        for y in xrange (y_size):
            cluster_id = (x * y_size) + y
            if ( mapping.clusters[cluster_id].procs ):
                for p in xrange( nprocs ):
                    trdid = (((x * y_size) + y) * nprocs) + p

                    mapping.addTask( vspace, 'sort_%d_%d_%d' % (x,y,p),
                                     trdid, x, y, p,
                                     'sort_stack_%d_%d_%d' % (x,y,p),
                                     'sort_heap_%d_%d' % (x,y), 0 )

    # extend mapping name
    mapping.name += '_sort'

    return vspace  # useful for test

################################ test ###################################################

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

