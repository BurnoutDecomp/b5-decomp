#include "SharedClasses/Graphics/ParticleDescriptionResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionFX.h"       // cLionFX
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionEffect.h"   // cLionEffectDefinition

#include <cstdio>   // snprintf (the BinLoad-failure diagnostic)

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

namespace CgsSound { namespace Playback { namespace Content
{
    int DoOnPostLoad(void* pContent);
    // ⭐ CORRECTED 2026-09-02: this is the universal ICF thunk `li r3,1; blr`
    // (0x82C296C8), not un-decompiled sound code -- see VFXPropsResourceType.cpp's note.
    int DoOnPostLoad(void*) { return 1; }
}}}

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
    //     return CgsSound::Playback::Content::DoOnPostLoad(this);
    // The record's second word is the saved LION blob (Serialise wrote `(u32)this + 16`
    // there); BinLoad rebases the effect graph inside it and hands back the blob itself
    // as a cLionEffectDefinition*, which is stored straight back over the word.
    //
    // ⭐ THIS WAS THE STUB, AND IT IS NOT ANY MORE (2026-09-03). It used to announce
    // "NOT RECONSTRUCTED ... no Lion effect can start from it" and leave the word as a
    // file offset. Both reasons it gave are paid: cLionFX::BinLoad @0x82914388 is
    // reconstructed, and the .lef payloads are byte-order-ported so its `mVersion ==
    // 65539` test can pass at all. The path runs the moment particles.bundle is fixed up
    // (CgsResource::Pool::FixUpEntry, stage 2 of ParticleModule::LoadFXBundle), i.e.
    // every boot.
    //
    // BinLoad returning NULL is the console's own outcome for a blob that is not a LION
    // effect, and it stores that NULL. Announced once here so a bad port shows up as a
    // named line rather than as descriptions that quietly never resolve.
    bool ParticleDescriptionResourceType::DeSerialise(void* lpResource) const
    {
        ParticleDescription* lpDescription = static_cast<ParticleDescription*>(lpResource);
        void* lpBlobIn = lpDescription->GetDefinition();
        lpDescription->SetDefinition(cLionFX::BinLoad(lpBlobIn));

        if (lpDescription->GetDefinition() == 0)
        {
            // The message names WHICH of BinLoad's two exits was taken and prints the
            // bytes that decided it -- a "rejected" line that cannot tell a null input
            // from a wrong magic is the kind of diagnostic this project keeps being
            // burned by.
            static bool sbLogged = false;
            if (!sbLogged)
            {
                sbLogged = true;
                const u32* lpWords = reinterpret_cast<const u32*>(lpResource);
                char lacMsg[512];
                std::snprintf(lacMsg, sizeof(lacMsg),
                    "[particles] cLionFX::BinLoad returned NULL for a .lef: resource=%p "
                    "word0(hash)=%08X word1(blob)=%08X word2=%08X word3=%08X blobPtr=%p "
                    "magic=%08X -- no effect can start from this description. If the magic is "
                    "byte-reversed, tools/assets/bundles/lef_transcode.py has not run over "
                    "PARTICLES.BUNDLE.\n",
                    lpResource, lpWords[0], lpWords[1], lpWords[2], lpWords[3], lpBlobIn,
                    lpBlobIn ? *reinterpret_cast<const u32*>(lpBlobIn) : 0u);
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }

        // The tail is the ICF thunk `li r3,1; blr` -- see VFXPropsResourceType.cpp's note.
        return true;
    }

    void* ParticleDescriptionResourceType::Serialise(const void* lpResource, const rw::Resource& lrDest) const
    {
        u32*       lpDst = reinterpret_cast<u32*>(lrDest.m_baseResources[0]);
        const u32* lpSrc = reinterpret_cast<const u32*>(lpResource);

        u32 luPayload = reinterpret_cast<u32>(lpDst) + 16;
        lpDst[0] = lpSrc[0];
        // Two-entry LionFX save stream descriptor: vtable + payload pointer.
        extern void* off_820A7F30;
        u32 laStream[4] = {};
        laStream[0] = reinterpret_cast<u32>(&off_820A7F30);
        laStream[1] = luPayload;
        lpDst[1] = luPayload;
        cLionFX::BinSave(reinterpret_cast<void*>(lpSrc[1]), 1, laStream);
        return lpDst;
    }
}

void* off_820A7F30 = nullptr;
