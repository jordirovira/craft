#pragma once

#include <craft_core.h>


// This should be defined in the craft.cpp file of the project being built.
void craft( Context& ctx );


// Make sure the method will be defined with the suitable C linking style, so that we can find it
// with dlopen.
extern "C" {

    CRAFTCOREI_API void craft_entry( const char* workspacePath )
    {
        std::shared_ptr<Context> context = Context::Create();
        Context& ctx = *context;
        craft( ctx );

        // Targets have been defined

        // The default behaviour is to build them all
        const TargetList& targets = ctx.get_targets();
        for ( size_t t=0; t<targets.size(); ++t )
        {
            if ( !targets[t]->Built() )
            {
                targets[t]->build( ctx );
            }
        }
    }

}

