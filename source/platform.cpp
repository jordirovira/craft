
#include "craft_private.h"

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/process.hpp>



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


const char* PlatformLinuxX32::arch() const
{
    return "x32";
}


bool PlatformLinuxX32::is_this() const
{
    // \todo Doesn't work for 32 exe in 64 bit OS
    return PlatformLinux::is_this() && (sizeof(void *) == 4);
}


const char* PlatformLinuxX64::arch() const
{
    return "x64";
}


bool PlatformLinuxX64::is_this() const
{
    // \todo Doesn't work for 32 exe in 64 bit OS
    return PlatformLinux::is_this() && (sizeof(void *) == 8);
}

