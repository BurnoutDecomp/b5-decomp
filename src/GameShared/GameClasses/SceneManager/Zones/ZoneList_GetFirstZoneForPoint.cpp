#include "GameShared/GameClasses/SceneManager/Zones/ZoneList.h"

// CgsSceneManager::ZoneList::GetFirstZoneForPoint, reconstructed from
// BURNOUT_X360_ARTIST.XEX (X360 0x822BADA8). This TU homes the point-in-zone query that
// finds the first Zone containing a 2D point, given a hint start-zone index.
//
// Search order (store-for-store faithful to the asm):
//   * liStartZone < 0            -> skip straight to the full linear scan of every zone.
//   * otherwise                  -> (1) test the hint zone itself; on a hit return it.
//                                   (2) walk its SAFE neighbours; return the first whose
//                                       zone contains the point.
//                                   (3) walk its UNSAFE neighbours; likewise.
//                                   (4) fall through to the full linear scan.
//   * full scan                  -> test each zone in order (with a dcbt-style prefetch of
//                                    the next Zone on the console); return the first hit, or
//                                    nullptr if none contain the point.
//
// X360 notes:
//   - The test point arrives in a vector register (v1) and is re-broadcast (vmr128 v1,v127)
//     before every IsPointInZone call; modelled here by passing lPoint by value.
//   - Zone stride is 0x30 (== sizeof(Zone)); the asm indexes mpZones with 48*liStartZone.
//   - On a neighbour hit the asm re-fetches Get*Neighbour(zone, idx) then Neighbour::GetZone
//     rather than reusing the earlier temporary; reproduced verbatim below.
//   - Neighbour-loop counters are sign-extended to 16 bits each iteration; kept as s16.
//   - Safe/unsafe neighbour counts live at Zone +0x1C / +0x1E (attested by the committed
//     Zone::FixDown TU); the accessors read those members.

namespace CgsSceneManager
{
    // X360 0x822BADA8.
    const Zone* ZoneList::GetFirstZoneForPoint(rw::math::vpu::Vector2 lPoint, s32 liStartZone) const
    {
        if (liStartZone >= 0)
        {
            const Zone* lpStartZone = &mpZones[liStartZone];

            // (1) The hint zone itself.
            if (lpStartZone->IsPointInZone(lPoint))
            {
                return lpStartZone;
            }

            // (2) Its safe neighbours.
            for (s16 liSafeIndex = 0; liSafeIndex < mpZones[liStartZone].GetNumSafeNeighbours(); ++liSafeIndex)
            {
                const Neighbour* lpSafeNeighbour = mpZones[liStartZone].GetSafeNeighbour(liSafeIndex);
                const Zone*      lpNeighbourZone = lpSafeNeighbour->GetZone();
                if (lpNeighbourZone->IsPointInZone(lPoint))
                {
                    return mpZones[liStartZone].GetSafeNeighbour(liSafeIndex)->GetZone();
                }
            }

            // (3) Its unsafe neighbours.
            for (s16 liUnsafeIndex = 0; liUnsafeIndex < mpZones[liStartZone].GetNumUnsafeNeighbours(); ++liUnsafeIndex)
            {
                const Neighbour* lpUnsafeNeighbour = mpZones[liStartZone].GetUnsafeNeighbour(liUnsafeIndex);
                const Zone*      lpNeighbourZone   = lpUnsafeNeighbour->GetZone();
                if (lpNeighbourZone->IsPointInZone(lPoint))
                {
                    return mpZones[liStartZone].GetUnsafeNeighbour(liUnsafeIndex)->GetZone();
                }
            }
        }

        // (4) Full linear scan of every zone.
        for (u32 luZoneIndex = 0; luZoneIndex < muTotalZones; ++luZoneIndex)
        {
            const Zone* lpZone = &mpZones[luZoneIndex];
            if (lpZone->IsPointInZone(lPoint))
            {
                return lpZone;
            }
        }

        return nullptr;
    }
}
