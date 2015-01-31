// Axe is a logging system.
// - complete
// - easy to use
// - efficient

// Every event happens in a thread, a plain category, a hierarchical section and a warning level.
// Every thread holds a section state (hierarchy)

#pragma once

#include <cassert>
#include <cstdio>
#include <vector>
#include <string>
#include <sstream>
#include <cstdarg>
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
        Event( AxeTime time, ThreadId thread, Level level, const char* category, const std::string& message )
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
        virtual void Process( const Event& e ) = 0;
    };

    class TerminalBin : public Bin
    {
    public:

        virtual void Process( const Event& e ) override
        {
            printf( "[%8s] %s\n", e.m_category.c_str(), e.m_message.c_str() );
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

        void AddMessage( const char* category, Level level, const std::string& message )
        {
            AxeTime now = get_now();
            ThreadId thisThread = 0;

            for( auto bin: m_bins )
            {
                bin->Process( Event(now,thisThread,level,category,message) );
            }
        }

    private:

        std::vector< std::shared_ptr<Bin> > m_bins;
    };


    inline void log( const char* category, Level level, const char* message, ... )
    {
        if (s_kernel)
        {
            va_list args;
            va_start(args, message);

            char temp[256];
            vsnprintf( temp, sizeof(temp), message, args );
            s_kernel->AddMessage( category, level, temp );

            va_end(args);
        }
    }


    inline void log( const char* category, Level level, std::string message, ... )
    {
        if (s_kernel)
        {
            va_list args;
            va_start(args, message);

            char temp[256];
            vsnprintf( temp, sizeof(temp), message.c_str(), args );
            s_kernel->AddMessage( category, level, temp );

            va_end(args);
        }
    }


    inline void log_single( const char* category, Level level, const char* message, ... )
    {
        if (s_kernel)
        {
            s_kernel->AddMessage( category, level, message );
        }
    }


    inline void log_single( const char* category, Level level, const std::string& message, ... )
    {
        if (s_kernel)
        {
            s_kernel->AddMessage( category, level, message );
        }
    }


    inline void begin_section( const char* name )
    {
        if (s_kernel)
        {
            s_kernel->AddMessage( "Section", L_Debug, name );
        }
    }


    inline void end_section()
    {
        if (s_kernel)
        {
            s_kernel->AddMessage( "EndSection", L_Debug, "" );
        }
    }


    class ScopedSectionHelper
    {
    public:
        inline ScopedSectionHelper( const char* name );
        inline ~ScopedSectionHelper();
    };
}

#define AXE_COMPILE_LEVEL_LIMIT   axe::L_All

#if !defined(AXE_ENABLE)

#define AXE_IMPLEMENT()
#define AXE_INITIALISE(APPNAME,OPTIONS)
#define AXE_FINALISE()
#define AXE_LOG(CAT,LEVEL,...)
#define AXE_DECLARE_SECTION(TAG,NAME)
#define AXE_BEGIN_SECTION(TAG)
#define AXE_END_SECTION()
#define AXE_SCOPED_SECTION(TAG)
#define AXE_DECLARE_CATEGORY(TAG,NAME)


#else

#define AXE_IMPLEMENT()                     \
    axe::Kernel* axe::s_kernel = nullptr;

#define AXE_INITIALISE(APPNAME,OPTIONS)     \
    {                                       \
        assert(!axe::s_kernel);             \
        axe::s_kernel = new axe::Kernel();  \
    }

#define AXE_FINALISE()                      \
    {                                       \
        assert(axe::s_kernel);              \
        delete axe::s_kernel;               \
        axe::s_kernel = nullptr;            \
    }

#define AXE_NUMARGS(...) AXE_NUMARGS_(__VA_ARGS__,AXE_NUMARGS_RSEQ_N())
#define AXE_NUMARGS_(...) AXE_NUMARGS_N(__VA_ARGS__)
#define AXE_NUMARGS_N( \
          _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
         _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
         _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
         _31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
         _41,_42,_43,_44,_45,_46,_47,_48,_49,_50, \
         _51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
         _61,_62,_63,N,...) N
#define AXE_NUMARGS_RSEQ_N() \
         63,62,61,60,                   \
         59,58,57,56,55,54,53,52,51,50, \
         49,48,47,46,45,44,43,42,41,40, \
         39,38,37,36,35,34,33,32,31,30, \
         29,28,27,26,25,24,23,22,21,20, \
         19,18,17,16,15,14,13,12,11,10, \
         9,8,7,6,5,4,3,2,1,0


#define AXE_LOG(CAT,LEVEL,...)                          \
    if (LEVEL<=AXE_COMPILE_LEVEL_LIMIT)                 \
    {                                                   \
        if (AXE_NUMARGS(__VA_ARGS__)>1)                 \
        {                                               \
            axe::log( CAT, LEVEL, __VA_ARGS__);         \
        }                                               \
        else                                            \
        {                                               \
            axe::log_single( CAT, LEVEL, __VA_ARGS__ ); \
        }                                               \
    }

#define AXE_DECLARE_SECTION(TAG,NAME)
#define AXE_BEGIN_SECTION(TAG)  axe::begin_section(TAG);
#define AXE_END_SECTION()       axe::end_section();
#define AXE_SCOPED_SECTION(TAG) axe::ScopedSectionHelper(TAG)

#define AXE_DECLARE_CATEGORY(TAG,NAME)

#endif //AXE_ENABLE


inline axe::ScopedSectionHelper::ScopedSectionHelper( const char* name )
{
    AXE_BEGIN_SECTION( name );
}

inline axe::ScopedSectionHelper::~ScopedSectionHelper()
{
    AXE_END_SECTION();
}
