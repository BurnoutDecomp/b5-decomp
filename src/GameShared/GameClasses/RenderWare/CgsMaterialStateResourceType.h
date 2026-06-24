#ifndef CGS_MATERIAL_STATE_RESOURCE_TYPE_H
#define CGS_MATERIAL_STATE_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
    // The material-state (blend-state) resource-type handler -- resource type id 0xF
    // ("MaterialState" / BlendState). Reconstructed from the DecFIGS PS3 header (GameShared/
    // GameClasses/RenderWare/CgsMaterialStateResourceType.h) and the X360 ARTIST bodies:
    //   GetTypeID                0x828A8108  (=15)
    //   GetSerialisedResourceDesc 0x828A98C0 (fixed {252,16} descriptor)
    //   FixDown                  0x828A8A80
    //   FixUp                    0x828A8AB8
    //
    // The runtime resource is a renderengine::MaterialState. FixUp/FixDown rebase three
    // main-memory self-pointers (a platform-agnostic pointer fixup -- a faithful port, NOT a
    // PC divergence). Overrides the seven virtuals the PS3 header overrides (no DebugValidate,
    // no GetDebugResourceCategory -- those inherit CgsResource::Type).
    class MaterialStateResourceType : public Type
    {
    public:
        MaterialStateResourceType();

        virtual uint32_t           GetTypeID() const;
        virtual ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const;
        virtual bool               DeSerialise(void* lpResource) const;
        virtual void               FixDown(void* lpResource, const rw::Resource& lrResource) const;
        virtual void               FixUp(void* lpResource, const rw::Resource& lrResource) const;
        virtual uint32_t           GetImportCount(const void* lpResource) const;
        virtual void               GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const;
    };
}

#endif
