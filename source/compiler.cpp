
#include "compiler.h"

#include "craft_private.h"
#include "target.h"
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

    add_configuration("debug",{"-O0","-g"},{});
    add_configuration("profile",{"-O3","-g"},{});
    add_configuration("release",{"-O3"},{});
    m_current_configuration=0;
}


void Compiler::set_configuration( const std::string& name )
{
    m_current_configuration = -1;
    for (std::size_t i=0; i<m_configurations.size(); ++i)
    {
        if (m_configurations[i].m_name==name)
        {
            m_current_configuration = i;
        }
    }
}


void Compiler::add_configuration( const std::string& name, const std::vector<std::string>& compileFlags, const std::vector<std::string>& linkFlags )
{
    Configuration data;
    data.m_name = name;
    data.m_compileFlags = compileFlags;
    data.m_linkFlags = linkFlags;
    m_configurations.push_back(data);
}


void Compiler::build_compile_argument_list( std::vector<std::string>& args, const std::string& source, const std::string& target, const std::vector<std::string>& includePaths )
{
    args.push_back("-std=c++11");

    // TODO: Not always, only for dynlibs
    args.push_back("-fPIC");

    args.push_back("-c");

    // TODO: Force c++
    args.push_back("-x");
    args.push_back("c++");

    // Configuration flags
    if (m_current_configuration>=0 && m_current_configuration<m_configurations.size())
    {
        const auto& f = m_configurations[m_current_configuration].m_compileFlags;
        args.insert( args.end(), f.begin(), f.end() );
    }

    args.push_back(source);
    args.push_back("-I");
    args.push_back(".");

    for (size_t i=0; i<includePaths.size(); ++i)
    {
        args.push_back("-I");
        args.push_back(includePaths[i]);
    }
}


int Compiler::get_compile_dependencies( NodeList& deps, const std::string& source, const std::string& target, const std::vector<std::string>& includePaths )
{
    AXE_SCOPED_SECTION(get_deps);

    int result = 0;

    std::vector<std::string> args;
    build_compile_argument_list(args,source,target,includePaths);

    args.push_back("-MM");

    try
    {
        std::string out, err;
        result = Run( "", m_exec, args,
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

                    std::shared_ptr<Node> targetNode = std::make_shared<Node>();
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
        result = -1;
    }

    return result;
}


int Compiler::compile( const std::string& source, const std::string& target, const std::vector<std::string>& includePaths )
{
    AXE_SCOPED_SECTION(compile);

    int result = 0;

    std::vector<std::string> args;
    build_compile_argument_list(args,source,target,includePaths);

    args.push_back("-o");
    args.push_back(target);

    try
    {
        std::string out, err;
        result = Run( "", m_exec, args,
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
        result = -1;
    }

    return result;
}


int Compiler::get_link_program_dependencies( NodeList& deps,
                                              const NodeList& objects,
                                              const std::vector<std::shared_ptr<BuiltTarget>>& uses)
{
    AXE_SCOPED_SECTION(get_deps);

    int result = 0;

    deps = objects;

    for (size_t i=0; i<uses.size(); ++i)
    {
        Target_Base* target = uses[i]->m_sourceTarget.get();
        if ( dynamic_cast<DynamicLibraryTarget*>(target) )
        {
            deps.push_back(uses[i]->m_outputNode);
        }
        else if ( dynamic_cast<ExternDynamicLibraryTarget*>(target) )
        {
        }
        else if ( dynamic_cast<StaticLibraryTarget*>(target) )
        {
            deps.push_back(uses[i]->m_outputNode);
        }
        else
        {
            // Unsuported use
            AXE_LOG( "Compiler", axe::L_Error, "Program uses an unknown target type [%s].", target->m_name.c_str() );
        }
    }

    return result;
}


int Compiler::link_program( const std::string& target,
                             const NodeList& objects,
                             const std::vector<std::shared_ptr<BuiltTarget>>& uses )
{
    AXE_SCOPED_SECTION(link_program);

    int result = 0;

    // Gather library options
//    std::vector<std::string> libraryOptions;
//    for( size_t u=0; u<uses.size(); ++u )
//    {
//        std::shared_ptr<BuiltTarget> usedTarget = uses[u];
//        assert( usedTarget );
//        for( size_t p=0; p<usedTarget->m_sourceTarget->m_export_library_options.size(); ++p )
//        {
//            split( usedTarget->m_sourceTarget->m_export_library_options[p], "\t\n ", libraryOptions );
//        }
//    }

    std::vector<std::string> args;
    args.push_back("-B");
    args.push_back("/usr/bin");
    args.push_back("-o");
    args.push_back(target);

    for (size_t i=0; i<objects.size(); ++i)
    {
         args.push_back(objects[i]->m_absolutePath);
    }

//    for (size_t i=0; i<libraryOptions.size(); ++i)
//    {
//         args.push_back(libraryOptions[i]);
//    }

    for (size_t i=0; i<uses.size(); ++i)
    {
        Target_Base* target = uses[i]->m_sourceTarget.get();
        if ( auto lib = dynamic_cast<DynamicLibraryTarget*>(target) )
        {
            args.push_back("-l");
            args.push_back(lib->m_name);

            // We need to add the search path for the library
            std::string pathToLib = FileGetPath( uses[i]->m_outputNode->m_absolutePath );
            //AXE_LOG( "compiler", axe::L_Verbose, "Used dynamic library output node [%s].", lib->m_outputNodes[0]->m_absolutePath.c_str() );
            //AXE_LOG( "compiler", axe::L_Verbose, "Used dynamic library ouput node path [%s].", pathToLib.c_str() );
            args.push_back("-L"+pathToLib);
        }
        else if ( auto lib = dynamic_cast<ExternDynamicLibraryTarget::Built*>(uses[i].get()) )
        {
            if (lib->m_library_path.size())
            {
                args.push_back("-L"+lib->m_library_path);
            }
            args.push_back("-l");
            args.push_back(target->m_name);
        }
        else if ( dynamic_cast<StaticLibraryTarget*>(target) )
        {
            args.push_back(uses[i]->m_outputNode->m_absolutePath);
        }
        else
        {
            // Unsuported use
            AXE_LOG( "Compiler", axe::L_Error, "Program uses an unknown target type [%s].", target->m_name.c_str() );
        }
    }

    try
    {
        std::string out, err;
        result = Run( "", m_exec, args,
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
        result = -1;
    }

    return result;
}



int Compiler::get_link_static_library_dependencies( NodeList& deps,
                               const std::string& target,
                               const NodeList& objects )
{
    AXE_SCOPED_SECTION(get_deps);
    deps = objects;

    return 0;
}


int Compiler::link_static_library( const std::string& target, const NodeList& objects )
{
    AXE_SCOPED_SECTION(link_static_lib);

    int result = 0;

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
        result = Run( "", m_arexec, args,
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
        result = -1;
    }

    return result;
}


int Compiler::get_link_dynamic_library_dependencies( NodeList& deps,
                               const std::string& target,
                               const NodeList& objects,
                               const std::vector<std::shared_ptr<BuiltTarget>>& uses )
{
    AXE_SCOPED_SECTION(get_deps);

    int result = 0;

    deps = objects;

    for (size_t i=0; i<uses.size(); ++i)
    {
        Target_Base* target = uses[i]->m_sourceTarget.get();
        if ( dynamic_cast<DynamicLibraryTarget*>(target) )
        {
            deps.push_back(uses[i]->m_outputNode);
        }
        else if ( dynamic_cast<ExternDynamicLibraryTarget*>(target) )
        {
        }
        else
        {
            // Unsuported use
            AXE_LOG( "Compiler", axe::L_Error, "Dynamic library uses an unknown target type [%s].", target->m_name.c_str() );
        }
    }

    return result;
}


int Compiler::link_dynamic_library( const std::string& target,
                                     const NodeList& objects,
                                     const std::vector<std::shared_ptr<BuiltTarget>>& uses )
{
    AXE_SCOPED_SECTION(link_shared_lib);

    int result = 0;

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
        Target_Base* target = uses[i]->m_sourceTarget.get();
        if ( auto lib = dynamic_cast<DynamicLibraryTarget*>(target) )
        {
            args.push_back("-l");
            args.push_back(lib->m_name);

            // We need to add the search path for the library
            std::string pathToLib = FileGetPath( uses[i]->m_outputNode->m_absolutePath );
            //AXE_LOG( "compiler", axe::L_Verbose, "Used dynamic library output node [%s].", lib->m_outputNodes[0]->m_absolutePath.c_str() );
            //AXE_LOG( "compiler", axe::L_Verbose, "Used dynamic library ouput node path [%s].", pathToLib.c_str() );
            args.push_back("-L"+pathToLib);
        }
        else if ( auto lib = dynamic_cast<ExternDynamicLibraryTarget::Built*>(uses[i].get()) )
        {
            if (lib->m_library_path.size())
            {
                args.push_back("-L"+lib->m_library_path);
            }
            args.push_back("-l");
            args.push_back(target->m_name);
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
        result = Run( "", m_exec, args,
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
        result = -1;
    }

    return result;
}

