#include <iostream>
#include "memo.h"

void print_help();

int main(int argc, char *argv[])
{
    std::string map_path("");
    std::string soft_path("soft.elf");
    bool show = false;
    bool show_map = false;

    if (argc > 1)
    {
        for( int n=1 ; n<argc ; n++ )
        {
            if( (strcmp(argv[n],"-o") == 0) && (n+1<argc) )
            {
                soft_path = argv[n+1];
                n++;
            }
            else 
            if( (strcmp(argv[n],"-v") == 0))
                show = true;
            else 
            if( (strcmp(argv[n],"-sm") == 0))
                show_map = true;
            else
                map_path = std::string(argv[n]);
            
        }
    }else
    {
        print_help();
    }

    //std::cout << map_path << std::endl;
    //std::cout << soft_path << std::endl;

    MeMo memo(map_path);

    memo.buildSoft(soft_path);

    if(show)
        std::cout << memo << std::endl;

    if(show_map)
        memo.print_mapping();

    std::cout << "Done: " << soft_path << std::endl;

    return 0;
}

void print_help()
{

    std::cout << "***Arguments are:***" << std::endl;
    std::cout << "  +mandatory argument:" << std::endl;
    std::cout << "      `mappath`: mapping info structure path" << std::endl;
    std::cout << "  +other argument:" << std::endl;
    std::cout << "      \"-v\" for a verbose printing" << std::endl;
    std::cout << "      \"-o\" output filename (default soft.bin)" << std::endl;
    std::cout << "      \"-sm\" print the content of each physical segment" << std::endl;
    std::cout << "***Examples:***" << std::endl;
    std::cout << "./memo map.bin" << std::endl;
    std::cout << "./memo map.bin -v" << std::endl;
    std::cout << "./memo map.bin -v -o mysoft.bin -sm" << std::endl;
}
