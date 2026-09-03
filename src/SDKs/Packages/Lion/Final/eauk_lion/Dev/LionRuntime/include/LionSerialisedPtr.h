#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialisedPtr.h
//
// tLionSerialisedPtr<T> -- a LION serialised link: the 4-byte slot a saved .lef record
// carries where the console source wrote a `T*`.
//
// WHY THIS TYPE EXISTS, precisely.
// A .lef payload is CONSOLE-LAYOUT SERIALISED DATA that the PC loads VERBATIM. Nothing
// converts it record by record: cLionFX::BinLoad @0x82914388 is handed the bytes as they
// sit in PARTICLES.BUNDLE and walks them in place. Every record size in that stream is
// pinned by the console's own serialiser calls --
//     cLionEffectDefinition   84   (cLionFX::BinSave      @0x82914438 DataStore(this, 84))
//     cLionParticleEffect     12   (cLionParticleEffect::Serialise @0x82912CA8, 12)
//     cParticleDescriptor     96   (cParticleDescriptor::Serialise @0x8290F640, 96)
//     cParticleBehaviour    1216   (cParticleBehaviour::Serialise  @0x8290F098, 1216)
//     cParticleMaterial      164   (cParticleMaterial::Serialise   @0x8290E720, 164)
// -- and every one of those numbers counts each link as FOUR bytes. Modelling a link as a
// host `T*` widens it to eight and every member after the first link is then read at the
// wrong offset. That is exactly the access violation this type closes: with host pointers
// the five records measured 136 / 24 / 136 / 1240 / 208 and
// `cParticleDescriptor::Relocate + 0xE` faulted reading 0xFFFFFFFFFFFFFFFF.
//
// This is the project's standing SERIALISED-SLOTS-STAY-32-BIT rule, and it is not a
// "PC accommodation": it is what the data says the field is. A slot is not a pointer that
// happens to be small -- on the console it is written to disk as a self-relative BYTE
// OFFSET and Relocate turns it into an absolute address with `slot += (u32)&owner`,
// entirely in 32-bit arithmetic. Reproducing that arithmetic in u32 is the faithful
// transcription; the reinterpret_cast to a usable `T*` happens only at the READ.
//
// WHY A 32-BIT ABSOLUTE ADDRESS IS VALID ON THE HOST. The game heap is below 4 GB, which
// is the same convention the three already-registered PARTICLES.BUNDLE handlers rely on
// (BrnParticle::ParticleDescriptionCollectionResourceType::FixUp @0x8267DF40 and
// ParticleDescriptionResourceType::FixUp @0x8267DF60 both do `*(u32*)p += (u32)p`).
// If that ever stops being true the failure is silent, so cLionFX::BinLoad announces a
// blob that lands at or above 4 GB rather than truncating it quietly.
//
// The type is a standard-layout, trivially-copyable struct holding exactly one u32, so a
// record that uses it is itself standard-layout and its sizeof matches the console's.
// ============================================================================

#include "types.hpp"

#include <cstddef>   // offsetof, used by every record's layout static_asserts
#include <cstdint>

template <typename T>
class tLionSerialisedPtr
{
public:
    // The read. A relocated slot holds an absolute address; before relocation it holds a
    // self-relative byte offset and Get() is meaningless (as it is on the console).
    T* Get() const { return reinterpret_cast<T*>(static_cast<uintptr_t>(muSlot)); }

    // The write. Truncating by construction -- see the header note on the below-4 GB heap.
    void Set(T* apValue)
    {
        muSlot = static_cast<u32>(reinterpret_cast<uintptr_t>(apValue));
    }

    // The raw serialised word, for the endian/relocation paths that must see it as data.
    u32  Raw() const        { return muSlot; }
    void SetRaw(u32 auWord) { muSlot = auWord; }
    u32* RawAddress()       { return &muSlot; }

    // offset -> absolute, the `if (v) field = base + v` the console's Relocate bodies run
    // once per slot. Kept as one named operation so the transcription reads as the asm
    // does; the null test IS the console's branch, not an added guard.
    void Relocate(const void* apBase)
    {
        if (muSlot != 0u)
            muSlot += static_cast<u32>(reinterpret_cast<uintptr_t>(apBase));
    }

    // absolute -> offset, the save-side inverse (`if (v) field = v - base`).
    void Delocate(const void* apBase)
    {
        if (muSlot != 0u)
            muSlot -= static_cast<u32>(reinterpret_cast<uintptr_t>(apBase));
    }

    bool IsNull() const { return muSlot == 0u; }

    // Pointer sugar so the walkers read the way the original source did.
    T* operator->() const { return Get(); }
    explicit operator bool() const { return muSlot != 0u; }

    tLionSerialisedPtr& operator=(T* apValue) { Set(apValue); return *this; }

private:
    u32 muSlot;
};

// The whole point of the type: four bytes, four-byte aligned, whatever T is.
static_assert(sizeof(tLionSerialisedPtr<char>) == 4,
              "a LION serialised link is one 4-byte slot");
static_assert(alignof(tLionSerialisedPtr<char>) == 4,
              "a LION serialised link is 4-byte aligned");
