#ifndef PLAYER_CAR_COLOURS_RESOURCE_TYPE_H
#define PLAYER_CAR_COLOURS_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised PlayerCarColours payload (a
// BrnWorld::GlobalColourPalette = 4 x PlayerCarColourPalette, each
// { Vector4* mpPaintColours; Vector4* mpPearlColours; s32 miNumColours; } = 12B),
// deriving from CgsResource::Type. FixUp/FixDown rebase the two colour-array
// pointers of every palette entry; Serialise copies the payload to the destination
// resource and rebases pointers in place. GetTypeID/GetSerialisedResourceDescriptor
// are sibling virtuals owned by other recon passes. Base/signatures recovered from
// the DecFIGS DWARF (PlayerCarColoursResourceType.h) and the X360 pseudocode.
class PlayerCarColoursResourceType : public CgsResource::Type
{
public:
    // The type's own leading virtual. The DWARF declares a trailing
    // `const ResourceDescriptor&` arg that the X360 body never reads (Hex-Rays drops
    // it); kept in the signature for fidelity.
    virtual void* Serialise(const void* lpResource, const rw::Resource& lrDest,
                            const ResourceDescriptor& lrDescriptor) const;

    uint32_t           GetTypeID() const override;
    ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void               FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    void               FixDown(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
