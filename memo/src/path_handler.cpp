/* -*- c++ -*-
 *TODO
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
 * SOCLIB_LGPL_HEADER_END
 *
 * Copyright (c) UPMC, Lip6, SoC
 *         Mohamed Lamine Karaoui <Mohamed.Karaoui@lip6.fr>, 2012
 */

#include "path_handler.h"
    
PathHandler::PathHandler(const std::string& filepath)
                        :delim('/')
{
   dirPath = getFilePath(filepath);
}
    
std::string PathHandler::getFullPath(const std::string& filepath) const
{
    return dirPath+filepath;
}

std::vector<std::string>& PathHandler::split(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) 
    {
        elems.push_back(item);
    }
    return elems;
}


std::vector<std::string> PathHandler::split(const std::string &s, char delim) 
{
    std::vector<std::string> elems;
    return split(s, delim, elems);
}


std::string PathHandler::getFilePath(const std::string& filepath)
{
    std::string path;

    std::vector<std::string> vst= split(filepath, delim);

    if(vst.size() < 2)
        return "";

    for (unsigned i=0; i<(vst.size()-1); i++)
        path+= vst.at(i);

    return path+'/';
}

std::string PathHandler::getFileName(const std::string& filepath)
{

    std::vector<std::string> vst= split(filepath, delim);

    return vst.back();
}
    
PathHandler::~PathHandler()
{
}
