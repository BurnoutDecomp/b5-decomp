// ===========================================================================
// CgsSceneManager::IntervalList — sweep-and-prune per-axis endpoint list.
//   Home: GameShared/GameClasses/SceneManager/ContactGen/CgsIntervalList.cpp
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. Eight bodies homed here:
//   AddObject / AddSentinelInterval / GetIntervalsAndRemoveObject / Prepare /
//   RemoveInterval / RemoveObject / RepairMappings / UpdateObject.
//
// Each object owns two Interval slots (a min-role and a max-role endpoint). The parallel
// ObjectToIntervalMap array records, per object, the two current slot indices so an object
// can be located/removed in O(1). The map is kept in sync by RemoveInterval / RepairMappings
// after any array move: the moved slot's owner is looked up by (mu16ObjectIndex), its role by
// (mu16Flags == 1 ? KI_MAX_INDEX : KI_MIN_INDEX), and its recorded slot is patched.
// ===========================================================================

#include "GameShared/GameClasses/SceneManager/ContactGen/CgsIntervalList.h"
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsIntervalStack.h"  // IntervalStack (sweep scratch)
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/fpu/scalar_operation.h"     // rw::math::fpu::IsZero (scalar box-extent tests)
#include "rw/math/vpu/vector3_operation.h"    // rw::math::vpu::IsValid (Vector3)

#include <algorithm>                          // std::sort (the console's own std::_Sort)

namespace CgsSceneManager
{
    // CgsIntervalList.cpp:92 @ 0x828B4658
    //
    // Binds the caller-owned interval + object-map backing storage into the list and resets
    // its live count. Always returns true (the X360 body has no failure path).
    bool IntervalList::Prepare(Interval* lpaIntervalsMem,
                               ObjectToIntervalMap* lpaObjectToIntervalMapMem,
                               u32 luMaxNumIntervals)
    {
        CGS_ASSERT(lpaIntervalsMem != NULL, "lpaIntervalsMem != NULL");
        CGS_ASSERT(lpaObjectToIntervalMapMem != NULL, "lpaObjectToIntervalMapMem != NULL");

        mpaIntervals           = lpaIntervalsMem;
        mpaObjectToIntervalMap = lpaObjectToIntervalMapMem;
        muMaxNumIntervals      = luMaxNumIntervals;
        muNumIntervals         = 0;
        return true;
    }

    // CgsIntervalList.h:96 -- IntervalList::Clear()
    //
    // No out-of-line X360 symbol; recovered from the copy inlined THREE times at the head of
    // SceneSweeper::Clear @0x828B57A0 (0x828B57DC-0x828B5804), once per list:
    //     stw r30(=0), 0(list)   ; mpaIntervals           = NULL
    //     stw r30,     4(list)   ; mpaObjectToIntervalMap = NULL
    //     stw r30,     8(list)   ; muNumIntervals         = 0
    // Note what is NOT stored: muMaxNumIntervals at +0xC survives, and SceneSweeper::Prepare
    // re-binds all three fields immediately afterwards via IntervalList::Prepare.
    //
    // FLAG: the DWARF declares BOTH `Clear()` (:96) and `ResetList()` (:143) and the X360
    // export set has an out-of-line body for NEITHER, so which of the two names owns this
    // three-field unbind is not directly attested. It is assigned to Clear() because the
    // caller is SceneSweeper::Clear and because the semantics ("drop the backing storage
    // too") match Clear rather than a list "reset". ResetList() is therefore left DECLARED
    // AND BODYLESS -- it has no console caller anywhere in the sweeper family; do not invent
    // one for it.
    void IntervalList::Clear()
    {
        mpaIntervals           = NULL;
        mpaObjectToIntervalMap = NULL;
        muNumIntervals         = 0;
    }

    // CgsIntervalList.cpp:156 @ 0x828B46E8
    //
    // Adds one swept object: consumes TWO interval slots (a min-endpoint interval and a
    // max-endpoint interval), copies the caller's pre-built Interval records into them, and
    // records the two slot indices in the object->interval map. Asserts capacity, then that
    // the box has non-zero width/height/depth (per-lane |diff| > epsilon, folded to a Vector3
    // IsZero test of the axis-diff vector).
    void IntervalList::AddObject(u16 luObjectIndex, const Interval& lrMin, const Interval& lrMax)
    {
        const u32 luNewObjectMin = muNumIntervals;
        muNumIntervals = luNewObjectMin + 1;
        CGS_ASSERT(luNewObjectMin < muMaxNumIntervals, "luNewObjectMin < muMaxNumIntervals");

        const u32 luNewObjectMax = muNumIntervals;
        muNumIntervals = luNewObjectMax + 1;
        CGS_ASSERT(luNewObjectMax < muMaxNumIntervals, "luNewObjectMax < muMaxNumIntervals");

        // Per-axis extent = |min-endpoint bound - max-endpoint bound|; asserted non-degenerate.
        //   width  =  lrMin.mfXInterval        - lrMax.mfXInterval
        //   height = -lrMax.mfMinusYMaxInterval - lrMin.mfYMinInterval
        //   depth  = -lrMax.mfMinusZMaxInterval - lrMin.mfZMinInterval
        const f32 lfWidth  =  lrMin.mfXInterval        - lrMax.mfXInterval;
        CGS_ASSERT(!rw::math::fpu::IsZero(lfWidth), "Bounding box added with zero width");

        const f32 lfHeight = -lrMax.mfMinusYMaxInterval - lrMin.mfYMinInterval;
        CGS_ASSERT(!rw::math::fpu::IsZero(lfHeight), "Bounding box added with zero height");

        const f32 lfDepth  = -lrMax.mfMinusZMaxInterval - lrMin.mfZMinInterval;
        CGS_ASSERT(!rw::math::fpu::IsZero(lfDepth), "Bounding box added with zero depth");

        // Copy the two 24-byte Interval records into their slots (the X360 6-dword copy loops).
        mpaIntervals[luNewObjectMin] = lrMin;
        mpaIntervals[luNewObjectMax] = lrMax;

        ObjectToIntervalMap& lrMap = mpaObjectToIntervalMap[luObjectIndex];
        lrMap.SetIntervalIndex(KI_MIN_INDEX, static_cast<u16>(luNewObjectMin));
        lrMap.SetIntervalIndex(KI_MAX_INDEX, static_cast<u16>(luNewObjectMax));
    }

    // CgsIntervalList.cpp:240 @ 0x828AB088
    //
    // Compacts one interval slot out of the array. If the removed slot is not already the last
    // live slot, the last slot is copied over it (24-byte record move) and the moved record's
    // owner map entry is patched to point at the vacated index. muNumIntervals is decremented.
    // The moved slot's role selector is (mu16Flags == 1) -> KI_MAX_INDEX else KI_MIN_INDEX.
    void IntervalList::RemoveInterval(u16 luIntervalIndex)
    {
        CGS_ASSERT(luIntervalIndex < muNumIntervals, "luIntervalIndex < muNumIntervals");

        const u32 luLastInterval = muNumIntervals - 1;
        if (luIntervalIndex != luLastInterval)
        {
            Interval& lrMoved = mpaIntervals[luIntervalIndex];
            lrMoved = mpaIntervals[luLastInterval];

            const u8 luMinMax = (lrMoved.mu16Flags == 1) ? KI_MAX_INDEX : KI_MIN_INDEX;
            mpaObjectToIntervalMap[lrMoved.mu16ObjectIndex].SetIntervalIndex(luMinMax, luIntervalIndex);
        }
        muNumIntervals = muNumIntervals - 1;
    }

    // CgsIntervalList.cpp:285 @ 0x828AB168
    //
    // Removes both of an object's intervals (min-role then max-role), asserting each recovered
    // slot still names this object, then resets the object's map entry to the empty sentinel.
    void IntervalList::RemoveObject(u16 luObjectIndex)
    {
        ObjectToIntervalMap& lrMap = mpaObjectToIntervalMap[luObjectIndex];

        u16 luRemoveInterval = lrMap.GetIntervalIndex(KI_MIN_INDEX);
        CGS_ASSERT(mpaIntervals[luRemoveInterval].mu16ObjectIndex == luObjectIndex,
                   "mpaIntervals[luRemoveInterval].GetObjectIndex() == luObjectIndex");
        RemoveInterval(luRemoveInterval);

        luRemoveInterval = lrMap.GetIntervalIndex(KI_MAX_INDEX);
        CGS_ASSERT(mpaIntervals[luRemoveInterval].mu16ObjectIndex == luObjectIndex,
                   "mpaIntervals[luRemoveInterval].GetObjectIndex() == luObjectIndex");
        RemoveInterval(luRemoveInterval);

        lrMap.Construct();
    }

    // CgsIntervalList.cpp:316 @ 0x828AB258
    //
    // Copies the object's two live Interval records out to the caller, removes both from the
    // list (compacting via RemoveInterval), and resets the object's map entry to empty.
    void IntervalList::GetIntervalsAndRemoveObject(u16 luObjectIndex,
                                                   Interval* lpMinIntervalOut,
                                                   Interval* lpMaxIntervalOut)
    {
        CGS_ASSERT(lpMinIntervalOut != NULL, "lpMinIntervalOut != NULL");
        CGS_ASSERT(lpMaxIntervalOut != NULL, "lpMaxIntervalOut != NULL");

        ObjectToIntervalMap& lrMap = mpaObjectToIntervalMap[luObjectIndex];

        u16 luRemoveInterval = lrMap.GetIntervalIndex(KI_MIN_INDEX);
        CGS_ASSERT(mpaIntervals[luRemoveInterval].mu16ObjectIndex == luObjectIndex,
                   "mpaIntervals[luRemoveInterval].GetObjectIndex() == luObjectIndex");
        *lpMinIntervalOut = mpaIntervals[luRemoveInterval];
        RemoveInterval(luRemoveInterval);

        luRemoveInterval = lrMap.GetIntervalIndex(KI_MAX_INDEX);
        CGS_ASSERT(mpaIntervals[luRemoveInterval].mu16ObjectIndex == luObjectIndex,
                   "mpaIntervals[luRemoveInterval].GetObjectIndex() == luObjectIndex");
        *lpMaxIntervalOut = mpaIntervals[luRemoveInterval];
        RemoveInterval(luRemoveInterval);

        lrMap.Construct();
    }

    // CgsIntervalList.cpp:356 @ 0x828B4F10
    //
    // Rewrites both of an object's interval endpoint bounds in place from fresh min/max
    // vectors (used by the sweeper when a body moves without changing slot ownership). Both
    // slots share the Z-min / Y-min / -Z-max / -Y-max bounds; they differ only in mfXInterval
    // (min-role stores lMin.x, max-role stores lMax.x). Asserts both input vectors are valid.
    void IntervalList::UpdateObject(u16 luObjectIndex, Vector3 lMin, Vector3 lMax)
    {
        CGS_ASSERT(rw::math::vpu::IsValid(lMin), "rw::math::IsValid( lMin )");
        CGS_ASSERT(rw::math::vpu::IsValid(lMax), "rw::math::IsValid( lMax )");

        const f32 lfMinX      =  lMin.x;
        const f32 lfMaxX      =  lMax.x;
        const f32 lfZMin      =  lMin.z;
        const f32 lfYMin      =  lMin.y;
        const f32 lfMinusZMax = -lMax.z;
        const f32 lfMinusYMax = -lMax.y;

        ObjectToIntervalMap& lrMap = mpaObjectToIntervalMap[luObjectIndex];

        Interval& lrMinInterval = mpaIntervals[lrMap.GetIntervalIndex(KI_MIN_INDEX)];
        lrMinInterval.mfXInterval         = lfMinX;
        lrMinInterval.mfZMinInterval      = lfZMin;
        lrMinInterval.mfMinusZMaxInterval = lfMinusZMax;
        lrMinInterval.mfYMinInterval      = lfYMin;
        lrMinInterval.mfMinusYMaxInterval = lfMinusYMax;

        Interval& lrMaxInterval = mpaIntervals[lrMap.GetIntervalIndex(KI_MAX_INDEX)];
        lrMaxInterval.mfXInterval         = lfMaxX;
        lrMaxInterval.mfZMinInterval      = lfZMin;
        lrMaxInterval.mfMinusZMaxInterval = lfMinusZMax;
        lrMaxInterval.mfYMinInterval      = lfYMin;
        lrMaxInterval.mfMinusYMaxInterval = lfMinusYMax;
    }

    // CgsIntervalList.cpp:682 @ 0x828AB3E8
    //
    // Rebuilds every ObjectToIntervalMap entry from the (freshly sorted) interval array: for
    // each live slot, patch the owning object's [role] slot index to that slot's array index.
    void IntervalList::RepairMappings()
    {
        for (u32 luCurrentInterval = 0; luCurrentInterval < muNumIntervals; ++luCurrentInterval)
        {
            const Interval& lrInterval = mpaIntervals[luCurrentInterval];
            const u8 luMinMax = (lrInterval.mu16Flags == 1) ? KI_MAX_INDEX : KI_MIN_INDEX;
            mpaObjectToIntervalMap[lrInterval.mu16ObjectIndex].SetIntervalIndex(
                luMinMax, static_cast<u16>(luCurrentInterval));
        }
    }

    // CgsIntervalList.cpp:400 @ 0x828B5178
    //
    // Appends a single terminating sentinel interval at slot muNumIntervals (NOT incremented
    // here — the caller appends it transiently), so the sweep walk always hits a guaranteed
    // endpoint. Sentinel object index is 0xFFFF and its bounds are +/-KF_SENTINEL_INTERVAL.
    void IntervalList::AddSentinelInterval()
    {
        const u32 luNewObjectMax = muNumIntervals;
        CGS_ASSERT(luNewObjectMax < muMaxNumIntervals, "luNewObjectMax < muMaxNumIntervals");

        Interval& lrInterval = mpaIntervals[luNewObjectMax];
        lrInterval.mfXInterval         =  KF_SENTINEL_INTERVAL;
        lrInterval.mfZMinInterval      = -KF_SENTINEL_INTERVAL;
        lrInterval.mfMinusZMaxInterval = -KF_SENTINEL_INTERVAL;
        lrInterval.mfYMinInterval      = -KF_SENTINEL_INTERVAL;
        lrInterval.mfMinusYMaxInterval = -KF_SENTINEL_INTERVAL;
        lrInterval.mu16ObjectIndex     = KU_SENTINEL_OBJECT_INDEX;
        lrInterval.mu16Flags           = 1;
    }

    // ================================================================================
    // CgsIntervalList.h:121 -- IntervalList::Sort()
    //
    // No out-of-line X360 symbol: SceneSweeper::SortLists @0x828D5980 carries this inlined
    // three times (once per list), each copy spelled
    //     lwz  r11, 8(list)          ; muNumIntervals
    //     lwz  r3,  0(list)          ; mpaIntervals                          -> _Sort arg 1
    //     r11*3<<3 == muNumIntervals*24 ; r4 = mpaIntervals + muNumIntervals -> _Sort arg 2
    //     divw r5, (r4-r3), 24       ; == muNumIntervals                     -> _Sort arg 3
    //     bl   std::_Sort<CgsSceneManager::Interval *,int>
    //     bl   CgsSceneManager::IntervalList::AddSentinelInterval
    //     bl   CgsSceneManager::IntervalList::RepairMappings
    // (0x828D599C-0x828D59CC, 0x828D59F0-0x828D5A20, 0x828D5A44-0x828D5A74).
    //
    // `std::_Sort<It, Diff>` IS the MSVC introsort core of std::sort: the third argument is
    // the recursion "ideal" depth budget, which std::sort seeds with the range length -- so
    // the host-side spelling of the whole three-argument call is simply `std::sort(first,
    // last)`. The instantiation carries NO predicate type parameter, so the ordering is
    // Interval's own operator< (recovered and documented in CgsInterval.h: strict less-than
    // on mfXInterval alone, measured in _Insertion_sort1 @0x828CD388 and
    // _Unguarded_partition @0x828CCA30).
    //
    // The sentinel is appended AFTER the sort (it must be the last element and it carries
    // +KF_SENTINEL_INTERVAL as its X, so sorting would place it there anyway) and WITHOUT
    // bumping muNumIntervals -- so the sweep walks muNumIntervals live endpoints and the
    // sentinel sits one past the end as the guard the unguarded partition/scan relies on.
    void IntervalList::Sort()
    {
        std::sort(mpaIntervals, mpaIntervals + muNumIntervals);
        AddSentinelInterval();
        RepairMappings();
    }

    // ================================================================================
    // CgsIntervalList.cpp:189 @ 0x828C1328 -- IntervalList::SweepIntervals
    //
    // The single-list half of the broadphase sweep. Walks THIS list's endpoint array in
    // sorted (mfXInterval) order exactly once, maintaining lpDynamicStack as the set of
    // intervals whose min-role endpoint has been passed but whose max-role endpoint has not:
    //
    //   0x828C134C  lwz r11, 8(r3)      ; muNumIntervals -- whole body skipped when 0
    //   0x828C1354  lwz r23, 0(r3)      ; mpaIntervals   (the walking cursor)
    //   0x828C136C  addi r27, r23, 4    ; the +4 view used for the two u16 field reads
    //   0x828C138C  lhz r11, 0x12(r27)  ; == interval +0x16 == mu16Flags
    //   0x828C1394  bne -> POP          ; flags != 0 (max-role endpoint) -> IntervalStack::Pop
    //               else               -> CheckOverlappingAndAddToQueue, then Push
    //   0x828C149C  addi r23, r23, 0x18 ; ++interval  (24-byte stride)
    //   0x828C1498  addi r22, r22, -1   ; countdown from muNumIntervals
    //
    // Both helpers are inlined by the console; their bodies live in CgsIntervalStack.cpp
    // (de-inlined per the project's inlining-reversal rule), which is where the VMX overlap
    // reduce and the swap-remove are documented instruction-for-instruction.
    void IntervalList::SweepIntervals(OverlapPairQueue* lpOverlappingPairQueue,
                                      IntervalStack* lpDynamicStack) const
    {
        if (muNumIntervals == 0)
        {
            return;
        }

        const Interval* lpInterval = mpaIntervals;
        for (u32 luRemaining = muNumIntervals; luRemaining != 0; --luRemaining, ++lpInterval)
        {
            if (lpInterval->mu16Flags != 0)
            {
                // Max-role endpoint: this object's interval closes here.
                lpDynamicStack->Pop(lpInterval->mu16ObjectIndex);
            }
            else
            {
                // Min-role endpoint: it overlaps every interval still open on the stack in X,
                // so only the Y/Z spans remain to be tested; then it joins the open set.
                lpDynamicStack->CheckOverlappingAndAddToQueue(*lpInterval, lpOverlappingPairQueue);
                lpDynamicStack->Push(*lpInterval);
            }
        }
    }

    // ================================================================================
    // CgsIntervalList.cpp:228 @ 0x828C1520 -- IntervalList::SweepAgainstList
    //
    // ⚠️ This function is ABSENT from progress/identity.json and from the ledger, and had no
    // per-address IDA export; recovered by a targeted headless idat run on a private .i64
    // copy (dump saved at scratchpad/waveQ5/q5_sweeper_holes.json). It is the cross-list
    // half of the broadphase -- the leg that makes a DYNAMIC body (a car) collide with an
    // INACTIVE one (a parked smash gate), i.e. the wave's critical path.
    //
    // A classic two-cursor merge on mfXInterval. THE TWO STACKS ARE NOT INTERCHANGEABLE:
    //   lpStaticStackA holds THIS list's currently-open intervals   (r27 in the asm),
    //   lpStaticStackB holds lrSortedListToSweep's                  (r26 in the asm),
    // and an opening endpoint is always tested against the OTHER list's stack, never its own
    // -- that is what makes this a cross-list sweep and not a second single-list sweep.
    // Measured:
    //   A-side (walking `this`, cursor r21):
    //     0x828C15BC  flags != 0 -> 0x828C1668 : pop from r27 (stack A)
    //     0x828C15C0/15EC/162C   : the overlap loop reads r26 (stack B)
    //     0x828C1658-1660        : Push(r27, *r21)
    //   B-side (walking lrSortedListToSweep, cursor r20):
    //     0x828C16F8  flags != 0 -> 0x828C17F4 : pop from r26 (stack B)
    //     0x828C16FC/1728/1768   : the overlap loop reads r27 (stack A)
    //     0x828C1794-179C        : Push(r26, *r20)
    //
    // THE MERGE PREDICATE, as the console spells it:
    //     0x828C15A8  fcmpu cr6, f30(A.X), f31(B.X) ; bge -> the B side
    //   so the A side runs iff A.X < B.X, and the B side otherwise (B.X <= A.X). The console
    //   keeps two "sticky" inner loops that re-test the same predicate at each tail
    //   (0x828C16D4-16DC keeps running A while A.X < B.X; 0x828C1860-1868 keeps running B
    //   while B.X <= A.X) plus a second, arithmetically-dead `bgt` at 0x828C16E8 -- an
    //   unrolling of the one merge test, re-rolled here.
    //
    // Either list being empty skips the whole body (0x828C156C-0x828C1578): with no endpoints
    // on one side there is nothing to cross-test.
    void IntervalList::SweepAgainstList(const IntervalList& lrSortedListToSweep,
                                        OverlapPairQueue* lpOverlappingPairQueue,
                                        IntervalStack* lpStaticStackA,
                                        IntervalStack* lpStaticStackB) const
    {
        const u32 luNumIntervalsA = muNumIntervals;
        const u32 luNumIntervalsB = lrSortedListToSweep.muNumIntervals;
        if (luNumIntervalsA == 0 || luNumIntervalsB == 0)
        {
            return;
        }

        const Interval* lpIntervalA = mpaIntervals;
        const Interval* lpIntervalB = lrSortedListToSweep.mpaIntervals;
        u32 luIndexA = 0;
        u32 luIndexB = 0;

        for (;;)
        {
            if (lpIntervalA->mfXInterval < lpIntervalB->mfXInterval)
            {
                if (lpIntervalA->mu16Flags != 0)
                {
                    lpStaticStackA->Pop(lpIntervalA->mu16ObjectIndex);
                }
                else
                {
                    lpStaticStackB->CheckOverlappingAndAddToQueue(*lpIntervalA, lpOverlappingPairQueue);
                    lpStaticStackA->Push(*lpIntervalA);
                }

                ++luIndexA;
                if (luIndexA >= luNumIntervalsA)
                {
                    break;
                }
                ++lpIntervalA;
            }
            else
            {
                if (lpIntervalB->mu16Flags != 0)
                {
                    lpStaticStackB->Pop(lpIntervalB->mu16ObjectIndex);
                }
                else
                {
                    lpStaticStackA->CheckOverlappingAndAddToQueue(*lpIntervalB, lpOverlappingPairQueue);
                    lpStaticStackB->Push(*lpIntervalB);
                }

                ++luIndexB;
                if (luIndexB >= luNumIntervalsB)
                {
                    break;
                }
                ++lpIntervalB;
            }
        }
    }
}
