#ifndef CGS_RESOURCE_DEBUG_COMPONENT_H
#define CGS_RESOURCE_DEBUG_COMPONENT_H

#include "DebugSystem/Core/CgsDebugComponent.h"

namespace CgsResource
{
class DebugComponent : public CgsDev::DebugComponent
{
public:
    virtual const char* GetPath() const { return "Core"; }
};
}

#endif
