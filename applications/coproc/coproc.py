#!/usr/bin/env python

from mapping import *

##################################################################################
#   file   : coproc.py  
#   date   : march 2015
#   author : Alain Greiner
##################################################################################
#  This file describes the mapping of the single thread "coproc" application.
#  The main characteristic of this application is to use an hardware coprocessor. 
#  It can run on any processor of a multi-clusters / multi-processors 
#  architecture, as long as it exist a coprocessor in the same cluster as the 
#  calling task.
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

    # define thread placement
    x = 0
    y = 0
    p = 0

    assert( (x < x_size) and (y < y_size) )

    assert( mapping.clusters[x * y_size + y].procs != 0 )

    # define vsegs base & size
    code_base  = 0x10000000
    code_size  = 0x00010000     # 64 Kbytes 
    
    data_base  = 0x20000000
    data_size  = 0x00010000     # 64 Kbytes 

    stack_base = 0x40000000 
    stack_size = 0x00200000     # 2 Mbytes

    # create vspace
    vspace = mapping.addVspace( name = 'coproc', startname = 'coproc_data' )
    
    # data vseg in cluster[x,y]
    mapping.addVseg( vspace, 'coproc_data', data_base , data_size, 
                     'C_WU', vtype = 'ELF', x = x, y = y, pseg = 'RAM', 
                     binpath = 'bin/coproc/appli.elf',
                     local = False )

    # code vseg in cluster[x,y]
    mapping.addVseg( vspace, 'coproc_code', code_base , code_size,
                     'CXWU', vtype = 'ELF', x = x, y = y, pseg = 'RAM', 
                     binpath = 'bin/coproc/appli.elf',
                     local = False )

    # stack vseg in cluster [x,y]
    mapping.addVseg( vspace, 'coproc_stack', stack_base, stack_size,
                     'C_WU', vtype = 'BUFFER', x = x , y = y , pseg = 'RAM',
                     local = False, big = True )

    # one task on processor[x,y,0]
    mapping.addTask( vspace, 'coproc', 0 , x , y , 0 ,
                     'coproc_stack' , '' , 0 )

    # extend mapping name
    mapping.name += '_coproc'

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

