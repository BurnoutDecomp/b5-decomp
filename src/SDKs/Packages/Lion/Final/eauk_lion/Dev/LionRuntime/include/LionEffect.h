#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionEffect.h
//
// cLionEffectDefinition -- the 84-byte header that fronts a saved LION effect. It is
// the record cLionFX::BinLoad @0x82914388 is handed, and the record every
// BrnParticle::ParticleDescription resource carries at +16 (the .lef payload).
//
// DECLARATION AUTHORITY: the DecFIGS DWARF (LionEffect.h:37, :151-172) declares the
// whole record. Every offset it implies is confirmed by the X360 asm that reaches
// them:
//   BinLoad @0x82914388             tests word 0 == 65539 and rebases word 18 (+0x48)
//   cLionEffectDefinition::Delocate @0x829129B0 delocates word 18, writes 65539 into
//                                   word 0, clears words 19/20 (+0x4C/+0x50), and --
//                                   when twiddling -- byte-swaps words 0, 1 and 18 plus
//                                   thirty-two 16-bit units from +8 (the eight-iteration
//                                   x4 loop), which is exactly m_name[32] as LionChar
//                                   (2 bytes)
//   cLionFX::BinSave  @0x82914438   DataStore(this, 84) -- so sizeof == 84 == 0x54
//   ParticleModule::StartLionEffect @0x82289F50 reads `*(definition + 0x48)` and hands
//                                   it to cLionParticleEffect::GetDurationMax, which
//                                   pins +0x48 as the cLionParticleEffect*
//
//   +0x00  U32                    mVersion      (== 65539 == 0x00010003)
//   +0x04  LionHash               m_key
//   +0x08  LionChar               m_name[32]    (64 bytes; UTF-16 in the shipped data)
//   +0x48  cLionParticleEffect*   mpParticles
//   +0x4C  cLionBindings*         mpBindings    (Delocate clears it)
//   +0x50  cLionEffectDefinition* mpNext        (Delocate clears it)
//
// ⭐ 2026-09-03: the three links are tLionSerialisedPtr, NOT host pointers. This record is
// read verbatim out of a .lef, so a widened link would move m_name's successors and make
// `BinSave`'s attested 84 unrepresentable; the static_assert below is what says so. See
// LionSerialisedPtr.h for the whole argument. Members are still reached BY NAME.
//
// ONLY THE MEMBERS AND THE TWO TRIVIAL ACCESSORS ARE DECLARED HERE. Of this class's
// DWARF method list, the X360 ledger attests exactly one function --
// cLionEffectDefinition::Delocate @0x829129B0 -- and that one is not reconstructed in
// this pass (it is the save path, which nothing on the PC runs). Nothing else is
// declared, so nothing can silently forward to a body that does not exist.
//
// ⭐⭐ 2026-09-03: cLionEffectInstance IS NOW HOMED HERE (second half of this file). It is
// the record cLionEffectManager::EffectCreate @0x829149E8 carves out of the manager's block
// pool, and its absence was what made the 0x90 item size in LionEffectManager.cpp a console
// literal with no sizeof() to replace it. See that class's own banner.
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialisedPtr.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBindings.h"

#ifndef LION_SCALAR_TYPEDEFS
#define LION_SCALAR_TYPEDEFS
typedef u32   U32;
typedef s32   S32;
typedef u8    U8;
typedef float FP32;
#endif

class cLionParticleEffect;   // LionParticleEffect.h (sibling home)
struct cLionBindings;        // LionBindings.h (sibling home; pointer-only here)

// DecFIGS DWARF LionEffect.h:37.
struct cLionEffectDefinition
{
    // LionEffect.h:151. The name field is a fixed 32-unit array.
    static const U32 KU_MAX_NAME_LENGTH = 32;

    // cLionFX::BinLoad's magic test (`if ( *a1 == 65539 )`); a blob whose first word is
    // anything else is rejected and the load returns NULL.
    static const U32 KU_VERSION = 65539;   // 0x00010003

    // LionEffect.h:47 / :55 / :106 -- trivial field reads the X360 build inlines.
    U32 Version() const { return mVersion; }
    U32 Key() const { return m_key; }
    cLionParticleEffect* GetParticles() const { return mpParticles.Get(); }

    U32                    mVersion;                     // console +0x00
    U32                    m_key;                        // console +0x04 (LionHash)
    // LionChar is the Lion 16-bit character type; the shipped .lef names are UTF-16, and
    // cLionEffectDefinition::Delocate byte-swaps this span as 32 x 16-bit units.
    u16                    m_name[KU_MAX_NAME_LENGTH];   // console +0x08 .. +0x47
    tLionSerialisedPtr<cLionParticleEffect>   mpParticles;   // console +0x48
    tLionSerialisedPtr<cLionBindings>         mpBindings;    // console +0x4C
    tLionSerialisedPtr<cLionEffectDefinition> mpNext;        // console +0x50
};

// cLionFX::BinSave @0x82914438 does `DataStore(this, 84)`; the record IS 84 bytes and the
// host must agree, because BinLoad reads a .lef image of exactly this shape.
static_assert(sizeof(cLionEffectDefinition) == 84,
              "cLionEffectDefinition is the 84-byte serialised .lef header "
              "(cLionFX::BinSave @0x82914438 DataStore(this, 84))");
static_assert(offsetof(cLionEffectDefinition, mpParticles) == 0x48,
              "mpParticles is the word cLionFX::BinLoad @0x82914388 rebases");

// ============================================================================
// cLionEffectInstance -- ONE PLAYING EFFECT.
//
// DECLARATION AUTHORITY: the DecFIGS DWARF (LionEffect.h:179, members h:242..245, the flag
// enum h:182). Every member offset the DWARF implies is confirmed by the two X360 bodies
// that build and tear one down:
//
//   cLionEffectManager::EffectCreate  @0x829149E8
//       v13 = cLionBlockAlloc::Alloc(&mAllocator)   -- carved from the manager's pool
//       *(v13 + 128) = 0                            -> mFlags        @+0x80
//       cLionBindings::Init(v13 + 16)               -> mBindings     @+0x10
//       v14[7] = a3 / v14[8] = a4 / v14[9] = a5     -> mBindings.mpLocator (+0x1C == 0x10+0x0C),
//                                                      mpScaler (+0x20), mpTrigger (+0x24)
//       v14[5] = a6                                 -> mBindings.mWorldIndex (+0x14 == 0x10+4)
//       v14[1] = a2                                 -> mpDefinition  @+0x04
//       *v14   = a1[3]                              -> mpNext        @+0x00
//   cLionEffectManager::EffectDestroy @0x82914FF0   reads the same four bindings offsets
//       (0x24 trigger / 0x1C locator / 0x20 scaler) and unlinks `this + 0x10` from the
//       definition's binding chain -- i.e. &mBindings IS the node that chain threads.
//
// The record's SIZE closes independently: cLionFX::Init @0x82914B14 sizes this pool's items
// with `li r5, 0x90` == 144, and mFlags at +0x80 plus its own 4 bytes is 0x84, which rounds
// to 0x90 under the 16-byte alignment cLionBindings' embedded cParticleRandomSeed forces.
// The two 4-byte console pointers at +0x00/+0x04 leave 8 bytes of that alignment padding
// before +0x10 -- on the HOST those two widen to 8 each and fill the same 16 bytes exactly,
// so mBindings lands at +0x10 on both. (Everything past it is reached BY NAME regardless;
// the host record is legitimately wider than 144 and LionEffectManager.cpp now sizes the
// pool with sizeof(cLionEffectInstance) instead of the console literal.)
//
// ⚠ THE X360 LEDGER ATTESTS NONE OF THIS CLASS'S DWARF METHODS AS A STANDALONE BODY -- there
// is no `cLionEffectInstance::*` row anywhere in the export set, because every one of them is
// a field access the compiler inlined into EffectCreate/EffectDestroy. They are de-inlined
// here onto their owning class (the project's standing rule) rather than left as raw offset
// pokes at the call sites. Only the ones those two bodies actually perform are written;
// AddChild / ToConsole / SetFlag / IsFlagSet are NOT declared, so nothing can forward to a
// body that does not exist.
// ============================================================================
struct cLionEffectInstance
{
    // DWARF LionEffect.h:182.
    enum LionEffectInstanceFlags
    {
        eLEI_AWAITING_DESTRUCTION = 1,
    };

    // DWARF LionEffect.h:187. Inlined into EffectCreate @0x829149E8: mFlags is cleared and
    // the bindings are Init()ed; mpNext / mpDefinition are written by the caller straight
    // afterwards, so they are not part of this.
    void Init()
    {
        mFlags = 0;
        mBindings.Init();
    }

    // DWARF LionEffect.h:192 / :197.
    const cLionEffectDefinition* GetEffectDefinition() const { return mpDefinition; }
    void SetpEffectDefinition(cLionEffectDefinition* apDefinition) { mpDefinition = apDefinition; }

    // DWARF LionEffect.h:202 / :207.
    const cLionBindings& GetBindings() const { return mBindings; }
    cLionBindings&       GetBindings()       { return mBindings; }

    // DWARF LionEffect.h:212 / :217. GetpNext returns a REFERENCE to the link, which is what
    // EffectDestroy's list walk needs (`r10 = node; r11 = *r10` -- mpNext is at +0, so the
    // node's address IS its link's address).
    cLionEffectInstance*& GetpNext()                      { return mpNext; }
    void SetpNext(cLionEffectInstance* apNext)            { mpNext = apNext; }

    // ----- members (DWARF LionEffect.h:242..245; offsets attested above) -----
    cLionEffectInstance*   mpNext;         // console +0x00
    cLionEffectDefinition* mpDefinition;   // console +0x04
    cLionBindings          mBindings;      // console +0x10
    u32                    mFlags;         // console +0x80
};
