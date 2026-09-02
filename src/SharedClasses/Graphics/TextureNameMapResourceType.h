#ifndef TEXTURE_NAME_MAP_RESOURCE_TYPE_H
#define TEXTURE_NAME_MAP_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "types.hpp"

namespace BrnParticle
{
// ADDITIVE GROW (flagged): the resource payload structures. Reconstructed from the
// DecFIGS DWARF (TextureNameMapResourceType.h:40/44/66/67/99/100). BrnParticle::TextureNameMap
// owns an Entry[] + count; each Entry pairs the precomputed FNV-1a hash of a Lion texture
// name with its GDB texture-name string.
//
// ⭐⭐ CORRECTED 2026-09-02 (tyre-mark wave) -- THIS IS A SERIALISED BLOB AND ITS SLOTS
// STAY 32-BIT. The first version declared the host shape
//     Entry* mpEntries;  u32 muEntryCount;          // and Entry { u32; char*; }
// which on x64 is an 8-byte pointer, muEntryCount at +0x08, and an Entry stride of 16.
// The object this type describes is never constructed by us: it is READ IN from
// particles.bundle in the console's own layout -- mpEntries at +0x00, muEntryCount at
// +0x04, Entry stride 8 -- and the sibling .cpp says so in as many words ("Entry stride
// is sizeof(Entry) = 8"), because TextureNameMapResourceType::{FixUp,FixDown,Serialise,
// GetSerialisedResourceDescriptor} all walk it through a TU-local struct with exactly
// these u32 slots. So the resource type read the blob correctly and every CONSUMER of
// the same bytes read it wrong.
//
// MEASURED CONSEQUENCE (ParticleModule::LoadFXBundle, run 6):
//     [skid-ready] FX texture stage: map=0000000005B70270 entries=0
//                  wanted hash=0xF2CC8984 ("fxskid") replies>=0
// -- GetEntryCount() returned the 4 bytes PAST the two-word header, so the FX texture
// loop never ran, "fxskid" was never matched, TrailSystem::mbIsReady stayed false, and
// TrailSystem::Render would have early-outed even with a segment queued. Note the shape
// of the failure: not a crash, a plausible zero. LionParticleRender::FindTexture read
// the same two fields and would have walked a table at (count << 32 | entries).
//
// The 32-bit slots resolve under the project's low-4 GB PointerFromU32 convention, the
// same one CgsSerialisedPtr.h / CgsLowMemoryPC.h commit to for every other serialised
// pointer slot. Fields are public and named exactly as the .cpp's local struct so that
// struct is now a typedef of this one -- one layout, one place, both sides.
struct TextureNameMap
{
    struct Entry
    {
        u32 muHashedLionTextureName;   // +0x00  FNV-1a hash of the (lowercased) Lion name
        u32 mpGDBTextureName;          // +0x04  serialised char* slot (low-4 GB)

        const char* GDBTextureName() const
        {
            return reinterpret_cast<const char*>(static_cast<uintptr_t>(mpGDBTextureName));
        }

        // FNV-1a (offset basis 0x811C9DC5, prime 0x01000193) over the lowercased
        // characters of a NUL-terminated string. X360 @0x82277CD0; body in the .cpp.
        // ⚠ STATIC (corrected 2026-09-02): the X360 prologue is `mr r27, r3` and then reads
        // the STRING out of r3 -- there is no implicit `this`. Existing call sites that spell
        // it through an instance still compile.
        static u32 HashString( const char* lpcName );
    };

    Entry* GetEntries() const
    {
        return reinterpret_cast<Entry*>(static_cast<uintptr_t>(mpEntries));
    }
    u32 GetEntryCount() const { return muEntryCount; }
    void Setup( Entry* lpEntries, u32 luEntryCount )
    {
        mpEntries    = static_cast<u32>(reinterpret_cast<uintptr_t>(lpEntries));
        muEntryCount = luEntryCount;
    }

    u32 mpEntries;       // +0x00  TextureNameMapResourceType.h:99
    u32 muEntryCount;    // +0x04  TextureNameMapResourceType.h:100
};

class TextureNameMapResourceType : public CgsResource::Type
{
public:
    uint32_t                        GetTypeID() const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void                            FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void                            FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    virtual void*                   Serialise(const void* lpResource, const rw::Resource& lrDest) const;
};
}

#endif
