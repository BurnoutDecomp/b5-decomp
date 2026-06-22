#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTraffic::HullRuntime::Construct              @ 0x82751428
//   BrnTraffic::HullRuntime::Prepare                @ 0x82751438
//   BrnTraffic::HullRuntime::Release                @ 0x82751578
//   BrnTraffic::HullRuntime::GetFirstParamInSection @ 0x82706768
//   BrnTraffic::HullRuntime::IsStoplineRed          @ 0x827068A0

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnTraffic
{
    static const u16 KU_INVALID_PARAM = 65535;

    struct JunctionLogicBox
    {
        u8 GetNumStates() const;
    };

    struct Hull
    {
        u8 muNumSections;
        u8 muNumSectionSpans;
        u8 muNumJunctions;
        u8 muNumStoplines;
        u8 muNumNeighbours;
        u8 muNumStaticTraffic;
        u8 muNumVehicleAssets;
        u8 maPad7;
        u16 muNumRungs;
        u16 muFirstTrafficLight;
        u16 muLastTrafficLight;
        u8 muNumLightTriggers;
        u8 muNumLightTriggersStartData;
        void* mpaSections;
        void* mpaRungs;
        f32* mpafCumulativeRungLengths;
        void* mpaNeighbourData;
        void* mpaSectionSpans;
        void* mpaStaticTrafficVehicles;
        void* mpaSectionFlows;
        JunctionLogicBox* mpaJunctions;
    };

    struct HullRuntime
    {
        void Construct();
        void Prepare(const Hull* lpHull, u16 luHull);
        void Release();

        // X360 @ 0x82706768 / 0x827068A0 -- bounds-asserted per-hull lookups.
        u16  GetFirstParamInSection(u32 luSection) const;
        bool IsStoplineRed(u32 luStopline) const;

        f32 mafJunctionStateChangeTimes[16];
        u8 mauJunctionCurrentStates[16];
        u16 mauFirstParamInSection[256];
        bool mabStoplineRedState[64];
        u16 mauSectionSpanVehicleCount[256];
        u16 muHullIndex;
        bool mbPrepared;
        u8 muNumSectionsInHull;
        u8 muNumStoplinesInHull;
    };

    void HullRuntime::Construct()
    {
        mbPrepared = false;
    }

    void HullRuntime::Prepare(const Hull* lpHull, u16 luHull)
    {
        CGS_ASSERT(lpHull != 0, "lpHull");
        CGS_ASSERT(mbPrepared == false, "mbPrepared == false");

        for (u32 luStopline = 0; luStopline < lpHull->muNumStoplines; ++luStopline)
        {
            mabStoplineRedState[luStopline] = false;
        }

        for (u32 luSection = 0; luSection < lpHull->muNumSections; ++luSection)
        {
            mauFirstParamInSection[luSection] = KU_INVALID_PARAM;
        }

        for (u32 luJunction = 0; luJunction < lpHull->muNumJunctions; ++luJunction)
        {
            mafJunctionStateChangeTimes[luJunction] = 0.0f;
            mauJunctionCurrentStates[luJunction] =
                static_cast<u8>(lpHull->mpaJunctions[luJunction].GetNumStates() - 1);
        }

        mbPrepared = true;
        muHullIndex = luHull;
        muNumSectionsInHull = lpHull->muNumSections;
        muNumStoplinesInHull = lpHull->muNumStoplines;
    }

    void HullRuntime::Release()
    {
        CGS_ASSERT(mbPrepared == true, "mbPrepared == true");
        mbPrepared = false;
    }

    // BrnTrafficHullRuntime.h:151-152 -- the section's first-param head index.
    // X360 @ 0x82706768: assert mbPrepared (line 151), bounds-assert the section
    // against muNumSectionsInHull (line 152), then return mauFirstParamInSection
    // [luSection] (the asm reads the u16 at this + 2*(luSection+40), and the table
    // base is at byte +0x50 == 2*40, so the index math is mauFirstParamInSection
    // [luSection]). The X360 bounds assert builds a rich streamed message
    // ("Out of range section in hull runtime for hull <i>, section was <n>, max is
    // <m>") via the out-of-line CgsDev debug-stream operators (un-homed, not owned
    // by this group); the faithful stringized-condition CGS_ASSERT carries the same
    // guard semantics and matches the surrounding TU style.
    u16 HullRuntime::GetFirstParamInSection(u32 luSection) const
    {
        CGS_ASSERT(mbPrepared, "mbPrepared");
        CGS_ASSERT(luSection < muNumSectionsInHull, "luSection < muNumSectionsInHull");
        return mauFirstParamInSection[luSection];
    }

    // BrnTrafficHullRuntime.h:161-162 -- whether the stopline's light is red.
    // X360 @ 0x827068A0: assert mbPrepared (line 161), bounds-assert the stopline
    // against muNumStoplinesInHull (line 162), then return mabStoplineRedState
    // [luStopline] (the asm reads the byte at this + 0x250 + luStopline, and the
    // table base is at byte +0x250, so the index is mabStoplineRedState[luStopline]).
    // Same rich-vs-stringized assert note as GetFirstParamInSection above.
    bool HullRuntime::IsStoplineRed(u32 luStopline) const
    {
        CGS_ASSERT(mbPrepared, "mbPrepared");
        CGS_ASSERT(luStopline < muNumStoplinesInHull, "luStopline < muNumStoplinesInHull");
        return mabStoplineRedState[luStopline];
    }
}
