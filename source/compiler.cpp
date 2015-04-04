
#include "craft_private.h"

#include "axe.h"
#include "platform.h"

#include <string>
#include <sstream>
#include <vector>


Compiler::Compiler()
{
    m_exec="/usr/bin/g++";
    m_arexec="/usr/bin/ar";
}

void Compiler::compile( const std::string& source, const std::string& target, const std::vector<std::string>& includePaths )
{
    AXE_SCOPED_SECTION(compile);

    std::vector<std::string> args;
    args.push_back("-std=c++11");

    // TODO: Not always, only for dynlibs
    args.push_back("-fPIC");

    args.push_back("-c");

    // TODO: Force c++
    args.push_back("-x");
    args.push_back("c++");

    args.push_back(source);
    args.push_back("-o");
    args.push_back(target);
    args.push_back("-I");
    args.push_back(".");

    for (size_t i=0; i<includePaths.size(); ++i)
    {
        args.push_back("-I");
        args.push_back(includePaths[i]);
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


void Compiler::link_program( const std::string& target,
                             const NodeList& objects,
                             const std::vector<std::string>& libraryOptions )
{
    AXE_SCOPED_SECTION(link);

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
            AXE_LOG( "compiler", axe::L_Verbose, "Used Dynamic library output node [%s].", lib->m_outputNodes[0]->m_absolutePath.c_str() );
            AXE_LOG( "compiler", axe::L_Verbose, "Used Dynamic library ouput node path [%s].", pathToLib.c_str() );
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

