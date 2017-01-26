#pragma once

#include "platform.h"
#include "axe.h"
#include "target.h"

#include <string>
#include <vector>
#include <memory>
#include <map>


//! Dynamic link library import and export
//! define CRAFTCOREI_BUILD when building the dynamic library
#if defined _WIN32 || defined __CYGWIN__

    #ifdef CRAFTCOREI_BUILD
        #define CRAFTCOREI_API __declspec(dllexport)
    #else
        #define CRAFTCOREI_API __declspec(dllimport)
    #endif

#else

    #define CRAFTCOREI_API

#endif

class Toolchain;
class Context;
class Node;
typedef std::vector< std::shared_ptr<Node> > NodeList;


class Node
{
public:
    std::string m_absolutePath;
};


//! Tasks created by the targets
class Task
{
public:
    Task( const std::string& type, const NodeList& outputs, std::function<int()> run )
        : m_type(type)
        , m_outputs(outputs)
        , m_runMethod(run)
    {
    }

    Task( const std::string& type, std::shared_ptr<Node>& output, std::function<int()> run )
        : m_type(type)
        , m_runMethod(run)
    {
        if (output)
        {
            m_outputs.push_back(output);
        }
    }

    Task( const std::string& type, std::function<int()> run )
        : m_type(type)
        , m_runMethod(run)
    {
    }

    std::string m_type;
    NodeList m_outputs;
    std::function<int()> m_runMethod;
    std::vector<std::shared_ptr<Task>> m_requirements;
};


class Context
{
    friend class ContextPlan;
public:

    CRAFTCOREI_API Context( bool buildFolderHasHost=true, bool buildFolderHasTarget=true );

    CRAFTCOREI_API virtual ~Context();

    // Build configuration

    //! Set the folder used to store all build artifacts. By default is "build" from the current
    //! working folder.
    CRAFTCOREI_API virtual void set_build_folder( const std::string& folder );

    // State query
    CRAFTCOREI_API virtual std::shared_ptr<Platform> get_host_platform();

    CRAFTCOREI_API virtual std::shared_ptr<Platform> get_current_target_platform();

    CRAFTCOREI_API virtual std::shared_ptr<Toolchain> get_current_toolchain();

    //! Returns the absolute path of the current build folder. This is the destination folder where
    //! built objects are stored.
    CRAFTCOREI_API virtual const std::string& get_current_path() const;

    CRAFTCOREI_API virtual std::shared_ptr<class Target_Base> get_target( const std::string& name );

    CRAFTCOREI_API virtual const TargetList& get_targets();
    CRAFTCOREI_API virtual TargetList get_default_targets();

    CRAFTCOREI_API virtual bool has_configuration( const std::string& name ) const;

    // Execution control
    // \todo hide when defining targets in a craftfile
    CRAFTCOREI_API virtual const std::vector<std::string>& get_default_configurations() const;

    // Target definition
    CRAFTCOREI_API virtual std::shared_ptr<Node> file( const std::string& absolutePath );

    CRAFTCOREI_API virtual class CppTarget& program( const std::string& name );

    CRAFTCOREI_API virtual class CppTarget& static_library( const std::string& name );

    CRAFTCOREI_API virtual class DynamicLibraryTarget& dynamic_library( const std::string& name );

    CRAFTCOREI_API virtual class ExternDynamicLibraryTarget& extern_dynamic_library( const std::string& name );

    CRAFTCOREI_API virtual class ObjectTarget& object( const std::string& name, const std::vector<std::string>& includePaths );

    //CRAFTCOREI_API virtual class DownloadTarget& download( const std::string& name );

    //CRAFTCOREI_API virtual class UnarchiveTarget& unarchive( const std::string& name );

    CRAFTCOREI_API virtual class ExecTarget& exec( const std::string& name );

    CRAFTCOREI_API virtual class CustomTarget& target( const std::string& name );

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

private:

    //! Rebuild the build folder based on host and target platforms
    void update_target_folder();

    //! Identify the currently running platform among the registered platforms
    std::shared_ptr<Platform> get_this_platform();

};



class ContextPlan
{
public:

    CRAFTCOREI_API ContextPlan( const Context& craftContext );

    CRAFTCOREI_API virtual ~ContextPlan();

    // State query
    CRAFTCOREI_API virtual std::shared_ptr<Platform> get_host_platform();
    CRAFTCOREI_API virtual std::shared_ptr<Platform> get_current_target_platform();
    CRAFTCOREI_API virtual std::shared_ptr<Toolchain> get_current_toolchain();

    //! Returns the absolute path of the current build folder. This is the destination folder where
    //! built objects are stored.
    CRAFTCOREI_API virtual const std::string& get_current_path() const;

    CRAFTCOREI_API virtual std::shared_ptr<class Target_Base> get_target( const std::string& name );
    CRAFTCOREI_API virtual std::shared_ptr<class BuiltTarget> get_built_target( const std::string& name );

    CRAFTCOREI_API virtual const TargetList& get_targets();

    CRAFTCOREI_API virtual bool has_configuration( const std::string& name ) const;

    // Execution control
    // \todo hide when defining targets in a craftfile
    CRAFTCOREI_API virtual void set_current_configuration( const std::string& name );
    CRAFTCOREI_API virtual const std::string& get_current_configuration() const;
    CRAFTCOREI_API virtual int run();


    CRAFTCOREI_API virtual bool IsTargetOutdated( FileTime target_time, const NodeList& dependencies, std::shared_ptr<Node>* failed=nullptr );

    //! Vector of tasks being filled up while planning
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
        std::map< std::shared_ptr<Target_Base>, std::shared_ptr<BuiltTarget> > m_targets;
    };
    std::vector<std::shared_ptr<BuiltTargets>> m_builtTargets;
    std::shared_ptr<BuiltTargets> m_currentBuiltTargets;

    //! These are built targets that don't depend on configuration options
    std::map< std::shared_ptr<Target_Base>, std::shared_ptr<BuiltTarget> > m_insensitiveBuiltTargets;

    std::vector<std::shared_ptr<Platform>> m_platforms;
    std::shared_ptr<Platform> m_target_platform;
    std::shared_ptr<Platform> m_host_platform;

    std::vector< std::shared_ptr<Toolchain> > m_toolchains;
    std::shared_ptr<Toolchain> m_toolchain;

    //! Definition of the available configurations that can be used to build targets
    std::vector<std::string> m_configurations;

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
