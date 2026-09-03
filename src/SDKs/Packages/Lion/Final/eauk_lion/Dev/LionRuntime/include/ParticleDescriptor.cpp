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

    // The eight owned links, in the asm's order. Each is a 32-bit serialised slot and the
    // console re-bases it in 32-bit arithmetic (`if (v) field = (u32)this + v`), which is
    // exactly what tLionSerialisedPtr::Relocate is.
    mpBehaviours.Relocate(this);       // asm word 16
    mpBehaviourTemp.Relocate(this);    // asm word 17
    mpMaterial.Relocate(this);         // asm word 19
    mpName.Relocate(this);             // asm word 14
    mpDef.Relocate(this);              // asm word 20
    mpParent.Relocate(this);           // asm word 22
    mpChild.Relocate(this);            // asm word 23
    mpNext.Relocate(this);             // asm word 21

    // The behaviour chain, re-based in place. The X360 INLINES cParticleBehaviour::
    // Relocate here (the asm loop over i[178]..i[183]); reproduced as the same six
    // re-bases so the walk follows each node's freshly re-based mpNext, as the asm does.
    for (cParticleBehaviour* lpBeh = mpBehaviours.Get(); lpBeh != nullptr;
         lpBeh = lpBeh->mpNext.Get())
    {
        lpBeh->mpWaveFormX.Relocate(lpBeh);
        lpBeh->mpWaveFormY.Relocate(lpBeh);
        lpBeh->mpWaveFormZ.Relocate(lpBeh);
        lpBeh->mpWaveFormAlpha.Relocate(lpBeh);
        lpBeh->mpWaveFormRGB.Relocate(lpBeh);
        lpBeh->mpNext.Relocate(lpBeh);
    }

    if (mpBehaviourTemp)
        mpBehaviourTemp->Relocate();
    if (mpMaterial)
        mpMaterial->Relocate();

    for (cParticleDescriptor* lpChild = mpChild.Get(); lpChild != nullptr;
         lpChild = lpChild->mpNext.Get())
    {
        lpChild->Relocate();
        lpChild->mpParent.Set(this);
    }

    mpBehaviour.SetRaw(mpBehaviours.Raw());
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

    cParticleBehaviour* lpBeh = mpBehaviours.Get();
    while (lpBeh != nullptr)
    {
        cParticleBehaviour* lpBehNext = lpBeh->mpNext.Get();
        lpBeh->Delocate(aEndianTwiddleFlag);
        lpBeh = lpBehNext;
    }

    if (mpBehaviourTemp)
        mpBehaviourTemp->Delocate(aEndianTwiddleFlag);
    if (mpMaterial)
        mpMaterial->Delocate(aEndianTwiddleFlag);

    cParticleDescriptor* lpChild = mpChild.Get();
    while (lpChild != nullptr)
    {
        cParticleDescriptor* lpChildNext = lpChild->mpNext.Get();
        lpChild->Delocate(aEndianTwiddleFlag);
        lpChild = lpChildNext;
    }

    // Pointer -> base-relative offset, in the asm's order.
    mpBehaviours.Delocate(this);
    mpBehaviourTemp.Delocate(this);
    mpMaterial.Delocate(this);
    mpName.Delocate(this);
    mpDef.Delocate(this);
    mpParent.Delocate(this);
    mpChild.Delocate(this);
    mpNext.Delocate(this);

    if (aEndianTwiddleFlag != 0)
    {
        gLionParticleParserDesTokenTable.EndianTwiddle(this);
        SwapSerialisedWord(&mBehaviourCount);
        SwapSerialisedWord(mpBehaviours.RawAddress());
        SwapSerialisedWord(mpBehaviourTemp.RawAddress());
        SwapSerialisedWord(mpMaterial.RawAddress());
        SwapSerialisedWord(mpDef.RawAddress());
        SwapSerialisedWord(mpParent.RawAddress());
        SwapSerialisedWord(mpChild.RawAddress());
        SwapSerialisedWord(mpNext.RawAddress());
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
    for (const cParticleBehaviour* lpBeh = mpBehaviours.Get(); lpBeh != nullptr;
         lpBeh = lpBeh->mpNext.Get())
    {
        const f32 lfLife = lpBeh->mLifeBase + lpBeh->mLifeVariance;
        lfBehaviourMax = (lfBehaviourMax - lfLife) >= 0.0f ? lfBehaviourMax : lfLife;
    }

    f32 lfDuration = mPauseTime + lfBehaviourMax + mPauseTimeVariance
                   + mEmitterLifeBase + mEmitterLifeVariance;

    if (mpChild)
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

    if (mpName)
    {
        // The asm's `while (*v4++) ; mStringSize += v4 - mpName` == strlen + 1.
        const char* lpcName = mpName.Get();
        u32 luLength = 0;
        while (lpcName[luLength] != 0)
            ++luLength;
        aSer.mStringSize += luLength + 1;
    }

    for (const cParticleBehaviour* lpBeh = mpBehaviours.Get(); lpBeh != nullptr;
         lpBeh = lpBeh->mpNext.Get())
        lpBeh->GetSerialiseSize(aSer);

    if (mpBehaviourTemp)
        mpBehaviourTemp->GetSerialiseSize(aSer);
    if (mpMaterial)
        mpMaterial->GetSerialiseSize(aSer);

    for (const cParticleDescriptor* lpChild = mpChild.Get(); lpChild != nullptr;
         lpChild = lpChild->mpNext.Get())
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

    tLionSerialisedPtr<cParticleBehaviour>* lppLastBeh = &lpCopy->mpBehaviours;
    for (const cParticleBehaviour* lpBeh = mpBehaviours.Get(); lpBeh != nullptr;
         lpBeh = lpBeh->mpNext.Get())
    {
        cParticleBehaviour* lpBehCopy = lpBeh->Serialise(aSer);
        lppLastBeh->Set(lpBehCopy);
        lppLastBeh = &lpBehCopy->mpNext;
    }

    if (mpBehaviourTemp)
        lpCopy->mpBehaviourTemp.Set(mpBehaviourTemp->Serialise(aSer));
    if (mpMaterial)
        lpCopy->mpMaterial.Set(mpMaterial->Serialise(aSer));

    tLionSerialisedPtr<cParticleDescriptor>* lppLastChild = &lpCopy->mpChild;
    for (const cParticleDescriptor* lpChild = mpChild.Get(); lpChild != nullptr;
         lpChild = lpChild->mpNext.Get())
    {
        cParticleDescriptor* lpChildCopy = lpChild->Serialise(aSer);
        lppLastChild->Set(lpChildCopy);
        lppLastChild = &lpChildCopy->mpNext;
    }

    // The asm interns the name unconditionally -- StringStore itself takes the null case.
    char* lpName = aSer.StringStore(mpName.Get());
    lpCopy->mpBehaviour.SetRaw(lpCopy->mpBehaviours.Raw());
    lpCopy->mpName.Set(lpName);
    return lpCopy;
}
