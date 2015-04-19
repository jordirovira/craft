
#include "craft_private.h"

#include "axe.h"
#include "platform.h"

#include <string>
#include <sstream>
#include <vector>


Target& Target::source( const std::string& files )
{
    m_sources.push_back( files );
    return *this;
}


Target& Target::include( const std::string& path )
{
    m_includes.push_back( path );
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

Target& Target::is_default( bool enabled )
{
    m_is_default = enabled;
    return *this;
}


std::shared_ptr<BuiltTarget> ExternDynamicLibraryTarget::build( Context& ctx )
{
    auto result = std::make_shared<BuiltTarget>();

    // \TODO Set the library to link for this configuration


    return result;
}


std::shared_ptr<BuiltTarget> ObjectTarget::build( Context& ctx )
{
    auto res = std::make_shared<BuiltTarget>();

    std::vector<std::shared_ptr<Task>> reqs;

    // Build dependencies
    for( size_t u=0; u<m_uses.size(); ++u )
    {
        std::shared_ptr<BuiltTarget> usedTarget = ctx.get_built_target( m_uses[u] );
        assert( usedTarget );

        reqs.insert( reqs.end(), usedTarget->m_outputTasks.begin(), usedTarget->m_outputTasks.end() );
    }

    // Gather include paths
    std::vector<std::string> includePaths;
    for( size_t u=0; u<m_uses.size(); ++u )
    {
        std::shared_ptr<Target> usedTarget = ctx.get_target( m_uses[u] );
        assert( usedTarget );
        for( size_t p=0; p<usedTarget->m_export_includes.size(); ++p )
        {
            split( usedTarget->m_export_includes[p], "\t\n ", includePaths );
        }
    }

    for( size_t p=0; p<m_includes.size(); ++p )
    {
        split( m_includes[p], "\t\n ", includePaths );
    }

    std::shared_ptr<Node> outputNode;
    std::shared_ptr<Task> compileTask = object( ctx, m_name, includePaths, outputNode );
    if (compileTask)
    {
        compileTask->m_requirements.insert( compileTask->m_requirements.end(), reqs.begin(), reqs.end() );
        res->m_outputTasks.push_back(compileTask);
    }
    res->m_outputNode = outputNode;

    return res;
}


std::shared_ptr<BuiltTarget> CppTarget::build( Context& ctx )
{    
    AXE_LOG( "Build", axe::L_Debug, "Building CPP target [%s] in configuration [%s]", m_name.c_str(), ctx.get_current_configuration().c_str() );

    auto res = std::make_shared<BuiltTarget>();

    std::vector<std::shared_ptr<Task>> reqs;
    NodeList objects;

    // Build dependencies
    for( size_t u=0; u<m_uses.size(); ++u )
    {
        std::shared_ptr<BuiltTarget> usedTarget = ctx.get_built_target( m_uses[u] );
        assert( usedTarget );

        reqs.insert( reqs.end(), usedTarget->m_outputTasks.begin(), usedTarget->m_outputTasks.end() );
    }

    // Gather include paths
    std::vector<std::string> includePaths;
    for( size_t u=0; u<m_uses.size(); ++u )
    {
        std::shared_ptr<Target> usedTarget = ctx.get_target( m_uses[u] );
        assert( usedTarget );
        for( size_t p=0; p<usedTarget->m_export_includes.size(); ++p )
        {
            split( usedTarget->m_export_includes[p], "\t\n ", includePaths );
        }
    }

    for( size_t p=0; p<m_includes.size(); ++p )
    {
        split( m_includes[p], "\t\n ", includePaths );
    }

    // Build own
    for( size_t i=0; i<m_sources.size(); ++i )
    {
        std::vector<std::string> sourceFiles;
        split( m_sources[i], "\t\n ", sourceFiles );

        // Compile
        for( const auto &s: sourceFiles )
        {
            std::shared_ptr<Node> outputNode;
            auto t = ObjectTarget::object( ctx, s, includePaths, outputNode );
            if (t)
            {
                reqs.push_back( t );
            }
            objects.push_back( outputNode );
        }
    }

    link( ctx, *res, objects );
    if (res->m_outputTasks.size())
    {
        res->m_outputTasks[0]->m_requirements.insert( res->m_outputTasks[0]->m_requirements.end(), reqs.begin(), reqs.end() );
    }

    return res;
}


void ProgramTarget::link( Context& ctx, BuiltTarget& builtTarget, const NodeList& objects )
{
    std::string target = ctx.get_current_path()+FileSeparator()+ctx.get_current_configuration();
    target += FileSeparator()+m_name;
    target = FileReplaceExtension(target,"");

    // Calculate if we need to compile in this variable
    bool outdated = false;

    // Make sure the target folder exists
    std::string targetPath = FileGetPath( target );
    outdated = FileCreateDirectories( targetPath );

    // If we didn't create the folder and the file exists,
    // see if we need to compile again or it is already up to date.
    if ( !outdated )
    {
        FileTime target_time = FileGetModificationTime( target );

        // Does the target exist?
        if (target_time.IsNull())
        {
            outdated = true;
        }
        else
        {
            NodeList dependencies;

            std::vector<std::shared_ptr<BuiltTarget>> uses;
            for(const auto& u: m_uses)
            {
                uses.push_back( ctx.get_built_target(u));
            }

            Compiler compiler;
            compiler.set_configuration( ctx.get_current_configuration() );
            compiler.get_link_program_dependencies( dependencies, objects, uses );

            outdated = ctx.IsTargetOutdated( target_time, dependencies );
        }
    }

    builtTarget.m_outputNode = std::make_shared<Node>();
    builtTarget.m_outputNode->m_absolutePath = target;

    // Create the link task if we really need to.
    if (outdated)
    {
        std::string configuration = ctx.get_current_configuration();
        std::vector<std::shared_ptr<BuiltTarget>> uses;
        for(const auto& u: m_uses)
        {
            uses.push_back( ctx.get_built_target(u));
        }

        auto result = std::make_shared<Task>( "link program", builtTarget.m_outputNode,
                                         [=]()
        {
            Compiler compiler;
            compiler.set_configuration( configuration );
            compiler.link_program( target, objects, uses );
        }
                    );

        ContextImpl* ctx_impl = dynamic_cast<ContextImpl*>(&ctx);
        ctx_impl->m_tasks.push_back(result);

        builtTarget.m_outputTasks.push_back( result );
    }
}


void StaticLibraryTarget::link( Context& ctx, BuiltTarget& builtTarget, const NodeList& objects )
{
    std::string target = ctx.get_current_path()+FileSeparator()+ctx.get_current_configuration();
    target += FileSeparator()+m_name;
    target = FileReplaceExtension(target,"a");

    bool outdated = false;

    // Make sure the target folder exists
    std::string targetPath = FileGetPath( target );
    outdated = FileCreateDirectories( targetPath );

    // If we didn't create the folder
    if ( !outdated )
    {
        FileTime target_time = FileGetModificationTime( target );

        // Does the target exist?
        if (target_time.IsNull())
        {
            outdated = true;
        }
        else
        {
            Compiler compiler;
            compiler.set_configuration( ctx.get_current_configuration() );
            NodeList dependencies;
            compiler.get_link_static_library_dependencies( dependencies, target, objects );

            outdated = ctx.IsTargetOutdated( target_time, dependencies );
        }
    }

    builtTarget.m_outputNode = std::make_shared<Node>();
    builtTarget.m_outputNode->m_absolutePath = target;

    // Create the link task if we really need to.
    if (outdated)
    {
        std::string configuration = ctx.get_current_configuration();
        auto result = std::make_shared<Task>( "link static library", builtTarget.m_outputNode,
                                         [=]()
        {
            Compiler compiler;
            compiler.set_configuration( configuration );
            compiler.link_static_library( target, objects );
        }
                    );

        ContextImpl* ctx_impl = dynamic_cast<ContextImpl*>(&ctx);
        ctx_impl->m_tasks.push_back(result);

        builtTarget.m_outputTasks.push_back( result );
    }

}


void DynamicLibraryTarget::link( Context& ctx, BuiltTarget& builtTarget, const NodeList& objects )
{
    // Create output file name
    std::string target = ctx.get_current_path()+FileSeparator()+ctx.get_current_configuration();
    std::string libraryName = ctx.get_current_target_platform()->get_dynamic_library_file_name( m_name );
    target += FileSeparator()+libraryName;

    bool outdated = false;

    // Make sure the target folder exists
    std::string targetPath = FileGetPath( target );
    outdated = FileCreateDirectories( targetPath );

    // If we didn't create the folder
    if ( !outdated )
    {
        FileTime target_time = FileGetModificationTime( target );

        // Does the target exist?
        if (target_time.IsNull())
        {
            outdated = true;
        }
        else
        {
            NodeList dependencies;

            std::vector<std::shared_ptr<BuiltTarget>> uses;
            for(const auto& u: m_uses)
            {
                uses.push_back( ctx.get_built_target(u));
            }

            Compiler compiler;
            compiler.set_configuration( ctx.get_current_configuration() );
            compiler.get_link_dynamic_library_dependencies( dependencies, target, objects, uses );

            outdated = ctx.IsTargetOutdated( target_time, dependencies );
        }
    }

    builtTarget.m_outputNode = std::make_shared<Node>();
    builtTarget.m_outputNode->m_absolutePath = target;

    // Create the link task if we really need to.
    if (outdated)
    {
        std::string configuration = ctx.get_current_configuration();
        std::vector<std::shared_ptr<BuiltTarget>> uses;
        for(const auto& u: m_uses)
        {
            uses.push_back( ctx.get_built_target(u));
        }

        auto result = std::make_shared<Task>( "link dynamic library", builtTarget.m_outputNode,
                                         [=]()
        {
            Compiler compiler;
            compiler.set_configuration( configuration );
            compiler.link_dynamic_library( target, objects, uses );
        }
                    );

        ContextImpl* ctx_impl = dynamic_cast<ContextImpl*>(&ctx);
        ctx_impl->m_tasks.push_back(result);

        builtTarget.m_outputTasks.push_back( result );
    }
}


std::shared_ptr<Task> ObjectTarget::object( Context& ctx, const std::string& name, const std::vector<std::string>& includePaths, std::shared_ptr<Node>& targetNode )
{
    FileCreateDirectories( ctx.get_current_path() );

    std::string target = ctx.get_current_path()+FileSeparator()+ctx.get_current_configuration()+FileSeparator()+name;
    target = FileReplaceExtension(target,"o");

    // Calculate if we need to compile in this variable
    bool outdated = false;

    // Make sure the target folder exists
    std::string targetPath = FileGetPath( target );
    outdated = FileCreateDirectories( targetPath );

    // If we didn't create the folder
    if ( !outdated )
    {
        FileTime target_time = FileGetModificationTime( target );

        // Does the target exist?
        if (target_time.IsNull())
        {
            outdated = true;
        }
        else
        {
            NodeList dependencies;

            Compiler compiler;
            compiler.set_configuration( ctx.get_current_configuration() );
            compiler.get_compile_dependencies( dependencies, name, target, includePaths );

            outdated = ctx.IsTargetOutdated( target_time, dependencies );
        }
    }

    targetNode = std::make_shared<Node>();
    targetNode->m_absolutePath = target;

    // Create the compile task if we really need to.
    std::shared_ptr<Task> result;

    if (outdated)
    {
        std::string configuration = ctx.get_current_configuration();
        result = std::make_shared<Task>( "compile", targetNode,
                                         [=]()
        {
            Compiler compiler;
            compiler.set_configuration( configuration );
            compiler.compile( name, target, includePaths );
        }
                    );

        ContextImpl* ctx_impl = dynamic_cast<ContextImpl*>(&ctx);
        ctx_impl->m_tasks.push_back(result);
    }

    return result;
}

