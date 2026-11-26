#include "SharedClasses/Graphics/ParticleDescriptionResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionFX.h"       // cLionFX
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionEffect.h"   // cLionEffectDefinition
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialiser.h" // iLionBinWriter
#include <cstring>   // memcpy (SerialMemoryWriter::Write)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnParticle::ParticleDescriptionCollectionResourceType::FixUp            @ 0x8267DF40
//   BrnParticle::ParticleDescriptionCollectionResourceType::GetImportPointer @ 0x826757E8
//   BrnParticle::ParticleDescriptionCollectionResourceType::GetTypeID        @ 0x826757C8
//   BrnParticle::ParticleDescriptionCollectionResourceType::Serialise        @ 0x826782B8
//   BrnParticle::ParticleDescriptionResourceType::DeSerialise                @ 0x82675868
//   BrnParticle::ParticleDescriptionResourceType::GetTypeID                  @ 0x82675858
//   BrnParticle::ParticleDescriptionResourceType::Serialise                  @ 0x8267C220
//
// The collection is a serialised table of per-description pointers (count at +4,
// table at +0); single descriptions wrap a LionFX binary payload. Serialise writes
// to the destination resource's buffer (the leading word of the rw::Resource).
// Serialised tables are accessed by offset.

// ⭐ 2026-09-03: cLionFX::BinLoad / ::BinSave used to be declared and defined RIGHT HERE
// as free functions in a local `namespace cLionFX`, with __debugbreak bodies. That is an
// ODR fork as well as a stub: the real cLionFX is a struct with static members (the asm
// passes no `this`), so those two definitions could never have satisfied a call from any
// other TU. Both now come from the class's own home.

namespace BrnParticle
{
    static const uint32_t KU_PARTICLE_DESCRIPTION_COLLECTION_RESOURCE_TYPE_ID = 65544;
    static const uint32_t KU_PARTICLE_DESCRIPTION_RESOURCE_TYPE_ID = 65565;

    uint32_t ParticleDescriptionCollectionResourceType::GetTypeID() const
    {
        return KU_PARTICLE_DESCRIPTION_COLLECTION_RESOURCE_TYPE_ID;
    }

    // GetSerialisedResourceDescriptor @ 0x8267C1C8. The collection serialises to one
    // main block: a 16-byte header followed by a count-pointer table of `count`
    // dwords (count == lpSrc[1]), padded up to a 16-byte boundary. So slot0 =
    // { 16 + ((4*count + 15) & ~15), align 16 }; the remaining four slots carry no
    // sub-allocation ({size 0, align 1}). The X360 packs slot0 as a single qword
    // store (LODWORD=16 align, HIDWORD=size) after the size arithmetic; reproduced
    // field-for-field here.
    CgsResource::ResourceDescriptor
    ParticleDescriptionCollectionResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const u32* lpSrc  = reinterpret_cast<const u32*>(lpResource);
        u32        luCount = lpSrc[1];
        u32        luTableBytes = ((4u * luCount + 15u) & 0xFFFFFFF0u) + 16u;

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luTableBytes;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16u;
        for (uint32_t luIndex = 1; luIndex < 5; ++luIndex)
        {
            lDescriptor.m_baseResourceDescriptors[luIndex].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[luIndex].m_alignment = 1u;
        }
        return lDescriptor;
    }

    // The table pointer is stored file-relative; FixUp rebases it to the loaded
    // address (the resource's own base); the rw::Resource argument is unused.
    void ParticleDescriptionCollectionResourceType::FixUp(void* lpResource, const rw::Resource&) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);
        *reinterpret_cast<u32*>(lRes) += static_cast<u32>(lRes);
    }

    void ParticleDescriptionCollectionResourceType::GetImportPointer(const void* lpResource, uint32_t luIndex,
                                                                     uint32_t* lpuOffset, const void** lppValue) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);
        u32 luTable = *reinterpret_cast<const u32*>(lRes);
        *lppValue = reinterpret_cast<const void*>(*reinterpret_cast<const u32*>(luTable + 4 * luIndex));
        *lpuOffset = 4 * (luIndex + 2);
    }

    void* ParticleDescriptionCollectionResourceType::Serialise(const void* lpResource, const rw::Resource& lrDest) const
    {
        u32*       lpDst = reinterpret_cast<u32*>(lrDest.m_baseResources[0]);
        const u32* lpSrc = reinterpret_cast<const u32*>(lpResource);

        u32 luCount = lpSrc[1];
        lpDst[1] = luCount;
        lpDst[0] = reinterpret_cast<u32>(lpDst + 2);
        if (luCount)
        {
            u32*       lpEntries    = reinterpret_cast<u32*>(lpDst[0]);
            const u32* lpSrcEntries = reinterpret_cast<const u32*>(lpSrc[0]);
            for (u32 lu = 0; lu < luCount; ++lu)
                lpEntries[lu] = lpSrcEntries[lu];
        }
        return lpDst;
    }

    uint32_t ParticleDescriptionResourceType::GetTypeID() const
    {
        return KU_PARTICLE_DESCRIPTION_RESOURCE_TYPE_ID;
    }

    // FixUp @0x8267DF60 (ICF-folded; see the header's note for how the vtable pins it).
    // `*(result + 4) += result` -- the blob slot is stored as the byte offset 16 and is
    // rebased to `this + 16`, which is where ParticleDescriptionResourceType::Serialise
    // @0x8267C220 put the cLionEffectDefinition. It runs immediately before DeSerialise
    // (Pool::FixUpEntry @0x828EB860 calls FixUp then DeSerialise), which is the only
    // reason DeSerialise can hand BinLoad a real address.
    void ParticleDescriptionResourceType::FixUp(void* lpResource, const rw::Resource&) const
    {
        ParticleDescription* lpDescription = static_cast<ParticleDescription*>(lpResource);
        lpDescription->muDefinition +=
            static_cast<u32>(reinterpret_cast<uintptr_t>(lpResource));
    }

    // DeSerialise @0x82675868, in full:
    //     *(res + 4) = cLionFX::BinLoad(*(res + 4));
    //     return Type::DeSerialise(lpResource);
    // The record's second word is the saved LION blob (Serialise wrote `(u32)this + 16`
    // there); BinLoad rebases the effect graph inside it and hands back the blob itself
    // as a cLionEffectDefinition*, which is stored straight back over the word.
    //
    // BinLoad returning NULL is the native outcome for an invalid LION effect.
    // Store it unchanged and take the same successful post-load tail.
    bool ParticleDescriptionResourceType::DeSerialise(void* lpResource) const
    {
        ParticleDescription* lpDescription = static_cast<ParticleDescription*>(lpResource);
        void* lpBlobIn = lpDescription->GetDefinition();
        lpDescription->SetDefinition(cLionFX::BinLoad(lpBlobIn));

        // The inherited DeSerialise slot in BrnVehicle::GraphicsSpecResourceType
        // at 0x820A1648 identifies the ICF body called at 0x8267589C.
        return CgsResource::Type::DeSerialise(lpResource);
    }

    void* ParticleDescriptionResourceType::Serialise(const void* lpResource, const rw::Resource& lrDest) const
    {
        u32*       lpDst = reinterpret_cast<u32*>(lrDest.m_baseResources[0]);
        const u32* lpSrc = reinterpret_cast<const u32*>(lpResource);

        // The LION writer BinSave appends each saved block through: DWARF
        // ParticleDescriptionResourceType.cpp:347-362 (_compile/BrnSharedClassesUnity.cpp), a
        // local struct deriving iLionBinWriter, with mpDestinationData (:362), the constructor
        // (:350), Write (:352) and GetDataPointer (:358). PS3 symtab names its vtable
        // _ZTVZNK11BrnParticle31ParticleDescriptionResourceType9Serialise...E18SerialMemoryWriter;
        // ARTIST's is 0x820A7F30 (IDA off_820A7F30), whose one slot is Write at 0x82675808:
        // memcpy, then advance the cursor by the size rounded up to 16 (clrrwi 4).
        struct SerialMemoryWriter : public iLionBinWriter
        {
        public:
            SerialMemoryWriter(u8* pDest) : mpDestinationData(pDest) {}

            virtual void Write(void* apData, u32 aSize)
            {
                memcpy(mpDestinationData, apData, aSize);
                mpDestinationData += (aSize + 15) & ~15u;
            }

            u8* GetDataPointer() { return mpDestinationData; }

        private:
            u8* mpDestinationData;
        };

        u8* lpDestination = reinterpret_cast<u8*>(lpDst) + 16;
        lpDst[0] = lpSrc[0];
        SerialMemoryWriter lWriter(lpDestination);
        lpDst[1] = reinterpret_cast<u32>(lpDestination);
        cLionFX::BinSave(reinterpret_cast<void*>(lpSrc[1]), 1, &lWriter);
        // ARTIST 0x8267C26C re-reads the destination from the resource after the call.
        return lrDest.GetMemoryResource();
    }
}
