#ifndef CGS_VOICE_HIERARCHY_RESOURCE_TYPE_H
#define CGS_VOICE_HIERARCHY_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
class VoiceHierarchyResourceType : public Type
{
public:
    uint32_t GetTypeID() const override;
};
}

#endif
