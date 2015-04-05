
#include "craft_private.h"

#include "axe.h"
#include "platform.h"

#include <string>
#include <sstream>
#include <vector>
#include <regex>


Compiler::Compiler()
{
    m_exec="/usr/bin/g++";
    m_arexec="/usr/bin/ar";
}


void Compiler::build_argument_list( std::vector<std::string>& args, const std::string& source, const std::string& target, const std::vector<std::string>& includePaths )
{
    args.push_back("-std=c++11");

    // TODO: Not always, only for dynlibs
    args.push_back("-fPIC");

    args.push_back("-c");

    // TODO: Force c++
    args.push_back("-x");
    args.push_back("c++");

    args.push_back(source);
    args.push_back("-I");
    args.push_back(".");

    for (size_t i=0; i<includePaths.size(); ++i)
    {
        args.push_back("-I");
        args.push_back(includePaths[i]);
    }
}


void Compiler::get_compile_dependencies( NodeList& deps, const std::string& source, const std::string& target, const std::vector<std::string>& includePaths )
{
    AXE_SCOPED_SECTION(get_deps);

    std::vector<std::string> args;
    build_argument_list(args,source,target,includePaths);

    args.push_back("-MM");

    try
    {
        std::string out, err;
        Run( m_exec, args,
             [&out](const char* text){ out += text; },
             [&err](const char* text){ err += text; } );

        if (err.size())
        {
            AXE_SCOPED_SECTION(stderr);
            AXE_LOG_LINES( "stderr", axe::L_Verbose, err );
        }

        // Process execution output to get the dependencies
        std::regex rule_regex("\\s\\n");
        std::sregex_token_iterator rule_begin(out.begin(), out.end(), rule_regex, -1);
        std::sregex_token_iterator rule_end;
        //AXE_LOG( "regex", axe::L_Verbose, "%d rules", std::distance(rule_begin,rule_end) );

        while( rule_begin!=rule_end )
        {
            //AXE_LOG( "rule", axe::L_Verbose, rule_begin->str() );

            std::string rule = rule_begin->str();

            std::regex dep_regex(R"([\s\n\\]+|([.\w]+:))");
            std::sregex_token_iterator dep_begin(rule.begin(), rule.end(), dep_regex, -1);
            std::sregex_token_iterator dep_end;

            while( dep_begin!=dep_end )
            {
                std::string dep = dep_begin->str();
                if (dep.size())
                {
                    //AXE_LOG( "dep", axe::L_Verbose, dep );

                    std::string dep_absolute_path = FileGetCurrentPath();
                    dep_absolute_path += FileSeparator()+dep;

                    std::shared_ptr<FileNode> targetNode = std::make_shared<FileNode>();
                    targetNode->m_absolutePath = dep_absolute_path;

                    deps.push_back(targetNode);
                }
                ++dep_begin;
            }

            ++rule_begin;
        }

    }
    catch(...)
    {
        AXE_LOG( "deps", axe::L_Error, "Execution failed!" );
    }
}


void Compiler::compile( const std::string& source, const std::string& target, const std::vector<std::string>& includePaths )
{
    AXE_SCOPED_SECTION(compile);

    std::vector<std::string> args;
    build_argument_list(args,source,target,includePaths);

    args.push_back("-o");
    args.push_back(target);

    try
    {
        std::string out, err;
        Run( m_exec, args,
             [&out](const char* text){ out += text; },
             [&err](const char* text){ err += text; } );

        {
            AXE_SCOPED_SECTION(stdout);
            AXE_LOG_LINES( "stdout", axe::L_Verbose, out );
        }

        {
            AXE_SCOPED_SECTION(stderr);
            AXE_LOG_LINES( "stderr", axe::L_Verbose, err );
        }
    }
    catch(...)
    {
        AXE_LOG( "run", axe::L_Error, "Execution failed!" );
    }
}


void Compiler::get_link_program_dependencies( NodeList& deps,
                                              Context& ctx,
                                              const std::string& target,
                                              const NodeList& objects,
                                              const std::vector<std::string>& uses)
{
    AXE_SCOPED_SECTION(get_deps);

    deps = objects;

    for (size_t i=0; i<uses.size(); ++i)
    {
        Target* target = ctx.get_target( uses[i] ).get();
        if ( auto lib = dynamic_cast<DynamicLibraryTarget*>(target) )
        {
            deps.push_back(lib->m_outputNodes[0]);
        }
        else if ( auto lib = dynamic_cast<ExternDynamicLibraryTarget*>(target) )
        {
        }
        else
        {
            // Unsuported use
            AXE_LOG( "Compiler", axe::L_Error, "Dynamic library uses an unknown target type [%s].", target->m_name.c_str() );
        }
    }
}


void Compiler::link_program( Context& ctx,
                             const std::string& target,
                             const NodeList& objects,
                             const std::vector<std::string>& uses )
{
    AXE_SCOPED_SECTION(link);

    // Gather library options
    std::vector<std::string> libraryOptions;
    for( size_t u=0; u<uses.size(); ++u )
    {
        std::shared_ptr<Target> usedTarget = ctx.get_target( uses[u] );
        assert( usedTarget );
        for( size_t p=0; p<usedTarget->m_export_library_options.size(); ++p )
        {
            split( usedTarget->m_export_library_options[p], "\t\n ", libraryOptions );
        }
    }

    std::vector<std::string> args;
    args.push_back("-B");
    args.push_back("/usr/bin");
    args.push_back("-o");
    args.push_back(target);

    for (size_t i=0; i<objects.size(); ++i)
    {
         args.push_back(objects[i]->m_absolutePath);
    }

    for (size_t i=0; i<libraryOptions.size(); ++i)
    {
         args.push_back(libraryOptions[i]);
    }

    try
    {
        std::string out, err;
        Run( m_exec, args,
             [&out](const char* text){ out += text; },
             [&err](const char* text){ err += text; } );

        {
            AXE_SCOPED_SECTION(stdout);
            AXE_LOG_LINES( "stdout", axe::L_Verbose, out );
        }

        {
            AXE_SCOPED_SECTION(stderr);
            AXE_LOG_LINES( "stderr", axe::L_Verbose, err );
        }
    }
    catch(...)
    {
        AXE_LOG( "run", axe::L_Error, "Execution failed!" );
    }

}



void Compiler::get_link_static_library_dependencies( NodeList& deps,
                               const std::string& target,
                               const NodeList& objects )
{
    AXE_SCOPED_SECTION(get_deps);
    deps = objects;
}


void Compiler::link_static_library( const std::string& target, const NodeList& objects )
{
    AXE_SCOPED_SECTION(link_static_lib);

    std::vector<std::string> args;
    args.push_back("-r");
    args.push_back("-c");
    args.push_back("-s");
    args.push_back(target);

    for (size_t i=0; i<objects.size(); ++i)
    {
         args.push_back(objects[i]->m_absolutePath);
    }

    try
    {
        std::string out, err;
        Run( m_arexec, args,
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
    catch(...)
    {
        AXE_LOG( "run", axe::L_Error, "Execution failed!" );
    }
}


void Compiler::get_link_dynamic_library_dependencies( NodeList& deps,
                               Context& ctx,
                               const std::string& target,
                               const NodeList& objects,
                               const std::vector<std::string>& uses )
{
    AXE_SCOPED_SECTION(get_deps);

    deps = objects;

    for (size_t i=0; i<uses.size(); ++i)
    {
        Target* target = ctx.get_target( uses[i] ).get();
        if ( auto lib = dynamic_cast<DynamicLibraryTarget*>(target) )
        {
            deps.push_back(lib->m_outputNodes[0]);
        }
        else if ( auto lib = dynamic_cast<ExternDynamicLibraryTarget*>(target) )
        {
        }
        else
        {
            // Unsuported use
            AXE_LOG( "Compiler", axe::L_Error, "Dynamic library uses an unknown target type [%s].", target->m_name.c_str() );
        }
    }
}


void Compiler::link_dynamic_library( Context& ctx,
                                     const std::string& target,
                                     const NodeList& objects,
                                     const std::vector<std::string>& uses )
{
    AXE_SCOPED_SECTION(link_shared_lib);

    // Gather parameters
    std::vector<std::string> args;
    args.push_back("-B");
    args.push_back("/usr/bin");
    args.push_back("-shared");
    args.push_back("-o");
    args.push_back(target);

    for (size_t i=0; i<objects.size(); ++i)
    {
         args.push_back(objects[i]->m_absolutePath);
    }

    for (size_t i=0; i<uses.size(); ++i)
    {
        Target* target = ctx.get_target( uses[i] ).get();
        if ( auto lib = dynamic_cast<DynamicLibraryTarget*>(target) )
        {
            args.push_back("-l");
            args.push_back(lib->m_name);

            // We need to add the search path for the library
            std::string pathToLib = FileGetPath( lib->m_outputNodes[0]->m_absolutePath );
            //AXE_LOG( "compiler", axe::L_Verbose, "Used dynamic library output node [%s].", lib->m_outputNodes[0]->m_absolutePath.c_str() );
            //AXE_LOG( "compiler", axe::L_Verbose, "Used dynamic library ouput node path [%s].", pathToLib.c_str() );
            args.push_back("-L"+pathToLib);
        }
        else if ( auto lib = dynamic_cast<ExternDynamicLibraryTarget*>(target) )
        {
            if (lib->m_library_path.size())
            {
                args.push_back("-L"+lib->m_library_path);
            }
            args.push_back("-l");
            args.push_back(lib->m_name);
        }
        else
        {
            // Unsuported use
            AXE_LOG( "Compiler", axe::L_Error, "Dynamic library uses an unknown target type [%s].", target->m_name.c_str() );
        }
    }

    try
    {
        std::string out, err;
        Run( m_exec, args,
             [&out](const char* text){ out += text; },
             [&err](const char* text){ err += text; } );

        {
            AXE_SCOPED_SECTION(stdout);
            AXE_LOG_LINES( "stdout", axe::L_Verbose, out );
        }

        {
            AXE_SCOPED_SECTION(stderr);
            AXE_LOG_LINES( "stderr", axe::L_Verbose, err );
        }
    }
    catch(...)
    {
        AXE_LOG( "run", axe::L_Error, "Execution failed!" );
    }
}

