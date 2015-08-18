/* -*- c++ -*-
 *
 * GIET_VM_LGPL_HEADER_BEGIN
 * 
 * This file is part of GIET_VM, GNU LGPLv2.1.
 * 
 * GIET_VM is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 of the License.
 * 
 * GIET_VM is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with GIET_VM; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 * 
 * GIET_VM_LGPL_HEADER_END
 *
 * Copyright (c) UPMC, Lip6, SoC
 *         Mohamed Lamine Karaoui <Mohamed.Karaoui@lip6.fr>, 2012
 *
 */

#ifndef GIET_VM_MEMO_H
#define GIET_VM_MEMO_H

#include  <stdlib.h>
#include  <cstdlib>
#include  <fcntl.h>
#include  <unistd.h>
#include  <cstdio>
#include  <string.h>

#include <string>
#include <ios>
#include <vector>
#include <map>
#include <fstream>

#include <dpp/foreach>

#include <elfpp/object>
#include <elfpp/symbol>
#include <elfpp/section>
#include <elfpp/segment>
#include <elfpp/elfpp_bits.hh>

#include "stdint.h"
#include "mapping_info.h"
#include "pseg_handler.h"
#include "path_handler.h"


class MeMo
{
// TODO: make the name defined in the map_info relative to this wd.

    std::string                                   m_path;       // map_info path name
    std::string                                   m_wd;         // map_info directory TODO
    std::string                                   m_simpleName; // map_info filename TODO
    void*                                         m_data;       // map_info structure 
    uintptr_t                                     m_addr;       // map_info address (virtual)
    size_t                                        m_size;       // size of the structure
    mutable std::map<std::string, elfpp::object*> m_loaders;
    PSegHandler                                   m_psegh;
    PathHandler                                   m_pathHandler;
    bool                                          m_ginit;
    elfpp::object*                                m_generator;

    size_t load_bin(std::string filename, void* buffer);    
    size_t bin_size(std::string filename);

    elfpp::section* get_sect_by_addr(elfpp::object *loader, unsigned int addr);

public:

    MeMo( const std::string   &name, 
          const size_t        pageSize = 4096);

    ~MeMo();

    void buildSoft(const std::string &filename);

    void print( std::ostream &o ) const;

    void print_mapping() const;

    friend std::ostream &operator<<( std::ostream &o, const MeMo &s )
    {
        s.print(o);
        return o;
    }

    // The following functions handle the map.bin structure
    // They must keep synchronised with functions defined in boot_init.c.

    mapping_cluster_t* get_cluster_base( mapping_header_t* header );
    mapping_pseg_t* get_pseg_base( mapping_header_t* header );
    mapping_vspace_t* get_vspace_base( mapping_header_t* header );
    mapping_vseg_t* get_vseg_base( mapping_header_t* header );
    mapping_vobj_t* get_vobj_base( mapping_header_t* header );
    void print_mapping_info(void* desc);
    void vseg_map( mapping_vseg_t* vseg);
    void buildMap(void* desc);

};


#endif /* GIET_VM_MEMO_H */

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

