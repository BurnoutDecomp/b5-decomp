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

        // Declared-only members (bodied in their own TUs); listed so the public
        // surface matches the DWARF declarations.
        void          FixDown(void* lpMemoryResource);                 // :236
        void          FixUp(const rw::Resource& lrResource);           // :240
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
}
