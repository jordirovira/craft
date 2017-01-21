#pragma once

#include "craft_core.h"
#include "platform.h"
#include "axe.h"

#include <string>
#include <vector>
#include <memory>


class Compiler
{
public:

    Compiler();

    void set_configuration( const std::string& name );
    void add_configuration( const std::string& name, const std::vector<std::string>& compileFlags, const std::vector<std::string>& linkFlags );

    int get_link_dynamic_library_dependencies( NodeList& deps,
                                   const std::string& target,
                                   const NodeList& objects,
                                   const std::vector<std::shared_ptr<BuiltTarget>>& uses);

    int get_link_static_library_dependencies( NodeList& deps,
                                   const std::string& target,
                                   const NodeList& objects );

    int get_link_program_dependencies(NodeList& deps,
                                   const NodeList& objects,
                                   const std::vector<std::shared_ptr<BuiltTarget>>& uses);


    virtual int get_compile_dependencies( NodeList& deps, const std::string& source, const std::string& target, const std::vector<std::string>& includePaths ) = 0;
    virtual int compile( const std::string& source, const std::string& target, const std::vector<std::string>& includePaths ) = 0;
    virtual int link_program( const std::string& target,
                       const NodeList& objects,
                       const std::vector<std::shared_ptr<BuiltTarget>>& uses )=0;
    virtual int link_static_library( const std::string& target, const NodeList& objects ) = 0;
    virtual int link_dynamic_library( const std::string& target,
                               const NodeList& objects,
                               const std::vector<std::shared_ptr<BuiltTarget>>& uses) = 0;
    virtual const char* get_default_object_extension() = 0;

protected:

    int m_current_configuration;

    struct Configuration
    {
        std::string m_name;
        std::vector<std::string> m_compileFlags;
        std::vector<std::string> m_linkFlags;
    };

    std::vector<Configuration> m_configurations;

};


class CompilerGCC : public Compiler
{
public:

    CompilerGCC();

    //! Compiler interface
    int get_compile_dependencies( NodeList& deps, const std::string& source, const std::string& target, const std::vector<std::string>& includePaths ) override;
    int compile( const std::string& source, const std::string& target, const std::vector<std::string>& includePaths ) override;
    int link_program( const std::string& target,
                       const NodeList& objects,
                       const std::vector<std::shared_ptr<BuiltTarget>>& uses ) override;
    int link_static_library( const std::string& target, const NodeList& objects ) override;
    int link_dynamic_library( const std::string& target,
                               const NodeList& objects,
                               const std::vector<std::shared_ptr<BuiltTarget>>& uses) override;
    const char* get_default_object_extension() override;

private:

    std::string m_exec;
    std::string m_arexec;

    void build_compile_argument_list( std::vector<std::string>& args, const std::string& source, const std::string& target, const std::vector<std::string>& includePaths );

};


class CompilerMSVC : public Compiler
{
public:

    CompilerMSVC();

    //! Compiler interface
    int get_compile_dependencies( NodeList& deps, const std::string& source, const std::string& target, const std::vector<std::string>& includePaths ) override;
    int compile( const std::string& source, const std::string& target, const std::vector<std::string>& includePaths ) override;
    int link_program( const std::string& target,
                       const NodeList& objects,
                       const std::vector<std::shared_ptr<BuiltTarget>>& uses ) override;
    int link_static_library( const std::string& target, const NodeList& objects ) override;
    int link_dynamic_library( const std::string& target,
                               const NodeList& objects,
                               const std::vector<std::shared_ptr<BuiltTarget>>& uses) override;
    const char* get_default_object_extension() override;

private:

    std::string m_exec;
    std::string m_arexec;

    void build_compile_argument_list( std::vector<std::string>& args, const std::string& source, const std::string& target, const std::vector<std::string>& includePaths );

};


class Toolchain
{
public:

    CRAFTCOREI_API Toolchain();
    CRAFTCOREI_API virtual const char* name() const;
    CRAFTCOREI_API virtual std::shared_ptr<Compiler> get_compiler() const;

private:

    std::shared_ptr<Compiler> m_compiler;
};

