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
 *         Mohamed Lamine KARAOUI <Mohamed.Karaoui@lip6.fr>, 2012
 */

#include <string.h>
#include <cassert>
#include <sstream>
#include <fstream>
#include <ios>
#include <iostream>
#include <cstdarg>
#include <cassert>
#include <iomanip>


#include "exception.h"
#include "memo.h"

//#define MOVER_DEBUG

size_t PSeg::m_pageSize;


////////////////////////////////////////////
MeMo::MeMo(const std::string     &filename, 
           const size_t          pageSize)
        : m_path(filename),
          m_pathHandler(filename),
          m_ginit(false),
          m_generator(new elfpp::object())
{
    PSeg::setPageSize(pageSize);

    // loading map_info blob 
    m_data = std::malloc(bin_size(m_path));
    if ( !m_data )
        throw exception::RunTimeError("malloc failed... No memory space");

    m_size = load_bin(m_path, m_data);

    // checking signature
    mapping_header_t*   header = (mapping_header_t*)m_data; 
    if((IN_MAPPING_SIGNATURE != header->signature))
    {
        std::cout  << "wrong signature " 
                   << std::hex <<header->signature 
                   << ", should be:"<< IN_MAPPING_SIGNATURE << std::endl;
        exit(1);
    }

#ifdef MOVER_DEBUG
    std::cout << "Binary file path: " << m_path << std::endl;
    print_mapping_info(m_data);
#endif

    buildMap(m_data);
    
#ifdef MOVER_DEBUG
    std::cout << "parsing done" << std::endl ;
#endif

#ifdef MOVER_DEBUG
    print_mapping();
    std::cout << *this << std::endl;
#endif

    m_psegh.check();
#ifdef MOVER_DEBUG
    std::cout << "checking done" << std::endl ;
#endif

}

/////////////////////////////////////////
void MeMo::print(std::ostream &o) const
{
    std::cout << "All sections:" << std::endl;
    FOREACH(sect, m_generator->get_section_table())
    {
        assert(&*sect != NULL);
        std::cout << *sect << std::endl; 
    }
}

////////////////////////////////
void MeMo::print_mapping() const
{
    std::cout << m_psegh << std::endl;
}

////////////////////////////////////////////////////////////////////////////////
elfpp::section* MeMo::get_sect_by_addr(elfpp::object *loader, unsigned int addr)
{
    FOREACH( sect, loader->get_section_table() )
    {
        assert(&*sect != NULL);
        elfpp::sh_flags_e eflags = sect->get_flags();
        if ( !(eflags & elfpp::SHF_ALLOC) )
            continue;
        if(sect->get_load_address() == addr) //load_addr ?
        {
            return (&*sect);
        }
    }
    return NULL;
}

/////////////////////////////////////////////////
void MeMo::buildSoft(const std::string &filename)
{
    if(!m_ginit)
        throw exception::RunTimeError(std::string("Can't get generator! ") );

    m_generator->write(filename);

}

/////////////////////////////////////////////////////////
size_t MeMo::load_bin(std::string filename, void* buffer)
{

#ifdef MOVER_DEBUG
    std::cout  << "Loading binary blob from file '" << filename << "'" << std::endl;
#endif

    std::ifstream input(filename.c_str(), std::ios_base::binary|std::ios_base::in);

    if ( ! input.good() )
        throw exception::RunTimeError(std::string("Can't open the file: ") + filename);

    input.seekg( 0, std::ifstream::end );
    size_t size = input.tellg();
    input.seekg( 0, std::ifstream::beg );

    if ( !buffer )
        throw exception::RunTimeError("Empty buffer!");

    input.read( (char*)buffer, size );

    input.close();

    return size;
}


/* get the size of the content of the filename */
//////////////////////////////////////////
size_t MeMo::bin_size(std::string filename)
{

#ifdef MOVER_DEBUG
    std::cout  << "Trying to get the size of the binary blob '" << filename << "'" << std::endl;
#endif

    std::ifstream input(filename.c_str(), std::ios_base::binary|std::ios_base::in);

    if ( ! input.good() )
        throw exception::RunTimeError(std::string("Can't open the file: ") + filename);

    input.seekg( 0, std::ifstream::end );
    size_t size = input.tellg();
    input.seekg( 0, std::ifstream::beg );

    input.close();

    return size;
}


/////////////
MeMo::~MeMo()
{
    //std::cout << "Deleted MeMo " << *this << std::endl;
    //std::free(m_data);//should be done by the elfpp driver since we passed the pointer
    std::map<std::string, elfpp::object*>::iterator l;
    for(l = m_loaders.begin(); l != m_loaders.end(); l++)
    {
        delete (*l).second;
    }
    //delete m_generator;
}


/////////////////////////////////////////////////////////////////////////////
// various mapping_info data structure access functions
/////////////////////////////////////////////////////////////////////////////
mapping_cluster_t* MeMo::get_cluster_base( mapping_header_t* header )
{
    return   (mapping_cluster_t*) ((char*)header +
                                  MAPPING_HEADER_SIZE);
}
/////////////////////////////////////////////////////////////////////////////
mapping_pseg_t* MeMo::get_pseg_base( mapping_header_t* header )
{
    return   (mapping_pseg_t*)    ((char*)header +
                                  MAPPING_HEADER_SIZE +
                                  MAPPING_CLUSTER_SIZE*header->clusters);
}
/////////////////////////////////////////////////////////////////////////////
mapping_vspace_t* MeMo::get_vspace_base( mapping_header_t* header )
{
    return   (mapping_vspace_t*)  ((char*)header +
                                  MAPPING_HEADER_SIZE +
                                  MAPPING_CLUSTER_SIZE*header->clusters +
                                  MAPPING_PSEG_SIZE*header->psegs);
}
/////////////////////////////////////////////////////////////////////////////
mapping_vseg_t* MeMo::get_vseg_base( mapping_header_t* header )
{
    return   (mapping_vseg_t*)    ((char*)header +
                                  MAPPING_HEADER_SIZE +
                                  MAPPING_CLUSTER_SIZE*header->clusters +
                                  MAPPING_PSEG_SIZE*header->psegs +
                                  MAPPING_VSPACE_SIZE*header->vspaces);
}
/////////////////////////////////////////////////////////////////////////////
mapping_vobj_t* MeMo::get_vobj_base( mapping_header_t* header )
{
    return   (mapping_vobj_t*)    ((char*)header +
                                  MAPPING_HEADER_SIZE +
                                  MAPPING_CLUSTER_SIZE*header->clusters +
                                  MAPPING_PSEG_SIZE*header->psegs +
                                  MAPPING_VSPACE_SIZE*header->vspaces +
                                  MAPPING_VSEG_SIZE*header->vsegs);
}

/////////////////////////////////////////////////////////////////////////////
// print the content of the mapping_info data structure 
////////////////////////////////////////////////////////////////////////
void MeMo::print_mapping_info(void* desc)
{
    mapping_header_t*   header = (mapping_header_t*)desc;  

    mapping_pseg_t*	    pseg    = get_pseg_base( header );;
    mapping_vspace_t*	vspace  = get_vspace_base ( header );;
    mapping_vseg_t*	    vseg    = get_vseg_base ( header );
    mapping_vobj_t*	    vobj    = get_vobj_base ( header );

    // header
    std::cout << std::hex << "mapping_info" << std::dec << std::endl
              << " + signature = " << header->signature << std::endl
              << " + name      = " << (char*)header->name      << std::endl
              << " + clusters  = " << header->clusters  << std::endl
              << " + psegs     = " << header->psegs     << std::endl
              << " + vspaces   = " << header->vspaces   << std::endl
              << " + globals   = " << header->globals   << std::endl
              << " + vsegs     = " << header->vsegs     << std::endl
              << " + vobjs     = " << header->vsegs     << std::endl
              << " + tasks     = " << header->tasks     << std::endl;

    // psegs
    for ( size_t pseg_id = 0 ; pseg_id < header->psegs ; pseg_id++ )
    {
        std::cout << "pseg " << pseg[pseg_id].name << std::hex << std::endl 
                  << " + base   = " << pseg[pseg_id].base   << std::endl 
                  << " + length = " << pseg[pseg_id].length << std::endl ;
    }

    // globals
    for ( size_t vseg_id = 0 ; vseg_id < header->globals ; vseg_id++ )
    {
        std::cout << std::endl;
        //std::cout << vseg[vseg_id].psegid << std::endl;
        std::cout << "global vseg "   << vseg[vseg_id].name << std::hex << std::endl 
                  << " + vbase    = " << vseg[vseg_id].vbase << std::endl 
                  << " + length   = " << vseg[vseg_id].length << std::endl 
                  << " + mode     = " << (size_t)vseg[vseg_id].mode << std::endl 
                  << " + ident    = " << (bool)vseg[vseg_id].ident << std::endl 
                  << " + psegname = " << pseg[vseg[vseg_id].psegid].name << std::endl;
        for( size_t vobj_id = vseg[vseg_id].vobj_offset ; 
                    vobj_id < vseg[vseg_id].vobj_offset + vseg[vseg_id].vobjs ; 
                    vobj_id++ )
        {
            std::cout << "\t vobj "        << vobj[vobj_id].name    << std::endl
                      << "\t + index   = " << std::dec << vobj_id   << std::endl
                      << "\t + type    = " << vobj[vobj_id].type    << std::endl
                      << "\t + length  = " << vobj[vobj_id].length  << std::endl
                      << "\t + align   = " << vobj[vobj_id].align   << std::endl
                      << "\t + binpath = " << vobj[vobj_id].binpath << std::endl
                      << "\t + init    = " << vobj[vobj_id].init    << std::endl;
        }
    }

    // vspaces
    for ( size_t vspace_id = 0 ; vspace_id < header->vspaces ; vspace_id++ )
    {
        std::cout << "***vspace "  << vspace[vspace_id].name   << std::endl 
                  << " + vsegs = " <<  vspace[vspace_id].vsegs << std::endl 
                  << " + vobjs = " <<  vspace[vspace_id].vobjs << std::endl 
                  << " + tasks = " <<  vspace[vspace_id].tasks << std::endl;

        for ( size_t vseg_id = vspace[vspace_id].vseg_offset ; 
              vseg_id < (vspace[vspace_id].vseg_offset + vspace[vspace_id].vsegs) ; 
              vseg_id++ )
        {
            std::cout << "\t vseg " << vseg[vseg_id].name  << std::endl
                      << "\t + vbase    = " << vseg[vseg_id].vbase             << std::endl
                      << "\t + length   = " << vseg[vseg_id].length            << std::endl
                      << "\t + mode     = " << (size_t)vseg[vseg_id].mode      << std::endl
                      << "\t + ident    = " << (bool)vseg[vseg_id].ident       << std::endl
                      << "\t + psegname = " << pseg[vseg[vseg_id].psegid].name << std::endl;
            for(size_t vobj_id = vseg[vseg_id].vobj_offset ; 
                       vobj_id < vseg[vseg_id].vobj_offset + vseg[vseg_id].vobjs ; 
                       vobj_id++ )
            {
                std::cout << "\t\t vobj "      << vobj[vobj_id].name      << std::endl
                          << "\t\t + index   = " << std::dec << vobj_id   << std::endl
                          << "\t\t + type    = " << vobj[vobj_id].type    << std::endl
                          << "\t\t + length  = " << vobj[vobj_id].length  << std::endl
                          << "\t\t + align   = " << vobj[vobj_id].align   << std::endl
                          << "\t\t + binpath = " << vobj[vobj_id].binpath << std::endl
                          << "\t\t + init    = " << vobj[vobj_id].init    << std::endl;
            }
        }

    }
} // end print_mapping_info()


//////////////////////////////////////////
void MeMo::vseg_map( mapping_vseg_t* vseg)
{
    mapping_vobj_t * vobj = get_vobj_base((mapping_header_t*) m_data );
    PSeg            * ps = &(m_psegh.get(vseg->psegid));
    elfpp::section * sect = NULL;
    size_t           cur_vaddr;
    paddr_t          cur_length;
    bool             first = true;
    bool             aligned = false;
    VSeg *           vSO = new VSeg;
    mapping_vobj_t * cur_vobj;

    vSO->m_name = std::string(vseg->name);
    vSO->m_vma  = vseg->vbase;
    vSO->m_lma  = 0;

    cur_vaddr = vseg->vbase;
    cur_length = 0; // curr_length of the vseg; a new vseg is necessarily aligned on a page boundary

#ifdef MOVER_DEBUG
    std::cout << "--------------- vseg_map "<< vseg->name <<" ------------------" << std::endl;
#endif

    for (size_t vobj_id = vseg->vobj_offset; 
          vobj_id < (vseg->vobj_offset + vseg->vobjs); vobj_id++)
    {
        cur_vobj = &vobj[vobj_id];

#ifdef MOVER_DEBUG
        std::cout << std::hex << "current vobj("<< vobj_id <<"): " << cur_vobj->name 
                  << " (" <<cur_vobj->vaddr << ")"
                  << " size: "<< cur_vobj->length 
                  << " type: " <<  cur_vobj->type << std::endl;
#endif
        if (cur_vobj->type == VOBJ_TYPE_BLOB)
        {
            size_t blob_size;
            std::string filePath(m_pathHandler.getFullPath(std::string(cur_vobj->binpath)));

#ifdef MOVER_DEBUG
            std::cout << std::hex << "Handling: " << filePath << " ..." << std::endl;
#endif

            if (!filePath.compare(m_path))    // local blob: map_info
            {
#ifdef MOVER_DEBUG
                std::cout << "Found the vseg of the mapping info" << std::endl;
#endif
                blob_size = this->m_size;
                assert((blob_size >0) and "MAPPING INFO file is empty !?");
            }
            else
            {
#ifdef MOVER_DEBUG
                std::cout << "Found an BLOB vseg" << std::endl;
#endif
                blob_size = bin_size(filePath);
            }

            // creating a new section 
            sect = new elfpp::section(*m_generator, elfpp::SHT_PROGBITS);

            sect->set_name(std::string(cur_vobj->name));
            sect->set_flags(elfpp::SHF_ALLOC | elfpp::SHF_WRITE);
            sect->set_size(blob_size);  //do the malloc for the get_content fonction

            assert(sect->get_content());//check allocation

            if (!filePath.compare(m_path)) {   //local blob: map_info
                //memcpy(sect->get_content(), m_data, sect->get_size());
                /* this way the modification of the elf size are propageted to the giet */
                sect->set_content(this->m_data);
            }
            else
            {
                load_bin(filePath, sect->get_content());
            }


            if (blob_size > cur_vobj->length)
            {
                std::cout << std::hex << "!!! Warning, specified blob type vobj ("<<
                cur_vobj->name  <<") size is "<< cur_vobj->length << ", the actual size is "
                << blob_size  << " !!!" <<std::endl;
                assert(0 and "blob vobj length smaller than the actual content" );//???
            }

            cur_vobj->length = blob_size; //set the true size of this BLOB vobj

            vSO->m_file = filePath;
            vSO->m_loadable = true;
        }
        else if (cur_vobj->type == VOBJ_TYPE_ELF)
        {
            if (!first)
                throw exception::RunTimeError(std::string("elf vobj type, must be placed first in a vseg"));

            size_t elf_size;
            std::string filePath(m_pathHandler.getFullPath(std::string(cur_vobj->binpath)));
#ifdef MOVER_DEBUG
            std::cout << "Handling: " << filePath << " ..." << std::endl;
            std::cout << "Found an ELF vseg" << std::endl;
#endif
            if (m_loaders.count(filePath) == 0)
                m_loaders[filePath] = new elfpp::object(filePath);
            elfpp::object * loader = m_loaders[filePath]; //TODO:free!?

            sect =  new elfpp::section(*get_sect_by_addr(loader, cur_vaddr)); //copy: for the case we replicate the code
            if (!sect)
            {
                std::cerr << "No section found for " << cur_vobj->name << " at "<< cur_vaddr << std::endl;
                exit(-1);
            }

            sect->set_name(std::string(cur_vobj->name));


            elf_size = sect->get_size();
            assert((elf_size > 0) and "ELF section empty ?");

            if (!m_ginit)
            {
                /** Initailising the header of the generator from the first binary,
                ** we suppose that the header is the same for all the binarys **/
                m_generator->copy_info(*loader, 64);
                m_ginit = true;
            }

            if (elf_size > cur_vobj->length)
            {
                std::cout << "Warning, specified elf type vobj (" <<
                cur_vobj->name  << ") size is " << cur_vobj->length << ", the actual size is "
                << elf_size  << std::endl;
                assert((0) and "elf vobj length smaller than the actual content");
            }

            cur_vobj->length = elf_size;//set the true size of this ELF vobj

            vSO->m_file = filePath;
            vSO->m_loadable = true;
        }

        //aligning the vobj->paddr if necessary
        if (cur_vobj->align)
        {
            cur_length = PSeg::align(cur_length, cur_vobj->align);
            aligned = true;
        }

        cur_vaddr += cur_vobj->length;
        cur_length += cur_vobj->length;
        first = false;
    }

    assert(cur_vaddr >= vseg->vbase);

    vSO->m_length = cur_length; //pageAlign is done by the psegs
    vSO->m_ident = vseg->ident;
    vSO->m_align = vobj[vseg->vobj_offset].align;

    //set the lma for vseg that are not peripherals
    if (ps->type() != PSEG_TYPE_PERI)
    {
        if (vseg->ident != 0) ps->addIdent(*vSO);
        else                  ps->add(*vSO);
    }

    if (!sect) return;

#ifdef MOVER_DEBUG
    std::cout << "section: "<< *sect <<"\n seted to: " << (*vSO) << std::endl;
#endif

    sect->set_vaddr((*vSO).lma());
    m_generator->add_section(*sect);

} // end vseg_map()
 

///////////////////////////////
void MeMo::buildMap(void * desc)
{
    mapping_header_t * header  = (mapping_header_t *) desc;  

    mapping_cluster_t * cluster = get_cluster_base(header);     
    mapping_vspace_t *  vspace  = get_vspace_base(header);     
    mapping_pseg_t *    pseg    = get_pseg_base(header); 
    mapping_vseg_t *    vseg    = get_vseg_base(header);

    // get the psegs

#ifdef MOVER_DEBUG
    std::cout << "\n******* Storing Pseg information *********\n" << std::endl;
#endif

    for (size_t cluster_id = 0; cluster_id < header->clusters; cluster_id++) {
        for (size_t pseg_id = cluster[cluster_id].pseg_offset;
              pseg_id < cluster[cluster_id].pseg_offset + cluster[cluster_id].psegs;
              pseg_id++) {
            std::string name(pseg[pseg_id].name);
            PSeg *ps = new PSeg(name, 
                                pseg[pseg_id].base, 
                                pseg[pseg_id].length, 
                                pseg[pseg_id].type);
            m_psegh.m_pSegs.push_back(*ps);
        }
    }

    // map global vsegs

#ifdef MOVER_DEBUG
    std::cout << "\n******* mapping global vsegs *********\n" << std::endl;
#endif

    // Mapping identity segments first
    for (size_t vseg_id = 0; vseg_id < header->globals; vseg_id++) {
        if (vseg[vseg_id].ident == 1) {
            vseg_map(&vseg[vseg_id]);
        }
    }

    // Mapping non-identity segments second
    for (size_t vseg_id = 0; vseg_id < header->globals; vseg_id++) {
        if (vseg[vseg_id].ident == 0) {
            vseg_map(&vseg[vseg_id]);
        }
    }



    // loop on virtual spaces to map private vsegs
    for (size_t vspace_id = 0; vspace_id < header->vspaces; vspace_id++) {
#ifdef MOVER_DEBUG
        std::cout << "\n******* mapping all vsegs of " << vspace[vspace_id].name << " *********\n" << std::endl;
#endif
        for (size_t vseg_id = vspace[vspace_id].vseg_offset; 
             vseg_id < (vspace[vspace_id].vseg_offset + vspace[vspace_id].vsegs); 
             vseg_id++) {
            if (vseg[vseg_id].ident == 1) {
                vseg_map(&vseg[vseg_id]); 
            }
        }
 
        for (size_t vseg_id = vspace[vspace_id].vseg_offset; 
             vseg_id < (vspace[vspace_id].vseg_offset + vspace[vspace_id].vsegs); 
             vseg_id++) {
            if (vseg[vseg_id].ident == 0) {
                vseg_map(&vseg[vseg_id]); 
            }
        }
    } 
} // end buildMap()



// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

