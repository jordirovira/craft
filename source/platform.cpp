
#include "craft_private.h"

#include "axe.h"

#include <string>
#include <vector>

#include <sys/stat.h>
#include <sys/wait.h>
#include <cstdio>

#ifdef _MSC_VER
#include <direct.h>
#else
#include <unistd.h>
#include <dlfcn.h>
#endif

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


std::string PlatformLinux::get_dynamic_library_file_name( const std::string& sourceName ) const
{
    std::string result = sourceName;
    result = FileReplaceExtension(result,"so");
    return result;
}


std::string PlatformLinux::get_program_file_name( const std::string& sourceName ) const
{
    std::string result = sourceName;
    result = FileReplaceExtension(result,"");
    return result;
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


const char* PlatformWindows::os() const
{
    return "windows";
}


bool PlatformWindows::is_this() const
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}


std::string PlatformWindows::get_dynamic_library_file_name( const std::string& sourceName ) const
{
    std::string result = sourceName;
    result = FileReplaceExtension(result,"dll");
    return result;
}


std::string PlatformWindows::get_program_file_name( const std::string& sourceName ) const
{
    std::string result = sourceName;
    result = FileReplaceExtension(result,"exe");
    return result;
}


const char* PlatformWindows64::arch() const
{
    return "x64";
}


bool PlatformWindows64::is_this() const
{
    return PlatformWindows::is_this() && (sizeof(void *) == 8);
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


std::string PlatformOSX::get_dynamic_library_file_name( const std::string& sourceName ) const
{
    std::string result = std::string("lib")+sourceName;
    result = FileReplaceExtension(result,"dylib");
    return result;
}


std::string PlatformOSX::get_program_file_name( const std::string& sourceName ) const
{
    std::string result = sourceName;
    result = FileReplaceExtension(result,"");
    return result;
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

#ifdef _MSC_VER
    const char* res = _getcwd(cwd, sizeof(cwd));
#else
    const char* res = getcwd(cwd, sizeof(cwd));
#endif

    if (!res)
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
    AXE_LOG( "Test", axe::Level::Verbose, "Creating directory [%s]", directory );
#ifdef _MSC_VER
    int status = _mkdir(directory);
#else
    int status = mkdir(directory, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
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
        //result.m_time = file_stat.st_mtimespec; OSX?
        result.m_time = file_stat.st_mtime;
    }

    return result;
}


//! Warning: don't use this in concurrent scenarios!
int Run( const std::string& workingPath,
         const std::string& command,
         const std::vector<std::string>& arguments,
         std::function<void(const char*)> out,
         std::function<void(const char*)> err,
         int maxMilliseconds, int* killedFlag )
{
    int result = 0;

    std::stringstream logstr;
    logstr << command << " ";
    for( auto s: arguments) { logstr << s << " "; }
    AXE_LOG( "run", axe::Level::Verbose, logstr.str() );

    if (killedFlag)
    {
        *killedFlag = 0;
    }


#ifdef _MSC_VER

    HANDLE g_hChildStd_OUT_Rd = NULL;
    HANDLE g_hChildStd_OUT_Wr = NULL;
    HANDLE g_hChildStd_ERR_Rd = NULL;
    HANDLE g_hChildStd_ERR_Wr = NULL;

    SECURITY_ATTRIBUTES saAttr;

    // Set the bInheritHandle flag so pipe handles are inherited.
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe for the child process's STDOUT.
    if ( !CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0) )
        return -1;

    // Ensure the read handle to the pipe for STDOUT is not inherited.
    if ( !SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0) )
        return -1;

    // Create a pipe for the child process's STDERR.
    if ( !CreatePipe(&g_hChildStd_ERR_Rd, &g_hChildStd_ERR_Wr, &saAttr, 0) )
        return -1;

    // Ensure the read handle to the pipe for STDERR is not inherited.
    if ( !SetHandleInformation(g_hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0) )
        return -1;

    // Create the child process.
    PROCESS_INFORMATION piProcInfo;
    {
        STARTUPINFO siStartInfo;
        BOOL bSuccess = FALSE;

        // Set up members of the PROCESS_INFORMATION structure.
        ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );

        // Set up members of the STARTUPINFO structure.
        // This structure specifies the STDIN and STDOUT handles for redirection.
        ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
        siStartInfo.cb = sizeof(STARTUPINFO);
        siStartInfo.hStdError = g_hChildStd_ERR_Wr;
        siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        // This is a limitation for CreateProcess
        assert( logstr.str().size()<32760 );

        // Stupid windows wants to modify this.
        char tempCmd[32760];
        strncpy(tempCmd, logstr.str().c_str(), sizeof(tempCmd)-1 );

        // Create the child process.
        bSuccess = CreateProcess(
                    nullptr,
                    tempCmd,                // command line
                    nullptr,                // process security attributes
                    nullptr,                // primary thread security attributes
                    TRUE,                   // handles are inherited
                    0,                      // creation flags
                    nullptr,                // use parent's environment
                    nullptr,//workingPath.c_str(),    // use parent's current directory
                    &siStartInfo,           // STARTUPINFO pointer
                    &piProcInfo);           // receives PROCESS_INFORMATION

        // If an error occurs, exit the application.
        if ( !bSuccess )
        {
            return -1;
        }

        // Close the unused pipe ends
        CloseHandle(g_hChildStd_OUT_Wr);
        CloseHandle(g_hChildStd_ERR_Wr);
    }


    HANDLE eventOut = CreateEvent(NULL, TRUE, FALSE, NULL);
    HANDLE eventErr = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Read from pipe that is the standard output for child process.
    {
        DWORD dwBytesRead;
        char chBuf[4096];
        OVERLAPPED stOverlappedOut = {0};
        OVERLAPPED stOverlappedErr = {0};
        BOOL bSuccess = FALSE;

        stOverlappedOut.hEvent = eventOut;
        stOverlappedErr.hEvent = eventErr;

        DWORD error;
        bool endOut = false;
        bool endErr = false;
        bool waitingOut = false;
        bool waitingErr = false;
        while (!endOut || !endErr || waitingOut || waitingErr)
        {
            if (!endOut && !waitingOut)
            {
                bSuccess = ReadFile( g_hChildStd_OUT_Rd, chBuf, sizeof(chBuf)-1, &dwBytesRead, &stOverlappedOut);
                if( bSuccess)
                {
                    // All in the first read
                    if (dwBytesRead)
                    {
                        chBuf[dwBytesRead]=0;
                        out(chBuf);
                    }
                    else
                    {
                        endOut = true;
                    }
                }
                else
                {
                    error = GetLastError();
                    switch (error)
                    {
                    case ERROR_BROKEN_PIPE:
                        endOut = true;
                        break;

                    case ERROR_HANDLE_EOF:
                        endOut = true;
                        break;

                    case ERROR_IO_PENDING:
                        waitingOut = true;
                        break;

                    default:
                        break;
                    }
                }
            }

            if (!endErr && !waitingErr)
            {
                bSuccess = ReadFile( g_hChildStd_ERR_Rd, chBuf, sizeof(chBuf)-1, &dwBytesRead, &stOverlappedErr);
                if( bSuccess)
                {
                    // All in the first read
                    if (dwBytesRead)
                    {
                        chBuf[dwBytesRead]=0;
                        err(chBuf);
                    }
                    else
                    {
                        endErr = true;
                    }
                }
                else
                {
                    error = GetLastError();
                    switch (error)
                    {
                    case ERROR_BROKEN_PIPE:
                        endErr = true;
                        break;

                    case ERROR_HANDLE_EOF:
                        endErr = true;
                        break;

                    case ERROR_IO_PENDING:
                        waitingErr = true;
                        break;

                    default:
                        break;
                    }
                }
            }


            if (waitingOut)
            {
                waitingOut = false;

                // Check the result of the asynchronous read without waiting (forth parameter FALSE).
                BOOL bResult = GetOverlappedResult(g_hChildStd_OUT_Rd, &stOverlappedOut, &dwBytesRead, FALSE) ;
                if (!bResult)
                {
                    switch (error = GetLastError())
                    {
                    case ERROR_HANDLE_EOF:
                        endOut = true;
                        break;

                    case ERROR_IO_INCOMPLETE:
                        waitingOut = true;
                        endOut = false;
                        break;

                    default:
                        break;
                    }
                }
                else
                {
                    // Manual-reset event should be reset since it is now signaled.
                    ResetEvent(stOverlappedOut.hEvent);
                }

            }


            if (waitingErr)
            {
                waitingErr = false;

                // Check the result of the asynchronous read without waiting (forth parameter FALSE).
                BOOL bResult = GetOverlappedResult(g_hChildStd_ERR_Rd, &stOverlappedErr, &dwBytesRead, FALSE) ;
                if (!bResult)
                {
                    switch (error = GetLastError())
                    {
                    case ERROR_HANDLE_EOF:
                        endErr = true;
                        break;

                    case ERROR_IO_INCOMPLETE:
                        waitingErr = true;
                        endErr = false;
                        break;

                    default:
                        break;
                    }
                }
                else
                {
                    // Manual-reset event should be reset since it is now signaled.
                    ResetEvent(stOverlappedErr.hEvent);
                }

            }
        }
    }

    DWORD exitCode;
    if (!GetExitCodeProcess(piProcInfo.hProcess, &exitCode))
        return -1;

    CloseHandle(eventOut);
    CloseHandle(eventErr);

    // The remaining open handles are cleaned up when this process terminates.
    // To avoid resource leaks in a larger application, close handles explicitly.
    CloseHandle(g_hChildStd_OUT_Rd);
    CloseHandle(g_hChildStd_ERR_Rd);

    // Close handles to the child process and its primary thread.
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);

    result = exitCode;

#else

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
        result = -1;
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

        // Change the path if necessary
        if (workingPath.size())
        {
            if (chdir( workingPath.c_str()) !=0 )
            {
                // Failed to enter the working path
                exit(-1);
            }
        }

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

        // If we are here, we failed to exec.
        delete[] argv;
        exit(-1);
    }
    else
    {
        // close fds not required by parent
        close(pipes[CHILD_OUT_PIPE][1]);
        close(pipes[CHILD_ERR_PIPE][1]);

        bool finished = false;
        bool killing = false;
        auto startTime = std::chrono::steady_clock::now();

        while (!finished)
        {
            fd_set set;
            struct timeval timeout;

            // Initialize the file descriptor set.
            FD_ZERO(&set);
            FD_SET(pipes[CHILD_OUT_PIPE][0], &set);
            FD_SET(pipes[CHILD_ERR_PIPE][0], &set);

            // Initialize the timeout data structure.
            timeout.tv_sec = 0;
            timeout.tv_usec = 10000;    // poll every 10ms

            // \todo Replace FD_SETSIZE for the max pipe index to optimise
            int ret = select(FD_SETSIZE, &set, NULL, NULL, &timeout);

            // a return value of 0 means that the time expired
            // without any acitivity on the file descriptor
            if (ret == 0)
            {
                finished = false;
            }
            else if (ret < 0)
            {
                // error occurred
                finished = true;
                result = -1;
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
                if (WIFEXITED(childStatus))
                {
                    result = WEXITSTATUS(childStatus);
                }
                else
                {
                    result = -1;
                }
            }

            // should we kill because it takes too long?
            if (maxMilliseconds>0)
            {
                auto now = std::chrono::steady_clock::now();
                auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();

                if (deltaTime>maxMilliseconds)
                {
                    if (!killing)
                    {
                        kill(childPid, SIGTERM);
                        killing = true;
                        if (killedFlag)
                        {
                            *killedFlag = 1;
                        }
                    }
                    else
                    {
                        // Two seconds to die gracefully
                        if (deltaTime>maxMilliseconds+2000)
                        {
                            kill(childPid, SIGKILL);
                            finished = true;
                        }
                    }
                }
            }
        }

        close(pipes[CHILD_OUT_PIPE][0]);
        close(pipes[CHILD_ERR_PIPE][0]);
    }

#endif

    return result;
}




void LoadAndRun( const char* lib, const char* methodName,
                 const char* workspace, const char** configurations, const char** targets )
{
    typedef void (*CraftMethod)( const char* workspace, const char** configurations, const char** targets, axe::Kernel* log_kernel );

#ifdef _MSC_VER

    // Get a handle to the DLL module.
    HINSTANCE hinstLib = LoadLibrary(TEXT(lib));
    assert(hinstLib);

    // If the handle is valid, try to get the function address.
    if (hinstLib)
    {
        CraftMethod craftMethod = (CraftMethod)GetProcAddress(hinstLib, methodName);
        assert(craftMethod);

        // If the function address is valid, call the function.
        if (craftMethod)
        {
            craftMethod(workspace, configurations, targets);
        }

        // Free the DLL module.
        BOOL result = FreeLibrary(hinstLib);
        assert(result);
    }

#else

    // Load the dynamic library
    void *libHandle = dlopen( lib, RTLD_LAZY | RTLD_LOCAL );
    assert( libHandle );

    dlerror();
    void *method = dlsym( libHandle, methodName );
    const char* error = dlerror();
    if (error)
    {
        std::cout << "Loading symbol error:" << error << std::endl;
    }

    assert( method );

    // Run it
    CraftMethod craftMethod = (CraftMethod)method;
    craftMethod(workspace, configurations, targets, axe::s_kernel);

    // todo: free library?

#endif
}
