#ifndef PATH_H_
#define PATH_H_

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <iterator>


//class MeMo;

class PathHandler
{
    //friend class MeMo;

    std::string dirPath;
    char delim;

    std::vector<std::string>& split(const std::string &s, char delim, std::vector<std::string> &elems);

    std::vector<std::string> split(const std::string &s, char delim);

    //extract the path without the filename
    std::string getFilePath(const std::string& filepath);

public:

    ~PathHandler();
    PathHandler(const std::string& filepath);

    //return the fullPath relative to where execution is done.
    std::string getFullPath(const std::string& filepath) const;

    std::string getFileName(const std::string& filepath);
};


#endif
