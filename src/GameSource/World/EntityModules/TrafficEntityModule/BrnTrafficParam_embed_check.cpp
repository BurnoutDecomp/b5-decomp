// Embed-check / compile tripwire for BrnTraffic::Param (11 funcs) + ParamSoaData.
// Pins the X360 byte layout by offset and forces every body to instantiate. Not a unit
// test -- a compile/link guard for this TU.
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"
#include <cstddef>

namespace
{
    using BrnTraffic::Param;
    using BrnTraffic::ParamPlan;
    using BrnTraffic::ParamSoaData;

    // Layout pins (asm STORE offsets).
    static_assert(offsetof(Param, muHullIndex) == 0x00, "muHullIndex @0x00");
    static_assert(offsetof(Param, muSectionIndex) == 0x02, "muSectionIndex @0x02");
    static_assert(offsetof(Param, muCurrentSegment) == 0x03, "muCurrentSegment @0x03");
    static_assert(offsetof(Param, mfParamAlong) == 0x04, "mfParamAlong @0x04");
    static_assert(offsetof(Param, maPlans) == 0x08, "maPlans @0x08");
    static_assert(offsetof(Param, mfSpeed) == 0x14, "mfSpeed @0x14");
    static_assert(offsetof(Param, mxEffectAndHistoryState) == 0x1A, "mxEffectAndHistoryState @0x1A");
    static_assert(offsetof(Param, miBehaviour) == 0x1B, "miBehaviour @0x1B");
    static_assert(offsetof(Param, mxFlags) == 0x40, "mxFlags @0x40");
    static_assert(offsetof(Param, muCurrentSectionDirection) == 0x41, "muCurrentSectionDirection @0x41");
    static_assert(offsetof(Param, mauHistorySegments) == 0x4C, "mauHistorySegments @0x4C");
    static_assert(offsetof(Param, mauHistoryHulls) == 0x58, "mauHistoryHulls @0x58");
    static_assert(offsetof(Param, muNextHistoryToWrite) == 0x64, "muNextHistoryToWrite @0x64");

    static_assert(sizeof(ParamPlan) == 6, "ParamPlan stride is 6 bytes");

    // ParamSoaData: three FastBitArray<601> at 0x00 / 0x50 / 0xA0.
    static_assert(offsetof(ParamSoaData, mAliveParams) == 0x00, "mAliveParams @0x00");
    static_assert(offsetof(ParamSoaData, mDyingParams) == 0x50, "mDyingParams @0x50");
    static_assert(offsetof(ParamSoaData, mZombieParams) == 0xA0, "mZombieParams @0xA0");

    int ExerciseParam()
    {
        Param lParam;
        ParamSoaData lSoa;
        lSoa.Construct();

        // Make the param live so the IsAlive() asserts pass at runtime.
        lParam.mxFlags = Param::E_FLAG_ALIVE;
        lParam.mxEffectAndHistoryState = 0;
        lParam.muNextHistoryToWrite = 0;
        lParam.miBehaviour = 1;
        lParam.mfSpeed = 0.0f;
        lParam.muCurrentSectionDirection = 0;
        lParam.maPlans[0].muType = ParamPlan::E_TYPE_NONE;
        lParam.maPlans[1].muType = ParamPlan::E_TYPE_NONE;

        lParam.SetParamAlong(1.5f);
        lParam.PushHistory(3u, 7u);

        u32 luSeg = 0;
        u32 luHull = 0;
        lParam.GetHistoryEntry(0u, &luSeg, &luHull);

        bool lbQueueing = lParam.IsQueueing();
        bool lbRight = lParam.ShouldBeIndicatingRight();

        lParam.SetChangedSection();
        lParam.SetShouldBeRemoved();

        const u32 luParam = 5;
        lSoa.mAliveParams.SetBit(luParam);
        lParam.SetZombie(luParam, lSoa);
        lParam.SetDyingState(luParam, lSoa);
        lParam.ClearDying(luParam, lSoa);

        lParam.SetInPurgatory(true);
        lParam.SetInPurgatory(false);

        return static_cast<int>(luSeg + luHull) + (lbQueueing ? 1 : 0) + (lbRight ? 2 : 0);
    }
}

int g_BrnTrafficParamEmbedCheck = ExerciseParam();
