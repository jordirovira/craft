// Axe is a logging system.
// - complete
// - easy to use
// - efficient

// Every event happens in a thread, a plain category, a hierarchical section and a warning level.
// Every thread holds a section state hierarchy)

#pragma once

#include "axe.h"

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


namespace axe
{


    class ReadableFileBin : public FileBin
    {
    private:

        bool file_read(void* dst, int size)
        {
            if (!m_pFile) return false;

            // partial copy
            int dataLeft = m_bufferData-m_bufferPtr;
            if (dataLeft && dataLeft<size)
            {
                memcpy(dst,&m_fileBuffer[m_bufferPtr], dataLeft);
                size -= dataLeft;
                dst = ((uint8_t*)dst)+dataLeft;
                m_bufferPtr+=dataLeft;
                dataLeft = 0;
            }

            // big read?
            if (size>sizeof(m_fileBuffer))
            {
                // direct read
                int read = (int)fread( dst, size, 1 , m_pFile );
                if (read!=1)
                {
                    fclose(m_pFile);
                    m_pFile = nullptr;
                    return false;
                }
                return true;
            }

            // read more into the buffer?
            if (dataLeft<size)
            {
                m_bufferPtr = 0;
                m_bufferData = (int)fread( m_fileBuffer, 1, sizeof(m_fileBuffer), m_pFile );
                if (m_bufferData==0)
                {
                    fclose(m_pFile);
                    m_pFile = nullptr;
                    return false;
                }
                dataLeft = m_bufferData;
            }

            // read the (rest of the) required data
            assert(size<=dataLeft);
            memcpy(dst,&m_fileBuffer[m_bufferPtr], size);
            m_bufferPtr+=size;
            return true;
        }


        bool InternalLoadEvents(const char* filePath, std::vector<Event>& result)
        {
            m_pFile = fopen( filePath, "rb" );
            if( !m_pFile )
            {
                return false;
            }

            char header[16];
            if (!file_read(header, 16))
            {
                fclose(m_pFile);
                m_pFile = nullptr;
                return false;
            }

            if (memcmp(header,AXE_FILEBIN_HEADER,16))
            {
                fclose(m_pFile);
                m_pFile = nullptr;
                return false;
            }

            uint32_t version=0;
            if (!file_read(&version, sizeof(uint32_t)))
            {
                fclose(m_pFile);
                m_pFile = nullptr;
                return false;
            }

            if (version!=AXE_FILEBIN_VERSION)
            {
                fclose(m_pFile);
                m_pFile = nullptr;
                return false;
            }

            while (true)
            {
                // read one event

                uint64_t eventSize;
                if (!file_read( &eventSize, sizeof(uint64_t))) break;

                result.push_back(Event());
                auto& e = result.back();

                if (!file_read( &e.m_time, sizeof(uint64_t))) break;
                if (!file_read( &e.m_thread, sizeof(uint32_t))) break;
                if (!file_read( &e.m_level, sizeof(Level))) break;
                if (!file_read( &e.m_type, sizeof(EventType))) break;

                // category
                int32_t textSize=0;
                if (!file_read( &textSize, sizeof(int32_t))) break;
                if (textSize)
                {
                    e.m_category.resize(textSize);
                    if (!file_read( &e.m_category[0], textSize)) break;
                }

                // message
                if (!file_read( &textSize, sizeof(int32_t))) break;
                if (textSize)
                {
                    e.m_message.resize(textSize);
                    if (!file_read( &e.m_message[0], textSize)) break;
                }

                // data
                if (!file_read( &textSize, sizeof(int32_t))) break;
                if (textSize)
                {
                    e.m_data.resize(textSize);
                    if (!file_read( &e.m_data[0], textSize)) break;
                }
            }

            if (m_pFile)
            {
                fclose(m_pFile);
                m_pFile = nullptr;
            }
            return true;
        }


    public:

        ReadableFileBin()
        {
        }

        static bool LoadEvents(const char* filePath, std::vector<Event>& result)
        {
            ReadableFileBin* bin = new ReadableFileBin();
            bool res = bin->InternalLoadEvents(filePath,result);
            delete bin;
            return res;
        }

     };

} // axe namespace

