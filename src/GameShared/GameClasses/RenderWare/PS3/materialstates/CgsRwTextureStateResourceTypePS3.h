#ifndef CGS_RW_TEXTURE_STATE_RESOURCE_TYPE_PS3_H
#define CGS_RW_TEXTURE_STATE_RESOURCE_TYPE_PS3_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Serialised RenderWare texture-state handler (registry id 14). Derives from
// CgsResource::Type; the listed virtuals are overrides. GetTypeID returns the id;
// FixDown is an empty no-op (a texture state holds no relocatable pointers, only its
// texture import handle); GetImportPointer reports that one import (the texture handle
// at +32); DebugValidate asserts the texture handle is valid (!= -1). The remaining
// base virtuals are inherited from the non-pure CgsResource::Type base.
// Sibling of CgsMaterialStateResourceTypePS3 / CgsRwShaderParameterResourceTypePS3.
class RwTextureStateResourceType : public Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void     GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const override;
    bool     DebugValidate(const void* lpResource) const override;
};
}

#endif
