#include "SharedClasses/Physics/Props/BrnPropGraphicsList.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"   // rw::Resource (m_baseResources[])

#include <cstddef>

// =============================================================================
// BrnPhysics::Props::PropGraphicsList -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   PropGraphicsList::FixUp           @ 0x8267DB38
//   PropGraphicsList::FixDown         @ 0x8267DCD0
//   PropGraphicsList::GetPartGraphics @ 0x826756C0
//   PropGraphicsList::GetPropGraphics @ 0x82675648
//
// Both fix-ups walk the serialised blob, which on the x64 target IS the host
// layout (the porter contract pinned in BrnPropGraphicsList.h), so the walks
// index by named members at the host strides -- semantic parity with the X360
// 12-byte-stride byte arithmetic.
// =============================================================================

namespace BrnPhysics
{
namespace Props
{
    // FixUp @ 0x8267DB38. Load fix-up: rebase the two table pointers, then each
    // PropGraphics element's mpParts, by the resource's base data address (v3 = *a2,
    // the first base-resource word). NO null guards -- the X360 adds the base
    // unconditionally (a null-tabled empty list ends up pointing at the blob base,
    // and consumers gate on the counts, never on the pointers). The Model slots are
    // bundle IMPORT slots (resolved by the pool import pass), not touched here.
    // Tail: a non-gating size-sanity tripwire assert (the X360 builds a StrStream
    // diagnostic with the size and address; the message content is debug-only).
    void PropGraphicsList::FixUp(const rw::Resource& lrBaseResource)
    {
        const ptrdiff_t liBase =
            reinterpret_cast<ptrdiff_t>(lrBaseResource.m_baseResources[0]);

        mpaPropGraphics = reinterpret_cast<PropGraphics*>(
            reinterpret_cast<ptrdiff_t>(mpaPropGraphics) + liBase);
        mpaPropPartGraphics = reinterpret_cast<PropPartGraphics*>(
            reinterpret_cast<ptrdiff_t>(mpaPropPartGraphics) + liBase);

        if ( muNumberOfPropModels != 0 )
        {
            u32 luIndex = 0;
            do
            {
                PropGraphics& lrElement = mpaPropGraphics[luIndex];
                lrElement.mpParts = reinterpret_cast<PropPartGraphics*>(
                    reinterpret_cast<ptrdiff_t>(lrElement.mpParts) + liBase);
                ++luIndex;
            }
            while ( luIndex < muNumberOfPropModels );
        }

        // X360: fires when muSizeInBytes >= 0x2800 (execution continues). The x64
        // serialised form widens the arrays, so the equivalent capacity bound scales
        // by the worst-case stride growth (24/12 = 2x). [PC DIVERGENCE: constant
        // scaled for the widened blob; the X360 literal is 0x2800.]
        CGS_ASSERT(muSizeInBytes < 2u * 0x2800u, "muSizeInBytes < KU_MAX_SERIALISED_SIZE");
    }

    // FixDown @ 0x8267DCD0 (store-for-store). Serialise-out fix-up: rebase every embedded
    // pointer from an absolute address to a base-relative offset by subtracting the resource's
    // base data address. The base word is the first field of the passed rw::Resource
    // (lwz r8,0(r4) => resource offset 0; on the X360 GetMemoryResource was inlined to this word).
    // Every subtract is null-preserving exactly as the asm (beq guards). The X360 walks each
    // PropGraphics element rebasing its two embedded pointers (mpPropModel, mpParts), then
    // rebases the two top-level table pointers.
    void PropGraphicsList::FixDown(const rw::Resource& lrBaseResource)
    {
        // v3 = *a2: the resource's base data address (first base-resource word of rw::Resource).
        const ptrdiff_t liBase =
            reinterpret_cast<ptrdiff_t>(lrBaseResource.m_baseResources[0]);

        // Walk each PropGraphics, rebasing its embedded pointers (preserve null).
        if ( muNumberOfPropModels != 0 )
        {
            u32 luIndex = 0;
            do
            {
                PropGraphics& lrElement = mpaPropGraphics[luIndex];

                ptrdiff_t liPropModel = reinterpret_cast<ptrdiff_t>(lrElement.mpPropModel);
                if ( liPropModel != 0 )
                    lrElement.mpPropModel = reinterpret_cast<Model*>(liPropModel - liBase);

                ptrdiff_t liParts = reinterpret_cast<ptrdiff_t>(lrElement.mpParts);
                if ( liParts != 0 )
                    lrElement.mpParts = reinterpret_cast<PropPartGraphics*>(liParts - liBase);

                ++luIndex;
            }
            while ( luIndex < muNumberOfPropModels );
        }

        // Top-level table pointers (preserve null).
        {
            ptrdiff_t liGraphics = reinterpret_cast<ptrdiff_t>(mpaPropGraphics);
            if ( liGraphics != 0 )
                mpaPropGraphics = reinterpret_cast<PropGraphics*>(liGraphics - liBase);
        }
        {
            ptrdiff_t liParts = reinterpret_cast<ptrdiff_t>(mpaPropPartGraphics);
            if ( liParts != 0 )
                mpaPropPartGraphics = reinterpret_cast<PropPartGraphics*>(liParts - liBase);
        }
    }

    // GetPartGraphics @ 0x826756C0 (store-for-store). Non-const overload: bounds-assert then
    // return &mpaPropPartGraphics[luIndex] (X360 stride 12). The assert is a non-gating tripwire --
    // execution continues past a failure exactly as the X360 (BeginAssert/FireAssert/EndAssert,
    // no early-out).
    PropPartGraphics* PropGraphicsList::GetPartGraphics(u32 luIndex)
    {
        CGS_ASSERT(luIndex < muNumberOfPropPartModels, "luIndex < muNumberOfPropPartModels");
        return &mpaPropPartGraphics[luIndex];
    }

    // GetPropGraphics @ 0x82675648 (store-for-store). Non-const overload: bounds-assert then
    // return &mpaPropGraphics[luIndex] (X360 stride 12). Non-gating tripwire assert (no early-out).
    PropGraphics* PropGraphicsList::GetPropGraphics(u32 luIndex)
    {
        CGS_ASSERT(luIndex < muNumberOfPropModels, "luIndex < muNumberOfPropModels");
        return &mpaPropGraphics[luIndex];
    }

} // namespace Props
} // namespace BrnPhysics
