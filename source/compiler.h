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

    //!
    //! \brief get_compile_dependencies
    //! \param deps
    //! \param source
    //! \param target
    //! \param includePaths
    //!
    int get_compile_dependencies( NodeList& deps, const std::string& source, const std::string& target, const std::vector<std::string>& includePaths );

    //!
    //! \brief link_dynamic_library
    //! \param ctx
    //! \param target
    //! \param objects
    //! \param uses
    //!
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

    int compile( const std::string& source, const std::string& target, const std::vector<std::string>& includePaths );
    int link_program( const std::string& target,
                       const NodeList& objects,
                       const std::vector<std::shared_ptr<BuiltTarget>>& uses );
    int link_static_library( const std::string& target, const NodeList& objects );
    int link_dynamic_library( const std::string& target,
                               const NodeList& objects,
                               const std::vector<std::shared_ptr<BuiltTarget>>& uses);


private:

    std::string m_exec;
    std::string m_arexec;

    int m_current_configuration;

    struct Configuration
    {
        std::string m_name;
        std::vector<std::string> m_compileFlags;
        std::vector<std::string> m_linkFlags;
    };

    std::vector<Configuration> m_configurations;

    void build_compile_argument_list( std::vector<std::string>& args, const std::string& source, const std::string& target, const std::vector<std::string>& includePaths );

};



class Toolchain
{
public:

    CRAFTCOREI_API virtual const char* name() const;

};

