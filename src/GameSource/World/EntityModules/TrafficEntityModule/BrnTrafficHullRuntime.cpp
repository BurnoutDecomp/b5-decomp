#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTraffic::HullRuntime::Construct @ 0x82751428
//   BrnTraffic::HullRuntime::Prepare   @ 0x82751438
//   BrnTraffic::HullRuntime::Release   @ 0x82751578

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
}
