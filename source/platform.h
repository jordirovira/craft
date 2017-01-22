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

#if _MSC_VER
    #include <Windows.h>
#endif

class Context;
class Node;
typedef std::vector< std::shared_ptr<Node> > NodeList;



class Platform
{
public:

    // This a name unique across all platforms, including a combination of its properties
    CRAFTCOREI_API virtual std::string name() const;

    CRAFTCOREI_API virtual const char* os() const = 0;

    CRAFTCOREI_API virtual const char* arch() const = 0;

    //! Return true if this is the running platform
    CRAFTCOREI_API virtual bool is_this() const = 0;

    //! Return the default dynamic library file name for this platform.
    CRAFTCOREI_API virtual std::string get_dynamic_library_file_name( const std::string& source ) const = 0;

    CRAFTCOREI_API virtual std::string get_program_file_name( const std::string& source ) const = 0;
};


class PlatformLinux : public Platform
{
public:

    // Platform interface
    virtual const char* os() const override;
    virtual bool is_this() const override;
    virtual std::string get_dynamic_library_file_name( const std::string& source ) const override;
    virtual std::string get_program_file_name( const std::string& source ) const override;

};


class PlatformLinux32 : public PlatformLinux
{
public:

    // Platform interface
    virtual const char* arch() const override;
    virtual bool is_this() const override;

};


class PlatformLinux64 : public PlatformLinux
{
public:

    // Platform interface
    virtual const char* arch() const override;
    virtual bool is_this() const override;

};


class PlatformWindows : public Platform
{
public:

    // Platform interface
    virtual const char* os() const override;
    virtual bool is_this() const override;
    virtual std::string get_dynamic_library_file_name( const std::string& source ) const override;
    virtual std::string get_program_file_name( const std::string& source ) const override;

};


class PlatformWindows64 : public PlatformWindows
{
public:

    // Platform interface
    virtual const char* arch() const override;
    virtual bool is_this() const override;

};


class PlatformOSX : public Platform
{
public:

    // Platform interface
    virtual const char* os() const override;
    virtual bool is_this() const override;
    virtual std::string get_dynamic_library_file_name( const std::string& source ) const override;
    virtual std::string get_program_file_name( const std::string& source ) const override;

};


class PlatformOSX64 : public PlatformOSX
{
public:

    // Platform interface
    virtual const char* arch() const override;
    virtual bool is_this() const override;

};


extern CRAFTCOREI_API bool FileExists( const std::string& path );
extern CRAFTCOREI_API std::string FileGetCurrentPath();
extern CRAFTCOREI_API std::string FileSeparator();
extern CRAFTCOREI_API std::string FileReplaceExtension( const std::string& source, const std::string& extension );

//! Return the parent directory of the path (without the filename or last directory name if it
//! doesn't have a path separator at the end)
extern CRAFTCOREI_API std::string FileGetPath( const std::string& source );

extern CRAFTCOREI_API bool FileIsAbsolute( const std::string& path );

//! Create all the folders in the path if they don't exist already.
//! \return true if any folder was actually created
extern CRAFTCOREI_API bool FileCreateDirectories( const std::string& path );

//!
//! \brief The FileTime struct
//!
#if 1
struct CRAFTCOREI_API FileTime
{
    FileTime()
    {
        m_time = 0;
    }

    bool IsNull() const
    {
        return m_time==0;
    }

    time_t m_time;

    friend bool operator<( const FileTime& left, const FileTime& right )
    {
        return left.m_time<right.m_time;
    }

    friend bool operator>( const FileTime& left, const FileTime& right )
    {
        return left.m_time>right.m_time;
    }
};

#else //OSX?

struct FileTime
{
    FileTime()
    {
        m_time.tv_sec=0;
        m_time.tv_nsec=0;
    }

    bool IsNull() const
    {
        return m_time.tv_sec==0 && m_time.tv_nsec==0;
    }

    timespec m_time;

    friend bool operator<( const FileTime& left, const FileTime& right )
    {
        return left.m_time.tv_sec<right.m_time.tv_sec
                || ( left.m_time.tv_sec==right.m_time.tv_sec
                     &&
                     left.m_time.tv_nsec<right.m_time.tv_nsec
                     );
    }

    friend bool operator>( const FileTime& left, const FileTime& right )
    {
        return left.m_time.tv_sec>right.m_time.tv_sec
                || ( left.m_time.tv_sec==right.m_time.tv_sec
                     &&
                     left.m_time.tv_nsec>right.m_time.tv_nsec
                     );
    }
};

#endif

//!
//! \brief FileGetModificationTime
//! \param path
//! \return
//!
FileTime CRAFTCOREI_API FileGetModificationTime( const std::string& path );


//!
//! \brief Run
//! \param workingPath
//! \param command
//! \param arguments
//! \param out
//! \param err
//! \return
//!
extern CRAFTCOREI_API int Run( const std::string& workingPath,
         const std::string& command,
         const std::vector<std::string>& arguments,
         std::function<void(const char*)> out,
         std::function<void(const char*)> err,
         int maxMilliseconds,
         int* killedFlag );



//!
//!
//!
extern CRAFTCOREI_API void LoadAndRun( const char* lib, const char* methodName,
                                       const char* workspace, const char** configurations, const char** targets );

