
#include "target.h"

#include "craft_private.h"
#include "axe.h"
#include "platform.h"

#include <string>
#include <sstream>
#include <vector>

#include <curl/curl.h>


DownloadTarget& DownloadTarget::url(const std::string& url)
{
    m_url = url;
    return *this;
}


struct CurlFileData
{
    std::string filename;
    FILE *stream;
};


static size_t curl_write(void *buffer, size_t size, size_t nmemb, void *stream)
{
    struct CurlFileData *out=(struct CurlFileData *)stream;
    if(out && !out->stream)
    {
        /* open file for writing */
        out->stream=fopen(out->filename.c_str(), "wb");
        if(!out->stream)
        {
            return -1; /* failure, can't open file to write */
        }
    }
    return fwrite(buffer, size, nmemb, out->stream);
}


std::shared_ptr<class BuiltTarget> DownloadTarget::build( ContextPlan& ctx )
{
    auto res = std::make_shared<BuiltTarget>();

    std::shared_ptr<Node> outputNode = std::make_shared<Node>();
    res->m_outputNode = outputNode;

    if ( m_url.size() )
    {
        std::string target;

        CURL *curl = nullptr;
        curl = curl_easy_init();
        if(curl)
        {
            char* strURL = curl_easy_escape( curl, m_url.c_str(), 0 );
            target = ctx.get_current_path()+FileSeparator()+std::string(strURL);
            curl_free( strURL );
            curl_easy_cleanup(curl);
        }

        outputNode->m_absolutePath = target;

        // Check if target already exists
        if (!FileExists(target))
        {
            // Generate dependencies
            std::vector<std::shared_ptr<Task>> reqs;
            for( const auto& u: m_uses )
            {
                std::shared_ptr<BuiltTarget> usedTarget = ctx.get_built_target( u );
                assert( usedTarget );

                reqs.insert( reqs.end(), usedTarget->m_outputTasks.begin(), usedTarget->m_outputTasks.end() );
            }

            // Generate download task
            auto task = std::make_shared<Task>( "download", outputNode,
                                             [=]()
            {
                int result = 0;

                CURL *curl = nullptr;
                CURLcode res;

                curl = curl_easy_init();
                if(curl)
                {

                    curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);

                    /* Set a pointer to our struct to pass to the callback */
                    CurlFileData fileData;
                    fileData.filename = target;
                    fileData.stream = nullptr;
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fileData);

                    /* Perform the request, res will get the return code */
                    res = curl_easy_perform(curl);

                    /* Check for errors */
                    if (res != CURLE_OK)
                    {
                        AXE_LOG( "download", axe::L_Error, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res) );
                        result = -1;
                    }

                    /* always cleanup */
                    curl_easy_cleanup(curl);
                }
                else
                {
                    result = -1;
                }

                return result;
            } );

            task->m_requirements.insert( task->m_requirements.end(), reqs.begin(), reqs.end() );
            res->m_outputTasks.push_back( task );
        }
    }

    return res;
}

