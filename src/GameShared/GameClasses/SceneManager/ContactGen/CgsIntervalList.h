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
    // The sweep surface's scratch stack. Pointer/reference-only here, so a forward
    // declaration is enough and CgsIntervalStack.h stays out of this header's closure
    // (CgsIntervalStack.h already includes CgsInterval.h, so including it here would be an
    // ordering hazard for no gain). OverlapPairQueue is NOT forward-declared: it is a
    // typedef of a class template instantiation (DWARF CgsInterval.h:61) and CgsInterval.h
    // -- already included above -- is its attested home. The `struct OverlapPairQueue;`
    // forward declaration that stood here until 2026-08-18 declared a DIFFERENT, never-
    // defined type; any TU that actually dereferenced the queue would have failed.
    class IntervalStack;

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

        // CgsIntervalList.h:121 (DWARF). Re-sort the endpoint array on mfXInterval, append the
        // terminating sentinel and rebuild the object->slot map. Body in CgsIntervalList.cpp.
        //
        // The X360 build has no out-of-line Sort symbol -- SceneSweeper::SortLists @0x828D5980
        // carries it inlined, three times: `std::_Sort<Interval*,int>(mpaIntervals,
        // mpaIntervals + muNumIntervals, muNumIntervals)` then `bl IntervalList::
        // AddSentinelInterval` then `bl IntervalList::RepairMappings`, all on the same list
        // pointer. Those last two are NOT SceneSweeper's to call -- AddSentinelInterval is
        // PRIVATE to IntervalList (DWARF CgsIntervalList.h:191) and SceneSweeper is not a
        // friend -- so the trio can only have been the body of this method, inlined. That
        // access rule is what pins the split; the asm alone would allow either reading.
        void Sort();

        // CgsIntervalList.h:126 (DWARF). @0x828C1328. Single-list sweep: walk the sorted
        // endpoints once, closing max-role endpoints off lpDynamicStack and testing each
        // min-role endpoint against the currently-open set before pushing it.
        void SweepIntervals(OverlapPairQueue* lpOverlappingPairQueue, IntervalStack* lpDynamicStack) const;

        // CgsIntervalList.h:133 (DWARF). ⚠️ NO X360 BODY AND NO CALLER -- `SweepWithStaticList`
        // does not appear in progress/identity.json, in the ledger, or as a callee of any
        // sweeper body (SceneSweeper::SweepLists @0x828C20F0 calls SweepIntervals and
        // SweepAgainstList only). Declaration kept because the DWARF attests it; NOT a
        // reconstruction target -- do not invent one.
        void SweepWithStaticList(IntervalList* lpSortedListToSweep, OverlapPairQueue* lpOverlappingPairQueue,
                                 IntervalStack lDynamicStack, IntervalStack lStaticStack);

        // CgsIntervalList.h:140 (DWARF). @0x828C1520. Cross-list sweep: merge-walk the two
        // sorted endpoint arrays on mfXInterval; lpStaticStackA holds THIS list's open
        // intervals and lpStaticStackB the other list's, and each opening endpoint is tested
        // against the OTHER list's open set.
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
