// Axe is a logging system.
// - complete
// - easy to use
// - efficient

// Every event happens in a thread, a plain category, a hierarchical section and a warning level.
// Every thread holds a section state (hierarchy)

#pragma once

#include <cassert>
#include <cstdio>

//! This macro enables it all. It shouldn't be here, but specified in the command line or project configuration.
#define AXE_ENABLE    1

namespace axe
{
    typedef enum
    {
        L_Fatal,
        L_Error,
        L_Warning,
        L_Info,
        L_Debug,
        L_Verbose,
        L_All
    } Level;

    typedef int64_t AxeTime;
    typedef int32_t ThreadId;

    class Kernel;
    class Bin;
    class TerminalBin;
    class Event;

    extern class Kernel* s_kernel;

    inline AxeTime get_now()
    {
        return 0;
    }

    class Event
    {
    public:
        Event( AxeTime time, ThreadId thread, Level level, const char* category, const char* message )
        {
            m_time = time;
            m_thread = thread;
            m_level = level;
            m_category = category;
            m_message = message;
        }

        AxeTime m_time;
        ThreadId m_thread;
        Level m_level;
        std::string m_category;
        std::string m_message;
    };

    class Bin
    {
    public:

        // Ensure virtual destruction
        virtual ~Bin() {}
        virtual void Process( Event& e ) = 0;
    };

    class TerminalBin : public Bin
    {
    public:

        virtual void Process( Event& e )
        {
            printf( "[%8s] %s", e.m_category.c_str(), e.m_message.c_str() );
        }
    };

    class Kernel
    {
    public:

        Kernel()
        {
            // Default setup
            m_bins.push_back( std::make_shared<TerminalBin>() );
        }

        void AddMessage( const char* category, Level level, const char* message )
        {
            AxeTime now = get_now();
            ThreadId thisThread = 0;
        }

    private:

        std::vector< std::shared_ptr<Bin> > m_bins;
    };


    inline void log( const char* category, Level level, const char* message )
    {
        assert( s_kernel != nullptr );
        s_kernel->AddMessage( category, level, message );
    }

}

#define AXE_COMPILE_LEVEL_LIMIT   axe::L_All

#if defined(AXE_ENABLE)

#define AXE_IMPLEMENT()
#define AXE_INITIALISE(APPNAME,OPTIONS)
#define AXE_FINALISE()
#define AXE_LOG(CAT,LEVEL,MESSAGE)
#define AXE_DECLARE_SECTION(TAG,NAME)
#define AXE_BEGIN_SECTION(TAG)
#define AXE_END_SECTION()
#define AXE_SCOPED_SECTION(TAG)
#define AXE_DECLARE_CATEGORY(TAG,NAME)


#else

#define AXE_IMPLEMENT()                     \
    static axe::Kernel* axe::s_kernel = nullptr;

#define AXE_INITIALISE(APPNAME,OPTIONS)
#define AXE_FINALISE()

#define AXE_LOG(CAT,LEVEL,MESSAGE)          \
    if (LEVEL<=AXE_COMPILE_LEVEL_LIMIT)     \
    {                                       \
        axe::log( CAT, LEVEL, MESSAGE)      \
    }

#define AXE_DECLARE_SECTION(TAG,NAME)
#define AXE_BEGIN_SECTION(TAG)
#define AXE_END_SECTION()
#define AXE_SCOPED_SECTION(TAG)
#define AXE_DECLARE_CATEGORY(TAG,NAME)

#endif //AXE_ENABLE
