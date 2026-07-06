#pragma once

// ===========================================================================
// CgsSceneManager::IntervalList  (+ ObjectToIntervalMap)
//   Home: GameShared/GameClasses/SceneManager/ContactGen/CgsIntervalList.{h,cpp}
//
// The per-axis sorted interval array of the sweep-and-prune broadphase. Each object
// contributes two Interval records: a min-role endpoint (mu16Flags = KI_MIN_INDEX) and a
// max-role endpoint (mu16Flags = KI_MAX_INDEX). ObjectToIntervalMap is the object ->
// {min slot, max slot} back-reference the sweeper keeps in sync as the array is sorted /
// compacted.
//
// Class shape + member names from the DecFIGS DWARF (CgsIntervalList.h:49/64/77/173-181).
// Member byte offsets pinned by the X360 asm (mutators load this+0/4/8/0xC):
//   +0x00  mpaIntervals            (Interval*)             lwz 0(this)
//   +0x04  mpaObjectToIntervalMap  (ObjectToIntervalMap*)  lwz 4(this)
//   +0x08  muNumIntervals          (u32)                   lwz 8(this)
//   +0x0C  muMaxNumIntervals       (u32)                   lwz 0xC(this)
// ===========================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"  // Vector3
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsInterval.h"  // Interval, OverlappingIntervalPair

namespace CgsSceneManager
{
    // Forward decls for the sweep surface (bodies not in this batch).
    class IntervalStack;
    struct OverlapPairQueue;

    // Sentinel object index stored on the terminating sentinel interval
    // (CgsIntervalList.cpp:28, KU_SENTINEL_OBJECT_INDEX = 65535).
    static const u16 KU_SENTINEL_OBJECT_INDEX = 0xFFFF;

    // Sentinel interval bound magnitude (CgsIntervalList.cpp:29, +/-1e7).
    static const f32 KF_SENTINEL_INTERVAL = 10000000.0f;

    // ObjectToIntervalMap (CgsIntervalList.h:49/64, DWARF). 4 bytes: a u16 pair holding the
    // current array slot of this object's min-role [0] and max-role [1] intervals.
    struct ObjectToIntervalMap
    {
        // Construct() resets both slots to the empty sentinel (0xFFFF). The X360 mutators
        // (RemoveObject / GetIntervalsAndRemoveObject) emit two 0xFFFF stores at end-of-life.
        void Construct()
        {
            mu16MinMaxIntervalIndex[0] = 0xFFFFu;
            mu16MinMaxIntervalIndex[1] = 0xFFFFu;
        }

        u16 GetIntervalIndex(u8 luMinMax) const { return mu16MinMaxIntervalIndex[luMinMax]; }

        void SetIntervalIndex(u8 luMinMax, u16 luIntervalIndex)
        {
            mu16MinMaxIntervalIndex[luMinMax] = luIntervalIndex;
        }

    private:
        u16 mu16MinMaxIntervalIndex[2];  // +0x00 [0]=min-role slot, [1]=max-role slot
    };

    // IntervalList (CgsIntervalList.h:77, DWARF). Method surface gated on the X360 ledger.
    class IntervalList
    {
    public:
        void Construct();
        void Destruct();
        bool Prepare(Interval* lpaIntervalsMem, ObjectToIntervalMap* lpaObjectToIntervalMapMem, u32 luMaxNumIntervals);
        void Release();
        void Clear();

        void AddObject(u16 luObjectIndex, const Interval& lrMin, const Interval& lrMax);
        void AddObject(u16 luObjectIndex, Vector3 lMin, Vector3 lMax);
        void RemoveObject(u16 luObjectIndex);
        void UpdateObject(u16 luObjectIndex, Vector3 lMin, Vector3 lMax);

        void Sort();
        void SweepIntervals(OverlapPairQueue* lpOverlappingPairQueue, IntervalStack* lpDynamicStack) const;
        void SweepWithStaticList(IntervalList* lpSortedListToSweep, OverlapPairQueue* lpOverlappingPairQueue,
                                 IntervalStack lDynamicStack, IntervalStack lStaticStack);
        void SweepAgainstList(const IntervalList& lrSortedListToSweep, OverlapPairQueue* lpOverlappingPairQueue,
                              IntervalStack* lpStaticStackA, IntervalStack* lpStaticStackB) const;

        void ResetList();
        void RepairMappings();
        void RemoveInterval(u16 luIntervalIndex);
        void GetIntervalsAndRemoveObject(u16 luObjectIndex, Interval* lpMinIntervalOut, Interval* lpMaxIntervalOut);
        void SanityCheck();

        u32 GetNumIntervals() const { return muNumIntervals; }
        const Interval* GetInterval(u32 luIndex) const { return &mpaIntervals[luIndex]; }

        // CgsIntervalList.h:173/174 (DWARF): min-role vs max-role selector into ObjectToIntervalMap.
        static const s32 KI_MIN_INDEX = 0;
        static const s32 KI_MAX_INDEX = 1;

    private:
        void AddSentinelInterval();

        Interval*            mpaIntervals;            // +0x00
        ObjectToIntervalMap* mpaObjectToIntervalMap;  // +0x04
        u32                  muNumIntervals;          // +0x08
        u32                  muMaxNumIntervals;       // +0x0C
    };
}
