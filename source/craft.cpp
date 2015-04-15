
#include "craft_private.h"

#include "axe.h"
#include "platform.h"

#include <string>
#include <sstream>
#include <vector>


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


NodeList Target::GetOutputNodes()
{
    NodeList result;

    for( const auto& t: m_outputTasks )
    {
        result.insert( result.end(), t->m_outputs.begin(), t->m_outputs.end() );
    }

    return result;
}



void ObjectTarget::build( Context& ctx )
{
    std::vector<std::shared_ptr<Task>> reqs;

    // Build dependencies
    for( size_t u=0; u<m_uses.size(); ++u )
    {
        std::shared_ptr<Target> usedTarget = ctx.get_target( m_uses[u] );
        assert( usedTarget );
        usedTarget->build( ctx );

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

    std::shared_ptr<Task> compileTask = object( ctx, m_name, includePaths );
    if (compileTask)
    {
        compileTask->m_requirements.insert( compileTask->m_requirements.end(), reqs.begin(), reqs.end() );
        m_outputTasks.push_back(compileTask);
    }
}


void CppTarget::build( Context& ctx )
{
    AXE_LOG( "Build", axe::L_Debug, "Building CPP target [%s] in configuration [%s]", m_name.c_str(), ctx.get_current_configuration().c_str() );

    std::vector<std::shared_ptr<Task>> reqs;
    NodeList objects;

    // Build dependencies
    for( size_t u=0; u<m_uses.size(); ++u )
    {
        std::shared_ptr<Target> usedTarget = ctx.get_target( m_uses[u] );
        assert( usedTarget );
        usedTarget->build( ctx );

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
            auto t = ObjectTarget::object( ctx, s, includePaths );
            if (t)
            {
                reqs.push_back( t );
                objects.push_back( t->m_outputs[0] );
            }
        }
    }

    std::shared_ptr<Task> linkTask = link( ctx, objects );
    if (linkTask)
    {
        linkTask->m_requirements.insert( linkTask->m_requirements.end(), reqs.begin(), reqs.end() );
        m_outputTasks.push_back(linkTask);
    }
}


std::shared_ptr<Task> ProgramTarget::link( Context& ctx, const NodeList& objects )
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
    if ( !outdated && FileExists(target) )
    {
        FileTime target_time = FileGetModificationTime( target );

        Compiler compiler;
        compiler.set_configuration( ctx.get_current_configuration() );

        NodeList dependencies;
        compiler.get_link_program_dependencies( dependencies, ctx, target, objects, m_uses );

        outdated = ctx.IsTargetOutdated( target_time, dependencies );
    }

    std::shared_ptr<Node> targetNode = std::make_shared<Node>();
    targetNode->m_absolutePath = target;

    // Create the link task if we really need to.
    std::shared_ptr<Task> result;

    if (outdated)
    {
        ContextImpl* ctx_impl = dynamic_cast<ContextImpl*>(&ctx);

        result = std::make_shared<Task>( "link static library", targetNode,
                                         [=](Context* ctx)
        {
            Compiler compiler;
            compiler.set_configuration( ctx->get_current_configuration() );
            compiler.link_program( *ctx, target, objects, m_uses );
        }
                    );

        ctx_impl->m_tasks.push_back(result);
    }

    return result;
}


std::shared_ptr<Task> StaticLibraryTarget::link( Context& ctx, const NodeList& objects )
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

    std::shared_ptr<Node> targetNode = std::make_shared<Node>();
    targetNode->m_absolutePath = target;
    m_export_library_options.push_back(target);

    // Create the link task if we really need to.
    std::shared_ptr<Task> result;

    if (outdated)
    {
        ContextImpl* ctx_impl = dynamic_cast<ContextImpl*>(&ctx);

        result = std::make_shared<Task>( "link static library", targetNode,
                                         [=](Context* ctx)
        {
            Compiler compiler;
            compiler.set_configuration( ctx->get_current_configuration() );
            compiler.link_static_library( target, objects );
        }
                    );

        ctx_impl->m_tasks.push_back(result);
    }

    return result;
}


std::shared_ptr<Task> DynamicLibraryTarget::link( Context& ctx, const NodeList& objects )
{
    // Create output file name
    std::string target = ctx.get_current_path()+FileSeparator()+ctx.get_current_configuration();
    target += FileSeparator()+m_name;
    target = FileReplaceExtension(target,"so");

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
            compiler.get_link_dynamic_library_dependencies( dependencies, ctx, target, objects, m_uses );

            outdated = ctx.IsTargetOutdated( target_time, dependencies );
        }
    }

    m_target = std::make_shared<Node>();
    m_target->m_absolutePath = target;
    m_export_library_options.push_back(target);

    // Create the link task if we really need to.
    std::shared_ptr<Task> result;

    if (outdated)
    {
        ContextImpl* ctx_impl = dynamic_cast<ContextImpl*>(&ctx);

        result = std::make_shared<Task>( "link dynamic library", m_target,
                                         [=](Context* ctx)
        {
            Compiler compiler;
            compiler.set_configuration( ctx->get_current_configuration() );
            compiler.link_dynamic_library( *ctx, target, objects, m_uses );
        }
                    );

        ctx_impl->m_tasks.push_back(result);
    }

    return result;
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

    // Initialize the default configurations
    m_configurations.push_back( "debug" );
    m_configurations.push_back( "profile" );
    m_configurations.push_back( "release" );
    m_default_configurations.push_back("release");

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


const std::string& ContextImpl::get_current_configuration() const
{
    return m_current_configuration;
}


bool ContextImpl::has_configuration( const std::string& name ) const
{
    for (const auto& c: m_configurations)
    {
        if (c==name)
        {
            return true;
        }
    }

    return false;
}


void ContextImpl::set_current_configuration( const std::string& name )
{
    bool found = false;

    for (const auto& c: m_configurations)
    {
        if (c==name)
        {
            found=true;
            m_current_configuration=c;
        }
    }

    assert( found );
}


const std::vector<std::string>& ContextImpl::get_default_configurations() const
{
    return m_default_configurations;
}


void ContextImpl::run()
{
    AXE_SCOPED_SECTION(tasks);

    // Things should happen here.
    for ( std::size_t i=0; i<m_tasks.size(); ++i )
    {
        AXE_LOG( "task", axe::L_Info, "[%3d of %3d] %s", i+1, m_tasks.size(), m_tasks[i]->m_type.c_str() );
        m_tasks[i]->m_runMethod( this );
    }
}


const std::string& ContextImpl::get_current_path()
{
    return m_currentPath;
}


std::shared_ptr<Node> ContextImpl::file( const std::string& absolutePath )
{
    assert( FileIsAbsolute(absolutePath) );

    std::shared_ptr<Node> result = std::make_shared<Node>();
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

DynamicLibraryTarget& ContextImpl::dynamic_library( const std::string& name )
{
    std::shared_ptr<DynamicLibraryTarget> target = std::make_shared<DynamicLibraryTarget>();
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


Target& ContextImpl::object( const std::string& name, const std::vector<std::string>& includePaths )
{
    std::shared_ptr<Target> target = std::make_shared<ObjectTarget>();
    target->m_name = name;
    target->m_includes = includePaths;

    m_targets.push_back( target );

    return *target;
}


std::shared_ptr<Task> ObjectTarget::object( Context& ctx, const std::string& name, const std::vector<std::string>& includePaths )
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
            Compiler compiler;
            compiler.set_configuration( ctx.get_current_configuration() );

            NodeList dependencies;
            compiler.get_compile_dependencies( dependencies, name, target, includePaths );

            outdated = ctx.IsTargetOutdated( target_time, dependencies );
        }
    }

    std::shared_ptr<Node> targetNode = std::make_shared<Node>();
    targetNode->m_absolutePath = target;

    // Create the compile task if we really need to.
    std::shared_ptr<Task> result;

    if (outdated)
    {
        ContextImpl* ctx_impl = dynamic_cast<ContextImpl*>(&ctx);

        result = std::make_shared<Task>( "compile", targetNode,
                                         [=](Context* ctx)
        {
            Compiler compiler;
            compiler.set_configuration( ctx->get_current_configuration() );
            compiler.compile( name, target, includePaths );
        }
                    );

        ctx_impl->m_tasks.push_back(result);
    }

    return result;
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


TargetList ContextImpl::get_default_targets()
{
    TargetList result;

    for( auto& t: m_targets )
    {
        if (t->m_is_default)
        {
            result.push_back( t );
        }
    }

    return result;
}


bool ContextImpl::IsNodePending( const Node& node )
{
    for( const auto& t: m_tasks )
    {
       for ( const auto& n: t->m_outputs )
       {
           if ( node.m_absolutePath==n->m_absolutePath )
           {
               return true;
           }
       }
    }

    return false;
}


bool ContextImpl::IsTargetOutdated( FileTime target_time, const NodeList& dependencies )
{
    if ( target_time.IsNull() )
    {
        return true;
    }

    for (const auto& n: dependencies)
    {
        if (IsNodePending(*n))
        {
            return true;
        }

        FileTime dep_time = FileGetModificationTime( n->m_absolutePath );
        if (dep_time.IsNull() || dep_time>target_time)
        {
            return true;
        }
    }

    return false;
}

