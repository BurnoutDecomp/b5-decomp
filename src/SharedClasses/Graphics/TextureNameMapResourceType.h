#ifndef TEXTURE_NAME_MAP_RESOURCE_TYPE_H
#define TEXTURE_NAME_MAP_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "types.hpp"

namespace BrnParticle
{
// ADDITIVE GROW (flagged): the resource payload structures. Reconstructed from the
// DecFIGS DWARF (TextureNameMapResourceType.h:40/44/66/67/99/100). BrnParticle::TextureNameMap
// owns an Entry[] + count; each Entry pairs the precomputed FNV-1a hash of a Lion texture
// name with its GDB texture-name string. Pre-existing TextureNameMapResourceType (the
// CgsResource::Type subclass) is left untouched below.
struct TextureNameMap
{
    struct Entry
    {
        u32   muHashedLionTextureName;   // FNV-1a hash of the (lowercased) Lion name
        char* mpGDBTextureName;          // owning GDB texture-name string

        // FNV-1a (offset basis 0x811C9DC5, prime 0x01000193) over the lowercased
        // characters of a NUL-terminated string. X360 @0x82277CD0; body in the .cpp.
        u32 HashString( const char* lpcName );
    };

    Entry* GetEntries() const { return mpEntries; }
    u32    GetEntryCount() const { return muEntryCount; }
    void   Setup( Entry* lpEntries, u32 luEntryCount )
    {
        mpEntries    = lpEntries;
        muEntryCount = luEntryCount;
    }

private:
    Entry* mpEntries;       // TextureNameMapResourceType.h:99
    u32    muEntryCount;    // TextureNameMapResourceType.h:100
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
