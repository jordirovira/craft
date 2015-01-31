
#include <iostream>

#include "axe.h"
#include "craft_core.h"

#include <cassert>

using namespace std;


// TODO: Move to platform
#include <dlfcn.h>


AXE_IMPLEMENT();



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
    typedef void (*CraftMethod)( const char* workspace );
    CraftMethod craftMethod = (CraftMethod)method;
    craftMethod("..");
}


int main( int argc, const char** argv )
{
    AXE_INITIALISE("craft",0);
    craft_core_initialize( axe::s_kernel );

    AXE_LOG( "Test", axe::L_Verbose, "Hello axe!" );

    // Build the craft framework if necessary
//    std::string env = "./env";

//    auto& target = ctx.static_library( "craft-framework" )
//            .source( root+"craft.cpp" )
//            .use( "craft-framework" );

    // Parse arguments
    std::string workspace = FileGetCurrentPath();
    int arg = 0;
    while (arg<argc)
    {
        if ( argv[arg]==std::string("-w") )
        {
            if (arg+1<argc)
            {
                workspace = argv[arg+1];
                ++arg;
            }
        }
        ++arg;
    }

    // Locate the craft file
    std::string root = "./";

    if ( !FileExists( root+"craftfile" ) )
    {
        cout << "Couldn't find craftfile" << endl;
    }
    else
    {
        // Compile it into a dynamic library
        std::shared_ptr<Context> ctx = Context::Create( true, false );

        // At some point this should point at the craft environment
        ctx->extern_dynamic_library( "craft-core" )
                .export_include( workspace+"/source" )
                //.library_path(ctx->get_current_path())
                .library_path( workspace+"/build/waf/OSX-x86_64-gcc6.0.0/debug/")
                ;

        auto& target = ctx->dynamic_library( "craftfile" )
                .source( root+"craftfile" )
                .use( "craft-core" );

        target.build(*ctx);

        // Load and run the dynamic library entry method
        {
            AXE_SCOPED_SECTION("Running craftfile");
            std::string craftLibrary = target.GetOutputNodes()[0]->m_absolutePath;
            LoadAndRun( craftLibrary.c_str(), "craft_entry" );
        }
    }

    AXE_FINALISE();

    // Done
    return 0;
}

