#ifndef PLAYER_CAR_COLOURS_RESOURCE_TYPE_H
#define PLAYER_CAR_COLOURS_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for a serialised PlayerCarColours payload (registry id
// 0x1001E / 65566): a BrnWorld::GlobalColourPalette == 4 x PlayerCarColourPalette,
// each { u32 muPaintColours; u32 muPearlColours; s32 miNumColours; } == 12 bytes.
// Derives from CgsResource::Type. FixUp/FixDown rebase the two 32-bit colour-array
// columns of every palette entry; Serialise copies the payload to the destination
// resource and rebases the columns in place. Base/signatures recovered from the
// DecFIGS DWARF (PlayerCarColoursResourceType.h) and the X360 pseudocode; the
// 12-byte stride and the 32-bit columns are proven twice in
// SharedClasses/Graphics/BrnGlobalColourPalette.h.
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
