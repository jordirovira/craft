
#include "Target.h"

#include "craft_private.h"
#include "axe.h"
#include "platform.h"

#include <string>
#include <sstream>
#include <vector>


CppTarget& CppTarget::source( const std::string& files )
{
    m_sources.push_back( files );
    return *this;
}


CppTarget& CppTarget::include( const std::string& path )
{
    m_includes.push_back( path );
    return *this;
}

CppTarget& CppTarget::export_include( const std::string& paths )
{
    m_export_includes.push_back( paths );
    return *this;
}

CppTarget& CppTarget::export_library_options( const std::string& options )
{
    m_export_library_options.push_back( options );
    return *this;
}


ExternDynamicLibraryTarget& ExternDynamicLibraryTarget::library_path( const std::string& path )
{
    m_library_path = path;
    return *this;
}

ExternDynamicLibraryTarget& ExternDynamicLibraryTarget::library_path( std::function<std::string(const ContextPlan&)> generator )
{
    m_library_path_generator = generator;
    return *this;
}

std::string ExternDynamicLibraryTarget::get_library_path( const ContextPlan& ctx ) const
{
    if (m_library_path_generator)
    {
        return m_library_path_generator(ctx);
    }
    else
    {
        return m_library_path;
    }
}


ExternDynamicLibraryTarget& ExternDynamicLibraryTarget::export_include( const std::string& paths )
{
    m_export_includes.push_back( paths );
    return *this;
}

ExternDynamicLibraryTarget& ExternDynamicLibraryTarget::export_library_options( const std::string& options )
{
    m_export_library_options.push_back( options );
    return *this;
}


std::shared_ptr<BuiltTarget> ExternDynamicLibraryTarget::build( ContextPlan& ctx )
{
    auto result = std::make_shared<Built>();

    // Build dependencies. This is necessary for libs built custom build processes
    for( size_t u=0; u<m_uses.size(); ++u )
    {
        std::shared_ptr<BuiltTarget> usedTarget = ctx.get_built_target( m_uses[u] );
        assert( usedTarget );

        result->m_outputTasks.insert( result->m_outputTasks.end(), usedTarget->m_outputTasks.begin(), usedTarget->m_outputTasks.end() );
    }


    // \TODO Set the library to link for this configuration
    result->m_library_path = get_library_path( ctx );

    return result;
}


std::shared_ptr<BuiltTarget> ObjectTarget::build( ContextPlan& ctx )
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
        std::shared_ptr<Target_Base> usedTargetBase = ctx.get_target( m_uses[u] );
        assert( usedTargetBase );
        if (ExternDynamicLibraryTarget* usedTarget = dynamic_cast<ExternDynamicLibraryTarget*>(usedTargetBase.get()))
        {
            for( size_t p=0; p<usedTarget->m_export_includes.size(); ++p )
            {
                split( usedTarget->m_export_includes[p], "\t\n ", includePaths );
            }
        }
        else if (CppTarget* usedTarget = dynamic_cast<CppTarget*>(usedTargetBase.get()))
        {
            for( size_t p=0; p<usedTarget->m_export_includes.size(); ++p )
            {
                split( usedTarget->m_export_includes[p], "\t\n ", includePaths );
            }
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


std::shared_ptr<BuiltTarget> CppTarget::build( ContextPlan& ctx )
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
        std::shared_ptr<Target_Base> usedTargetBase = ctx.get_target( m_uses[u] );
        assert( usedTargetBase );
        if (ExternDynamicLibraryTarget* usedTarget = dynamic_cast<ExternDynamicLibraryTarget*>(usedTargetBase.get()))
        {
            for( size_t p=0; p<usedTarget->m_export_includes.size(); ++p )
            {
                split( usedTarget->m_export_includes[p], "\t\n ", includePaths );
            }
        }
        else if (CppTarget* usedTarget = dynamic_cast<CppTarget*>(usedTargetBase.get()))
        {
            for( size_t p=0; p<usedTarget->m_export_includes.size(); ++p )
            {
                split( usedTarget->m_export_includes[p], "\t\n ", includePaths );
            }
        }
    }

    for( size_t p=0; p<m_includes.size(); ++p )
    {
        split( m_includes[p], "\t\n ", includePaths );
    }

    // Build own objects
    std::vector<std::shared_ptr<Task>> objectTasks;
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
                objectTasks.push_back( t );
            }
            objects.push_back( outputNode );
        }
    }

    link( ctx, *res, objects );
    if (res->m_outputTasks.size())
    {
        res->m_outputTasks[0]->m_requirements.insert( res->m_outputTasks[0]->m_requirements.end(), reqs.begin(), reqs.end() );
        res->m_outputTasks[0]->m_requirements.insert( res->m_outputTasks[0]->m_requirements.end(), objectTasks.begin(), objectTasks.end() );
    }

    res->m_outputTasks.insert( res->m_outputTasks.begin(), objectTasks.begin(), objectTasks.end() );

    return res;
}


void ProgramTarget::link( ContextPlan& ctx, BuiltTarget& builtTarget, const NodeList& objects )
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
            return compiler.link_program( target, objects, uses );
        }
                    );

        builtTarget.m_outputTasks.push_back( result );
    }
}


void StaticLibraryTarget::link( ContextPlan& ctx, BuiltTarget& builtTarget, const NodeList& objects )
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
            return compiler.link_static_library( target, objects );
        }
                    );

        builtTarget.m_outputTasks.push_back( result );
    }

}


void DynamicLibraryTarget::link( ContextPlan& ctx, BuiltTarget& builtTarget, const NodeList& objects )
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
            return compiler.link_dynamic_library( target, objects, uses );
        }
                    );

        builtTarget.m_outputTasks.push_back( result );
    }
}


std::shared_ptr<Task> ObjectTarget::object( ContextPlan& ctx, const std::string& name, const std::vector<std::string>& includePaths, std::shared_ptr<Node>& targetNode )
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
            return compiler.compile( name, target, includePaths );
        }
                    );
    }

    return result;
}
