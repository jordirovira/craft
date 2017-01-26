// This file is compiled in the craftfile dynamic library, but not in the craft program or craft-core library.

#include "craft.h"
#include "craft_private.h"

#include "axe.h"
#include "platform.h"

#include <string>
#include <sstream>
#include <vector>


// axe for the craftfile library
AXE_IMPLEMENT()

// Make sure the method will be defined with the suitable C linking style, so that we can find it
// with dlopen.
extern "C"
{
    CRAFTCOREI_API void craft_entry( const char* workspacePath, const char** configurations, const char** targets, axe::Kernel* log_kernel );
}


void plan_targets(std::shared_ptr<Context> context, std::shared_ptr<ContextPlan> contextPlan, const char** targets)
{
    TargetList plannedTargets;
    if (!targets || targets[0]==nullptr)
    {
        plannedTargets = context->get_default_targets();
    }
    else
    {
        int t=0;
        while (targets[t])
        {
            auto target = context->get_target(targets[t]);
            if (!target)
            {
                AXE_LOG("craft",axe::Level::Error,"Target [%s] not found.", targets[t]);
            }
            else
            {
                plannedTargets.push_back(target);
            }
            ++t;
        }
    }

    for ( size_t t=0; t<plannedTargets.size(); ++t )
    {
        contextPlan->get_built_target(plannedTargets[t]->m_name);
    }
}


void craft_entry( const char* workspacePath, const char** configurations, const char** targets, axe::Kernel* log_kernel )
{
    // Create a context for the build process
    std::shared_ptr<Context> context = std::make_shared<Context>();

    // Run the user craftfile to get the target definitions
    // \todo: catch exceptions
    craft( *context );

    std::shared_ptr<ContextPlan> contextPlan = std::make_shared<ContextPlan>(*context);

    // If configurations have been defined in the command line, find them
    if (configurations && configurations[0])
    {
        int c = 0;
        while ( configurations[c] )
        {
            if (!contextPlan->has_configuration( configurations[c] ))
            {
                // Error
                AXE_LOG("craft",axe::Level::Error,"Configuration not found.");
                return;
            }
            else
            {
                contextPlan->set_current_configuration( configurations[c] );
                plan_targets(context,contextPlan,targets);
            }
            ++c;
        }
    }

    // Use the default configurations (maybe they have been defined in the craftfile)
    else
    {
        for (std::size_t i=0; i<context->get_default_configurations().size(); ++i)
        {
            contextPlan->set_current_configuration( context->get_default_configurations()[i] );
            plan_targets(context,contextPlan,targets);
        }
    }

    contextPlan->run();
}


