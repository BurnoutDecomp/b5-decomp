// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleEffect.cpp
//
// Out-of-line bodies for the cLionParticleEffect serialisation / build path, reconstructed
// store-for-store from the X360 ARTIST asm. Each function walks the effect's descriptor
// chain (mpDescriptors -> cParticleDescriptor::mpNext) and forwards to the matching
// per-descriptor operation. Delocate/Relocate additionally convert the effect's own
// descriptor-list head pointer to/from a self-relative offset (serialisation), and Delocate
// optionally endian-twiddles the two serialised head words.
// ============================================================================

#include "LionParticleEffect.h"

#include "ParticleBehaviour.h"
#include "ParticleDescriptor.h"
#include "ParticleMaterial.h"
#include "LionSerialiser.h"

#include <cstdint>

namespace
{
    // The Lion scalar 2-argument max helper (fmax2 in the DecFIGS locals). The X360
    // implements it as `fsel(a - b, a, b)` == `(a - b) >= 0 ? a : b`, i.e. max(a, b)
    // with ties resolving to the first operand. Reproduced exactly so the fsel semantics
    // (including the sign-of-difference tie/NaN behaviour) match. File-local until the
    // real Lion math helper is homed.
    inline FP32 fmax2(FP32 afA, FP32 afB)
    {
        return (afA - afB) >= 0.0f ? afA : afB;
    }
}

// cLionParticleEffect::Build @ 0x8290EF28.
// Finalise every descriptor in the chain: set/clear the descriptor's "3D geometry" flag
// (bit 0x10000) from its render mode (modes 5/6/7 set it, all others clear it), build the
// descriptor's material, then build each behaviour in the descriptor's behaviour chain.
void cLionParticleEffect::Build()
{
    if (this == nullptr)   // ARTIST guards a null effect pointer before touching the chain
        return;

    for (cParticleDescriptor* lpDes = mpDescriptors; lpDes != nullptr;
         lpDes = lpDes->GetNextDescriptor())
    {
        const u32 luRenderMode = lpDes->GetRenderMode();
        if (luRenderMode == 5 || luRenderMode == 6 || luRenderMode == 7)
            lpDes->mFlags |= 0x10000u;
        else
            lpDes->mFlags &= 0xFFFEFFFFu;

        lpDes->mpMaterial->Build();

        for (cParticleBehaviour* lpBeh = lpDes->GetBehaviours(); lpBeh != nullptr;
             lpBeh = lpBeh->mpNext)
        {
            lpBeh->Build();
        }
    }
}

// cLionParticleEffect::Delocate @ 0x8290EDB8.
// Prepare the effect for saving: delocate each descriptor (reading the next link BEFORE the
// call, since a descriptor's own Delocate rewrites its pointers), convert the descriptor-list
// head pointer to a self-relative offset, then -- if requested -- endian-twiddle the two
// serialised head words in place.
void cLionParticleEffect::Delocate(U32 aEndianTwiddleFlag)
{
    if (this == nullptr)
        return;

    cParticleDescriptor* lpDes = mpDescriptors;
    while (lpDes != nullptr)
    {
        cParticleDescriptor* lpDesNext = lpDes->GetNextDescriptor();
        lpDes->Delocate(aEndianTwiddleFlag);
        lpDes = lpDesNext;
    }

    // Pointer -> self-relative offset for the descriptor-list head (serialised form).
    if (mpDescriptors != nullptr)
    {
        mpDescriptors = reinterpret_cast<cParticleDescriptor*>(
            reinterpret_cast<uintptr_t>(mpDescriptors) - reinterpret_cast<uintptr_t>(this));
    }

    if (aEndianTwiddleFlag != 0)
    {
        // Byte-reverse the two serialised head words in place (mHash, then the descriptor
        // offset just written into the mpDescriptors slot). The X360 assembles each word
        // from ascending memory bytes packed most-significant-first and stores it back
        // native-endian -- a byte reversal on a little-endian host.
        {
            U8* lpBY = reinterpret_cast<U8*>(&mHash);
            mHash = (static_cast<U32>(lpBY[0]) << 24) | (static_cast<U32>(lpBY[1]) << 16)
                  | (static_cast<U32>(lpBY[2]) << 8)  |  static_cast<U32>(lpBY[3]);
        }
        {
            U8* lpBY = reinterpret_cast<U8*>(&mpDescriptors);
            const U32 luOffset = (static_cast<U32>(lpBY[0]) << 24) | (static_cast<U32>(lpBY[1]) << 16)
                               | (static_cast<U32>(lpBY[2]) << 8)  |  static_cast<U32>(lpBY[3]);
            mpDescriptors = reinterpret_cast<cParticleDescriptor*>(static_cast<uintptr_t>(luOffset));
        }
    }
}

// cLionParticleEffect::GetDurationMax @ 0x8290ACF0.
// The longest descriptor duration in the chain, starting from 0. A descriptor that reports a
// negative duration is taken as the running value and terminates the walk (the next iteration
// sees a negative running max and breaks); otherwise the running max is fmax2'd with it.
FP32 cLionParticleEffect::GetDurationMax() const
{
    FP32 lDurationMax = 0.0f;

    for (cParticleDescriptor* lpDes = mpDescriptors; lpDes != nullptr;
         lpDes = lpDes->GetNextDescriptor())
    {
        if (lDurationMax < 0.0f)
            break;

        const FP32 lDurTmp = lpDes->GetDurationMax();
        if (lDurTmp >= 0.0f)
            lDurationMax = fmax2(lDurationMax, lDurTmp);
        else
            lDurationMax = lDurTmp;
    }

    return lDurationMax;
}

// cLionParticleEffect::GetSerialiseSize @ 0x8290EE78.
// Account for this effect in the serialiser: add the 16-byte effect header to the data-size
// tally, then let every descriptor add its own serialised size.
void cLionParticleEffect::GetSerialiseSize(cLionSerialiser& aSer) const
{
    if (this == nullptr)
        return;

    aSer.mDataSize += 16;

    for (cParticleDescriptor* lpDes = mpDescriptors; lpDes != nullptr;
         lpDes = lpDes->GetNextDescriptor())
    {
        lpDes->GetSerialiseSize(aSer);
    }
}

// cLionParticleEffect::Relocate @ 0x82912C50.
// Inverse of Delocate on load: convert the descriptor-list head offset back to a pointer,
// then relocate each descriptor (reading the next link AFTER the call, since the descriptor's
// own Relocate fixes up its next pointer first).
void cLionParticleEffect::Relocate()
{
    if (this == nullptr)
        return;

    // Self-relative offset -> pointer for the descriptor-list head.
    if (mpDescriptors != nullptr)
    {
        mpDescriptors = reinterpret_cast<cParticleDescriptor*>(
            reinterpret_cast<uintptr_t>(this) + reinterpret_cast<uintptr_t>(mpDescriptors));
    }

    for (cParticleDescriptor* lpDes = mpDescriptors; lpDes != nullptr;
         lpDes = lpDes->GetNextDescriptor())
    {
        lpDes->Relocate();
    }
}

// cLionParticleEffect::Serialise @ 0x82912CA8.
// Emit the 12-byte effect record into the serialiser buffer, then serialise each descriptor
// and relink the copies through the copied effect's descriptor-list head and each copied
// descriptor's next pointer. Returns the serialised effect copy (null for a null effect).
cLionParticleEffect* cLionParticleEffect::Serialise(cLionSerialiser& aSer) const
{
    if (this == nullptr)
        return nullptr;

    // Copy the 12-byte serialised effect header (mHash, mpDescriptors, mpNext -- console
    // 3 x 4-byte words) into the buffer; DataStore returns the destination copy.
    cLionParticleEffect* lpEff =
        reinterpret_cast<cLionParticleEffect*>(aSer.DataStore(this, 12));

    // Rebuild the descriptor chain in the copy: link the first descriptor copy through the
    // copied effect's head, each subsequent copy through the previous copy's next pointer.
    cParticleDescriptor** lppLast = &lpEff->mpDescriptors;
    for (cParticleDescriptor* lpDes = mpDescriptors; lpDes != nullptr;
         lpDes = lpDes->GetNextDescriptor())
    {
        cParticleDescriptor* lpCopy = lpDes->Serialise(aSer);
        *lppLast = lpCopy;
        lppLast = &lpCopy->mpNext;
    }

    return lpEff;
}
