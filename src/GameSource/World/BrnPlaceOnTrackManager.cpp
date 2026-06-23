#include "GameSource/World/BrnPlaceOnTrackManager.h"

#include <cfloat>   // FLT_MAX

// =============================================================================
// BrnWorld::PlaceOnTrackManager — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnPlaceOnTrackManager.h for the
// candidate-list/node shape recovered from the access pattern.
//
// dep_flags: none un-homed. Self-contained (math only).
// =============================================================================

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// ComputeBestPlaceOnT  @ 0x822BE238
//
// Store-for-store reconstruction of the X360 loop. The X360 is hand-vectorised but
// each VMX step is a simple per-lane operation whose result is consumed as a scalar,
// so the loop lowers faithfully to scalar C++:
//
//   bestDist = bestFilteredDist = FLT_MAX;      // f13 / f12 = flt_8201442C (FLT_MAX)
//   best = bestFiltered = 0;                    // r3  / r6
//   n = list.muNumCandidates;                   // *(a2+4)
//   if (n <= 0) return 0;
//   for each node (stride 0x40, first at a2+0x10):
//       sqDist = dot3(node.pos - query, node.pos - query);   // vsubfp + vmsum3fp128
//       eligible = filterOff ? true : ((node.muFlags >> 14) & 1) == 0;
//       if (eligible) {
//           heightPass = (query.y + 1.0f) > node.pos.y;       // vcmpgtfp, +1.0 = flt_82001C98
//           if (heightPass && sqDist < bestFilteredDist) {
//               bestFilteredDist = sqDist; bestFiltered = node;
//           }
//           if (sqDist < bestDist) { bestDist = sqDist; best = node; }
//       }
//   }
//   if (bestFiltered) return bestFiltered;
//   if (!best)        return 0;
//   return best;
//
// Notes:
//  * `dot3` is the X360 vmsum3fp128 of the (node - query) delta with itself = the
//    squared 3D distance. The query's vector is whatever lanes the caller passes; the
//    height test uses lane 1 (the .y component) of both query and node.
//  * The +1.0f height tolerance is the shared rodata constant flt_82001C98, attested
//    as the literal 1.0f at every other use site (see BrnStaticSoundMap.cpp).
//  * FLT_MAX is the rodata constant flt_8201442C (3.4028235e38).
//  * `bestFiltered` ranks only the filter-bit-eligible candidates that also pass the
//    height test; `best` ranks all filter-bit-eligible candidates. The filtered pick
//    wins when present.
//  * Comparisons use `<` for the nearest update and `>` for the height test, matching
//    the X360 `bge`/`bge` skip branches (NaN/equal keep the incumbent).
// ---------------------------------------------------------------------------
const PlaceOnTrackCandidate* PlaceOnTrackManager::ComputeBestPlaceOnT(
    const PlaceOnTrackCandidateList& lrCandidates,
    const Vector4& lrQuery,
    bool lbApplyHeightFilter) const
{
    const PlaceOnTrackCandidate* lpBest         = 0;
    const PlaceOnTrackCandidate* lpBestFiltered = 0;
    f32 lfBestDist         = FLT_MAX;
    f32 lfBestFilteredDist = FLT_MAX;

    const s32 liCount = lrCandidates.muNumCandidates;
    if (liCount <= 0)
    {
        return 0;
    }

    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        const PlaceOnTrackCandidate& lrNode = lrCandidates.maCandidates[liIndex];

        // Squared 3D distance: dot3(node.pos - query).
        const f32 lfDx = lrNode.mPosition.x - lrQuery.x;
        const f32 lfDy = lrNode.mPosition.y - lrQuery.y;
        const f32 lfDz = lrNode.mPosition.z - lrQuery.z;
        const f32 lfSqDist = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;

        const bool lbEligible =
            lbApplyHeightFilter ? ((lrNode.muFlags & KU_PLACE_ON_TRACK_FILTER_BIT) == 0)
                                : true;

        if (lbEligible)
        {
            const bool lbHeightPass = (lrQuery.y + 1.0f) > lrNode.mPosition.y;

            if (lbHeightPass && lfSqDist < lfBestFilteredDist)
            {
                lfBestFilteredDist = lfSqDist;
                lpBestFiltered     = &lrNode;
            }

            if (lfSqDist < lfBestDist)
            {
                lfBestDist = lfSqDist;
                lpBest     = &lrNode;
            }
        }
    }

    if (lpBestFiltered)
    {
        return lpBestFiltered;
    }
    if (!lpBest)
    {
        return 0;
    }
    return lpBest;
}

} // namespace BrnWorld
