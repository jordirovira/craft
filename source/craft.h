#pragma once

#include <craft_core.h>
#include <target.h>


// This should be defined in the craft.cpp file of the project being built.
void craft( Context& ctx );


// Make sure the method will be defined with the suitable C linking style, so that we can find it
// with dlopen.
extern "C"
{
    CRAFTCOREI_API void craft_entry( const char* workspacePath, const char** configurations );

    void craft_entry( const char* workspacePath, const char** configurations )
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
                    return;
                }
                else
                {
                    contextPlan->set_current_configuration( configurations[c] );

                    TargetList targets = context->get_default_targets();
                    for ( size_t t=0; t<targets.size(); ++t )
                    {
                        contextPlan->get_built_target(targets[t]->m_name);
                    }
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

                TargetList targets = context->get_default_targets();
                for ( size_t t=0; t<targets.size(); ++t )
                {
                    contextPlan->get_built_target(targets[t]->m_name);
                }
            }
        }

        contextPlan->run();
    }

}

