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

#include "SharedClasses/Traffic/BrnTrafficSection.h"   // real Section/LaneRung (must precede Hull.h)
#include "SharedClasses/Traffic/BrnTrafficHull.h"
#include <cstdint>                                     // uintptr_t

namespace BrnTraffic
{
    // -------------------------------------------------------------------------
    // Hull::FixUp @0x827620A0 / Hull::FixDown @0x827622E0.
    //
    // Twelve consecutive pointer slots (X360 +0x10 .. +0x3C) rebased against the resource
    // block base, unconditionally -- an empty per-hull array serialises as a one-past-the-end
    // offset, never as null, so there is nothing to guard. The X360 walks NO sub-elements:
    // Section / LaneRung / Neighbour / SectionSpan / SectionFlow / StopLine /
    // StaticTrafficVehicle / LightTrigger / LightTriggerStartData / JunctionLogicBox hold no
    // pointers at all (their FixUp bodies are empty on console), and Section::muRungOffset /
    // muNeighbourOffset / muStopLineOffset are INDICES into the hull's own tables, not
    // pointers -- they must not be relocated or widened.
    //
    // Asserts, in the asm's order: muNumJunctions < 16 (BrnTrafficHull.cpp:143), then the two
    // 16-byte alignment guards on the rung tables (cpp:146 / :147). FixDown checks the same
    // two on the offset form (cpp:259 / :260) and rebases in the same slot order.
    // -------------------------------------------------------------------------
    namespace
    {
        template <typename T>
        inline void Rebase(T*& lrpPointer, uintptr_t luBase)
        {
            lrpPointer = reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(lrpPointer) + luBase);
        }

        template <typename T>
        inline void UnBase(T*& lrpPointer, uintptr_t luBase)
        {
            lrpPointer = reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(lrpPointer) - luBase);
        }

        inline bool Is16Aligned(const void* lpAddress)
        {
            return (reinterpret_cast<uintptr_t>(lpAddress) & 0xFu) == 0u;
        }

        static const u32 KU_MAX_JUNCTIONS_PER_HULL = 16;
    }

    void Hull::FixUp(const void* lpBaseData)
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);

        Rebase(mpaSections, luBase);
        Rebase(mpaRungs, luBase);
        Rebase(mpafCumulativeRungLengths, luBase);
        Rebase(mpaNeighbourData, luBase);
        Rebase(mpaSectionSpans, luBase);
        Rebase(mpaStaticTrafficVehicles, luBase);
        Rebase(mpaSectionFlows, luBase);
        Rebase(mpaJunctions, luBase);
        Rebase(mpaStopLines, luBase);
        Rebase(mpaLightTriggers, luBase);
        Rebase(mpaLightTriggerStartData, luBase);
        Rebase(mpaLightTriggerJunctionLookup, luBase);

        CGS_ASSERT(muNumJunctions < KU_MAX_JUNCTIONS_PER_HULL, "Hull has too many junctions");
        CGS_ASSERT(Is16Aligned(mpaRungs), "Rungs are not aligned");
        CGS_ASSERT(Is16Aligned(mpafCumulativeRungLengths), "Cumulative rung lengths are not aligned");
    }

    void Hull::FixDown(const void* lpBaseData)
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);

        CGS_ASSERT(Is16Aligned(mpaRungs), "Rungs are not aligned");
        CGS_ASSERT(Is16Aligned(mpafCumulativeRungLengths), "Cumulative rung lengths are not aligned");

        UnBase(mpaSections, luBase);
        UnBase(mpaRungs, luBase);
        UnBase(mpafCumulativeRungLengths, luBase);
        UnBase(mpaNeighbourData, luBase);
        UnBase(mpaSectionSpans, luBase);
        UnBase(mpaStaticTrafficVehicles, luBase);
        UnBase(mpaSectionFlows, luBase);
        UnBase(mpaJunctions, luBase);
        UnBase(mpaStopLines, luBase);
        UnBase(mpaLightTriggers, luBase);
        UnBase(mpaLightTriggerStartData, luBase);
        UnBase(mpaLightTriggerJunctionLookup, luBase);
    }

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
