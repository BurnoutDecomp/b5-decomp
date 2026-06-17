#ifndef CGS_RW_TEXTURE_STATE_RESOURCE_TYPE_H
#define CGS_RW_TEXTURE_STATE_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
    // The texture-state (sampler) resource-type handler -- resource type id 0xE ("TextureState"
    // / RwTextureState). Reconstructed from the DecFIGS PS3 header (GameShared/GameClasses/
    // RenderWare/CgsRwTextureStateResourceType.h) and the X360 ARTIST bodies:
    //   GetTypeID                0x828A8118  (=14)
    //   GetSerialisedResourceDesc 0x828A9A60 (fixed {36,16} descriptor)
    //   FixDown                  0x828A8120  (a stub on X360)
    //   GetImportPointer         0x828A8128  (import 0 = the raster at +0x20)
    //   DebugValidate            0x828A8140  (asserts the raster slot != -1)
    //
    // The runtime resource is a renderengine::TextureState (a sampler block + the raster it
    // samples). It imports exactly one raster (RwRaster, id 0); the pool resolves that import
    // into the state via GetImportPointer. Overrides the nine virtuals the PS3 header overrides
    // (this handler DOES override DebugValidate, unlike the raster handler).
    class RwTextureStateResourceType : public Type
    {
    public:
        RwTextureStateResourceType();

        virtual uint32_t               GetTypeID() const;
        virtual ResourceDescriptor     GetSerialisedResourceDescriptor(const void* lpResource) const;
        virtual bool                   DeSerialise(void* lpResource) const;
        virtual void                   FixDown(void* lpResource, const rw::Resource& lrResource) const;
        virtual void                   FixUp(void* lpResource, const rw::Resource& lrResource) const;
        virtual uint32_t               GetImportCount(const void* lpResource) const;
        virtual void                   GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const;
        virtual bool                   DebugValidate(const void* lpResource) const;
        virtual EDebugResourceCategory GetDebugResourceCategory() const;
    };

    // Construct the singleton handler and register it (type id 0xE). Call once at bring-up.
    void RegisterRwTextureStateResourceType();
}

#endif
