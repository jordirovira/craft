#pragma once

#include <string>
#include <vector>
#include <functional>
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
typedef std::vector< std::shared_ptr<Node> > NodeList;
class Target;
typedef std::vector< std::shared_ptr<Target> > TargetList;




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


class PlatformLinux32 : public PlatformLinux
{
public:

    // Platform interface
    virtual const char* arch() const;
    virtual bool is_this() const;

};


class PlatformLinux64 : public PlatformLinux
{
public:

    // Platform interface
    virtual const char* arch() const;
    virtual bool is_this() const;

};


class PlatformOSX : public Platform
{
public:

    // Platform interface
    virtual const char* os() const;
    virtual bool is_this() const;

};


class PlatformOSX64 : public PlatformOSX
{
public:

    // Platform interface
    virtual const char* arch() const;
    virtual bool is_this() const;

};


extern bool FileExists( const std::string& path );
extern std::string FileGetCurrentPath();
extern std::string FileSeparator();
extern std::string FileReplaceExtension( const std::string& source, const std::string& extension );

//! Return the parent directory of the path (without the filename or last directory name if it
//! doesn't have a path separator at the end)
extern std::string FileGetPath( const std::string& source );

extern bool FileIsAbsolute( const std::string& path );
extern void FileCreateDirectories( const std::string& path );

extern void Run( const std::string& command,
                 const std::vector<std::string>& arguments,
                 std::function<void(const char*)> out = nullptr,
                 std::function<void(const char*)> err = nullptr );

