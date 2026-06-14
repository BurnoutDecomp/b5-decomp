#ifndef CGS_TEXT_FILE_RESOURCE_H
#define CGS_TEXT_FILE_RESOURCE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
class TextFileResourceType : public Type
{
public:
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
};
}

#endif
