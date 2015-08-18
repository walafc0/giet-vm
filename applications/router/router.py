#!/usr/bin/env python

from mapping import *

######################################################################################
#   file   : router.py
#   date   : november 2014
#   author : Alain Greiner
#######################################################################################
#  This file describes the mapping of the multi-threaded "router" 
#  application on a multi-clusters, multi-processors architecture.
#  This application contains N+2 parallel tasks communicating through MWMR channels:
#  The mapping of virtual segments on the clusters is the following:
#    - The code vsegs are replicated on all clusters.
#    - the data_0 vseg is mapped on cluster[0,0].
#    - the data_1 vseg is mapped on cluster[x_size-1,y_size-1].
#    - the stacks vsegs are distibuted on all clusters.
#  The mapping of tasks on processors is the following:
#    - one "producer" task  => on proc[0,0,0]
#    - one "consume"  task  => on proc[x_size-1,y_size-1,nprocs-1]
#    - N   "router"   tasks => on all others processors
#  This mapping uses 5 platform parameters, (obtained from the "mapping" argument)
#    - x_size    : number of clusters in a row
#    - y_size    : number of clusters in a column
#    - nprocs    : number of processors per cluster
####################################################################################

######################
def extend( mapping ):

    x_size    = mapping.x_size
    y_size    = mapping.y_size
    nprocs    = mapping.nprocs
    x_width   = mapping.x_width
    y_width   = mapping.y_width

    # define vsegs base & size
    code_base    = 0x10000000
    code_size    = 0x00010000     # 64 Kbytes (replicated in each cluster)
    
    data_0_base  = 0x20000000
    data_0_size  = 0x00010000     # 64 Kbytes (non replicated)

    data_1_base  = 0x30000000
    data_1_size  = 0x00010000     # 64 Kbytes (non replicated)

    stack_base   = 0x40000000 
    stack_size   = 0x00200000     # 2 Mbytes (per cluster)

    # create vspace
    vspace = mapping.addVspace( name = 'router', startname = 'router_data_0' )
    
    # data_0 vseg : shared / in cluster [0,0]
    mapping.addVseg( vspace, 'router_data_0', data_0_base , data_0_size, 
                     'C_WU', vtype = 'ELF', x = 0, y = 0, pseg = 'RAM', 
                     binpath = 'bin/router/appli.elf',
                     local = False )

    # data_1 vseg : shared / in cluster[x_size-1,y_size-1]
    mapping.addVseg( vspace, 'router_data_1', data_1_base , data_1_size, 
                     'C_WU', vtype = 'ELF', x = x_size - 1, y = y_size - 1, pseg = 'RAM', 
                     binpath = 'bin/router/appli.elf',
                     local = False )

    # code vsegs : local (one copy in each cluster)
    for x in xrange (x_size):
        for y in xrange (y_size):
            mapping.addVseg( vspace, 'router_code_%d_%d' %(x,y), code_base , code_size,
                             'CXWU', vtype = 'ELF', x = x, y = y, pseg = 'RAM', 
                             binpath = 'bin/router/router.elf',
                             local = True )

    # stacks vsegs: local (one stack per processor => nprocs stacks per cluster)            
    for x in xrange (x_size):
        for y in xrange (y_size):
            for p in xrange( nprocs ):
                proc_id = (((x * y_size) + y) * nprocs) + p
                size    = stack_size / nprocs
                base    = stack_base + (proc_id * size)
                mapping.addVseg( vspace, 'router_stack_%d_%d_%d' % (x,y,p), base, size,
                                 'C_WU', vtype = 'BUFFER', x = x , y = y , pseg = 'RAM',
                                 local = True, big = True )

    # distributed tasks / one task per processor
    for x in xrange (x_size):
        for y in xrange (y_size):
            for p in xrange( nprocs ):
                trdid = (((x * y_size) + y) * nprocs) + p
                if   (x==0) and (y==0) and (p== 0):                      # task producer
                    task_index = 2
                    task_name  = 'producer'            
                elif (x==x_size-1) and (y==y_size-1) and (p==nprocs-1):  # task consumer 
                    task_index = 1  
                    task_name  = 'consumer'
                else :                                                   # task router
                    task_index = 0
                    task_name  = 'router_%d_%d_%d' % (x,y,p)
                mapping.addTask( vspace, task_name, trdid, x, y, p,
                                 'router_stack_%d_%d_%d' % (x,y,p), '' , task_index )

    # extend mapping name
    mapping.name += '_router'

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

