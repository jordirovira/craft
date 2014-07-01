#ifndef CRAFT_PRIVATE_H
#define CRAFT_PRIVATE_H

#include <craft_core.h>

#include <boost/algorithm/string.hpp>
#include <boost/process.hpp>

class ContextImpl : public Context
{

public:

    ContextImpl( bool buildFolderHasHost,
                 bool buildFolderHasTarget );

    virtual void set_build_folder( const std::string& folder ) override;
    virtual std::shared_ptr<Platform> get_host_platform() override;
    virtual std::shared_ptr<Platform> get_current_target_platform() override;
    virtual std::shared_ptr<Toolchain> get_current_toolchain() override;
    virtual std::shared_ptr<Version> get_current_version() override;
    virtual const char* get_current_path() override;
    virtual std::shared_ptr<Target> get_target( const std::string& name ) override;
    virtual const TargetList& get_targets() override;
    virtual std::shared_ptr<FileNode> file( const std::string& absolutePath ) override;
    Target& program( const std::string& name ) override;
    Target& static_library( const std::string& name ) override;
    Target& dynamic_library( const std::string& name ) override;
    Target& extern_dynamic_library( const std::string& name ) override;
    void object(const std::string& name, NodeList& objects, const std::vector<std::string>& includePaths) override;

protected:

    //! Root of the folder structure where generated files are stored during the build process
    //! It includes information about the version and toolchain: every combination of version and
    //! toolchain will have its own subfolder.
    boost::filesystem::path m_buildRoot;

    //!
    bool m_buildFolderHasHost;
    bool m_buildFolderHasTarget;

    //! Prefix of the build folder, by deafult "build". It is appended to the current working folder
    //! and more folders may be appended to it, based on the above booleans.
    std::string m_buildFolder;

    //! Current folder below m_buildRoot where generated files will be stored.
    boost::filesystem::path m_currentPath;

    std::vector<std::shared_ptr<Node>> m_nodes;
    TargetList m_targets;

    std::vector<std::shared_ptr<Platform>> m_platforms;
    std::shared_ptr<Platform> m_target_platform;
    std::shared_ptr<Platform> m_host_platform;

    std::vector<std::shared_ptr<Toolchain>> m_toolchains;
    std::shared_ptr<Toolchain> m_toolchain;

    std::shared_ptr<Version> m_version;

private:

    // Rebuild the build folder based on host and target platforms
    void update_target_folder();

    // Identify the currently running platform among the registered platforms
    std::shared_ptr<Platform> get_this_platform();

};


#endif
