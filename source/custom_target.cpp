
#include "target.h"

#include "craft_private.h"
#include "axe.h"
#include "platform.h"

#include <string>
#include <sstream>
#include <vector>


CustomTarget& CustomTarget::custom_build(std::function<std::shared_ptr<class BuiltTarget>(ContextPlan&,Target_Base&)> buildMethod)
{
    m_buildMethod = buildMethod;
    return *this;
}


std::shared_ptr<class BuiltTarget> CustomTarget::build( ContextPlan& ctx )
{
    std::shared_ptr<class BuiltTarget> res;

    if (m_buildMethod)
    {
        // The build methos should take care of everything including dependencies
        res = m_buildMethod(ctx,*this);
    }
    else
    {
        // No build method: do the minimum work

        res = std::make_shared<BuiltTarget>();

        // Generate dependencies
        std::vector<std::shared_ptr<Task>> reqs;
        for( const auto& u: m_uses )
        {
            std::shared_ptr<BuiltTarget> usedTarget = ctx.get_built_target( u );
            assert( usedTarget );

            reqs.insert( reqs.end(), usedTarget->m_outputTasks.begin(), usedTarget->m_outputTasks.end() );
        }

        auto task = std::make_shared<Task>( "custom", [=](){ return 0; });
        task->m_requirements.insert( task->m_requirements.end(), reqs.begin(), reqs.end() );
        res->m_outputTasks.push_back( task );
    }

    return res;
}

