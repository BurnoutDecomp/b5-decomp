#ifndef CGS_RW_SHADER_PARAMETER_RESOURCE_TYPE_PS3_H
#define CGS_RW_SHADER_PARAMETER_RESOURCE_TYPE_PS3_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised shader-parameter resource. Derives from
// CgsResource::Type; GetTypeID/FixDown/FixUp are virtual overrides. From DecFIGS DWARF.
class RwShaderParameterResourceType : public Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
