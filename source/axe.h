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

#include <algorithm>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <tuple>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <climits>
#include <cstring>


//! This macro enables it all. It shouldn't be here, but specified in the command line or project configuration.
//#define AXE_ENABLE    1

//! This is used to filter logging at compile time.
#define AXE_COMPILE_LEVEL_LIMIT   axe::Level::All


// Platform-specific requirements
#ifdef _WIN32
    #include <Windows.h>
#else
    #include <unistd.h>
#endif


namespace axe
{

    // Platform helpers
    namespace platform
    {
        std::string GetHostName();

        // Platform-specific helpers implementation
        #ifdef _WIN32

            inline std::string GetHostName()
            {
                std::string result;

                TCHAR  infoBuf[MAX_COMPUTERNAME_LENGTH+1];
                DWORD  bufCharCount = MAX_COMPUTERNAME_LENGTH+1;

                // Get and display the name of the computer.
                if( !GetComputerName( infoBuf, &bufCharCount ) )
                {
                    result = "unknown-host";
                }
                else
                {
#ifdef _UNICODE
                    char strInfoBuf[MAX_COMPUTERNAME_LENGTH+1];
                    WideCharToMultiByte(CP_ACP, 0, infoBuf, -1, strInfoBuf, MAX_COMPUTERNAME_LENGTH+1, NULL, NULL);
                    result = strInfoBuf;
#else
                    result = infoBuf;
#endif
                }

                // Get and display the user name.
            //    if( !GetUserName( infoBuf, &bufCharCount ) )
            //      printError( TEXT("GetUserName") );
            //    _tprintf( TEXT("\nUser name:          %s"), infoBuf );

                return result;
            }

        #else

            inline std::string GetHostName()
            {
                char hostname[HOST_NAME_MAX];
                int result = gethostname(hostname, HOST_NAME_MAX);
                if (result)
                {
                    return "unknown-host";
                }

//                char username[LOGIN_NAME_MAX];
//                result = getlogin_r(username, LOGIN_NAME_MAX);
//                if (result)
//                {
//                    return "unknown-host";
//                }

                return hostname;
            }

        #endif

    } // namespace platform


    enum class EventType : uint8_t
    {
        Null,
        Message,
        RecursiveSpanBegin,
        RecursiveSpanEnd,

        // For these types the message is a field name and the value is in data of the appropiate type.
        StringValue,
        TimeValue,      // Calendar date+time up to seconds
        IntValue,
        FloatValue,
        _Count
    };

    enum class Level : uint8_t
    {
        Fatal,
        Error,
        Warning,
        Info,
        Debug,
        Verbose,
        All
    };

    // Microseconds from an arbitray point (usually the axe kernel creation time)
    typedef int64_t AxeTime;
    typedef std::thread::id ThreadId;

    class Kernel;
    class Bin;
    class TerminalBin;
    class Event;

    extern class Kernel* s_kernel;

    class Event
    {
    public:
        Event()
        {
            m_time = 0;
            m_thread = std::thread::id();
            m_level = Level::Fatal;
            m_type = EventType::Null;
        }

        Event( AxeTime time, ThreadId thread, EventType type, Level level, const char* category, const std::string& message )
        {
            m_time = time;
            m_thread = thread;
            m_type = type;
            m_level = level;
            m_category = category;
            m_message = message;
        }

        AxeTime m_time;
        ThreadId m_thread;
        Level m_level;
        EventType m_type;
        std::string m_category;
        std::string m_message;
        std::vector<uint8_t> m_data;

        void EncodeStringData( const std::string& value )
        {
            if (value.size())
            {
                m_data.resize(value.size());
                memcpy(&m_data[0],&value[0],value.size());
            }
        }

        void DecodeStringData( std::string& value ) const
        {
            value.resize(m_data.size());
            if (m_data.size())
            {
                memcpy(&value[0],&m_data[0],m_data.size());
            }
        }

        void EncodeTimeData( const std::time_t& value )
        {
            uint64_t seconds = (uint64_t)value;
            m_data.resize(sizeof(seconds));
            memcpy(&m_data[0],&seconds,sizeof(seconds));
        }

        void DecodeTimeData( std::time_t& value ) const
        {
            value = 0;
            if ( m_data.size()==sizeof(uint64_t) )
            {
                uint64_t seconds;
                memcpy(&seconds,&m_data[0],sizeof(seconds));
                value = (time_t)seconds;
            }
        }

        void EncodeIntData( int64_t value )
        {
            m_data.resize(sizeof(value));
            memcpy(&m_data[0],&value,sizeof(value));
        }

        void DecodeIntData( int64_t& value ) const
        {
            value = 0;
            if ( m_data.size()==sizeof(value) )
            {
                 memcpy(&value,&m_data[0],sizeof(value));
            }
        }

        void EncodeFloatData( float value )
        {
            m_data.resize(sizeof(value));
            memcpy(&m_data[0],&value,sizeof(value));
        }

        void DecodeFloatData( float& value ) const
        {
            value = 0.0f;
            if ( m_data.size()==sizeof(value) )
            {
                memcpy(&value,&m_data[0],sizeof(value));
            }
        }

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
            // Ignore spans and others.
            if (e.m_type==EventType::Message)
            {
                printf( "[%8s] %s\n", e.m_category.c_str(), e.m_message.c_str() );
                fflush(stdout);
            }
            else if (e.m_type==EventType::IntValue)
            {
                int64_t v = 0;
                e.DecodeIntData(v);
                printf( "[%8s] (int) %s : %lld\n", e.m_category.c_str(), e.m_message.c_str(), v );
                fflush(stdout);
            }
        }
    };

    class FileBin : public Bin
    {
    protected:
        FILE* m_pFile = nullptr;
        uint64_t m_writtenSize = 0;
        uint64_t m_writeLimit = 64*1024*1024;

        uint8_t m_fileBuffer[1024*1024];
        uint32_t m_bufferPtr=0;
        uint32_t m_bufferData=0;

#define AXE_FILEBIN_VERSION     2
#define AXE_FILEBIN_HEADER      "AxeLogBinaryFile"


        bool file_write(const void* src, int size)
        {
            if (!m_pFile) return false;

            // does it fit?
            int dataLeft = sizeof(m_fileBuffer)-m_bufferPtr;
            if (dataLeft<size)
            {
                flush_write_buffer();
            }

            // big write?
            if (size>sizeof(m_fileBuffer))
            {
                // direct write
                int written = (int)fwrite( src, size, 1 , m_pFile );
                if (written!=1)
                {
                    fclose(m_pFile);
                    m_pFile = nullptr;
                    return false;
                }
                return true;
            }

            memcpy(&m_fileBuffer[m_bufferPtr], src, size);
            m_bufferPtr+=size;
            return true;
        }

        void flush_write_buffer()
        {
            if (m_pFile && m_bufferPtr)
            {
                fwrite( m_fileBuffer, 1, m_bufferPtr, m_pFile );
                m_bufferPtr = 0;
            }
        }


        FileBin()
        {
        }

    public:

        FileBin(const char* strFileName)
        {
            m_pFile = fopen( strFileName, "wb" );
            if( !m_pFile )
            {
//                throw FileNotFoundException
//                        ( boost::str( boost::format("File not found : %s") % strFile ).c_str() );

//                return boost::shared_array<uint8_t>();
            }
            else
            {
                fwrite( AXE_FILEBIN_HEADER, 16, 1 , m_pFile );

                uint32_t ver = AXE_FILEBIN_VERSION;
                fwrite( &ver, sizeof(uint32_t), 1 , m_pFile );

                m_writtenSize = 20;
            }

        }

        ~FileBin()
        {
            if (m_pFile)
            {
                flush_write_buffer();
                fclose(m_pFile);
            }
        }

        virtual void Process( const Event& e ) override
        {
            if (m_pFile)
            {
                uint64_t eventSize =
                        8   // time
                        +4  // thread
                        +1  // level
                        +1  // type
                        +4+e.m_category.size()
                        +4+e.m_message.size()
                        +4+e.m_data.size()
                        ;
                if (m_writtenSize+eventSize>m_writeLimit)
                {
                    fclose(m_pFile);
                    m_pFile = nullptr;
                }
                else
                {
                    file_write( &eventSize, sizeof(uint64_t) );

                    file_write( &e.m_time, sizeof(uint64_t) );
                    file_write( &e.m_thread, sizeof(uint32_t) );
                    file_write( &e.m_level, sizeof(Level) );
                    file_write( &e.m_type, sizeof(EventType) );

                    uint32_t s;

                    s = (uint32_t)e.m_category.size();
                    file_write( &s, sizeof(uint32_t) );
                    if (s>0) file_write( &e.m_category[0], s );

                    s = (uint32_t)e.m_message.size();
                    file_write( &s, sizeof(uint32_t) );
                    if (s>0) file_write( &e.m_message[0], s );

                    s = (uint32_t)e.m_data.size();
                    file_write( &s, sizeof(uint32_t) );
                    if (s>0) file_write( &e.m_data[0], s );

                    m_writtenSize += sizeof(uint64_t)+eventSize;
                }
            }
        }


     };

    class Kernel
    {
    public:

        Kernel(const char* strAppName, const char* strVersion, uint64_t options)
        {
            startWallTime = std::chrono::steady_clock::now();

            // Gather system information
            std::string machineName = platform::GetHostName();
            std::string programName = strAppName ? strAppName : "";
            std::string programVersion = strVersion ? strVersion : "";

            auto t = std::time(nullptr);
            auto tm = *std::gmtime(&t);

            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y_%m_%d-%H_%M_%S");
            std::string nowStr = oss.str();

            // Default setup
            m_bins.push_back( std::make_shared<TerminalBin>() );

            char logFileName[256];
            if (programVersion.size())
            {
                snprintf( logFileName, sizeof(logFileName), "%s-%s-%s-%s.axe_log", programName.c_str(), strVersion, machineName.c_str(), nowStr.c_str() );
            }
            else
            {
                snprintf( logFileName, sizeof(logFileName), "%s-%s-%s.axe_log", programName.c_str(), machineName.c_str(), nowStr.c_str() );
            }
            m_bins.push_back( std::make_shared<FileBin>(logFileName) );

            // Initial system logs
            AddStringValue( "system", Level::Info, "Program", programName );
            AddStringValue( "system", Level::Info, "Version", programVersion );
            AddStringValue( "system", Level::Info, "Machine", machineName );
            AddTimeValue( "system", Level::Info, "StartTime", t );
        }

    private:

        inline void AddEvent( const Event& e )
        {
            while (m_binsLock.test_and_set(std::memory_order_acquire))  // acquire lock
                 ; // spin

            for( auto bin: m_bins )
            {
                bin->Process( e );
            }

            m_binsLock.clear(std::memory_order_release);               // release lock
        }

        void InitEvent( Event& e )
        {
            auto now = std::chrono::steady_clock::now();
            e.m_time = std::chrono::duration_cast<std::chrono::microseconds>(now - startWallTime).count();
            e.m_thread = std::this_thread::get_id();
        }

    public:

        void AddMessage( const char* category, EventType type, Level level, const std::string& message )
        {
            Event e;
            InitEvent(e);
            e.m_type = type;
            e.m_level = level;
            e.m_category = category;
            e.m_message = message;
            AddEvent(e);
        }

        void AddStringValue( const char* category, Level level, const std::string& field, const std::string& value )
        {
            Event e;
            InitEvent(e);
            e.m_type = EventType::StringValue;
            e.m_level = level;
            e.m_category = category;
            e.m_message = field;
            e.EncodeStringData(value);
            AddEvent(e);
        }

        void AddTimeValue( const char* category, Level level, const std::string& field, const std::time_t& value )
        {
            Event e;
            InitEvent(e);
            e.m_type = EventType::TimeValue;
            e.m_level = level;
            e.m_category = category;
            e.m_message = field;
            e.EncodeTimeData(value);
            AddEvent(e);
        }

        void AddIntValue( const char* category, Level level, const std::string& field, int64_t value )
        {
            Event e;
            InitEvent(e);
            e.m_type = EventType::IntValue;
            e.m_level = level;
            e.m_category = category;
            e.m_message = field;
            e.EncodeIntData(value);
            AddEvent(e);
        }

        void AddFloatValue( const char* category, Level level, const std::string& field, float value )
        {
            Event e;
            InitEvent(e);
            e.m_type = EventType::IntValue;
            e.m_level = level;
            e.m_category = category;
            e.m_message = field;
            e.EncodeFloatData(value);
            AddEvent(e);
        }

    private:

        std::atomic_flag m_binsLock = ATOMIC_FLAG_INIT;

        std::vector< std::shared_ptr<Bin> > m_bins;

        std::chrono::time_point<std::chrono::steady_clock> startWallTime;
    };


    inline void log( const char* category, Level level, const char* message, ... )
    {
        if (s_kernel)
        {
            va_list args;
            va_start(args, message);

            char temp[256];
            vsnprintf( temp, sizeof(temp), message, args );
            s_kernel->AddMessage( category, EventType::Message, level, temp );

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
            s_kernel->AddMessage( category, EventType::Message, level, temp );

            va_end(args);
        }
    }


    inline void log_single( const char* category, Level level, const char* message, ... )
    {
        if (s_kernel)
        {
            s_kernel->AddMessage( category, EventType::Message, level, message );
        }
    }


    inline void log_single( const char* category, Level level, const std::string& message, ... )
    {
        if (s_kernel)
        {
            s_kernel->AddMessage( category, EventType::Message, level, message );
        }
    }


    inline void log_lines( const char* category, Level level, const std::string& s )
    {
        if (s_kernel)
        {
            std::string delims="\n\r";
            std::string::const_iterator b = s.begin();
            std::string::const_iterator e = s.end();

            std::string::const_iterator c = b;
            while (c!=e)
            {
                if ( std::find( delims.begin(), delims.end(), *c) != delims.end() )
                {
                    // Don't log empty lines
                    if (c-b>1)
                    {
                        s_kernel->AddMessage( category, EventType::Message, level, std::string(b,c) );
                    }
                    b=c;
                    ++b;
                }
                ++c;
            }

            // Add the last element
            if (b!=c)
            {
                s_kernel->AddMessage( category, EventType::Message, level, std::string(b,c) );
            }
        }
    }


    inline void begin_section( const char* name, axe::Level level )
    {
        if (s_kernel)
        {
            s_kernel->AddMessage( "code", axe::EventType::RecursiveSpanBegin, level, name );
        }
    }


    inline void end_section(axe::Level level)
    {
        if (s_kernel)
        {
            s_kernel->AddMessage( "code", axe::EventType::RecursiveSpanEnd, level, "" );
        }
    }


    class ScopedSectionHelper
    {
    public:
        inline ScopedSectionHelper( const char* name );
        inline ~ScopedSectionHelper();
    };


    class ScopedSectionLevelHelper
    {
    public:
        inline ScopedSectionLevelHelper( const char* name, axe::Level level );
        inline ~ScopedSectionLevelHelper();
    private:
        axe::Level m_level;
    };


    inline void log_string_value(const char* category, Level level, const char* key, const char* value)
    {
        if (s_kernel && category && key )
        {
            s_kernel->AddStringValue(category,level,key,value?value:"");
        }
    }


    inline void log_int_value(const char* category, Level level, const char* key, int64_t value)
    {
        if (s_kernel && category && key )
        {
            s_kernel->AddIntValue(category,level,key,value);
        }
    }


    inline void log_float_value(const char* category, Level level, const char* key, float value)
    {
        if (s_kernel && category && key )
        {
            s_kernel->AddFloatValue(category,level,key,value);
        }
    }

}


#if !defined(AXE_ENABLE)

#define AXE_IMPLEMENT()
#define AXE_INITIALISE(APPNAME,APPVER,OPTIONS)
#define AXE_FINALISE()
#define AXE_LOG(CAT,LEVEL,...)
#define AXE_LOG_LINES(CAT,LEVEL,TEXT)
#define AXE_DECLARE_SECTION(TAG,NAME)
#define AXE_BEGIN_SECTION(TAG)
#define AXE_END_SECTION()
#define AXE_SCOPED_SECTION(TAG)
#define AXE_SCOPED_SECTION_DETAILED(TAG,TEXT)
#define AXE_BEGIN_SECTION_LEVEL(TAG,LEVEL)
#define AXE_END_SECTION_LEVEL(LEVEL)
#define AXE_SCOPED_SECTION_LEVEL(TAG,LEVEL)
#define AXE_SCOPED_SECTION_DETAILED_LEVEL(TAG,TEXT,LEVEL)
#define AXE_DECLARE_CATEGORY(TAG,NAME)
#define AXE_STRING_VALUE(CAT,LEVEL,KEY,VALUE)
#define AXE_INT_VALUE(CAT,LEVEL,KEY,VALUE)
#define AXE_FLOAT_VALUE(CAT,LEVEL,KEY,VALUE)

#else

#define AXE_IMPLEMENT()                     \
    axe::Kernel* axe::s_kernel = nullptr;

#define AXE_INITIALISE(APPNAME,APPVER,OPTIONS)     \
    {                                       \
        assert(!axe::s_kernel);             \
        axe::s_kernel = new axe::Kernel(APPNAME,APPVER,OPTIONS);  \
    }

#define AXE_FINALISE()                      \
    {                                       \
        assert(axe::s_kernel);              \
        delete axe::s_kernel;               \
        axe::s_kernel = nullptr;            \
    }

#define AXE_NUMARGS(...) (std::tuple_size<decltype(std::make_tuple(__VA_ARGS__))>::value)


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

#define AXE_LOG_LINES(CAT,LEVEL,TEXT)                   \
    if (LEVEL<=AXE_COMPILE_LEVEL_LIMIT)                 \
    {                                                   \
        axe::log_lines( CAT, LEVEL, TEXT);              \
    }

#define AXE_DECLARE_SECTION(TAG,NAME)
#define AXE_BEGIN_SECTION(TAG)                      axe::begin_section(TAG,axe::Level::Debug)
#define AXE_END_SECTION()                           axe::end_section(axe::Level::Debug)
#define AXE_SCOPED_SECTION(TAG)                     axe::ScopedSectionHelper axe_scoped_helper_##TAG(#TAG)
#define AXE_SCOPED_SECTION_DETAILED(TAG,TEXT)       axe::ScopedSectionHelper axe_scoped_helper_##TAG(TEXT)
#define AXE_BEGIN_SECTION_LEVEL(TAG,LEVEL)                  axe::begin_section(TAG,LEVEL)
#define AXE_END_SECTION_LEVEL(LEVEL)                        axe::end_section(LEVEL)
#define AXE_SCOPED_SECTION_LEVEL(TAG,LEVEL)                 axe::ScopedSectionLevelHelper axe_scoped_helper_##TAG(#TAG,LEVEL)
#define AXE_SCOPED_SECTION_DETAILED_LEVEL(TAG,TEXT,LEVEL)   axe::ScopedSectionLevelHelper axe_scoped_helper_##TAG(TEXT,LEVEL)

#define AXE_DECLARE_CATEGORY(TAG,NAME)

#define AXE_STRING_VALUE(CAT,LEVEL,KEY,VALUE)       axe::log_string_value(CAT,LEVEL,KEY,VALUE)
#define AXE_INT_VALUE(CAT,LEVEL,KEY,VALUE)          axe::log_int_value(CAT,LEVEL,KEY,VALUE)
#define AXE_FLOAT_VALUE(CAT,LEVEL,KEY,VALUE)        axe::log_float_value(CAT,LEVEL,KEY,VALUE)

#endif //AXE_ENABLE


inline axe::ScopedSectionHelper::ScopedSectionHelper( const char* name )
{
    AXE_BEGIN_SECTION( name );
}

inline axe::ScopedSectionHelper::~ScopedSectionHelper()
{
    AXE_END_SECTION();
}


inline axe::ScopedSectionLevelHelper::ScopedSectionLevelHelper( const char* name, axe::Level level )
{
    m_level = level;
    AXE_BEGIN_SECTION_LEVEL( name, m_level );
}

inline axe::ScopedSectionLevelHelper::~ScopedSectionLevelHelper()
{
    AXE_END_SECTION_LEVEL(m_level);
}
