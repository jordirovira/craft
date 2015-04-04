#pragma once

#include <craft_core.h>

#include <string>
#include <vector>
#include <atomic>
#include <memory>

namespace packages { namespace boost
{

enum options
{
    static_link     = 1 << 0,
    dynamic_link    = 1 << 1
};

void craft( Context& ctx, unsigned options )
{
    std::string option_string;
    option_string += " -lboost_filesystem -lboost_thread -lboost_timer -lboost_chrono -lboost_iostreams -lpthread";

    ctx.extern_dynamic_library("boost-filesystem")
            .export_include("/usr/include")
            .export_library_options( option_string )
            ;

    ctx.extern_dynamic_library("boost_system")
            .export_include("Extern/boost_1_52_0")
            .library_path("/Extern/boost_1_52_0")
            .export_library_options( "-lboost_system" )
            ;


    ctx.target("boost")
            .use("boost_system")
            ;
}


}}

