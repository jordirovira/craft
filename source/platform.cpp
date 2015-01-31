
#include "craft_private.h"

#include "axe.h"

#include <string>
#include <vector>

#include <boost/process.hpp>

#include <sys/stat.h>


std::string Platform::name() const
{
    std::string name;

    name += os();
    name += "-";
    name += arch();

    return name;
}


const char* PlatformLinux::os() const
{
    return "linux";
}


bool PlatformLinux::is_this() const
{
#ifdef __linux__
    return true;
#else
    return false;
#endif
}


const char* PlatformLinux32::arch() const
{
    return "x32";
}


bool PlatformLinux32::is_this() const
{
    // \todo Doesn't work for 32 exe in 64 bit OS
    return PlatformLinux::is_this() && (sizeof(void *) == 4);
}


const char* PlatformLinux64::arch() const
{
    return "x64";
}


bool PlatformLinux64::is_this() const
{
    // \todo Doesn't work for 32 exe in 64 bit OS
    return PlatformLinux::is_this() && (sizeof(void *) == 8);
}


const char* PlatformOSX::os() const
{
    return "osx";
}

#if __APPLE__
    #include "TargetConditionals.h"
    #if TARGET_IPHONE_SIMULATOR
         // iOS Simulator
    #elif TARGET_OS_IPHONE
        // iOS device
    #elif TARGET_OS_MAC
        // Other kinds of Mac OS
    #else
        // Unsupported platform
    #endif
#endif

bool PlatformOSX::is_this() const
{
#if TARGET_OS_MAC
    return true;
#else
    return false;
#endif
}


const char* PlatformOSX64::arch() const
{
    return "x64";
}


bool PlatformOSX64::is_this() const
{
    // \todo Doesn't work for 32 exe in 64 bit OS
    return PlatformOSX::is_this() && (sizeof(void *) == 8);
}


bool FileExists( const std::string& path )
{
    struct stat buffer;
    return (stat (path.c_str(), &buffer) == 0);
}


std::string FileGetCurrentPath()
{
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
        fprintf(stdout, "Current working dir: %s\n", cwd);
    else
        perror("getcwd() error");

    return cwd;
}

std::string FileSeparator()
{
    return "/";
}

std::string FileReplaceExtension( const std::string& source, const std::string& extension )
{
    //AXE_LOG("Platform", axe::L_Verbose, "Replacing extension from [%s] with [%s] ...", source.c_str(), extension.c_str() );
    std::string::size_type LastSeparatorPos = source.find_last_of("/\\");
    std::string::size_type LastDotPos = source.find_last_of('.');
    std::string result;
    if ( LastSeparatorPos>LastDotPos)
    {
        result = source;
    }
    else
    {
        result = source.substr(0,LastDotPos);
    }

    if (extension.size())
    {
        result += "."+extension;
    }
    //AXE_LOG("Platform", axe::L_Verbose, "... results in [%s]", result.c_str() );
    return result;
}

std::string FileGetPath( const std::string& source )
{
    std::string::size_type LastSeparatorPos = source.find_last_of("/\\");
    std::string result = source.substr(0,LastSeparatorPos);
    return result;
}

bool FileIsAbsolute( const std::string& path )
{
    return path.size()>0 && path[0]=='/';
}

void CreateDirectory( const char* directory )
{
    AXE_LOG( "Test", axe::L_Verbose, "Creating directory [%s]", directory );
    int status = mkdir(directory, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    assert( status==0 );
}

void FileCreateDirectories( const std::string& path )
{
    AXE_LOG( "Test", axe::L_Verbose, "FileCreateDirectories [%s]", path.c_str() );

    std::string::size_type pos = 0;
    while ( pos!=std::string::npos )
    {
        std::string::size_type new_pos = path.find( '/', pos+1 );
        if (new_pos!=std::string::npos && new_pos>pos+1)
        {
            std::string directory = path.substr( 0, new_pos );
            if (!FileExists(directory))
            {
                CreateDirectory( directory.c_str() );
            }
        }
        else if (new_pos==std::string::npos && pos!=path.size()-1)
        {
            if (!FileExists(path))
            {
                CreateDirectory( path.c_str() );
            }
        }
        pos = new_pos;
    }
}
