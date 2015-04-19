
#include "craft_private.h"

#include "axe.h"
#include "platform.h"

#include <string>
#include <sstream>
#include <vector>


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

            // Get the sortcut to built targets
            m_currentBuiltTargets = nullptr;
            for ( size_t i=0; !m_currentBuiltTargets && i<m_builtTargets.size(); ++i )
            {
                if ( m_builtTargets[i]->m_conditions.m_configuration==name)
                {
                    m_currentBuiltTargets = m_builtTargets[i];
                }
            }

            if (!m_currentBuiltTargets)
            {
                m_currentBuiltTargets = std::make_shared<BuiltTargets>();
                m_currentBuiltTargets->m_conditions.m_configuration = name;
                m_builtTargets.push_back( m_currentBuiltTargets );
            }
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

    AXE_LOG( "task", axe::L_Info, "%3d tasks", m_tasks.size() );

    // Things should happen here.
    for ( std::size_t i=0; i<m_tasks.size(); ++i )
    {
        AXE_LOG( "task", axe::L_Info, "[%3d of %3d] %s", i+1, m_tasks.size(), m_tasks[i]->m_type.c_str() );
        m_tasks[i]->m_runMethod();
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


std::shared_ptr<BuiltTarget> ContextImpl::get_built_target( const std::string& name )
{
    std::shared_ptr<Target> target = get_target(name);
    assert(target);

    std::shared_ptr<BuiltTarget> result;

    assert(m_currentBuiltTargets);
    auto it = m_currentBuiltTargets->m_targets.find(target);
    if ( it==m_currentBuiltTargets->m_targets.end() )
    {
        result = target->build( *this );
        result->m_sourceTarget = target;
        m_currentBuiltTargets->m_targets[target] = result;
    }
    else
    {
        result = it->second;
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

