
#include "craft_private.h"

#include "axe.h"
#include "platform.h"

#include <string>
#include <sstream>
#include <vector>

#include <boost/process.hpp>


AXE_IMPLEMENT()

void craft_core_initialize( axe::Kernel* log_kernel )
{
    axe::s_kernel = log_kernel;
}


Target& Target::source( const std::string& files )
{
    m_sources.push_back( files );
    return *this;
}

Target& Target::use( const std::string& targets )
{
    m_uses.push_back( targets );
    return *this;
}

Target& Target::export_include( const std::string& paths )
{
    m_export_includes.push_back( paths );
    return *this;
}

Target& Target::export_library_options( const std::string& options )
{
    m_export_library_options.push_back( options );
    return *this;
}

Target& Target::library_path( const std::string& path )
{
    m_library_path = path;
    return *this;
}

void Target::build( Context& ctx )
{
    m_built = true;
}


void CppTarget::build( Context& ctx )
{
    AXE_LOG( "Build", axe::L_Debug, "Building CPP target." );

    m_built = true;

    NodeList objects;

    // Build dependencies
    for( size_t u=0; u<m_uses.size(); ++u )
    {
        std::shared_ptr<Target> usedTarget = ctx.get_target( m_uses[u] );
        assert( usedTarget );
        if (!usedTarget->Built())
        {
            usedTarget->build( ctx );
        }
    }

    // Gather include paths
    std::vector<std::string> includePaths;
    for( size_t u=0; u<m_uses.size(); ++u )
    {
        std::shared_ptr<Target> usedTarget = ctx.get_target( m_uses[u] );
        assert( usedTarget );
        for( size_t p=0; p<usedTarget->m_export_includes.size(); ++p )
        {
            //boost::split(includePaths, usedTarget->m_export_includes[p], boost::is_any_of("\t\n "));
            split( usedTarget->m_export_includes[p], "\t\n ", includePaths );
        }
    }

    // Get dependencies

    // Build own sources
    for( size_t i=0; i<m_sources.size(); ++i )
    {
        std::vector<std::string> sourceFiles;
        //boost::split(sourceFiles, m_sources[i], boost::is_any_of("\t\n "));
        split( m_sources[i], "\t\n ", sourceFiles );

        // Compile
        for( std::vector<std::string>::const_iterator it=sourceFiles.begin();
             it!= sourceFiles.end();
             ++it )
        {
            ctx.object( *it, objects, includePaths );
        }
    }

    link( ctx, objects );
}


void ProgramTarget::link( Context& ctx, const NodeList& objects )
{
    // Gather library options
    std::vector<std::string> libraryOptions;
    for( size_t u=0; u<m_uses.size(); ++u )
    {
        std::shared_ptr<Target> usedTarget = ctx.get_target( m_uses[u] );
        assert( usedTarget );
        for( size_t p=0; p<usedTarget->m_export_library_options.size(); ++p )
        {
            split( usedTarget->m_export_library_options[p], "\t\n ", libraryOptions );
        }
    }

    std::string target = ctx.get_current_path();
    target += FileSeparator()+m_name;
    target = FileReplaceExtension(target,"");
    Compiler compiler;
    compiler.link_program( target, objects, libraryOptions );

    std::shared_ptr<FileNode> targetNode = std::make_shared<FileNode>();
    targetNode->m_absolutePath = target;
    m_outputNodes.push_back( targetNode );
}


void StaticLibraryTarget::link( Context& ctx, const NodeList& objects )
{
    std::string target = ctx.get_current_path();
    target += FileSeparator()+m_name;
    target = FileReplaceExtension(target,"a");
    Compiler compiler;
    compiler.link_static_library( target, objects );

    std::shared_ptr<FileNode> targetNode = std::make_shared<FileNode>();
    targetNode->m_absolutePath = target;
    m_outputNodes.push_back( targetNode );

    m_export_library_options.push_back(target);
}


void DynamicLibraryTarget::link( Context& ctx, const NodeList& objects )
{
    // Create output file name
    std::string target = ctx.get_current_path();
    target += FileSeparator()+m_name;
    target = FileReplaceExtension(target,"so");

    Compiler compiler;
    compiler.link_dynamic_library( ctx, target, objects, m_uses );

    std::shared_ptr<FileNode> targetNode = std::make_shared<FileNode>();
    targetNode->m_absolutePath = target;
    m_outputNodes.push_back( targetNode );

    m_export_library_options.push_back(target);
}




Compiler::Compiler()
{
    m_exec="/usr/bin/g++";
    m_arexec="/usr/bin/ar";
}

void Compiler::compile( const std::string& source, const std::string& target, const std::vector<std::string>& includePaths )
{
    std::vector<std::string> args;
    args.push_back(m_exec);
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

    boost::process::context processContext;
    processContext.stdout_behavior = boost::process::capture_stream();
    processContext.stderr_behavior = boost::process::capture_stream();

    AXE_SCOPED_SECTION("compile");

    try
    {
        std::stringstream command;
        for( auto s: args) { command << s<<" "; }
        AXE_LOG( "command", axe::L_Verbose, command.str() );

        boost::process::child child = boost::process::launch( m_exec, args, processContext );
        //boost::process::status status =
                child.wait();

        {
            AXE_SCOPED_SECTION("stdout");
            boost::process::pistream& is = child.get_stdout();
            std::string line;
            while (std::getline(is, line))
            {
                AXE_LOG( "stdout", axe::L_Verbose, line );
            }
        }

        {
            AXE_SCOPED_SECTION("stderr");
            boost::process::pistream& ies = child.get_stderr();
            std::string line;
            while (std::getline(ies, line))
            {
                AXE_LOG( "stderr", axe::L_Verbose, line );
            }
        }
    }
    catch(...)
    {
        AXE_LOG( "command", axe::L_Error, "Execution failed!" );
    }
}


void Compiler::link_program( const std::string& target,
                             const NodeList& objects,
                             const std::vector<std::string>& libraryOptions )
{
    std::vector<std::string> args;
    args.push_back(m_exec);
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

    boost::process::context processContext;
    processContext.stdout_behavior = boost::process::capture_stream();
    processContext.stderr_behavior = boost::process::capture_stream();

    try
    {
        std::cout<< "[link]" << std::endl;
        std::cout<< "[command] ";
        for ( size_t a=0; a<args.size(); ++a)
        {
            std::cout<<" "<<args[a];
        }
        std::cout<< std::endl;

        boost::process::child child = boost::process::launch( m_exec, args, processContext );
        //boost::process::status status =
                child.wait();

        boost::process::pistream& is = child.get_stdout();
        std::string line;
        while (std::getline(is, line))
        {
            std::cout << line << std::endl;
        }

        boost::process::pistream& ies = child.get_stderr();
        while (std::getline(ies, line))
        {
            std::cout << line << std::endl;
        }


        std::cout<< "[/link]" << std::endl;
    }
    catch(...)
    {
        std::cout<< "Execution failed!" << std::endl;
    }
}


void Compiler::link_static_library( const std::string& target, const NodeList& objects )
{
    std::vector<std::string> args;
    args.push_back(m_arexec);
    args.push_back("-r");
    args.push_back("-c");
    args.push_back("-s");
    args.push_back(target);

    for (size_t i=0; i<objects.size(); ++i)
    {
         args.push_back(objects[i]->m_absolutePath);
    }

    boost::process::context processContext;
    processContext.stdout_behavior = boost::process::capture_stream();
    processContext.stderr_behavior = boost::process::capture_stream();

    try
    {
        std::cout<< "[link]" << std::endl;
        std::cout<< "[command] ";
        for ( size_t a=0; a<args.size(); ++a)
        {
            std::cout<<" "<<args[a];
        }
        std::cout<< std::endl;

        boost::process::child child = boost::process::launch( m_arexec, args, processContext );
        //boost::process::status status =
                child.wait();

        boost::process::pistream& is = child.get_stdout();
        std::string line;
        while (std::getline(is, line))
        {
            std::cout << line << std::endl;
        }

        boost::process::pistream& ies = child.get_stderr();
        while (std::getline(ies, line))
        {
            std::cout << line << std::endl;
        }


        std::cout<< "[/link]" << std::endl;
    }
    catch(...)
    {
        std::cout<< "Execution failed!" << std::endl;
    }
}


void Compiler::link_dynamic_library( Context& ctx,
                                     const std::string& target,
                                     const NodeList& objects,
                                     const std::vector<std::string>& uses )
{
    std::vector<std::string> args;
    args.push_back(m_exec);
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
            AXE_LOG( "Compiler", axe::L_Verbose, "Used Dynamic library output node [%s].", lib->m_outputNodes[0]->m_absolutePath.c_str() );
            AXE_LOG( "Compiler", axe::L_Verbose, "Used Dynamic library ouput node path [%s].", pathToLib.c_str() );
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

    boost::process::context processContext;
    processContext.stdout_behavior = boost::process::capture_stream();
    processContext.stderr_behavior = boost::process::capture_stream();

    try
    {
        std::cout<< "[link]" << std::endl;
        std::cout<< "[command] ";
        for ( size_t a=0; a<args.size(); ++a)
        {
            std::cout<<" "<<args[a];
        }
        std::cout<< std::endl;

        boost::process::child child = boost::process::launch( m_exec, args, processContext );
        //boost::process::status status =
                child.wait();

        boost::process::pistream& is = child.get_stdout();
        std::string line;
        while (std::getline(is, line))
        {
            std::cout << line << std::endl;
        }

        boost::process::pistream& ies = child.get_stderr();
        while (std::getline(ies, line))
        {
            std::cout << line << std::endl;
        }


        std::cout<< "[/link]" << std::endl;
    }
    catch(...)
    {
        std::cout<< "Execution failed!" << std::endl;
    }
}


std::shared_ptr<Context> Context::Create( bool buildFolderHasHost,
                                          bool buildFolderHasTarget )
{
    return std::make_shared<ContextImpl>( buildFolderHasHost, buildFolderHasTarget );
}


ContextImpl::ContextImpl( bool buildFolderHasHost,
                          bool buildFolderHasTarget )
{
    m_buildFolder = "build";
    m_buildFolderHasHost = buildFolderHasHost;
    m_buildFolderHasTarget = buildFolderHasTarget;

    m_buildRoot = FileGetCurrentPath();

    // Initialize the versions

    // Initialize the platforms
    m_platforms.push_back( std::make_shared<PlatformLinux32>() );
    m_platforms.push_back( std::make_shared<PlatformLinux64>() );
    m_platforms.push_back( std::make_shared<PlatformOSX64>() );
    m_host_platform = get_this_platform();

    // \todo See if a different target has been specified in the command line?
    m_target_platform = m_host_platform;

    // Initialize the toolchains

    // Target folder may depend on host and target platforms
    update_target_folder();
}


void ContextImpl::update_target_folder()
{
    m_currentPath = m_buildRoot+FileSeparator()+m_buildFolder;

    if (m_buildFolderHasHost)
    {
        m_currentPath += FileSeparator()+m_host_platform->name();
    }

    if (m_buildFolderHasTarget)
    {
        m_currentPath += FileSeparator()+m_target_platform->name();
    }
}


void ContextImpl::set_build_folder( const std::string& folder )
{
    m_buildFolder = folder;
    update_target_folder();
}


std::shared_ptr<Platform> ContextImpl::get_this_platform()
{
    std::shared_ptr<Platform> result;

    for( auto it=m_platforms.begin();
         !result && it!=m_platforms.end();
         ++it )
    {
        if ((*it)->is_this())
        {
            result = *it;
        }
    }

    return result;
}


std::shared_ptr<Platform> ContextImpl::get_host_platform()
{
    return m_host_platform;
}


std::shared_ptr<Platform> ContextImpl::get_current_target_platform()
{
    return m_target_platform;
}


std::shared_ptr<Toolchain> ContextImpl::get_current_toolchain()
{
    return m_toolchain;
}


std::shared_ptr<Version> ContextImpl::get_current_version()
{
    return m_version;
}


const std::string& ContextImpl::get_current_path()
{
    return m_currentPath;
}


std::shared_ptr<FileNode> ContextImpl::file( const std::string& absolutePath )
{
    assert( FileIsAbsolute(absolutePath) );

    std::shared_ptr<FileNode> result = std::make_shared<FileNode>();
    result->m_absolutePath = absolutePath;

    return result;
}


Target& ContextImpl::program( const std::string& name )
{
    std::shared_ptr<Target> target = std::make_shared<ProgramTarget>();
    target->m_name = name;

    m_targets.push_back( target );

    return *target;
}

Target& ContextImpl::static_library( const std::string& name )
{
    std::shared_ptr<Target> target = std::make_shared<StaticLibraryTarget>();
    target->m_name = name;

    m_targets.push_back( target );

    return *target;
}

Target& ContextImpl::dynamic_library( const std::string& name )
{
    std::shared_ptr<Target> target = std::make_shared<DynamicLibraryTarget>();
    target->m_name = name;

    m_targets.push_back( target );

    return *target;
}


Target& ContextImpl::extern_dynamic_library( const std::string& name )
{
    std::shared_ptr<Target> target = std::make_shared<ExternDynamicLibraryTarget>();
    target->m_name = name;

    m_targets.push_back( target );

    return *target;
}


void ContextImpl::object( const std::string& name, NodeList& objects, const std::vector<std::string>& includePaths )
{
    FileCreateDirectories( m_currentPath );

    std::string target = m_currentPath+FileSeparator()+name;
    target = FileReplaceExtension(target,"o");
    Compiler compiler;
    compiler.compile( name, target, includePaths );

    assert( FileExists(target) );

    std::shared_ptr<FileNode> targetNode = std::make_shared<FileNode>();
    targetNode->m_absolutePath = target;
    objects.push_back( targetNode );
}


std::shared_ptr<Target> ContextImpl::get_target( const std::string& name )
{
    std::shared_ptr<Target> result;

    for( size_t t=0; t<m_targets.size(); ++t )
    {
        if (m_targets[t]->m_name==name)
        {
            result = m_targets[t];
        }
    }

    return result;
}


const TargetList& ContextImpl::get_targets()
{
    return m_targets;
}

