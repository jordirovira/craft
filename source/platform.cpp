
#include "craft_private.h"

#include "axe.h"

#include <string>
#include <vector>

#include <unistd.h>
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
    {
        //fprintf(stdout, "Current working dir: %s\n", cwd);
    }
    else
    {
        cwd[0]=0;
    }

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

bool FileCreateDirectories( const std::string& path )
{
    //AXE_LOG( "Test", axe::L_Verbose, "FileCreateDirectories [%s]", path.c_str() );

    bool anythingCreated = false;

    std::string::size_type pos = 0;
    while ( pos!=std::string::npos )
    {
        std::string::size_type new_pos = path.find( '/', pos+1 );
        if (new_pos!=std::string::npos && new_pos>pos+1)
        {
            std::string directory = path.substr( 0, new_pos );
            if (!FileExists(directory))
            {
                anythingCreated = true;
                CreateDirectory( directory.c_str() );
            }
        }
        else if (new_pos==std::string::npos && pos!=path.size()-1)
        {
            if (!FileExists(path))
            {
                anythingCreated = true;
                CreateDirectory( path.c_str() );
            }
        }
        pos = new_pos;
    }

    return anythingCreated;
}


FileTime FileGetModificationTime( const std::string& path )
{
    FileTime result;

    struct stat file_stat;
    if (stat (path.c_str(), &file_stat) == 0)
    {
        result.m_time = file_stat.st_mtimespec;
    }

    return result;
}


#include <unistd.h>
#include <cstdio>

void Run( const std::string& command,
          const std::vector<std::string>& arguments,
          std::function<void(const char*)> out,
          std::function<void(const char*)> err )
{

    std::stringstream logstr;
    logstr << command << " ";
    for( auto s: arguments) { logstr << s << " "; }
    AXE_LOG( "run", axe::L_Verbose, logstr.str() );

#define CHILD_OUT_PIPE      0
#define CHILD_ERR_PIPE      1

    int pipes[2][2];

    // pipes for parent to write and read
    pipe(pipes[CHILD_OUT_PIPE]);
    pipe(pipes[CHILD_ERR_PIPE]);

    pid_t childPid = fork();
    if (childPid<0)
    {
        // Failed to execute
        assert( false );
    }
    else if (childPid==0)
    {
        // We are the child
        dup2(pipes[CHILD_OUT_PIPE][1], STDOUT_FILENO);
        dup2(pipes[CHILD_ERR_PIPE][1], STDERR_FILENO);

        // Close fds not required by child. Also, we don't
        // want the exec'ed program to know these existed
        close(pipes[CHILD_OUT_PIPE][0]);
        close(pipes[CHILD_ERR_PIPE][0]);
        close(pipes[CHILD_OUT_PIPE][1]);
        close(pipes[CHILD_ERR_PIPE][1]);

        // Build a raw list of char* for the command and arguments
        // const_casting is apparently safe here.
        char** argv = new char* [arguments.size()+2];
        argv[0] = const_cast<char*>(command.c_str());
        argv[arguments.size()+1] = nullptr;
        for (size_t a=0;a<arguments.size();++a)
        {
            argv[a+1] = const_cast<char*>(arguments[a].c_str());
        }

        // Call
        execv(argv[0], argv);

        delete[] argv;
    }
    else
    {
        // close fds not required by parent
        close(pipes[CHILD_OUT_PIPE][1]);
        close(pipes[CHILD_ERR_PIPE][1]);

        bool finished = false;

        while (!finished)
        {
            fd_set set;
            struct timeval timeout;

            // Initialize the file descriptor set.
            FD_ZERO(&set);
            FD_SET(pipes[CHILD_OUT_PIPE][0], &set);
            FD_SET(pipes[CHILD_ERR_PIPE][0], &set);

            // Initialize the timeout data structure.
            timeout.tv_sec = 60;
            timeout.tv_usec = 0;

            // \todo Replace FD_SETSIZE for the max pipe index to optimise
            int ret = select(FD_SETSIZE, &set, NULL, NULL, &timeout);

            // a return value of 0 means that the time expired
            // without any acitivity on the file descriptor
            if (ret == 0)
            {
                printf("time out!");
                finished = true;
            }
            else if (ret < 0)
            {
                // error occurred
                finished = true;
            }
            else
            {
                char buffer[256];
                int count;

                // Read from child’s stdout
                bool read_completed = false;
                while (!read_completed)
                {
                    count = read(pipes[CHILD_OUT_PIPE][0], buffer, sizeof(buffer)-1);
                    if (count >= 0)
                    {
                        buffer[count] = 0;

                        if (out)
                        {
                            out(buffer);
                        }
                    }
                    else
                    {
                        int error = errno;
                        if (error!=EINTR && error!=EAGAIN)
                        {
                            perror("IO Error\n");
                            assert(false);
                        }
                    }

                    if ( count==0 )
                    {
                        read_completed = true;
                    }
                }

                // Read from child’s stderr
                read_completed = false;
                while (!read_completed)
                {
                    count = read(pipes[CHILD_ERR_PIPE][0], buffer, sizeof(buffer)-1);
                    if (count >= 0)
                    {
                        buffer[count] = 0;

                        if (err)
                        {
                            err(buffer);
                        }
                    }
                    else
                    {
                        printf("IO Error\n");
                        assert(false);
                    }

                    if ( count==0 )
                    {
                        read_completed = true;
                    }
                }
            }

            int childStatus=0;
            if (waitpid( childPid, &childStatus, WNOHANG )!=0 )
            {
                finished = true;
            }
        }

        close(pipes[CHILD_OUT_PIPE][0]);
        close(pipes[CHILD_ERR_PIPE][0]);
    }
}


