
#include <iostream>

#include <boost/filesystem.hpp>

#include "craft_core.h"



using namespace std;


// TODO: Move to platform
#include <dlfcn.h>

// TODO: Move to platform
void LoadAndRun( const char* lib, const char* methodName )
{
    // Load the dynamic library
    void *libHandle = dlopen( lib, RTLD_LAZY | RTLD_LOCAL );
    assert( libHandle );

    dlerror();
    void *method = dlsym( libHandle, methodName );
    const char* error = dlerror();
    if (error)
    {
        std::cout << "Loading symbol error:" << error << std::endl;
    }

    assert( method );

    // Run it
    typedef void (*CraftMethod)();
    CraftMethod craftMethod = (CraftMethod)method;
    craftMethod();
}


int main( int argc, const char** argv )
{
    // Build the craft framework if necessary
//    std::string env = "./env";

//    auto& target = ctx.static_library( "craft-framework" )
//            .source( root+"craft.cpp" )
//            .use( "craft-framework" );


    // Locate the craft file
    std::string root = "./";

    if ( !boost::filesystem::exists( root+"craftfile" ) )
    {
        cout << "Couldn't find craftfile" << endl;
    }
    else
    {
        // Compile it into a dynamic library
        std::shared_ptr<Context> ctx = Context::Create( true, false );

        // At some point this should point at the craft environment
        ctx->extern_dynamic_library( "craft-core" )
                .export_include( boost::filesystem::current_path().string()+"/../source" );

        auto& target = ctx->dynamic_library( "craft" )
                .source( root+"craftfile" )
                .use( "craft-core" );

        target.build(*ctx);

        // Load and run the dynamic library entry method
        std::string craftLibrary = target.GetOutputNodes()[0]->m_absolutePath;
        LoadAndRun( craftLibrary.c_str(), "craft_entry" );
    }

    // Done
    return 0;
}

