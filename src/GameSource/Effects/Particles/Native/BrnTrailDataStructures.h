#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnTrailDataStructures.h
//
// BrnParticle::Native::TrailSegmentCollection -- the 16-segment strip one skid
// TrailEmitter writes. Each segment is two 16-byte lanes:
//
//   mPosition : xyz = the contact point lifted by kTrailHeightAdjustment,
//               w   = the skid STRENGTH the segment was laid with
//   mTangent  : xyz = Cross(direction-of-travel, contact normal) -- the half-
//                     width axis the renderer extrudes the quad along,
//               w   = the TIME the segment was laid (ages the mark)
//
// Which "plus" lane holds which scalar is pinned by the ARTIST asm of
// TrailEmitter::AddTrailSegment @0x8227A9E0: the current-time splat is
// vrlimi128'd into the TANGENT's w (0x8227ABF4 / 0x8227ACCC) and the skid-
// strength splat into the POSITION's w (0x8227AC08 / 0x8227ACEC), and
// TrailRenderer::Render @0x82295930 reads them back the same way (age from
// tangent.w @0x82295AEC-0x82295B18, alpha from position.w @0x82295AE8).
//
// DWARF AUTHORITY (DecFIGS BrnTrailDataStructures.h):
//   knMaxTrailSize = 16 (:33); struct TrailSegment { Vector3Plus mPosition;
//   Vector3Plus mTangent; } (:93-96); TrailSegment[16] maSegments (:99); the
//   Read/Write accessor set (:46-88). Console sizeof == 512, the stride the
//   TrailSystem double-buffer copies use (memcpy 49152 == 96 * 512).
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 / Vector3Plus (rw::math::vpu)

namespace BrnParticle
{
namespace Native
{
    // DWARF BrnTrailDataStructures.h:33 -- the segment capacity of one emitter.
    const s32 KN_MAX_TRAIL_SIZE = 16;

    struct TrailSegmentCollection
    {
        struct TrailSegment
        {
            Vector3Plus mPosition;   // +0x00  xyz contact point, w = skid strength
            Vector3Plus mTangent;    // +0x10  xyz half-width axis, w = time laid
        };

        // ---- accessors (DWARF :46-88). The X360 inlines every one of them as a
        // lvx128 / vrlimi128 (keep-the-other-lane insert) / stvx128 triplet, so a
        // Write*Position/Tangent keeps the existing w lane and Write*Time/Strength
        // rewrite only w. No bounds checks: the asm carries none, and
        // TrailEmitter::AddTrailSegment deliberately reads index -1 (see there).
        void WriteSegmentPosition(Vector3 lPosition, s32 lnIndex)
        {
            maSegments[lnIndex].mPosition.SetVector3(lPosition);
        }
        Vector3 ReadSegmentPosition(s32 lnIndex) const
        {
            return maSegments[lnIndex].mPosition.GetVector3();
        }
        void WriteSegmentTangent(Vector3 lTangent, s32 lnIndex)
        {
            maSegments[lnIndex].mTangent.SetVector3(lTangent);
        }
        Vector3 ReadSegmentTangent(s32 lnIndex) const
        {
            return maSegments[lnIndex].mTangent.GetVector3();
        }
        // DWARF types the scalar lanes as the broadcast VecFloat; the console splats
        // the f32 (vspltw) before the lane insert, so the scalar is the whole value.
        void WriteSegmentTime(f32 lfTime, s32 lnIndex)
        {
            maSegments[lnIndex].mTangent.SetPlus(lfTime);
        }
        f32 ReadSegmentTime(s32 lnIndex) const
        {
            return maSegments[lnIndex].mTangent.GetPlus();
        }
        void WriteSegmentStrength(f32 lfStrength, s32 lnIndex)
        {
            maSegments[lnIndex].mPosition.SetPlus(lfStrength);
        }
        f32 ReadSegmentStrength(s32 lnIndex) const
        {
            return maSegments[lnIndex].mPosition.GetPlus();
        }
        // Whole-segment (32-byte) copy between collections -- the X360 continuance
        // seed in TrailSystem::AddTrailSegment (four ld/std pairs @0x8228C4DC-0x8228C500).
        void CopySegmentFromCollection(s32 lnDestIndex, const TrailSegmentCollection* lpSource,
                                       s32 lnSourceIndex)
        {
            maSegments[lnDestIndex] = lpSource->maSegments[lnSourceIndex];
        }
        void CopyTangentToSegmentFromSegment(s32 lnDestIndex, s32 lnSourceIndex)
        {
            maSegments[lnDestIndex].mTangent = maSegments[lnSourceIndex].mTangent;
        }

        TrailSegment maSegments[KN_MAX_TRAIL_SIZE];   // +0x000 .. +0x200
    };
}
}
