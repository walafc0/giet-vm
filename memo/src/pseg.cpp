/* -*- c++ -*-
 *
 * GIET_VM_LGPL_HEADER_BEGIN
 * 
 * This file is part of SoCLib, GNU LGPLv2.1.
 * 
 * SoCLib is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 of the License.
 * 
 * SoCLib is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with SoCLib; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 * 
 * GIET_VM_LGPL_HEADER_END
 *
 * Copyright (c) UPMC, Lip6, SoC
 *         Mohamed Lamine Karaoui <Mohamed.Karaoui@lip6.fr>, 2012
 */

#include <algorithm>
#include <string.h>
#include <cassert>
#include <cstring>
#include <stdexcept>

#include <iostream>
#include <sstream>
#include <iomanip>

#include <cstdio>

#include "pseg.h"
#include "exception.h"


#define max(x, y) (((x) > (y)) ? (x) : (y))


/*
 * VSeg
 */

//////////////////////////////////////
const std::string & VSeg::name() const {
	return m_name;
}

//////////////////////////////////////
const std::string & VSeg::file() const {
	return m_file;
}

///////////////////////////
uintptr_t VSeg::vma() const {
	return m_vma;
}

/////////////////////////
paddr_t VSeg::lma() const {
	return m_lma;
}

///////////////////////////
size_t VSeg::length() const {
	return m_length;
}

/////////////////////////
size_t VSeg::type() const {
	return m_type;
}

/////////////////////////////////////////
void VSeg::print(std::ostream &o) const
{
	o << std::hex << std::noshowbase 
      << "<Virtual segment from(vaddr): 0x" 
      << std::setw (8) << std::setfill('0') 
      << m_vma << ", to(paddr) 0x"
      << std::setw (16) << std::setfill('0') 
      << m_lma << ", size: 0x"
      << std::setw (8) << std::setfill('0') 
          << m_length  << ",ident: " 
          << (m_ident ? "yes" : "no") << ", in(file): "
          << m_file << ", name: " << m_name << ">";
}

/////////////
VSeg::~VSeg() {}

/////////////////////////////////////////
VSeg & VSeg::operator=(const VSeg & ref)
{
    if (&ref == this)
        return *this;

    m_name = ref.m_name;
    m_file = ref.m_file;
    m_vma = ref.m_vma;
    m_lma = ref.m_lma;
    m_length = ref.m_length;
    m_ident = ref.m_ident;
    return *this;
}

////////////
VSeg::VSeg()
    : m_name("No Name"),
    m_file("Empty section"),
    m_vma(0),
    m_length(0),
    m_loadable(false),
    m_ident(0)
{
}

////////////////////////////////////
VSeg::VSeg(std::string&  binaryName, 
        std::string&  name, 
        uintptr_t     vma, 
        size_t        length, 
        bool          loadable, 
        bool          ident)
: m_name(name),
    m_file(binaryName),
    m_vma(vma),
    m_length(length),
    m_loadable(loadable),
    m_ident(ident)
{
}

/////////////////////////////
VSeg::VSeg(const VSeg &ref):
    m_name(ref.m_name),
    m_file(ref.m_file),
    m_vma(ref.m_vma),
    m_length(ref.m_length),
    m_loadable(ref.m_loadable),
    m_ident(ref.m_ident)
{
    (*this) = ref;
}


/*
 * PSeg
 */

/////////////////////////
paddr_t PSeg::lma() const {
    return m_lma;
}

///////////////////////////
paddr_t PSeg::length() const {
    return m_length;
}

/////////////////////////
size_t PSeg::type() const {
    return m_type;
}

//////////////////////////////////////
const std::string & PSeg::name() const {
    return m_name;
}

//////////////////////// initialisation used[][] ??? (AG)
void PSeg::check() const
{

    if (this->m_type == PSEG_TYPE_PERI)
        return;

    std::vector<VSeg>::const_iterator it;
    size_t    size = m_vsegs.size();
    paddr_t   used[size][2];          // lma, lma+length
    size_t    i, j, error = 0;

    for (it = m_vsegs.begin(), i = 0; it < m_vsegs.end(); it++, i++)
    {
        paddr_t it_limit = (*it).lma() + (*it).length();
        for(j = 0; j < i; j++)
        {
            if (used[j][0] == (*it).lma()) //not the same lma ,
                {
                    error = 1;
                }
            if (used[j][1] == it_limit)  // and not the same limit
            {
                error = 2;
            }
            if ((used[j][0] < (*it).lma()) and ((*it).lma() < used[j][1])) // lma  within
            {
                error = 3;
            }
            if (((used[j][0] < it_limit) and (it_limit < used[j][1]))) // limit no within
            {
                error = 4;
                std::cout << "base: " << std::hex << (*it).lma() << std::endl;
                std::cout << "limit: " << std::hex << it_limit << std::endl;
                std::cout << "used[j][0]: " << std::hex << used[j][0] << std::endl;
                std::cout << "used[j][1]: " << std::hex << used[j][1] << std::endl;
            }
            if (error)
            {
                std::ostringstream err;
                err << " Error" << error << ", ovelapping Buffers:" << std::endl 
                    << *it << std::endl << m_vsegs[j] << std::endl; 
                throw exception::RunTimeError( err.str().c_str() );
            }

        }
        used[i][0] = (*it).lma();
        used[i][1] = it_limit;
    }
}


/////////////////////////////////////////////////////////
paddr_t PSeg::align(paddr_t toAlign, unsigned alignPow2) {
    return ((toAlign + (1 << alignPow2) - 1 ) >> alignPow2) << alignPow2; 
}

//////////////////////////////////////////
paddr_t PSeg::pageAlign(paddr_t toAlign) {
    size_t pgs = pageSize();
    size_t pageSizePow2 = __builtin_ctz(pgs);

    return align(toAlign, pageSizePow2); 
}



////////////////////////////
void PSeg::add(VSeg& vseg) {
    std::vector<VSeg>::iterator it;
    int nb_elems = m_vsegs.size();
    int i = 0;
    bool mapped = false;
    paddr_t prev_base = m_lma;
    paddr_t prev_length = 0x0;
    paddr_t curr_base = 0x0;
    paddr_t curr_length = 0x0;
    paddr_t next_base = 0x0;
    paddr_t next_length = 0x0;

    const int alignment = max(vseg.m_align, __builtin_ctz(pageSize())); // 12

    if (vseg.length() == 0) {
        std::cout << "*** Error: Adding a vseg of size 0 (base " << vseg.vma() << ")" << std::endl;
        exit(1);
    }

    if (nb_elems == 0) {
        if (vseg.length() <= m_length) {
            vseg.m_lma = m_lma;
            m_vsegs.push_back(vseg);
            return;
        }
        else {
            std::cout << "*** Error: Not enough space to map first VSeg (base = "
                      << std::hex << vseg.vma() << " - Size = " << vseg.length() << ")" << std::endl;
            std::cout << "    PSeg too small! (base = " << m_lma << " - size = "
                      << m_length << ")" << std::endl;
            exit(1);
        }
    }

    curr_base = m_vsegs[0].lma(); // Initialisation avant recherche du min
    curr_length = m_vsegs[0].length();
    for (it = m_vsegs.begin(); it != m_vsegs.end(); it++) {
        if ((*it).lma() < curr_base) {
            curr_base = (*it).lma();
            curr_length = (*it).length();
        }
    }

    while (i < nb_elems) {
        if (align(prev_base + prev_length, alignment) + vseg.length() <= curr_base) {
            vseg.m_lma = align(prev_base + prev_length, alignment);
            mapped = true;
            break;
        }
        else if (i < nb_elems - 1) {
            // Searching for the vseg already mapped with lowest paddr > curr_base
            next_base = 0;
            bool found = false;
            for (it = m_vsegs.begin(); it != m_vsegs.end(); it++) {
                if ((!found || (*it).lma() < next_base) && (*it).lma() > curr_base) {
                    found = true;
                    next_base = (*it).lma();
                    next_length = (*it).length();
                }
            }
            assert(found);

            prev_base = curr_base;
            prev_length = curr_length;

            curr_base = next_base;
            curr_length = next_length;
        }
        else {
            if (align(curr_base + curr_length, alignment) + vseg.length() <= m_lma + m_length) {
                vseg.m_lma = align(curr_base + curr_length, alignment);
                mapped = true;
            }
        }
        i++;
    }

    if (!mapped) {
        std::cout << "*** Error: Not enough space to map VSeg (base = " << std::hex << vseg.vma() << " - Size = " << vseg.length() << ")" << std::endl;
        exit(1);
    }

    m_vsegs.push_back(vseg); 
}


/////////////////////////////////
void PSeg::addIdent(VSeg& vseg) {
    std::vector<VSeg>::iterator it;

    for (it = m_vsegs.begin(); it != m_vsegs.end(); it++) {
        if ((vseg.vma() == (*it).lma()) ||
            ((vseg.vma() < (*it).lma()) && (vseg.vma() + vseg.length() > (*it).lma())) ||
            ((vseg.vma() > (*it).lma()) && ((*it).lma() + (*it).length() > vseg.vma()))) {
            std::cout << "*** Error: Identity VSeg overlaps another segment:" << std::endl
                      << "Added Segment Base : " << std::hex << vseg.vma()
                      << " - Size : " << vseg.length() << std::endl;
            std::cout << "Existing Segment Base : " << (*it).lma()
                      << " - size : " << (*it).length() << std::endl;
            exit(1);
        }
    }

    vseg.m_lma = vseg.m_vma;
    m_vsegs.push_back(vseg); 
}


/////////////////////////////////////////
PSeg & PSeg::operator=(const PSeg &ref) {
    if (&ref == this) {
        return *this;
    }

    m_name = ref.m_name;
    m_length = ref.m_length;
    m_lma = ref.m_lma;
    m_vsegs = ref.m_vsegs;
    m_type = ref.m_type;

	return *this;
}

//////////////////////////////////////////
void PSeg::print(std::ostream &o) const
{
	o << "<Physical segment "
	  << std::showbase << m_name
	  << ", from: " << std::hex 
      << m_lma
      << ", size : "  << m_length 
      << ", type : "  << m_type 
      << ", containing: "<< std::endl;
        std::vector<VSeg>::const_iterator it;
        for(it = m_vsegs.begin(); it < m_vsegs.end(); it++)
    o << " " << *it << std::endl;

    o << ">";
}

////////////////////////////////////
PSeg::PSeg( const std::string &name,
            paddr_t           lma,
            paddr_t           length,
            size_t            type)
{
    m_name = name;
    m_length = length;
    m_type = type;
    m_lma = lma;
}

////////////////////////////////////
PSeg::PSeg(const std::string &name):
      m_lma(0),
      m_length(0) {
    m_name = name;
}


////////////////////////////////////
PSeg::PSeg(const PSeg& pseg) {
    m_name = pseg.m_name;
    m_length = pseg.m_length;
    m_lma = pseg.m_lma;
    m_vsegs = pseg.m_vsegs;
    m_type = pseg.m_type;

    (*this) = pseg;
}

PSeg::~PSeg() {}



// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

