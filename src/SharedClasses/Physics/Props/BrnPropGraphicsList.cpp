#include "SharedClasses/Physics/Props/BrnPropGraphicsList.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"   // rw::Resource (m_baseResources[])

#include <cstddef>

// =============================================================================
// BrnPhysics::Props::PropGraphicsList -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   PropGraphicsList::FixDown         @ 0x8267DCD0
//   PropGraphicsList::GetPartGraphics @ 0x826756C0
//   PropGraphicsList::GetPropGraphics @ 0x82675648
//
// FixDown walks the SERIALISED X360 blob (PropGraphics stride 12) by raw byte
// arithmetic, matching the X360 store-for-store; it rebases the embedded pointers
// to base-relative offsets. The Get* accessors index the (host-strided) live arrays.
// =============================================================================

namespace BrnPhysics
{
namespace Props
{
    // FixDown @ 0x8267DCD0 (store-for-store). Serialise-out fix-up: rebase every embedded
    // pointer from an absolute address to a base-relative offset by subtracting the resource's
    // base data address. The base word is the first field of the passed rw::Resource
    // (lwz r8,0(r4) => resource offset 0; on the X360 GetMemoryResource was inlined to this word).
    // Every subtract is null-preserving exactly as the asm (beq guards). The X360 walks each
    // PropGraphics element (stride 12) rebasing its two embedded pointers (mpPropModel @+4,
    // mpParts @+8), then rebases the two top-level table pointers.
    void PropGraphicsList::FixDown(const rw::Resource& lrBaseResource)
    {
        // v3 = *a2: the resource's base data address (first base-resource word of rw::Resource).
        const ptrdiff_t liBase =
            reinterpret_cast<ptrdiff_t>(lrBaseResource.m_baseResources[0]);

        // Walk each PropGraphics (stride 12), rebasing its embedded pointers (preserve null).
        if ( muNumberOfPropModels != 0 )
        {
            u32 luIndex = 0;
            char* lpElementBase = reinterpret_cast<char*>(mpaPropGraphics);
            do
            {
                // PropGraphics::mpPropModel @ +4
                char**    lppPropModel = reinterpret_cast<char**>(lpElementBase + 4);
                ptrdiff_t liPropModel  = reinterpret_cast<ptrdiff_t>(*lppPropModel);
                if ( liPropModel != 0 )
                    *lppPropModel = reinterpret_cast<char*>(liPropModel - liBase);

                // PropGraphics::mpParts @ +8
                char**    lppParts = reinterpret_cast<char**>(lpElementBase + 8);
                ptrdiff_t liParts  = reinterpret_cast<ptrdiff_t>(*lppParts);
                if ( liParts != 0 )
                    *lppParts = reinterpret_cast<char*>(liParts - liBase);

                ++luIndex;
                lpElementBase += 12;   // X360 sizeof(PropGraphics)
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
