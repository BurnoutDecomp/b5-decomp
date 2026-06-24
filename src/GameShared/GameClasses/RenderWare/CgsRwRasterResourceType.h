#ifndef CGS_RW_RASTER_RESOURCE_TYPE_H
#define CGS_RW_RASTER_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
    // The texture (raster) resource-type handler -- resource type id 0 ("Texture" / RwRaster).
    // Reconstructed from the DecFIGS PS3 header (GameShared/GameClasses/RenderWare/
    // CgsRwRasterResourceType.h) and the X360 ARTIST bodies:
    //   GetSerialisedResourceDescriptor  0x828A9858  (delegates to renderengine::Texture)
    //   FixDown                          0x828A88E0
    //   FixUp                            0x828A8930
    //
    // The runtime resource is a renderengine::Texture (the X360 GetSerialisedResourceDescriptor
    // and the PS3 FixUp both operate on one). On X360/PS3 the raster IS GPU memory, so FixUp/
    // FixDown rebase packed pixel offsets; on PC the D3D9 raster has no such offsets, so FixUp
    // realises the texture (creates the D3D texture) and FixDown releases it -- the platform
    // divergence is contained to those two bodies (see the .cpp). Overrides exactly the eight
    // virtuals the PS3 header overrides; the rest (PostFixUp/ReBase/CanDefrag/DebugValidate)
    // inherit CgsResource::Type.
    class RwRasterResourceType : public Type
    {
    public:
        RwRasterResourceType();

        virtual uint32_t               GetTypeID() const;
        virtual ResourceDescriptor     GetSerialisedResourceDescriptor(const void* lpResource) const;
        virtual bool                   DeSerialise(void* lpResource) const;
        virtual void                   FixDown(void* lpResource, const rw::Resource& lrResource) const;
        virtual void                   FixUp(void* lpResource, const rw::Resource& lrResource) const;
        virtual uint32_t               GetImportCount(const void* lpResource) const;
        virtual void                   GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const;
        virtual EDebugResourceCategory GetDebugResourceCategory() const;
    };
}

#endif
