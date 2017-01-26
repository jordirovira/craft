
#include "target.h"

#include "craft_private.h"
#include "axe.h"
#include "platform.h"

#include <string>
#include <sstream>
#include <vector>

ExecTarget& ExecTarget::program(const std::string& path)
{
    m_program = path;
    return *this;
}

ExecTarget& ExecTarget::working_folder(const std::string& path)
{
    m_workingFolder = path;
    return *this;
}

ExecTarget& ExecTarget::args(const std::string& args)
{
    m_arguments.push_back(args);
    return *this;
}

ExecTarget& ExecTarget::max_time(int milliseconds)
{
    m_maxTimeMilliseconds = milliseconds;
    return *this;
}

ExecTarget& ExecTarget::log_output(bool log)
{
    m_logOutput = log;
    return *this;
}

ExecTarget& ExecTarget::log_error(bool log)
{
    m_logError = log;
    return *this;
}

ExecTarget& ExecTarget::log_name(const char* logName)
{
    m_logName = logName;
    return *this;
}

ExecTarget& ExecTarget::ignore_fail(bool f)
{
    m_ignoreFail = f;
    return *this;
}

std::shared_ptr<class BuiltTarget> ExecTarget::build( ContextPlan& ctx )
{
    auto res = std::make_shared<BuiltTarget>();

    std::shared_ptr<Node> outputNode = std::make_shared<Node>();
    res->m_outputNode = outputNode;

    // Generate dependencies
    std::vector<std::shared_ptr<Task>> reqs;
    for( const auto& u: m_uses )
    {
        std::shared_ptr<BuiltTarget> usedTarget = ctx.get_built_target( u );
        assert( usedTarget );

        reqs.insert( reqs.end(), usedTarget->m_outputTasks.begin(), usedTarget->m_outputTasks.end() );
    }

    // Generate the task
    auto task = std::make_shared<Task>( "exec", outputNode,
                                        [=]()
    {
        int result = 0;

        AXE_SCOPED_SECTION_DETAILED(craft, m_logName.c_str());

        try
        {
            int wasKilled = 0;
            std::string out, err;
            result = Run( m_workingFolder, m_program, m_arguments,
                          [&out](const char* text){ out += text; },
                          [&err](const char* text){ err += text; },
                          m_maxTimeMilliseconds,
                          &wasKilled);

            if (m_logOutput)
            {
                AXE_SCOPED_SECTION(stdout);
                AXE_LOG_LINES( "stdout", axe::Level::Verbose, out );
            }

            if (m_logError)
            {
                AXE_SCOPED_SECTION(stderr);
                AXE_LOG_LINES( "stderr", axe::Level::Verbose, err );
            }

            if (m_maxTimeMilliseconds>0)
            {
                AXE_INT_VALUE("exec",axe::Level::Info, "killed", wasKilled);

                if (wasKilled)
                {
                    AXE_LOG( "exec", axe::Level::Error, "Killed because time exceeded: %d", m_maxTimeMilliseconds );
                }
            }
        }
        catch(...)
        {
            result = -1;
        }

        AXE_INT_VALUE("exec",axe::Level::Info, "result", result);

        if (result!=0)
        {
            AXE_LOG( "exec", axe::Level::Error, "Execution failed: %d", result );
        }

        if (m_ignoreFail)
        {
            result = 0;
        }

        return result;
    } );

    task->m_requirements.insert( task->m_requirements.end(), reqs.begin(), reqs.end() );
    res->m_outputTasks.push_back( task );

    return res;
}

