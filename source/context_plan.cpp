
#include "craft_private.h"

#include "axe.h"
#include "platform.h"
#include "target.h"

#include <string>
#include <sstream>
#include <vector>


void ContextPlan::update_target_folder()
{
    m_currentPath = m_buildRoot+FileSeparator()+m_buildFolder;

    if (m_buildFolderHasHost)
    {
        m_currentPath += FileSeparator()+m_host_platform->name();
    }

    if (m_buildFolderHasTarget)
    {
        m_currentPath += FileSeparator()+m_target_platform->name();
    }
}


std::shared_ptr<Platform> ContextPlan::get_this_platform()
{
    std::shared_ptr<Platform> result;

    for( auto it=m_platforms.begin();
         !result && it!=m_platforms.end();
         ++it )
    {
        if ((*it)->is_this())
        {
            result = *it;
        }
    }

    return result;
}


std::shared_ptr<Platform> ContextPlan::get_host_platform()
{
    return m_host_platform;
}


std::shared_ptr<Platform> ContextPlan::get_current_target_platform()
{
    return m_target_platform;
}


std::shared_ptr<Toolchain> ContextPlan::get_current_toolchain()
{
    return m_toolchain;
}


const std::string& ContextPlan::get_current_configuration() const
{
    return m_current_configuration;
}


bool ContextPlan::has_configuration( const std::string& name ) const
{
    for (const auto& c: m_configurations)
    {
        if (c==name)
        {
            return true;
        }
    }

    return false;
}


void ContextPlan::set_current_configuration( const std::string& name )
{
    bool found = false;

    for (const auto& c: m_configurations)
    {
        if (c==name)
        {
            found=true;
            m_current_configuration=c;

            // Get the sortcut to built targets
            m_currentBuiltTargets = nullptr;
            for ( size_t i=0; !m_currentBuiltTargets && i<m_builtTargets.size(); ++i )
            {
                if ( m_builtTargets[i]->m_conditions.m_configuration==name)
                {
                    m_currentBuiltTargets = m_builtTargets[i];
                }
            }

            if (!m_currentBuiltTargets)
            {
                m_currentBuiltTargets = std::make_shared<BuiltTargets>();
                m_currentBuiltTargets->m_conditions.m_configuration = name;
                m_builtTargets.push_back( m_currentBuiltTargets );
            }
        }
    }

    assert( found );
}


int ContextPlan::run()
{
    AXE_SCOPED_SECTION(tasks);

    AXE_LOG( "task", axe::L_Info, "%3d tasks", m_tasks.size() );

    int result = 0;

    // Things should happen here.
    for ( std::size_t i=0; i<m_tasks.size(); ++i )
    {
        AXE_LOG( "task", axe::L_Info, "[%3d of %3d] %s", i+1, m_tasks.size(), m_tasks[i]->m_type.c_str() );
        int taskResult = m_tasks[i]->m_runMethod();
        if (taskResult!=0)
        {
            AXE_LOG( "task", axe::L_Error, "failed! [%d]", taskResult );
            result = -1;
            break;
        }
    }

    return result;
}


const std::string& ContextPlan::get_current_path() const
{
    return m_currentPath;
}


std::shared_ptr<Target_Base> ContextPlan::get_target( const std::string& name )
{
    std::shared_ptr<Target_Base> result;

    for( size_t t=0; t<m_targets.size(); ++t )
    {
        if (m_targets[t]->m_name==name)
        {
            result = m_targets[t];
        }
    }

    return result;
}


std::shared_ptr<BuiltTarget> ContextPlan::get_built_target( const std::string& name )
{
    std::shared_ptr<Target_Base> target = get_target(name);

    if (!target)
    {
        AXE_LOG("fatal",axe::L_Error,"Unknown target [%s]", name.c_str());
        assert(target);
    }

    std::shared_ptr<BuiltTarget> result;

    if (target->is_configuration_sensitive())
    {
        assert(m_currentBuiltTargets);
        auto it = m_currentBuiltTargets->m_targets.find(target);
        if ( it==m_currentBuiltTargets->m_targets.end() )
        {
            AXE_LOG("plan",axe::L_Error,"Building new target [%s], %d built", name.c_str(), m_currentBuiltTargets->m_targets.size() );
            result = target->build( *this );
            result->m_sourceTarget = target;
            m_currentBuiltTargets->m_targets[target] = result;
            m_tasks.insert(m_tasks.end(),result->m_outputTasks.begin(),result->m_outputTasks.end());
        }
        else
        {
            AXE_LOG("plan",axe::L_Error,"Reused target [%s]", name.c_str());
            result = it->second;
        }
    }
    else
    {
        auto it = m_insensitiveBuiltTargets.find(target);
        if ( it==m_insensitiveBuiltTargets.end() )
        {
            result = target->build( *this );
            result->m_sourceTarget = target;
            m_insensitiveBuiltTargets[target] = result;
            m_tasks.insert(m_tasks.end(),result->m_outputTasks.begin(),result->m_outputTasks.end());
        }
        else
        {
            AXE_LOG("plan",axe::L_Error,"Reused target [%s]", name.c_str());
            result = it->second;
        }
    }

    return result;
}


const TargetList& ContextPlan::get_targets()
{
    return m_targets;
}


bool ContextPlan::IsNodePending( const Node& node )
{
    for( const auto& t: m_tasks )
    {
       for ( const auto& n: t->m_outputs )
       {
           if ( node.m_absolutePath==n->m_absolutePath )
           {
               return true;
           }
       }
    }

    return false;
}


bool ContextPlan::IsTargetOutdated( FileTime target_time, const NodeList& dependencies, std::shared_ptr<Node>* failed )
{
    if ( target_time.IsNull() )
    {
        return true;
    }

    for (const auto& n: dependencies)
    {
        if (IsNodePending(*n))
        {
            if (failed)
            {
                (*failed) = n;
            }
            return true;
        }

        FileTime dep_time = FileGetModificationTime( n->m_absolutePath );
        if (dep_time.IsNull() || dep_time>target_time)
        {
            if (failed)
            {
                (*failed) = n;
            }
            return true;
        }
    }

    return false;
}

