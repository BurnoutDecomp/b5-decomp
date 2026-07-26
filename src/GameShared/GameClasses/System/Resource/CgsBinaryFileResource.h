#ifndef CGS_BINARY_FILE_RESOURCE_H
#define CGS_BINARY_FILE_RESOURCE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
    // The serialised binary-file resource RECORD the BinaryFileResourceType handler manages
    // (DWARF CgsBinaryFileResource.h:46: struct CgsResource::BinaryFileResource, members
    // mu32DataSize @+0 / mu32DataOffset @+4, accessors GetData/GetSize/GetDataSize/
    // GetHeaderSize). GetData() is header-inline in the original: the X360 folds it into its
    // callers as `base + *(u32*)(base + 4)` (attested by BrnGameState::StreetManager::
    // SetupParRivals @0x8233F560, which resolves the "Districts" district-map blob through it
    // before CgsWorld::WorldMap2D::Construct). The remaining three accessors are other-TU
    // surface and are declared-only. ADDITIVE GROW (StreetManager wave-C keystone): new type
    // in this owning header; no change to the existing BinaryFileResourceType handler.
    struct BinaryFileResource
    {
    public:
        // DWARF :51. Start of the payload: the record base plus the stored byte offset.
        const void* GetData() const
        {
            return reinterpret_cast<const char*>(this) + mu32DataOffset;
        }

        // DWARF :54/:57/:60 -- declared-only here (bodies land with their own TU).
        uint32_t GetSize() const;
        uint32_t GetDataSize() const;
        uint32_t GetHeaderSize() const;

    private:
        uint32_t mu32DataSize;    // +0
        uint32_t mu32DataOffset;  // +4
    };

    // Resource-type base for plain binary-file resources (the common case). It
    // overrides the relocation + serialisation virtuals; concrete handlers that
    // derive from it usually only add their GetTypeID. Recovered from the DecFIGS
    // DWARF (CgsBinaryFileResource.h).
    class BinaryFileResourceType : public Type
    {
    public:
        BinaryFileResourceType();

        uint32_t           GetTypeID() const override;
        ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
        void               FixDown(void* lpResource, const rw::Resource& lrResource) const override;
        void               FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    };
}

#endif
