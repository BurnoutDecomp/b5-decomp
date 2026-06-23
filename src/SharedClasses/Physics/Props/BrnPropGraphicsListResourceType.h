#ifndef BRN_PROP_GRAPHICS_LIST_RESOURCE_TYPE_H
#define BRN_PROP_GRAPHICS_LIST_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnPhysics
{
namespace Props
{
// Resource-type handler for a serialised BrnPhysics::Props::PropGraphicsList, deriving from
// CgsResource::Type. The serialised payload's first word is PropGraphicsList::muSizeInBytes (the
// precomputed total byte size of the resource), which GetSerialisedResourceDescriptor reports as
// block 0's size. Base/signatures recovered from the DecFIGS DWARF
// (BrnPropGraphicsListResourceType.h) and the X360 binary. GetTypeID/FixDown/FixUp/GetImportCount/
// GetImportPointer are sibling virtuals owned by other recon passes (declared here for the correct
// vtable surface).
class PropGraphicsListResourceType : public CgsResource::Type
{
public:
    uint32_t                        GetTypeID() const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void                            FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void                            FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    uint32_t                        GetImportCount(const void* lpResource) const override;
    void                            GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const override;
};
}
}

#endif
