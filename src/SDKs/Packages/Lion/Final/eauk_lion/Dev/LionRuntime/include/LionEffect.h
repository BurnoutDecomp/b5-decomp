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
// X360 pointers are 32-bit; on the x64 host the three pointer members widen, so the
// absolute byte offsets differ by design -- members are reached BY NAME. Grow this
// record additively as further Lion effect TUs land.
//
// ONLY THE MEMBERS AND THE TWO TRIVIAL ACCESSORS ARE DECLARED HERE. Of this class's
// DWARF method list, the X360 ledger attests exactly one function --
// cLionEffectDefinition::Delocate @0x829129B0 -- and that one is not reconstructed in
// this pass (it is the save path, which nothing on the PC runs). Nothing else is
// declared, so nothing can silently forward to a body that does not exist.
// ============================================================================

#include "types.hpp"

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
    cLionParticleEffect* GetParticles() const { return mpParticles; }

    U32                    mVersion;                     // console +0x00
    U32                    m_key;                        // console +0x04 (LionHash)
    // LionChar is the Lion 16-bit character type; the shipped .lef names are UTF-16, and
    // cLionEffectDefinition::Delocate byte-swaps this span as 32 x 16-bit units.
    u16                    m_name[KU_MAX_NAME_LENGTH];   // console +0x08 .. +0x47
    cLionParticleEffect*   mpParticles;                  // console +0x48
    cLionBindings*         mpBindings;                   // console +0x4C
    cLionEffectDefinition* mpNext;                       // console +0x50
};
