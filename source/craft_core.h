#pragma once

#include "platform.h"
#include "axe.h"

#include <string>
#include <vector>
#include <memory>


//! Dynamic link library import and export
//! define CRAFTCOREI_BUILD when building the dynamic library
#if _MSC_VER

    #ifdef CRAFTCOREI_BUILD
        #define CRAFTCOREI_API __declspec(dllexport)
    #else
        #define CRAFTCOREI_API __declspec(dllimport)
    #endif

#else

    #define CRAFTCOREI_API

#endif


extern void craft_core_initialize( axe::Kernel* log_kernel=nullptr );


class Context;
class Node;
typedef std::vector< std::shared_ptr<Node> > NodeList;
class Target;
typedef std::vector< std::shared_ptr<Target> > TargetList;


class Target
{
public:

    Target() { m_built=false; }

    // Definition
    CRAFTCOREI_API Target& source( const std::string& files );
    CRAFTCOREI_API Target& use( const std::string& files );
    CRAFTCOREI_API Target& export_include( const std::string& files );
    CRAFTCOREI_API Target& export_library_options( const std::string& options );
    CRAFTCOREI_API Target& library_path( const std::string& path );

    // Operation
    CRAFTCOREI_API virtual void build( Context& ctx );

    CRAFTCOREI_API const NodeList& GetOutputNodes() { return m_outputNodes; }

    CRAFTCOREI_API bool Built() const { return m_built; }

    std::string m_name;
    bool m_built;

    //! Source files required to build this target
    std::vector<std::string> m_sources;

    //! Other targets whose output is used to build this target
    std::vector<std::string> m_uses;

    //! Include directories required to build those targets that use this target
    std::vector<std::string> m_export_includes;
    std::vector<std::string> m_export_library_options;
    std::string m_library_path;

    //! List of output files generated by this target's build process.
    //! Only valid after building the target
    NodeList m_outputNodes;

};


class ExternDynamicLibraryTarget : public Target
{
public:

};


class CppTarget : public Target
{
public:

    virtual void build( Context& ctx ) override;


protected:
    virtual void link( Context& ctx, const NodeList& objects ) = 0;

};


class ProgramTarget : public CppTarget
{
protected:
    virtual void link( Context& ctx, const NodeList& objects ) override;
};


class StaticLibraryTarget : public CppTarget
{
protected:
    virtual void link( Context& ctx, const NodeList& objects ) override;
};


class DynamicLibraryTarget : public CppTarget
{
protected:
    virtual void link( Context& ctx, const NodeList& objects ) override;
};


class Node
{
public:
    std::string m_absolutePath;
};


class FileNode : public Node
{
};


class Compiler
{
public:

    Compiler();

    void compile( const std::string& source, const std::string& target, const std::vector<std::string>& includePaths );
    void link_program( const std::string& target,
                       const NodeList& objects,
                       const std::vector<std::string>& libraryOptions );
    void link_static_library( const std::string& target, const NodeList& objects );
    void link_dynamic_library( Context& ctx,
                               const std::string& target,
                               const NodeList& objects,
                               const std::vector<std::string>& uses);

private:

    std::string m_exec;
    std::string m_arexec;

};



class Toolchain
{
public:

    CRAFTCOREI_API virtual const char* name() const;

};


class Version
{
public:

    CRAFTCOREI_API virtual const char* name() const;

};


class Context
{
public:

    CRAFTCOREI_API static std::shared_ptr<Context> Create( bool buildFolderHasHost=true,
                                                           bool buildFolderHasTarget=true );

    virtual ~Context() {}

    // Build configuration

    //! Set the folder used to store all build artifacts. By default is "build" from the current
    //! working folder.
    CRAFTCOREI_API virtual void set_build_folder( const std::string& folder ) = 0;

    // State query
    CRAFTCOREI_API virtual std::shared_ptr<Platform> get_host_platform() = 0;

    CRAFTCOREI_API virtual std::shared_ptr<Platform> get_current_target_platform() = 0;

    CRAFTCOREI_API virtual std::shared_ptr<Toolchain> get_current_toolchain() = 0;

    CRAFTCOREI_API virtual std::shared_ptr<Version> get_current_version() = 0;

    CRAFTCOREI_API virtual const std::string& get_current_path() = 0;

    CRAFTCOREI_API virtual std::shared_ptr<Target> get_target( const std::string& name ) = 0;

    CRAFTCOREI_API virtual const TargetList& get_targets() = 0;


    // Target definition
    CRAFTCOREI_API virtual std::shared_ptr<FileNode> file( const std::string& absolutePath ) = 0;

    CRAFTCOREI_API virtual Target& program( const std::string& name ) = 0;

    CRAFTCOREI_API virtual Target& static_library( const std::string& name ) = 0;

    CRAFTCOREI_API virtual Target& dynamic_library( const std::string& name ) = 0;

    CRAFTCOREI_API virtual Target& extern_dynamic_library( const std::string& name ) = 0;

    CRAFTCOREI_API virtual void object( const std::string& name, NodeList& objects, const std::vector<std::string>& includePaths ) = 0;


protected:

    Context() {}

};

