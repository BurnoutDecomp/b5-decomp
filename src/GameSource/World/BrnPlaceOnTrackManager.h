#ifndef BRN_WORLD_PLACE_ON_TRACK_MANAGER_H
#define BRN_WORLD_PLACE_ON_TRACK_MANAGER_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4

// =============================================================================
// BrnWorld::PlaceOnTrackManager
//   GameSource/World/BrnPlaceOnTrackManager.{h,cpp}
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The ONE recovered function is
// ComputeBestPlaceOnT @ 0x822BE238 — given a query point and a list of candidate
// "place on track" nodes, it selects the nearest node, optionally restricted to nodes
// that pass a height test.
//
// LAYOUT NOTE / FLAG (modelled from the access pattern, no DWARF): the candidate list
// and node types have NO DWARF/leak home. Their shape is recovered ENTIRELY from the
// X360 access pattern in ComputeBestPlaceOnT:
//   * the list (`a2`) holds a node count at +0x04 and an inline node array at +0x10;
//   * each node is 64 bytes (`_R11 += 0x40` per iteration);
//   * node+0x00 is the position (a 16-byte vector, read with `lvx128`);
//   * node+0x30 holds a 16-bit flag word; bit 14 (`>> 14 & 1`) is the height-filter
//     gate (when the `a3` filter is on, a node is considered only if bit 14 is clear).
// These offsets are X360-attested by the loads; the FIELD NAMES are descriptive (the
// source names are unknown). Absolute offsets are NOT static_asserted (host vs X360
// pointer/vector widths differ); the 64-byte node stride is reproduced via the array
// element type. FLAG: PlaceOnTrackCandidate / PlaceOnTrackCandidateList are modelled
// minimally from the access pattern — grow them additively when their own TU lands.
// =============================================================================

namespace BrnWorld
{

// One candidate place-on-track node (64-byte stride, per the X360 loop).
struct PlaceOnTrackCandidate
{
    Vector4 mPosition;        // +0x00  node position (read via lvx128)
    u8      maReserved10[32]; // +0x10..+0x2F  (untouched by ComputeBestPlaceOnT)
    u16     muFlags;          // +0x30  flag word; bit 14 = height-filter gate
    u8      maReserved32[14]; // +0x32..+0x3F  pad to the 64-byte node stride
};

// The candidate list: a count and an inline node array. (X360 list base `a2`:
// muNumCandidates @ +0x04, maCandidates @ +0x10.)
struct PlaceOnTrackCandidateList
{
    u8                    maReserved00[4];  // +0x00 (untouched here)
    s32                   muNumCandidates;  // +0x04
    u8                    maReserved08[8];  // +0x08..+0x0F (untouched here)
    PlaceOnTrackCandidate maCandidates[1];  // +0x10  (flexible inline array)
};

// Bit 14 of PlaceOnTrackCandidate::muFlags. When the height filter is requested, a
// candidate is eligible only if this bit is CLEAR.
const u16 KU_PLACE_ON_TRACK_FILTER_BIT = (1u << 14);

class PlaceOnTrackManager
{
public:
    // ComputeBestPlaceOnT @ 0x822BE238 — pick the best candidate for lrQuery.
    //
    //   * lbApplyHeightFilter: when true, candidates whose filter bit (muFlags bit 14)
    //     is set are excluded from consideration; when false every candidate is
    //     eligible.
    //   * Returns the nearest eligible candidate that ALSO passes the height test if
    //     one exists; otherwise the nearest eligible candidate; otherwise nullptr
    //     (empty list / nothing eligible).
    //
    // The X360 ranks candidates by squared distance from the query point (the
    // `vmsum3fp128` 3-lane dot product of (node - query)). The height test compares
    // (query.y + 1.0) > node.y (the +1.0 tolerance is the shared rodata constant
    // flt_82001C98, attested as the literal 1.0f elsewhere) and only narrows the
    // preferred (height-passing) pick among eligible candidates.
    const PlaceOnTrackCandidate* ComputeBestPlaceOnT(
        const PlaceOnTrackCandidateList& lrCandidates,
        const Vector4& lrQuery,
        bool lbApplyHeightFilter) const;
};

} // namespace BrnWorld

#endif // BRN_WORLD_PLACE_ON_TRACK_MANAGER_H
