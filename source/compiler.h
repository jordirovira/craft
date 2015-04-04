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

