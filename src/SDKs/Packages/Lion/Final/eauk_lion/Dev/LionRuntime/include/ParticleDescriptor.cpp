// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleDescriptor.cpp
//
// cParticleDescriptor runtime. Reconstructed store-for-store from the X360 ARTIST asm:
//   cParticleDescriptor::GetRequiredBucketType @ 0x82908660
//   cParticleDescriptor::Relocate              @ 0x8290F488   (2026-09-03)
//   cParticleDescriptor::Delocate              @ 0x8290CE50   (2026-09-03)
//   cParticleDescriptor::GetDurationMax        @ 0x82909698   (2026-09-03)
//   cParticleDescriptor::GetSerialiseSize      @ 0x8290D100   (2026-09-03)
//   cParticleDescriptor::Serialise             @ 0x8290F640   (2026-09-03)
//
// The five added here are what cLionParticleEffect's own Relocate/Delocate/Build/
// GetDurationMax/GetSerialiseSize/Serialise call once per descriptor in the chain --
// they were declared and called but never defined, so LionParticleEffect.cpp could not
// link and therefore could not be mounted.
// ============================================================================

#include "ParticleDescriptor.h"

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBehaviour.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleMaterial.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialiser.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionTokeniser.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleParser.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleWaveForm.h"

#include <cstdint>

// Out-of-line body for cParticleDescriptor::GetRequiredBucketType.
//
// Reconstructed store-for-store from the X360 ARTIST asm at 0x82908660:
//   - If mFlags bit 0x10 is clear, no bucket is needed -> return 0 (the function
//     returns r3=0 immediately).
//   - Otherwise switch on mRenderMode (0..9): cases {0,3,4,9} -> bucket type 1, every
//     other case (the jump-table default) -> bucket type 2.
//   - Finally, if mFlags bit 0x20 is also set, force bucket type 2 regardless.
//
// The render-mode block evaluates first, then the 0x20 override; that ordering matches
// the asm (the switch picks 1 or 2, then a second `rlwinm` test bumps the result to 2).

cParticleDescriptor::BucketType cParticleDescriptor::GetRequiredBucketType() const
{
    BucketType result = E_BUCKET_NONE;

    if ((mFlags & E_FLAG_NEEDS_BUCKET) != 0)
    {
        switch (mShape)
        {
            case 0:
            case 3:
            case 4:
            case 9:
                result = E_BUCKET_LIGHT;
                break;
            default:
                result = E_BUCKET_HEAVY;
                break;
        }

        if ((mFlags & E_FLAG_FORCE_HEAVY) != 0)
            return E_BUCKET_HEAVY;
    }

    return result;
}

// ----------------------------------------------------------------------------
// Byte-reverse the 32-bit word a serialised slot occupies, in place. The X360 builds
// the word from ascending bytes packed most-significant-first and stores it back
// native-endian, which on a little-endian host is a plain reversal. Serialised Lion
// slots are 32 bits (they stay 32 bits on the host -- see cLionParticleEffect's
// record note); for the members that widen to a host pointer this reverses the four
// bytes the console record carries, which is exactly what the committed sibling
// cParticleBehaviour::Delocate does. Save-side only: nothing on the PC calls
// cLionFX::BinSave, so this never runs here.
// ----------------------------------------------------------------------------
namespace
{
    inline void SwapSerialisedWord(void* apField)
    {
        u8* lp = static_cast<u8*>(apField);
        const u8 lb0 = lp[0];
        const u8 lb1 = lp[1];
        const u8 lb2 = lp[2];
        const u8 lb3 = lp[3];
        const u32 luSwapped = (((((static_cast<u32>(lb0) << 8) | lb1) << 8) | lb2) << 8) | lb3;
        *reinterpret_cast<u32*>(lp) = luSwapped;
    }
}

// ----------------------------------------------------------------------------
// cParticleDescriptor::Relocate  @ 0x8290F488
//
// Inverse of Delocate's pointer->offset conversion, run on load (cLionFX::BinLoad ->
// cLionParticleEffect::Relocate -> here, once per descriptor in the chain). Every owned
// pointer is stored in the file as a byte offset from the record that owns it, so each
// one is re-based against its own base:
//   * this descriptor's eight own pointers (asm words 16,17,19,14,20,22,23,21);
//   * then, for each behaviour in the chain, that behaviour's five wave-form pointers
//     and its mpNext -- INLINED here in the X360 build (the asm walks i[178]..i[183]
//     itself rather than calling cParticleBehaviour::Relocate), so the chain is walked
//     through each node's freshly re-based mpNext;
//   * then the temp behaviour and the material through their own Relocate;
//   * then every child descriptor recursively, each of which has its mpParent set to
//     this descriptor (the asm's `*(j + 88) = v1`, which overwrites the file's value).
// Finally mpBehaviour is seeded to the chain head (`v1[18] = v1[16]`).
// ----------------------------------------------------------------------------
void cParticleDescriptor::Relocate()
{
    if (this == nullptr)
        return;

    u8* lpBase = reinterpret_cast<u8*>(this);

    // The eight owned pointers, in the asm's order.
    if (mpBehaviours != nullptr)
        mpBehaviours = reinterpret_cast<cParticleBehaviour*>(lpBase + reinterpret_cast<uintptr_t>(mpBehaviours));
    if (mpBehaviourTemp != nullptr)
        mpBehaviourTemp = reinterpret_cast<cParticleBehaviour*>(lpBase + reinterpret_cast<uintptr_t>(mpBehaviourTemp));
    if (mpMaterial != nullptr)
        mpMaterial = reinterpret_cast<cParticleMaterial*>(lpBase + reinterpret_cast<uintptr_t>(mpMaterial));
    if (mpName != nullptr)
        mpName = reinterpret_cast<char*>(lpBase + reinterpret_cast<uintptr_t>(mpName));
    if (mpDef != nullptr)
        mpDef = reinterpret_cast<cLionEffectDefinition*>(lpBase + reinterpret_cast<uintptr_t>(mpDef));
    if (mpParent != nullptr)
        mpParent = reinterpret_cast<cParticleDescriptor*>(lpBase + reinterpret_cast<uintptr_t>(mpParent));
    if (mpChild != nullptr)
        mpChild = reinterpret_cast<cParticleDescriptor*>(lpBase + reinterpret_cast<uintptr_t>(mpChild));
    if (mpNext != nullptr)
        mpNext = reinterpret_cast<cParticleDescriptor*>(lpBase + reinterpret_cast<uintptr_t>(mpNext));

    // The behaviour chain, re-based in place. The X360 INLINES cParticleBehaviour::
    // Relocate here (the asm loop over i[178]..i[183]); reproduced as the same six
    // re-bases so the walk follows each node's freshly re-based mpNext, as the asm does.
    for (cParticleBehaviour* lpBeh = mpBehaviours; lpBeh != nullptr; lpBeh = lpBeh->mpNext)
    {
        u8* lpBehBase = reinterpret_cast<u8*>(lpBeh);
        if (lpBeh->mpWaveFormX != nullptr)
            lpBeh->mpWaveFormX = reinterpret_cast<cParticleWaveForm*>(lpBehBase + reinterpret_cast<uintptr_t>(lpBeh->mpWaveFormX));
        if (lpBeh->mpWaveFormY != nullptr)
            lpBeh->mpWaveFormY = reinterpret_cast<cParticleWaveForm*>(lpBehBase + reinterpret_cast<uintptr_t>(lpBeh->mpWaveFormY));
        if (lpBeh->mpWaveFormZ != nullptr)
            lpBeh->mpWaveFormZ = reinterpret_cast<cParticleWaveForm*>(lpBehBase + reinterpret_cast<uintptr_t>(lpBeh->mpWaveFormZ));
        if (lpBeh->mpWaveFormAlpha != nullptr)
            lpBeh->mpWaveFormAlpha = reinterpret_cast<cParticleWaveForm*>(lpBehBase + reinterpret_cast<uintptr_t>(lpBeh->mpWaveFormAlpha));
        if (lpBeh->mpWaveFormRGB != nullptr)
            lpBeh->mpWaveFormRGB = reinterpret_cast<cParticleWaveForm*>(lpBehBase + reinterpret_cast<uintptr_t>(lpBeh->mpWaveFormRGB));
        if (lpBeh->mpNext != nullptr)
            lpBeh->mpNext = reinterpret_cast<cParticleBehaviour*>(lpBehBase + reinterpret_cast<uintptr_t>(lpBeh->mpNext));
    }

    if (mpBehaviourTemp != nullptr)
        mpBehaviourTemp->Relocate();
    if (mpMaterial != nullptr)
        mpMaterial->Relocate();

    for (cParticleDescriptor* lpChild = mpChild; lpChild != nullptr; lpChild = lpChild->mpNext)
    {
        lpChild->Relocate();
        lpChild->mpParent = this;
    }

    mpBehaviour = mpBehaviours;
}

// ----------------------------------------------------------------------------
// cParticleDescriptor::Delocate  @ 0x8290CE50
//
// The save-side inverse: delocate the owned graph, convert this record's eight owned
// pointers to base-relative offsets, and -- when the endian-twiddle flag is set --
// byte-swap the descriptor's own member image through the Lion descriptor token table
// (off_82F36A34) and then the eight pointer words by hand.
//
// The asm reads each behaviour's mpNext BEFORE delocating that behaviour, because
// cParticleBehaviour::Delocate rewrites the link it would otherwise follow; the child
// walk does the same.
//
// Which words the token table does NOT cover, and are therefore twiddled here:
// mBehaviourCount (+60), mpBehaviours (+64), mpBehaviourTemp (+68), mpMaterial (+76),
// mpDef (+80), mpParent (+88), mpChild (+92), mpNext (+84). mpName (+56) IS in the
// table (a POINTER token) and mpBehaviour (+72) is never twiddled at all -- Relocate
// re-seeds it from mpBehaviours.
// ----------------------------------------------------------------------------
void cParticleDescriptor::Delocate(u32 aEndianTwiddleFlag)
{
    if (this == nullptr)
        return;

    cParticleBehaviour* lpBeh = mpBehaviours;
    while (lpBeh != nullptr)
    {
        cParticleBehaviour* lpBehNext = lpBeh->mpNext;
        lpBeh->Delocate(aEndianTwiddleFlag);
        lpBeh = lpBehNext;
    }

    if (mpBehaviourTemp != nullptr)
        mpBehaviourTemp->Delocate(aEndianTwiddleFlag);
    if (mpMaterial != nullptr)
        mpMaterial->Delocate(aEndianTwiddleFlag);

    cParticleDescriptor* lpChild = mpChild;
    while (lpChild != nullptr)
    {
        cParticleDescriptor* lpChildNext = lpChild->mpNext;
        lpChild->Delocate(aEndianTwiddleFlag);
        lpChild = lpChildNext;
    }

    // Pointer -> base-relative offset, in the asm's order.
    const uintptr_t lBase = reinterpret_cast<uintptr_t>(this);
    if (mpBehaviours != nullptr)
        mpBehaviours = reinterpret_cast<cParticleBehaviour*>(reinterpret_cast<uintptr_t>(mpBehaviours) - lBase);
    if (mpBehaviourTemp != nullptr)
        mpBehaviourTemp = reinterpret_cast<cParticleBehaviour*>(reinterpret_cast<uintptr_t>(mpBehaviourTemp) - lBase);
    if (mpMaterial != nullptr)
        mpMaterial = reinterpret_cast<cParticleMaterial*>(reinterpret_cast<uintptr_t>(mpMaterial) - lBase);
    if (mpName != nullptr)
        mpName = reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(mpName) - lBase);
    if (mpDef != nullptr)
        mpDef = reinterpret_cast<cLionEffectDefinition*>(reinterpret_cast<uintptr_t>(mpDef) - lBase);
    if (mpParent != nullptr)
        mpParent = reinterpret_cast<cParticleDescriptor*>(reinterpret_cast<uintptr_t>(mpParent) - lBase);
    if (mpChild != nullptr)
        mpChild = reinterpret_cast<cParticleDescriptor*>(reinterpret_cast<uintptr_t>(mpChild) - lBase);
    if (mpNext != nullptr)
        mpNext = reinterpret_cast<cParticleDescriptor*>(reinterpret_cast<uintptr_t>(mpNext) - lBase);

    if (aEndianTwiddleFlag != 0)
    {
        gLionParticleParserDesTokenTable.EndianTwiddle(this);
        SwapSerialisedWord(&mBehaviourCount);
        SwapSerialisedWord(&mpBehaviours);
        SwapSerialisedWord(&mpBehaviourTemp);
        SwapSerialisedWord(&mpMaterial);
        SwapSerialisedWord(&mpDef);
        SwapSerialisedWord(&mpParent);
        SwapSerialisedWord(&mpChild);
        SwapSerialisedWord(&mpNext);
    }
}

// ----------------------------------------------------------------------------
// cParticleDescriptor::GetDurationMax  @ 0x82909698
//
// How long an emitter built from this descriptor can live, in seconds:
//     mPauseTime + mPauseTimeVariance + mEmitterLifeBase + mEmitterLifeVariance
//   + max over the behaviour chain of (mLifeBase + mLifeVariance)
// plus, when a child descriptor exists, that child's own duration (or the child's value
// alone when the child reports the "never ends" sentinel). A descriptor whose
// mEmitterLifeInfiniteFlag is set returns -1.0 -- the sentinel
// BrnParticle::ParticleModule::StartLionEffect @0x82289F50 tests to give a playing
// effect the 1.0e10 "endless" expiry instead of a real one.
//
// The behaviour running max is the X360's `fsel(running - (life + variance), running,
// life + variance)`, i.e. max(), with the fsel's tie/NaN behaviour preserved.
//
// asm note: the infinite test is the LAST thing the function does, after the sum has
// been computed and after the child recursion -- so a descriptor that is BOTH infinite
// and has children still recurses. Reproduced in that order.
// ----------------------------------------------------------------------------
f32 cParticleDescriptor::GetDurationMax() const
{
    f32 lfBehaviourMax = 0.0f;
    for (const cParticleBehaviour* lpBeh = mpBehaviours; lpBeh != nullptr; lpBeh = lpBeh->mpNext)
    {
        const f32 lfLife = lpBeh->mLifeBase + lpBeh->mLifeVariance;
        lfBehaviourMax = (lfBehaviourMax - lfLife) >= 0.0f ? lfBehaviourMax : lfLife;
    }

    f32 lfDuration = mPauseTime + lfBehaviourMax + mPauseTimeVariance
                   + mEmitterLifeBase + mEmitterLifeVariance;

    if (mpChild != nullptr)
    {
        const f32 lfChild = mpChild->GetDurationMax();
        lfDuration = (lfChild >= 0.0f) ? (lfChild + lfDuration) : lfChild;
    }

    if (mEmitterLifeInfiniteFlag != 0)
        return -1.0f;

    return lfDuration;
}

// ----------------------------------------------------------------------------
// cParticleDescriptor::GetSerialiseSize  @ 0x8290D100
//
// Account for this descriptor in the serialiser: 96 bytes of data (its own record --
// the same 96 Serialise's DataStore copies, so 96 IS sizeof the console record), the
// NUL-terminated name in the string area, then every behaviour in the chain, the temp
// behaviour, the material and every child descriptor.
//
// The X360 INLINES cParticleBehaviour::GetSerialiseSize for the chain (adding 1216 for
// the node plus 64 per present wave-form) and CALLS it for the temp behaviour. The
// committed out-of-line body is that same arithmetic, so both go through it here.
// ----------------------------------------------------------------------------
void cParticleDescriptor::GetSerialiseSize(cLionSerialiser& aSer) const
{
    if (this == nullptr)
        return;

    aSer.mDataSize += 96;

    if (mpName != nullptr)
    {
        // The asm's `while (*v4++) ; mStringSize += v4 - mpName` == strlen + 1.
        u32 luLength = 0;
        while (mpName[luLength] != 0)
            ++luLength;
        aSer.mStringSize += luLength + 1;
    }

    for (const cParticleBehaviour* lpBeh = mpBehaviours; lpBeh != nullptr; lpBeh = lpBeh->mpNext)
        lpBeh->GetSerialiseSize(aSer);

    if (mpBehaviourTemp != nullptr)
        mpBehaviourTemp->GetSerialiseSize(aSer);
    if (mpMaterial != nullptr)
        mpMaterial->GetSerialiseSize(aSer);

    for (const cParticleDescriptor* lpChild = mpChild; lpChild != nullptr; lpChild = lpChild->mpNext)
        lpChild->GetSerialiseSize(aSer);
}

// ----------------------------------------------------------------------------
// cParticleDescriptor::Serialise  @ 0x8290F640
//
// Emit the 96-byte descriptor record into the serialiser's data area, then serialise
// the owned graph and relink the COPIES through the copy's own pointers: the behaviour
// chain through the copy's mpBehaviours and each copied behaviour's mpNext, the child
// chain through the copy's mpChild and each copied child's mpNext, and the temp
// behaviour / material through their single slots. The name is interned into the string
// area last, and the copy's mpBehaviour is seeded from the copy's mpBehaviours.
// Returns the copy, or null for a null descriptor.
// ----------------------------------------------------------------------------
cParticleDescriptor* cParticleDescriptor::Serialise(cLionSerialiser& aSer) const
{
    if (this == nullptr)
        return nullptr;

    cParticleDescriptor* lpCopy =
        reinterpret_cast<cParticleDescriptor*>(aSer.DataStore(this, 96));

    cParticleBehaviour** lppLastBeh = &lpCopy->mpBehaviours;
    for (const cParticleBehaviour* lpBeh = mpBehaviours; lpBeh != nullptr; lpBeh = lpBeh->mpNext)
    {
        cParticleBehaviour* lpBehCopy = lpBeh->Serialise(aSer);
        *lppLastBeh = lpBehCopy;
        lppLastBeh = &lpBehCopy->mpNext;
    }

    if (mpBehaviourTemp != nullptr)
        lpCopy->mpBehaviourTemp = mpBehaviourTemp->Serialise(aSer);
    if (mpMaterial != nullptr)
        lpCopy->mpMaterial = mpMaterial->Serialise(aSer);

    cParticleDescriptor** lppLastChild = &lpCopy->mpChild;
    for (const cParticleDescriptor* lpChild = mpChild; lpChild != nullptr; lpChild = lpChild->mpNext)
    {
        cParticleDescriptor* lpChildCopy = lpChild->Serialise(aSer);
        *lppLastChild = lpChildCopy;
        lppLastChild = &lpChildCopy->mpNext;
    }

    // The asm interns the name unconditionally -- StringStore itself takes the null case.
    char* lpName = aSer.StringStore(mpName);
    lpCopy->mpBehaviour = lpCopy->mpBehaviours;
    lpCopy->mpName = lpName;
    return lpCopy;
}
