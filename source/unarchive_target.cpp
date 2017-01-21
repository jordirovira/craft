
#include "target.h"

#include "craft_private.h"
#include "axe.h"
#include "platform.h"

#include <string>
#include <sstream>
#include <vector>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#include "unzip.h"


UnarchiveTarget& UnarchiveTarget::archive(const std::string& url)
{
    m_archive = url;
    return *this;
}


std::shared_ptr<class BuiltTarget> UnarchiveTarget::build( ContextPlan& ctx )
{
    auto res = std::make_shared<BuiltTarget>();

    std::shared_ptr<Node> outputNode = std::make_shared<Node>();
    res->m_outputNode = outputNode;

    if ( m_archive.size() )
    {
        std::string target;

        target = ctx.get_current_path()+FileSeparator()+std::string(m_name);

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

            std::string archivePath;
            {
                std::shared_ptr<BuiltTarget> usedTarget = ctx.get_built_target( m_archive );
                assert( usedTarget );

                archivePath = usedTarget->m_outputNode->m_absolutePath;
            }

            // Generate unzip task
            auto result = std::make_shared<Task>( "unarchive", outputNode,
                                             [=]()
            {
                int result = 0;

                // Open the zip file
                unzFile zipfile = unzOpen( archivePath.c_str() );
                if ( zipfile == NULL )
                {
                    AXE_LOG( "unzip", axe::L_Error, "Failed to find archive [%s]", archivePath.c_str() );
                    return -1;
                }

                // Get info about the zip file
                unz_global_info global_info;
                if ( unzGetGlobalInfo( zipfile, &global_info ) != UNZ_OK )
                {
                    AXE_LOG( "unzip", axe::L_Error, "could not read file global info\n" );
                    unzClose( zipfile );
                    return -1;
                }

                // Buffer to hold data read from the zip file.
                const int READ_SIZE = 0xffff;
                const int MAX_FILENAME = 0xfff;
                char read_buffer[ READ_SIZE ];

                // Loop to extract all files
                uLong i;
                for ( i = 0; i < global_info.number_entry; ++i )
                {
                    // Get info about current file.
                    unz_file_info file_info;
                    char filename[ MAX_FILENAME ];
                    if ( unzGetCurrentFileInfo(
                        zipfile,
                        &file_info,
                        filename,
                        MAX_FILENAME,
                        NULL, 0, NULL, 0 ) != UNZ_OK )
                    {
                        AXE_LOG( "unzip", axe::L_Error, "could not read file info\n" );
                        unzClose( zipfile );
                        return -1;
                    }

                    std::string absoluteTargetFile = target+FileSeparator()+std::string(filename);
                    AXE_LOG( "unzip", axe::L_Info, "Extracting file [%s]", absoluteTargetFile.c_str() );

                    // Check if this entry is a directory or file.
                    const size_t filename_length = strlen( filename );
                    if ( filename[ filename_length-1 ] == '/' )
                    {
                        // Entry is a directory, so create it.
                        printf( "dir:%s\n", filename );
                        FileCreateDirectories( absoluteTargetFile );
                    }
                    else
                    {
                        // Entry is a file, so extract it.

                        // Make sure the target folder exists
                        std::string targetPath = FileGetPath( absoluteTargetFile );
                        FileCreateDirectories( targetPath );

                        printf( "file:%s\n", filename );
                        if ( unzOpenCurrentFile( zipfile ) != UNZ_OK )
                        {
                            AXE_LOG( "unzip", axe::L_Error, "could not open file" );
                            unzClose( zipfile );
                            return -1;
                        }

                        // Open a file to write out the data.
                        FILE *out = fopen( absoluteTargetFile.c_str(), "wb" );
                        if ( out == NULL )
                        {
                            AXE_LOG( "unzip", axe::L_Error, "could not open destination file\n" );
                            unzCloseCurrentFile( zipfile );
                            unzClose( zipfile );
                            return -1;
                        }

                        int error = UNZ_OK;
                        do
                        {
                            error = unzReadCurrentFile( zipfile, read_buffer, READ_SIZE );
                            if ( error < 0 )
                            {
                                AXE_LOG( "unzip", axe::L_Error, "error %d\n", error );
                                unzCloseCurrentFile( zipfile );
                                unzClose( zipfile );
                                return -1;
                            }

                            // Write data to file.
                            if ( error > 0 )
                            {
                                fwrite( read_buffer, error, 1, out ); // You should check return of fwrite...
                            }
                        } while ( error > 0 );

                        fclose( out );

                        // Set the file attributes
                        // \todo other OSs
                        mode_t mode = (file_info.external_fa >> 16 ) & 0x01FF;
                        chmod( absoluteTargetFile.c_str(), mode );
                    }

                    unzCloseCurrentFile( zipfile );

                    // Go the the next entry listed in the zip file.
                    if ( ( i+1 ) < global_info.number_entry )
                    {
                        if ( unzGoToNextFile( zipfile ) != UNZ_OK )
                        {
                            AXE_LOG( "unzip", axe::L_Error, "cound not read next file\n" );
                            unzClose( zipfile );
                            return -1;
                        }
                    }
                }

                unzClose( zipfile );

                return result;
            } );

            result->m_requirements.insert( result->m_requirements.end(), reqs.begin(), reqs.end() );
            res->m_outputTasks.push_back( result );
        }
    }

    return res;
}

