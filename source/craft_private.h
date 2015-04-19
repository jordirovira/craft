#ifndef CRAFT_PRIVATE_H
#define CRAFT_PRIVATE_H

#include "craft_core.h"
#include "compiler.h"

#include <algorithm>
#include <map>


class ContextImpl : public Context
{

public:

    ContextImpl( bool buildFolderHasHost,
                 bool buildFolderHasTarget );

    virtual void set_build_folder( const std::string& folder ) override;
    virtual std::shared_ptr<Platform> get_host_platform() override;
    virtual std::shared_ptr<Platform> get_current_target_platform() override;
    virtual std::shared_ptr<Toolchain> get_current_toolchain() override;
    virtual bool has_configuration( const std::string& name ) const override;
    virtual void set_current_configuration( const std::string& name ) override;
    virtual const std::string& get_current_configuration() const override;
    virtual const std::vector<std::string>& get_default_configurations() const override;
    virtual const std::string& get_current_path() override;
    virtual std::shared_ptr<Target> get_target( const std::string& name ) override;
    virtual std::shared_ptr<BuiltTarget> get_built_target( const std::string& name ) override;
    virtual const TargetList& get_targets() override;
    virtual TargetList get_default_targets() override;
    virtual std::shared_ptr<Node> file( const std::string& absolutePath ) override;
    virtual Target& program( const std::string& name ) override;
    virtual Target& static_library( const std::string& name ) override;
    virtual DynamicLibraryTarget& dynamic_library( const std::string& name ) override;
    virtual Target& extern_dynamic_library( const std::string& name ) override;
    virtual Target& object(const std::string& name, const std::vector<std::string>& includePaths) override;
    virtual void run() override;
    virtual bool IsTargetOutdated( FileTime target_time, const NodeList& dependencies ) override;

    //! Vector of tasks being filled up while defining targets
    std::vector<std::shared_ptr<Task>> m_tasks;

protected:

    //! Root of the folder structure where generated files are stored during the build process
    //! It includes information about the version and toolchain: every combination of version and
    //! toolchain will have its own subfolder.
    std::string m_buildRoot;

    //!
    bool m_buildFolderHasHost;
    bool m_buildFolderHasTarget;

    //! Prefix of the build folder, by deafult "build". It is appended to the current working folder
    //! and more folders may be appended to it, based on the above booleans.
    std::string m_buildFolder;

    //! Current folder below m_buildRoot where generated files will be stored.
    std::string m_currentPath;

    std::vector< std::shared_ptr<Node> > m_nodes;

    //! List of targets produced by the craftfile.
    TargetList m_targets;

    struct BuildConditions
    {
        std::string m_configuration;

        // \TODO: Target platform?
    };

    struct BuiltTargets
    {
        BuildConditions m_conditions;
        std::map< std::shared_ptr<Target>, std::shared_ptr<BuiltTarget> > m_targets;
    };
    std::vector<std::shared_ptr<BuiltTargets>> m_builtTargets;
    std::shared_ptr<BuiltTargets> m_currentBuiltTargets;

    std::vector<std::shared_ptr<Platform>> m_platforms;
    std::shared_ptr<Platform> m_target_platform;
    std::shared_ptr<Platform> m_host_platform;

    std::vector< std::shared_ptr<Toolchain> > m_toolchains;
    std::shared_ptr<Toolchain> m_toolchain;

    //! Definition of the available configurations that can be used to build targets
    std::vector<std::string> m_configurations;

    //! Name of the configurations that will be built if none is specified by other means, like the
    //! command line.
    std::vector<std::string> m_default_configurations;

    //! Configuration that we are currently parsing targets for.
    std::string m_current_configuration;

private:

    //! Rebuild the build folder based on host and target platforms
    void update_target_folder();

    //! Identify the currently running platform among the registered platforms
    std::shared_ptr<Platform> get_this_platform();

    //!
    //! \brief IsNodePending
    //! \param node
    //! \return
    //!
    bool IsNodePending( const Node& node );

};


// Split a string with a delimit set.
// It skips empty elements.
inline void split(const std::string &s, std::string delims, std::vector<std::string>& elems)
{
    std::string::const_iterator b = s.begin();
    std::string::const_iterator e = s.end();

    std::string::const_iterator c = b;
    while (c!=e)
    {
        if ( b!=c && std::find( delims.begin(), delims.end(), *c) != delims.end() )
        {
            elems.push_back( std::string( b, c ) );
            b=c;
            ++b;
        }
        ++c;
    }

    // Add the last element
    if (b!=c)
    {
        elems.push_back( std::string( b, c ) );
    }
}


#endif
