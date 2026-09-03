#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleEffect.h
//
// cLionParticleEffect -- one Lion (eauk_lion) particle effect: a hashed effect record
// that owns a singly-linked chain of cParticleDescriptor nodes and links into a global
// chain of sibling effects. It is the serialisable root the cLionFX binary load/save
// path walks: Build/UnBuild finalise the runtime graph, Delocate/Relocate convert the
// owned pointers to/from self-relative offsets for saving, and GetSerialiseSize/Serialise
// emit the effect + its descriptor chain into a cLionSerialiser buffer.
//
// LAYOUT AUTHORITY (X360 ARTIST asm + DecFIGS DWARF LionParticleEffect.h:28):
//   mHash        @ console +0x00  U32                  effect name hash
//   mpDescriptors@ console +0x04  cParticleDescriptor* descriptor chain head
//   mpNext       @ console +0x08  cLionParticleEffect* next effect in the global chain
//
// The serialised on-disk record is 12 bytes (3 x 4-byte console words) -- Serialise copies
// exactly 12 bytes via cLionSerialiser::DataStore and Delocate/Relocate byte-twiddle only
// the first two words.
//
// ⭐ 2026-09-03: the two links are tLionSerialisedPtr, so the host record IS 12 bytes, as
// the static_assert below states. A .lef is loaded verbatim, so a widened link would put
// every descriptor chain head at the wrong address. Members are accessed BY NAME. Grow this
// record additively as further Lion effect TUs land; never reorder/retype the members.
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialisedPtr.h"

// Lion scalar spellings (kept local so this header is self-contained; the duplicate-typedef
// rule allows the identical block that ParticleMaterial.h / other Lion headers also carry).
#ifndef LION_SCALAR_TYPEDEFS
#define LION_SCALAR_TYPEDEFS
typedef u32   U32;
typedef s32   S32;
typedef u8    U8;
typedef float FP32;
#endif

class cParticleDescriptor;   // ParticleDescriptor.h (sibling home)
class cLionSerialiser;       // LionSerialiser.h (sibling home)

// DecFIGS DWARF LionParticleEffect.h:28.
class cLionParticleEffect
{
public:
    // --- lifecycle / graph finalisation (own TUs) ---
    void Init(const char* apcName);
    void DeInit();
    void Build();
    void UnBuild();

    // --- serialisation (pointer<->offset relocation, size, emit, remap) ---
    void Delocate(U32 aEndianTwiddleFlag);
    void Relocate();
    void GetSerialiseSize(cLionSerialiser& aSer) const;
    cLionParticleEffect* Serialise(cLionSerialiser& aSer) const;
    void Remap(cLionSerialiser& aSer);

    // --- descriptor-chain editing (own TUs) ---
    cParticleDescriptor* DescriptorAdd();
    cParticleDescriptor* DescriptorClone(const cParticleDescriptor* apDes);
    void DescriptorRemove(cParticleDescriptor* apDes);
    void DescriptorMoveUp(cParticleDescriptor* apDes);
    void DescriptorMoveDown(cParticleDescriptor* apDes);

    // --- derived queries ---
    FP32 GetDurationMax() const;
    FP32 GetRadius() const;

    S32 GetGlobalDescriptorsCount();

    // Chain head, as a usable pointer. The X360 reads the field directly.
    cParticleDescriptor* GetDescriptors() const { return mpDescriptors.Get(); }

    // ----- serialised record members -----
    U32                                       mHash;          // console +0x00
    tLionSerialisedPtr<cParticleDescriptor>   mpDescriptors;  // console +0x04 chain head
    tLionSerialisedPtr<cLionParticleEffect>   mpNext;         // console +0x08 next effect
};

// cLionParticleEffect::Serialise @0x82912CA8 does `DataStore(this, 12)`.
static_assert(sizeof(cLionParticleEffect) == 12,
              "cLionParticleEffect is the 12-byte serialised effect header "
              "(cLionParticleEffect::Serialise @0x82912CA8 DataStore(this, 12))");
