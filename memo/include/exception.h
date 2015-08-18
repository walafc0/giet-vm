/* -*- c++ -*-
 *
 * Copyright (c) UPMC, Lip6, Asim
 *      copy of the soclib exception file, original writer:
 *          Nicolas Pouillon <nipo@ssji.net>, 2007
 *      modified by:
 *         Mohamed Lamine Karaoui <Mohamed.Karaoui@lip6.fr>, 2012
 *
 */

#ifndef _EXCEPTION_H_
#define _EXCEPTION_H_

#include <exception>
#include <string>
#include <iostream>

namespace exception {

class Exception
    : public std::exception
{
    std::string m_type;
    std::string m_message;

public:
    Exception( const std::string &type,
               const std::string &message )
            : m_type(type), m_message(message)
    {}

    virtual ~Exception() throw()
    {
    }

    virtual const char * what() const throw()
    {
        return m_message.c_str();
    }

    void print( std::ostream &o ) const
    {
        o << "<soclib::exception::" << m_type << ": " << m_message << ">";
    }

    friend std::ostream &operator << (std::ostream &o, const Exception &e)
    {
        e.print(o);
        return o;
    }
};

class ValueError
    : public Exception
{
public:
    ValueError( const std::string &message )
            : Exception( "ValueError", message )
    {}
};

class Collision
    : public Exception
{
public:
    Collision( const std::string &message )
            : Exception( "Collision", message )
    {}
};

class RunTimeError
    : public Exception
{
public:
    RunTimeError( const std::string &message )
            : Exception( "RunTimeError", message )
    {}
};

}

#endif /* SOCLIB_EXCEPTION_H_ */

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

