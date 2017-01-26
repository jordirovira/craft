#ifndef CRAFT_PRIVATE_H
#define CRAFT_PRIVATE_H

#include "craft_core.h"
#include "compiler.h"

#include <algorithm>


// Split a string with a delimit set.
// It skips empty elements.
inline void split(const std::string &s, std::string delims, std::vector<std::string>& elems)
{
    std::string::const_iterator b = s.begin();
    std::string::const_iterator e = s.end();

    std::string::const_iterator c = b;
    while (c!=e)
    {
        if ( b!=c && std::find( delims.begin(), delims.end(), *c) != delims.end() )
        {
            elems.push_back( std::string( b, c ) );
            b=c;
            ++b;
        }
        ++c;
    }

    // Add the last element
    if (b!=c)
    {
        elems.push_back( std::string( b, c ) );
    }
}


#endif
