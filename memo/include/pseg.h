/* -*- c++ -*-
 *
 * GIET-VM_LGPL_HEADER_BEGIN
 * 
 * This file is part of the GIET-VMS, GNU LGPLv2.1.
 * 
 * The GIET-VM is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 of the License.
 * 
 * THe GIET-VM is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with SoCLib; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 * 
 * GIET-VM_LGPL_HEADER_END
 *
 * Copyright (c) UPMC, Lip6, SoC
 *         Mohamed Lamine Karaoui <Mohamed.Karaoui@lip6.fr>, 2012
 *          
 */
#ifndef GIET_MEMO_PSEG_H
#define GIET_MEMO_PSEG_H

#include <string>
#include <vector>
#include <ios>
#include <elfpp/section>

#include "stdint.h"
#include "mapping_info.h"


class MeMo;

//////////
class VSeg
{
    friend class PSegHandler;
    friend class PSeg;
    friend class MeMo;

    typedef unsigned long long paddr_t;

    std::string m_name;     // vseg name
    std::string m_file;     // file name
    uintptr_t   m_vma;      // Virtual address of the section to load in the binary file.
    paddr_t     m_lma;      // Physical address  
    size_t      m_length;
    size_t      m_type;
    bool        m_loadable; // loadable vseg ( code, data...)

public:

    int32_t     m_align;    // alignment of the first vobj
    bool        m_ident;    // identity mapping required if true

    const std::string& name() const;
    const std::string& file() const;
    uintptr_t vma() const;
    paddr_t lma() const;
    size_t length() const;
    size_t type() const;

    void print(std::ostream &o) const;
    friend std::ostream &operator<<( std::ostream &o, const VSeg &s )
    {
        s.print(o);
        return o;
    }

    VSeg& operator=(const VSeg &ref);

    VSeg();
    VSeg(const VSeg &ref);
    VSeg(std::string&   binaryName, 
         std::string&   name, 
         uintptr_t      vma, 
         size_t         length, 
         bool           loadable, 
         bool           ident);

    ~VSeg();
};

//////////
class PSeg
{
    std::string   m_name;
    paddr_t       m_lma;
    paddr_t       m_length;
    size_t        m_type;

    static size_t m_pageSize;

public:

    std::vector<VSeg> m_vsegs;

    const std::string& name() const;
    paddr_t lma() const;
    paddr_t length() const;
    size_t type() const;

    void check() const;

    void setName(std::string& name);
    void setLma(paddr_t lma);
    void setLength(paddr_t length);

    static paddr_t align(paddr_t toAlign, unsigned alignPow2);
    static paddr_t pageAlign(paddr_t toAlign);

    static void setPageSize(size_t pg) {
       m_pageSize = pg;
    }
    static size_t pageSize() {
       return m_pageSize;
    }

    void add(VSeg& vseg);    //add a VSeg
    void addIdent(VSeg& vseg);


    void print(std::ostream &o) const;

    friend std::ostream &operator<<(std::ostream &o, const PSeg &s )
    {
        s.print(o);
        return o;
    }
    PSeg & operator=(const PSeg &ref);

    PSeg();
    PSeg(const PSeg &ref );
    PSeg(const std::string &name);
    PSeg(const std::string &name,
         paddr_t lma,
         paddr_t length,
         size_t type);
    ~PSeg();
};


#endif /* GIET_MEMO_PSEG_H */
