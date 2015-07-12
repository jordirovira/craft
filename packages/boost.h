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


const std::string s_boost_version = "boost_1_52_0";
const std::string s_boost_url = "http://downloads.sourceforge.net/project/boost/boost/1.52.0/boost_1_52_0.zip?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Fboost%2Ffiles%2Fboost%2F1.52.0%2F&ts=1436720498&use_mirror=vorboss";


std::shared_ptr<BuiltTarget> boost_custom_build(ContextPlan& ctx, Target_Base& thisTarget, unsigned options);


void craft( Context& ctx, unsigned options = options::static_link )
{
    std::string option_string;
    option_string += " -lboost_filesystem -lboost_thread -lboost_timer -lboost_chrono -lboost_iostreams -lpthread";

    ctx.extern_dynamic_library("boost_system")
            .export_include(ctx.get_current_path()+FileSeparator()+"boost-source"+FileSeparator()+s_boost_version)
            .library_path( [](const ContextPlan& ctx){ return ctx.get_current_path()+FileSeparator()+ctx.get_current_configuration()+"/boost/lib"; })
            .use("boost-build")
            ;

    // This task makes sure boost is built with its own build system for the necessary building conditions
    ctx.target("boost-build")
            .use("boost-source")
            .custom_build( [=]( ContextPlan& ctx, Target_Base& thisTarget ) { return boost_custom_build(ctx,thisTarget,options); } );

    // This tasks make sure boost source is available
    ctx.unarchive("boost-source")
            .archive("boost-download")
            ;

    // This tasks make sure boost is downloaded
    ctx.download("boost-download")
            .url(s_boost_url)
            ;

}


std::shared_ptr<BuiltTarget> boost_custom_build( ContextPlan& ctx, Target_Base& thisTarget, unsigned options )
{
    auto res = std::make_shared<BuiltTarget>();

    std::shared_ptr<Node> outputNode = std::make_shared<Node>();
    res->m_outputNode = outputNode;

    // \todo: doesn't work with custom configurations
    std::string stageDir = ctx.get_current_path()+FileSeparator()+ctx.get_current_configuration()+"/boost/";

    // \todo
    std::string boostAddressModel = "64";
    std::string boostToolset = "darwin";

    // libraries to build
    std::vector<std::string> boostLibs = { "filesystem", "date_time", "system", "timer", "program_options", "random",
                  "thread", "chrono", "iostreams" };

    outputNode->m_absolutePath = stageDir;

    // Check if target already exists
    if (!FileExists(stageDir))
    {
        // Generate dependencies
        std::vector<std::shared_ptr<Task>> reqs;
        for( const auto& u: thisTarget.m_uses )
        {
            std::shared_ptr<BuiltTarget> usedTarget = ctx.get_built_target( u );
            assert( usedTarget );

            reqs.insert( reqs.end(), usedTarget->m_outputTasks.begin(), usedTarget->m_outputTasks.end() );
        }

        // \todo: this use of uses is questionable
        // locate boost build tool
        std::string workingPath  = ctx.get_built_target(thisTarget.m_uses[0])->m_outputNode->m_absolutePath
                + FileSeparator() + "boost_1_52_0" + FileSeparator();

        std::string command  = ctx.get_built_target(thisTarget.m_uses[0])->m_outputNode->m_absolutePath
                + FileSeparator() + "boost_1_52_0" + FileSeparator() + ctx.get_host_platform()->get_program_file_name("b2");

        std::string bootstrapCommand  = ctx.get_built_target(thisTarget.m_uses[0])->m_outputNode->m_absolutePath
                + FileSeparator() + "boost_1_52_0" + FileSeparator() + "bootstrap.sh";

        // build argument list
        std::vector<std::string> args;
        args.push_back( "--stagedir="+stageDir );
        args.push_back( "variant="+ctx.get_current_configuration() );
        args.push_back( (options & options::static_link) ? "link=static" : "link=dynamic" );
        args.push_back("--build-dir="+stageDir);
        args.push_back("--layout=system");
        args.push_back("threading=multi");
        args.push_back("address-model="+boostAddressModel);
        args.push_back("toolset="+boostToolset);
//        args.push_back("-s");
//        args.push_back("BZIP2_SOURCE="+externPath+"/bzip2-1.0.6");
//        args.push_back("-s");
//        args.push_back("ZLIB_SOURCE="+externPath+"/zlib-1.2.8");
        for( const auto& lib: boostLibs)
        {
            args.push_back("--with-"+lib );
        }

        // Generate the task
        auto task = std::make_shared<Task>( "boostbuild", outputNode,
                                         [=]()
        {
            bool result = 0;

            // Run the command
            try
            {
                // If the b2 tool is not there, try to bootstrap it
                if (!FileExists(command) )
                {
                    std::vector<std::string> bootstrapArgs;

                    std::string out, err;
                    result = Run( workingPath, bootstrapCommand, bootstrapArgs,
                         [&out](const char* text){ out += text; },
                         [&err](const char* text){ err += text; } );

                    if (out.size())
                    {
                        AXE_SCOPED_SECTION(stdout);
                        AXE_LOG_LINES( "stdout", axe::L_Verbose, out );
                    }

                    if (err.size())
                    {
                        AXE_SCOPED_SECTION(stderr);
                        AXE_LOG_LINES( "stderr", axe::L_Verbose, err );
                    }

                }

                if (result==0)
                {
                    std::string out, err;
                    result = Run( workingPath, command, args,
                         [&out](const char* text){ out += text; },
                         [&err](const char* text){ err += text; } );

                    if (out.size())
                    {
                        AXE_SCOPED_SECTION(stdout);
                        AXE_LOG_LINES( "stdout", axe::L_Verbose, out );
                    }

                    if (err.size())
                    {
                        AXE_SCOPED_SECTION(stderr);
                        AXE_LOG_LINES( "stderr", axe::L_Verbose, err );
                    }
                }
            }
            catch(...)
            {
                AXE_LOG( "run", axe::L_Error, "Execution failed!" );
                result = -1;
            }

            return result;
        });

        task->m_requirements.insert( task->m_requirements.end(), reqs.begin(), reqs.end() );
        res->m_outputTasks.push_back( task );
    }

    return res;
}


}}

