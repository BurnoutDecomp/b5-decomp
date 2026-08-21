#ifndef BRN_TRAFFIC_HULL_RUNTIME_H
#define BRN_TRAFFIC_HULL_RUNTIME_H

// ============================================================================
// BrnTraffic::HullRuntime -- OWNING HEADER.
//
// The mutable per-ACTIVE-hull scratch the traffic module keeps beside the immutable
// streamed Hull record: which param heads each section's list, which stoplines are red,
// where each junction's light cycle has got to, and how many vehicles occupy each section
// span. TrafficEntityModule owns 72 of these (KU_MAX_ACTIVE_HULLS) and indirects into them
// through mauHullRuntimeDataIndices.
//
// Member list, order and names: DecFIGS DWARF BrnTrafficHullRuntime.h:96..:105.
// Offsets are X360-attested and safe to pin absolutely because the record is POINTER-FREE
// (five plain arrays plus four scalars), so console and host agree:
//   +0x000 mafJunctionStateChangeTimes[16]  (Prepare @0x82751438)
//   +0x040 mauJunctionCurrentStates[16]     (Prepare)
//   +0x050 mauFirstParamInSection[256]      SetFirstParamInSection @0x82706408 indexes
//                                           (luSection + 0x28)*2 == 0x50 + 2*luSection
//   +0x250 mabStoplineRedState[64]          SetStoplineRed @0x82706630 `stb r25, 0x250(r11)`
//   +0x290 mauSectionSpanVehicleCount[256]  (ship-only; absent from the Feb-2007 leak)
//   +0x490 muHullIndex                      SetFirstParamInSection `lhz r4, 0x490(r31)`
//   +0x492 mbPrepared                       `lbz r11, 0x492(...)` in both setters
//   +0x493 muNumSectionsInHull              `lbz r11, 0x493(r31)` bounds test
//   +0x494 muNumStoplinesInHull             `lbz r11, 0x494(r29)` bounds test
//   sizeof == 0x498 == 1176. (The leak's CheckClassSize pins ITS build at 664, i.e. exactly
//   1176 - 512: mauSectionSpanVehicleCount is the whole ship delta.)
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnTraffic
{
    class Hull;

    class HullRuntime
    {
    public:
        // Table widths. The first three carry their Feb-2007 spelling from that build's
        // BrnTrafficSharedConstants.h; the fourth is a ship addition. They live here because
        // this tree's BrnTrafficSharedConstants.h does not carry them yet.
        static const u32 KU_MAX_JUNCTIONS_PER_HULL     = 16;
        static const u32 KU_MAX_SECTIONS_PER_HULL      = 256;
        static const u32 KU_MAX_STOP_LINES_PER_HULL    = 64;
        static const u32 KU_MAX_SECTION_SPANS_PER_HULL = 256;

        // DWARF :54 / :60 / :64 / :68. Construct/Prepare/Release are X360-attested at
        // 0x82751428 / 0x82751438 / 0x82751578; Destruct is inlined everywhere on ARTIST
        // and is ported from the Feb-2007 body (a lone mbPrepared==false assert).
        void Construct();
        void Prepare(const Hull* lpHull, u16 luHull);
        void Release();
        void Destruct();

        // DWARF :72 / :73 -- X360 @0x82706408 / @0x82706630.
        void SetFirstParamInSection(u32 luSection, u16 luParam, u16 luOldParam);
        void SetStoplineRed(u32 luStopline, bool lbRed);

        // DWARF :75 / :76 -- X360 @0x82706768 / @0x827068A0.
        u16  GetFirstParamInSection(u32 luSection) const;
        bool IsStoplineRed(u32 luStopline) const;

        // DWARF :79 / :80 / :81. Not in the ARTIST ledger; the console inlines each at its
        // call site (the junction update reads HullRuntime+0x00 and +0x40 directly). The
        // mbPrepared asserts come from the Feb-2007 original.
        f32* GetJunctionStateChangeTimes()
        {
            CGS_ASSERT(mbPrepared, "mbPrepared");
            return mafJunctionStateChangeTimes;
        }
        u8* GetJunctionCurrentStates()
        {
            CGS_ASSERT(mbPrepared, "mbPrepared");
            return mauJunctionCurrentStates;
        }
        const u8* GetJunctionCurrentStates() const
        {
            CGS_ASSERT(mbPrepared, "mbPrepared");
            return mauJunctionCurrentStates;
        }

        // DWARF :84 / :88 / :92 -- the ship-only section-span occupancy counters, inlined
        // throughout ARTIST and so absent from the ledger. The array is attested by the
        // 1176-byte record size and by the 0x290 base the traffic logger hashes.
        void ResetSectionSpanVehicleCounts()
        {
            CGS_ASSERT(mbPrepared, "mbPrepared");
            for (u32 luSpan = 0; luSpan < KU_MAX_SECTION_SPANS_PER_HULL; ++luSpan)
            {
                mauSectionSpanVehicleCount[luSpan] = 0;
            }
        }
        void IncrementSectionSpanVehicleCount(u32 luSpan)
        {
            CGS_ASSERT(mbPrepared, "mbPrepared");
            CGS_ASSERT(luSpan < KU_MAX_SECTION_SPANS_PER_HULL,
                       "luSpan < KU_MAX_SECTION_SPANS_PER_HULL");
            ++mauSectionSpanVehicleCount[luSpan];
        }
        u16 GetSectionSpanVehicleCount(u32 luSpan) const
        {
            CGS_ASSERT(mbPrepared, "mbPrepared");
            CGS_ASSERT(luSpan < KU_MAX_SECTION_SPANS_PER_HULL,
                       "luSpan < KU_MAX_SECTION_SPANS_PER_HULL");
            return mauSectionSpanVehicleCount[luSpan];
        }

        static void _AssertLayout();   // never called; body in the .cpp

    private:
        f32  mafJunctionStateChangeTimes[KU_MAX_JUNCTIONS_PER_HULL];      // :96   +0x000
        u8   mauJunctionCurrentStates[KU_MAX_JUNCTIONS_PER_HULL];         // :97   +0x040
        u16  mauFirstParamInSection[KU_MAX_SECTIONS_PER_HULL];            // :98   +0x050
        bool mabStoplineRedState[KU_MAX_STOP_LINES_PER_HULL];             // :99   +0x250
        u16  mauSectionSpanVehicleCount[KU_MAX_SECTION_SPANS_PER_HULL];   // :100  +0x290
        u16  muHullIndex;                                                 // :102  +0x490
        bool mbPrepared;                                                  // :103  +0x492
        u8   muNumSectionsInHull;                                         // :104  +0x493
        u8   muNumStoplinesInHull;                                        // :105  +0x494
    };
}

#endif // BRN_TRAFFIC_HULL_RUNTIME_H
