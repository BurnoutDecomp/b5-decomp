// =============================================================================
// BrnTrafficHull.cpp  (owning .cpp for the BrnTraffic::Hull accessor family)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The per-hull traffic-graph block
// BrnTraffic::Hull is laid out in BrnTrafficHull.h; its bounds-checked element
// accessors are inline there. This .cpp is the definition home that forces the
// accessors out-of-line so the TU has a real object-code presence (and is the
// landing site for any future non-inline Hull bodies).
//
// Owned accessors (brn-traffic3 group):
//   BrnTraffic::Hull::GetNeighbour      @ 0x821F5358
//   BrnTraffic::Hull::GetStaticVehicle  @ 0x82705C90
//   BrnTraffic::Hull::GetStopLine       @ 0x82705C20
// (GetSection @ 0x821F52E0 is owned by an earlier slice; defined inline in the
//  header and not re-emitted here.)
// =============================================================================

#include "SharedClasses/Traffic/BrnTrafficHull.h"

namespace BrnTraffic
{
    // Out-of-line forwarders pin the three inline accessors into this TU. They
    // are the exact bodies attested by the X360 asm (raw element strides 4/80/2).
    const void* Hull_GetNeighbour(const Hull& lrHull, u32 luIndex)
    {
        return lrHull.GetNeighbour(luIndex);
    }

    const void* Hull_GetStaticVehicle(const Hull& lrHull, u32 luIndex)
    {
        return lrHull.GetStaticVehicle(luIndex);
    }

    const void* Hull_GetStopLine(const Hull& lrHull, u32 luIndex)
    {
        return lrHull.GetStopLine(luIndex);
    }
}
