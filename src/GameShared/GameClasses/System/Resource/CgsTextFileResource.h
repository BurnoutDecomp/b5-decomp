#ifndef CGS_TEXT_FILE_RESOURCE_H
#define CGS_TEXT_FILE_RESOURCE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
class TextFileResourceType : public Type
{
public:
    virtual uint32_t GetTypeID() const;
    virtual EDebugResourceCategory GetDebugResourceCategory() const;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const ;
};
}

#endif
