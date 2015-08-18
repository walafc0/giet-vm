//////////////////////////////////////////////////////////////////////////////////
// File     : stdlib.c 
// Date     : 05/12/2013
// Author   : Clément DEVIGNE
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>

/////////////////////////
int atoi(const char *str)
{
    int res  = 0; // Initialize result
    int sign = 1; // Initialize sign as positive
    int i    = 0; // Initialize index of first digit

    if (str[0] == '-') //If number is negative, then update sign
    {
        sign = -1;  
        i++;           // Also update index of first digit
    }

    for (; str[i] != '\0'; ++i) // Iterate through all digits and update the result
    {
        res = res*10 + str[i] - '0';
    }

    // Return result with sign
    return sign*res;
}

////////////////////////////
double atof(const char *str)
{
    const char *pstr = str;
    double res = 0;
    double exp = 0.1;
    short sign = 1;
    short dec = 0;

    while (*pstr != '\0')
    {
        if (*pstr == '-')
        {
            if (str != pstr) break;
            sign = -1;
        }
        
        else if (*pstr == '.')
        {
            if (dec) break;
            dec = 1;
        }
        
        else if (*pstr >= '0' && *pstr <= '9')
        {
            if (dec)
            {
                res = res + ((*pstr - '0')*exp);
                exp = exp / 10;
            }
            else
            {
                res = (res * 10) + (*pstr - '0');
            }
        }
        
        else
        {
            break;
        }
        pstr++;
    }
    return sign * res;
}

///////////////////////////////////////////////////////////////
void * memcpy(void *_dst, const void * _src, unsigned int size) 
{
    unsigned int * dst = _dst;
    const unsigned int * src = _src;
    if (!((unsigned int) dst & 3) && !((unsigned int) src & 3) )
    {
        while (size > 3) 
        {
            *dst++ = *src++;
            size -= 4;
        }
    }

    unsigned char *cdst = (unsigned char*)dst;
    unsigned char *csrc = (unsigned char*)src;

    while (size--) 
    {
        *cdst++ = *csrc++;
    }
    return _dst;
}

//////////////////////////////////////////////////////////
inline void * memset(void * dst, int s, unsigned int size) 
{
    char * a = (char *) dst;
    while (size--)
    {
        *a++ = (char)s;
    }
    return dst;
}

///////////////////////////////////
unsigned int strlen( char* string )
{
    unsigned int i = 0;
    while ( string[i] != 0 ) i++;
    return i;
}

///////////////////////////////
unsigned int strcmp( char * s1,
                     char * s2 )
{
    while (1)
    {
        if (*s1 != *s2) return 1;
        if (*s1 == 0)   break;
        s1++, s2++;
    }
    return 0;
}

/////////////////////////
char* strcpy( char* dest, 
              char* source )
{
    if (!dest || !source) return dest;

    while (*source)
    {
        *(dest) = *(source);
        dest++;
        source++;
    }
    *dest = 0;
    return dest;
}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
