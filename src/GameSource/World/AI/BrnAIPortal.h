#pragma once

// BrnAI::Portal -- an AI-section portal: a 3D position plus the set of boundary
// lines (the section's drivable gaps) the racing-line / reset-on-track code walks
// across. DWARF home: SharedClasses/AI/AISectionsData.h (struct BrnAI::Portal,
// AISectionsData.h:140 in the DecFIGS dump; declared lines :276-285). Forward-
// declared by AISectionsResourceType.h (AISection::mpaPortals); this header owns the
// full Portal layout.
//
// LAYOUT (DecFIGS DWARF, AISectionsData.h:276-285; offsets confirmed against the
// X360 asm of GetBoundaryLine @0x82764480 -- mu8NumBoundaryLines read at this+0x12,
// mpaBoundaryLines read at this+0xC). 20 bytes total. The DWARF spells the position
// fields float32_t; the project scalar is f32 (same width). Access is by name.
//
// Only GetBoundaryLine (@0x82764480) is bodied in this TU; the remaining members
// (FixUp/FixDown, the position getters/setters, SetNumBoundaryLines, ...) are
// declared here and bodied in their own TUs.

#include "types.hpp"
#include "BrnCommonTypes.h"                       // Vector2 / Vector3
#include "GameSource/World/AI/BrnAIBoundaryLine.h" // BrnAI::BoundaryLine (16-byte line)

namespace rw { class Resource; }

namespace BrnAI
{
    // DWARF AISectionsData.h:140. 20-byte stride.
    struct Portal
    {
        // @0x82764480 -- return the lu8BoundryIndex'th boundary line of this portal.
        // Asserts the index is in range (CGS_ASSERT folds the original dev-assert),
        // then returns &mpaBoundaryLines[lu8BoundryIndex]. Called by
        // AIDebugComponent::DrawChevrons and
        // ResetOnTrackManager::ConvertNodesToPositionAndDirection.
        const BoundaryLine* GetBoundaryLine(u8 lu8BoundryIndex) const;

        // Load-time pointer relocation of mpaBoundaryLines. On the console this is INLINED
        // into AISection::FixUp @0x8267D8C8 / FixDown @0x8267D978 (under a null guard, and
        // with an empty per-line inner loop because BoundaryLine holds no pointers); the
        // DWARF still declares the pair (:236 / :240), so it is de-inlined here and bodied
        // in BrnAIPortal.cpp.
        // ⚠️ The base is passed as a plain block pointer rather than the DWARF's
        // MemoryResource/rw::Resource, matching the Traffic relocation path: the x64 lane
        // data carries 64-bit slots, so nothing may narrow the base to 32 bits.
        void          FixUp(const void* lpBaseData);                   // :240
        void          FixDown(const void* lpBaseData);                 // :236
        u8            GetNumBoundryLines() const;                      // :243
        Vector3       GetPosition() const;                            // :250
        Vector2       GetPosition2D() const;                          // :253
        f32           GetPositionX() const;                            // :256
        f32           GetPositionY() const;                            // :259
        f32           GetPositionZ() const;                            // :262
        void          SetPosition(Vector3 lPosition);                  // :266
        void          SetNumBoundaryLines(u8 lu8NumBoundaryLines);     // :270
        u16           GetLinkSectionIndex() const;                     // :273

    private:
        f32           mPositionX;          // +0x00  :276
        f32           mPositionY;          // +0x04  :277
        f32           mPositionZ;          // +0x08  :278
        BoundaryLine* mpaBoundaryLines;    // +0x0C  :280
        u16           mu16LinkSection;     // +0x10  :282
        u8            mu8NumBoundaryLines; // +0x12  :284
        u8            mau8Pad[1];          // +0x13  :285 (tail pad to 20 bytes)
    };

    // Host layout contract with tools/assets/bundles/lane_transcode.py's emitter: the one
    // pointer widens 4->8, so the console's 20-byte record becomes 32 bytes on the host and
    // the two trailing scalars shift with it. (X360: mpaBoundaryLines +0x0C,
    // mu8NumBoundaryLines +0x12, sizeof 20 -- confirmed by GetBoundaryLine @0x82764480 and
    // AISection::GetPortal @0x8230F5D0's *20 index maths.)
    static_assert(sizeof(Portal) == 0x20, "Portal host sizeof (X360 20 -> widened 32)");
}
