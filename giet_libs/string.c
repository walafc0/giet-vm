//////////////////////////////////////////////////////////////////////////////////
// File     : string.c 
// Date     : 23/05/2013
// Author   : Alexandre JOANNOU, Laurent LAMBERT
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////
// char * strcpy ( char * destination, const char * source )
///////////////////////////////////////////////////////////////////////////////////
char * strcpy ( char * destination, const char * source )
{
    if (!destination || !source)
        return destination;

    while (*source)
        *(destination++) = *(source++);

    return destination;
}

///////////////////////////////////////////////////////////////////////////////////
// int strcmp ( const char * str1, const char * str2 )
///////////////////////////////////////////////////////////////////////////////////
int strcmp ( const char * str1, const char * str2 )
{
    if (!str1 || !str2)
        return -123456; // return a value out of the char's bounds

    while (*str1 && *str1 == *str2)
    {
        str1++;
        str2++;
    }

    return (*str1 - *str2);
}

///////////////////////////////////////////////////////////////////////////////////
// int strlen ( const char * str )
///////////////////////////////////////////////////////////////////////////////////
int strlen ( const char * str )
{
    const char *s = str;

    while (*s)
        s++;

    return (s - str);
}

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
