
#include "target.h"

#include "craft_private.h"
#include "axe.h"
#include "platform.h"

#include <string>
#include <sstream>
#include <vector>

#include <curl/curl.h>


CustomTarget& CustomTarget::custom_build(std::function<std::shared_ptr<class BuiltTarget>(ContextPlan&,Target_Base&)> buildMethod)
{
    m_buildMethod = buildMethod;
    return *this;
}


std::shared_ptr<class BuiltTarget> CustomTarget::build( ContextPlan& ctx )
{
    auto res = std::make_shared<BuiltTarget>();

    if (m_buildMethod)
    {
        res = m_buildMethod(ctx,*this);
    }

    return res;
}

