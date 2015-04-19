
#include "craft_private.h"

#include "axe.h"
#include "platform.h"

#include <string>
#include <sstream>
#include <vector>


AXE_IMPLEMENT()


void craft_core_initialize( axe::Kernel* log_kernel )
{
    axe::s_kernel = log_kernel;
}


