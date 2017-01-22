
#include <iostream>

#include "axe.h"
#include "craft_core.h"
#include "target.h"
#include "platform.h"

#include <cassert>

using namespace std;

AXE_IMPLEMENT();


int main( int argc, const char** argv )
{
    AXE_INITIALISE("craft",0,0);

    // Build the craft framework if necessary
//    std::string env = "./env";

//    auto& target = ctx.static_library( "craft-framework" )
//            .source( root+"craft.cpp" )
//            .use( "craft-framework" );

    // Parse arguments
    std::string craftProgramLocation;
    if(argc>0 && argv && argv[0])
    {
        craftProgramLocation = FileGetPath(argv[0]);
    }

    std::string workspace = FileGetCurrentPath();
    std::vector<const char*> configurations;
    std::vector<const char*> targets;
    {
        int arg = 1;
        while (arg<argc)
        {
            // Workspace
            if ( argv[arg]==std::string("-w") )
            {
                if (arg+1<argc)
                {
                    // Do we really have a workspace path, or do we have another option?
                    if (argv[arg+1][0]!='-')
                    {
                        workspace = argv[arg+1];
                        ++arg;
                    }
                }
            }
            // Configuration
            else if (argv[arg]==std::string("-c") )
            {
                if (arg+1<argc)
                {
                    // Do we really have a configuration name, or do we have another option?
                    if (argv[arg+1][0]!='-')
                    {
                        configurations.push_back( argv[arg+1] );
                        ++arg;
                    }
                }
            }
            // Target
            else
            {
               targets.push_back( argv[arg+1] );
            }

            ++arg;
        }

        configurations.push_back( nullptr );
        targets.push_back( nullptr );
    }


    // Locate the craft file
    std::string root = "./";

    if ( !FileExists( root+"craftfile" ) )
    {
        AXE_LOG( "craft", axe::Level::Fatal, "Couldn't find craftfile in [%s].", root.c_str() );
    }
    else
    {
        // Compile it into a dynamic library
        std::shared_ptr<Context> ctx = std::make_shared<Context>( true, false );

        // \TODO
        ctx->extern_dynamic_library( "craft-core" )
                .library_path( craftProgramLocation )
                .export_include( workspace+"/source" )  // To find craft.h
                .export_include( workspace )            // To find packages
                ;

        DynamicLibraryTarget& target = ctx->dynamic_library( "craftfile" );
        target.source( root+"craftfile" )
                .source(workspace+"/source/craft.cpp")
                .use( "craft-core" );

        std::shared_ptr<ContextPlan> ctxPlan = std::make_shared<ContextPlan>( *ctx );
        ctxPlan->set_current_configuration( "debug" );
        auto builtTarget = ctxPlan->get_built_target(target.m_name);
        if (builtTarget->has_errors())
        {
            AXE_LOG( "craft", axe::Level::Fatal, "Failed to build the craftfile." );
        }
        else
        {
            int buildCraftFileResult = ctxPlan->run();

            if (buildCraftFileResult!=0)
            {
                AXE_LOG( "craft", axe::Level::Fatal, "Failed to build the craftfile." );
            }
            else
            {
                // Load and run the dynamic library entry method
                AXE_SCOPED_SECTION_DETAILED(RunningCraftfile,"Running craftfile");
                std::string craftLibrary = builtTarget->m_outputNode->m_absolutePath;
                LoadAndRun( craftLibrary.c_str(), "craft_entry", workspace.c_str(), &configurations[0], &targets[0] );
            }
        }
    }

    AXE_FINALISE();

    // Done
    return 0;
}

