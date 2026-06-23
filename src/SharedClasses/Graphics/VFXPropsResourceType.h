#ifndef VFX_PROPS_RESOURCE_TYPE_H
#define VFX_PROPS_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnParticle
{
// Resource-type handler for VFX prop collections (registry type id 0x1001B =
// 65563), deriving from CgsResource::Type. GetTypeID/DeSerialise/FixUp/
// GetImportPointer are virtual overrides; Serialise is the type's own virtual
// (a non-tool-build stub). Base/signatures recovered from the DecFIGS DWARF
// (VFXPropsResourceType.h) and the X360 pseudocode.
class VFXPropCollectionResourceType : public CgsResource::Type
{
public:
    uint32_t                        GetTypeID() const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    bool                            DeSerialise(void* lpResource) const override;
    void                            FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    void                            GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const override;
    virtual void*                   Serialise(const void* lpResource, const rw::Resource& lrDest) const;
};
}

#endif
