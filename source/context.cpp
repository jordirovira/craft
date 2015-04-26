
#include "craft_private.h"

#include "axe.h"
#include "platform.h"
#include "target.h"

#include <string>
#include <sstream>
#include <vector>


Context::Context( bool buildFolderHasHost,
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


void Context::update_target_folder()
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


void Context::set_build_folder( const std::string& folder )
{
    m_buildFolder = folder;
    update_target_folder();
}


std::shared_ptr<Platform> Context::get_this_platform()
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


std::shared_ptr<Platform> Context::get_host_platform()
{
    return m_host_platform;
}


std::shared_ptr<Platform> Context::get_current_target_platform()
{
    return m_target_platform;
}


std::shared_ptr<Toolchain> Context::get_current_toolchain()
{
    return m_toolchain;
}


bool Context::has_configuration( const std::string& name ) const
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


const std::vector<std::string>& Context::get_default_configurations() const
{
    return m_default_configurations;
}


const std::string& Context::get_current_path() const
{
    return m_currentPath;
}


std::shared_ptr<Node> Context::file( const std::string& absolutePath )
{
    assert( FileIsAbsolute(absolutePath) );

    std::shared_ptr<Node> result = std::make_shared<Node>();
    result->m_absolutePath = absolutePath;

    return result;
}


CppTarget& Context::program( const std::string& name )
{
    std::shared_ptr<CppTarget> target = std::make_shared<ProgramTarget>();
    target->m_name = name;

    m_targets.push_back( target );

    return *target;
}

CppTarget& Context::static_library( const std::string& name )
{
    std::shared_ptr<CppTarget> target = std::make_shared<StaticLibraryTarget>();
    target->m_name = name;

    m_targets.push_back( target );

    return *target;
}

DynamicLibraryTarget& Context::dynamic_library( const std::string& name )
{
    std::shared_ptr<DynamicLibraryTarget> target = std::make_shared<DynamicLibraryTarget>();
    target->m_name = name;

    m_targets.push_back( target );

    return *target;
}


ExternDynamicLibraryTarget& Context::extern_dynamic_library( const std::string& name )
{
    std::shared_ptr<ExternDynamicLibraryTarget> target = std::make_shared<ExternDynamicLibraryTarget>();
    target->m_name = name;

    m_targets.push_back( target );

    return *target;
}


ObjectTarget& Context::object( const std::string& name, const std::vector<std::string>& includePaths )
{
    std::shared_ptr<ObjectTarget> target = std::make_shared<ObjectTarget>();
    target->m_name = name;
    target->m_includes = includePaths;

    m_targets.push_back( target );

    return *target;
}


DownloadTarget& Context::download( const std::string& name )
{
    std::shared_ptr<DownloadTarget> target = std::make_shared<DownloadTarget>();
    target->m_name = name;

    m_targets.push_back( target );

    return *target;
}


UnarchiveTarget& Context::unarchive( const std::string& name )
{
    std::shared_ptr<UnarchiveTarget> target = std::make_shared<UnarchiveTarget>();
    target->m_name = name;

    m_targets.push_back( target );

    return *target;
}


CustomTarget& Context::target( const std::string& name )
{
    std::shared_ptr<CustomTarget> target = std::make_shared<CustomTarget>();
    target->m_name = name;

    m_targets.push_back( target );

    return *target;
}


std::shared_ptr<Target_Base> Context::get_target( const std::string& name )
{
    std::shared_ptr<Target_Base> result;

    for( size_t t=0; t<m_targets.size(); ++t )
    {
        if (m_targets[t]->m_name==name)
        {
            result = m_targets[t];
        }
    }

    return result;
}


const TargetList& Context::get_targets()
{
    return m_targets;
}


TargetList Context::get_default_targets()
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
