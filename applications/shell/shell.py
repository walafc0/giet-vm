#!/usr/bin/env python

from mapping import *

######################################################################################
#   file   : shell.py  
#   date   : july 2015
#   author : Alain Greiner
#######################################################################################
#  This file describes the mapping of the single thread "shell" application 
#  on processor[0][0][0] of a multi-clusters, multi-processors architecture.
####################################################################################

######################
def extend( mapping ):

    nprocs    = mapping.nprocs
    x_width   = mapping.x_width
    y_width   = mapping.y_width

    # define vsegs base & size
    code_base  = 0x10000000
    code_size  = 0x00010000     # 64 Kbytes 
    
    data_base  = 0x20000000
    data_size  = 0x00080000     # 512 Kbytes 

    stack_base = 0x40000000 
    stack_size = 0x00200000     # 2 Mbytes 

    heap_base  = 0x60000000 
    heap_size  = 0x00001000     # 4 Kbytes 

    # create vspace
    vspace = mapping.addVspace( name = 'shell', startname = 'shell_data' , active = True )
    
    # data vseg
    mapping.addVseg( vspace, 'shell_data', data_base , data_size, 
                     'C_WU', vtype = 'ELF', x = 0, y = 0, pseg = 'RAM', 
                     binpath = 'bin/shell/appli.elf',
                     local = False )

    # code vseg
    mapping.addVseg( vspace, 'shell_code', code_base , code_size,
                     'CXWU', vtype = 'ELF', x = 0, y = 0, pseg = 'RAM', 
                     binpath = 'bin/shell/appli.elf',
                     local = False )

    # stack vseg             
    mapping.addVseg( vspace, 'shell_stack', stack_base, stack_size,
                     'C_WU', vtype = 'BUFFER', x = 0 , y = 0 , pseg = 'RAM',
                     local = False, big = True )

    # heap vseg (unused)            
    mapping.addVseg( vspace, 'shell_heap', heap_base, heap_size,
                     'C_WU', vtype = 'BUFFER', x = 0 , y = 0 , pseg = 'RAM',
                     local = False )

    # task 
    mapping.addTask( vspace, 'shell', 0, 0, 0, 0, 'shell_stack', 'shell_heap', 0 )

    # extend mapping name
    mapping.name += '_shell'

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

