#ifndef CGS_ATTRIB_SYS_SCHEMA_RESOURCE_TYPE_H
#define CGS_ATTRIB_SYS_SCHEMA_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
class AttribSysSchemaResourceType : public Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
