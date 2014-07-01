#pragma once

#include <string>
#include <vector>
#include <atomic>
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


class Context;
class Node;
typedef std::vector<std::shared_ptr<Node>> NodeList;
class Target;
typedef std::vector<std::shared_ptr<Target>> TargetList;




class Platform
{
public:

    // This a name unique across all platforms, including a combination of its properties
    CRAFTCOREI_API virtual std::string name() const;

    CRAFTCOREI_API virtual const char* os() const = 0;

    CRAFTCOREI_API virtual const char* arch() const = 0;

    //! Return true if this is the running platform
    CRAFTCOREI_API virtual bool is_this() const = 0;

};


class PlatformLinux : public Platform
{
public:

    // Platform interface
    virtual const char* os() const;
    virtual bool is_this() const;

};


class PlatformLinuxX32 : public PlatformLinux
{
public:

    // Platform interface
    virtual const char* arch() const;
    virtual bool is_this() const;

};


class PlatformLinuxX64 : public PlatformLinux
{
public:

    // Platform interface
    virtual const char* arch() const;
    virtual bool is_this() const;

};

