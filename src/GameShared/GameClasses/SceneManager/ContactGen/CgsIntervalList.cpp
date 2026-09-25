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

// ============================================================================
// FOLDED FROM CgsIntervalList_wQ5_01.cpp (wave Q5) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// CgsSceneManager::IntervalList::AddObject( u16, Vector3, Vector3 )
//   Home: GameShared/GameClasses/SceneManager/ContactGen/CgsIntervalList.cpp
//   Partfile: CgsIntervalList_wQ5_01.cpp   (wave Q5 round 2, cluster "sweeper (i)")
//
// X360: sub_828B49A0 @ 0x828B49A0, 348 instructions.
//
// ---------------------------------------------------------------------------
// WHY THIS IS `IntervalList::AddObject`, AND WHY IT LIVES IN CgsIntervalList.cpp
// (the symbol is UNNAMED in the IDB -- `sub_828B49A0` -- so ownership is derived,
//  and every leg of the derivation is measured, not assumed):
//
//  1. IT IS A MEMBER OF IntervalList. r3 is dereferenced at +0x08 and +0x0C only
//     (0x828B49D0 `lwz r10,0xC(r31)` == muMaxNumIntervals, 0x828B49DC `lwz r11,8(r31)`
//     == muNumIntervals), and at +0x00 / +0x04 for the two backing arrays
//     (0x828B4D90 `lwz r11,0(r31)` == mpaIntervals, 0x828B4EEC `lwz r11,4(r31)` ==
//     mpaObjectToIntervalMap). That is exactly the four-field IntervalList and
//     nothing else -- see the offset table in CgsIntervalList.h.
//  2. ITS ASSERTS BAKE THIS FILE. Every FireAssert in the body passes the string at
//     0x820F35B0 as its `file` argument; dumped verbatim by headless idat on a private
//     .i64 copy it reads
//       "d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\scenemanager\ContactGen/CgsIntervalList.cpp"
//     -- byte-identical to the file string used by IntervalList::Prepare @0x828B4658 and
//     by the sibling overload IntervalList::AddObject @0x828B46E8.
//  3. ITS .pdata ROW SITS BETWEEN TWO NAMED IntervalList METHODS. The exception table at
//     0x821DC490 lists, in address order:
//       IntervalList::Prepare | IntervalList::AddObject | sub_828B49A0 |
//       IntervalList::UpdateObject | IntervalList::AddSentinelInterval
//     so this body is emitted inside the CgsIntervalList.cpp object, between two of its
//     methods, not in a sweeper object.
//  4. ITS SOURCE LINES INTERLEAVE CORRECTLY. The sibling overload's asserts bake lines
//     162/164/167/168/169; this body's bake 202/204/207/208/209/210/211; UpdateObject's
//     are later still. One file, in order.
//  5. THE DWARF DECLARES EXACTLY THIS SECOND OVERLOAD (CgsIntervalList.h:108,
//     `AddObject(uint16_t, Vector3, Vector3)`), it had no body anywhere in the tree, and
//     the two X360 callers -- SceneSweeper::AddObject @0x828B5958 and
//     SceneSweeper::UpdateObject @0x828B6160 -- are precisely the two the DWARF's call
//     graph names for it.
//
// ---------------------------------------------------------------------------
// PARAMETER ABI. The two Vector3s travel in VECTOR registers, not GPRs: the caller loads
// `lvx128 v1, r0, <box+0x00>` and `lvx128 v2, r0, <box+0x10>` (SceneSweeper::AddObject
// 0x828B5CF8/0x828B5D00), and this body's prologue spills v1 -> arg_20 and v2 -> arg_30
// (0x828B49C8/0x828B49D8). The u16 object index rides r4 alone -- the vector arguments do
// NOT consume GPR slots (the PPC vector analogue of the float-skips-a-GPR rule). So the
// argument order really is (index, min, max), matching the DWARF declaration.
//
// LEDGER NOTE (measured, gotcha 10): progress/ledger marks `IntervalList::AddObject`
// `reviewed`. Only the `(u16, const Interval&, const Interval&)` overload had a body; this
// one had none. The row was true of a different function with the same name.
// ============================================================================


namespace CgsSceneManager
{
    // ------------------------------------------------------------------------
    // CgsIntervalList.cpp:196-215 @ 0x828B49A0 (348 insns, store-for-store)
    //
    // The interval-record BUILDER: the overload that takes the world box's two corner rows
    // and manufactures the two Interval endpoint records itself, rather than being handed
    // them ready-made like the `(u16, const Interval&, const Interval&)` sibling above it in
    // the file. Both console callers (SceneSweeper::AddObject / ::UpdateObject) use this one,
    // so it is the function that actually puts a car -- or a smash gate -- into the sweep.
    //
    // Two slots are consumed, min-role then max-role, and the SHARED axis bounds are taken
    // from BOTH corners: every field except mfXInterval is identical in the two records
    // (that is what makes the sweep single-axis with per-pair Y/Z rejection). The negations
    // are the console's `vxor` against the 0x80000000 lane mask built by
    // `vspltisw128 v127,-1 ; vslw128 vX,v127,v127` (shift each word left by 31) --
    // a sign flip, not an arithmetic negate of a different quantity:
    //
    //   0x828B4DC0  sth  r20, 0x14(min)   ; mu16ObjectIndex     = luObjectIndex
    //   0x828B4DF4  stfs f0,  0x00(min)   ; mfXInterval         =  lMin.x
    //   0x828B4DF8  stfs f13, 0x0C(min)   ; mfYMinInterval      =  lMin.y
    //   0x828B4E0C  stfs f12, 0x04(min)   ; mfZMinInterval      =  lMin.z
    //   0x828B4E1C  stfs f0,  0x10(min)   ; mfMinusYMaxInterval = -lMax.y   (vspltw 1 + vxor)
    //   0x828B4E3C  stfs f0,  0x08(min)   ; mfMinusZMaxInterval = -lMax.z   (vspltw 2 + vxor)
    //   0x828B4E4C  sth  r23(=0), 0x16(min) ; mu16Flags         = 0   -> min-role endpoint
    //   0x828B4E58  sth  r20, 0x14(max)   ; ... the same record again, except
    //   0x828B4E8C  stfs f0,  0x00(max)   ; mfXInterval         =  lMax.x
    //   0x828B4EE8  sth  r7(=1), 0x16(max) ; mu16Flags          = 1   -> max-role endpoint
    //   0x828B4EF0  sthx r29, map+idx*4   ; map[idx][KI_MIN_INDEX] = luNewObjectMin
    //   0x828B4EFC  sth  r24, 2(map+idx*4); map[idx][KI_MAX_INDEX] = luNewObjectMax
    //
    // ⭐ THE ZERO-EXTENT EPSILON IS NOT A GUESS. The three degenerate-box tests are spelled
    // in VMX as `|lMax.<axis> - lMin.<axis>| > eps` -- `vsubfp` then `vandc` against the same
    // 0x80000000 lane mask (an absolute value) then `vcmpgtfp` against lane 0 of the .rdata
    // constant at 0x820F25D0. That constant reads as 32 opaque bytes in the export
    // (`unk_820F25D0`), which is exactly the shape of the "unrecoverable zero" trap; dumped
    // for real it is { 1.1920928955078125e-07, 0.1, 2.0, ... } and lane 0 is FLT_EPSILON --
    // BIT-IDENTICAL to rw::math::fpu::KF_IS_ZERO_TOLERANCE as this tree already committed it.
    // So `!rw::math::fpu::IsZero(diff)` below is the console's own predicate, not an
    // approximation of it, and it is spelled the same way as the sibling overload's three
    // asserts (CgsIntervalList.cpp:90-96) which bake the identical message strings.
    // (Sub-note on polarity: the console's single `vcmpgtfp` reports FALSE for a NaN extent
    // and would therefore fire -- and so does `!IsZero(NaN)` since b5 d61a63f4, because
    // rw::math::fpu::IsZero now answers TRUE for a NaN, as its console inlines do. The case is
    // unreachable here anyway -- the two IsValid asserts immediately above reject NaN corners
    // first, and both are non-gating tripwires either way.)
    //
    // Assert lines/messages, all recovered in full from the private .i64 (IDA's inline
    // listing truncates at 39 chars):
    //   :202/:204  streamed "Too many intervals: " << luNewObject<Min|Max>
    //   :207       "rw::math::IsValid( lMin )"      (per-lane vcmpeqfp self-compare, x/y/z)
    //   :208       "rw::math::IsValid( lMax )"
    //   :209/:210/:211  "Bounding box added with zero width|height|depth"
    // ------------------------------------------------------------------------
    void IntervalList::AddObject(u16 luObjectIndex, Vector3 lMin, Vector3 lMax)
    {
        // Two slots, claimed one at a time; the console bumps muNumIntervals BEFORE each
        // bounds test and re-reads both fields for the second one (0x828B49DC/0x828B49EC,
        // then 0x828B4A74/0x828B4A84), so a failed assert still leaves the count advanced.
        const u32 luNewObjectMin = muNumIntervals;
        muNumIntervals = luNewObjectMin + 1;
        CGS_ASSERT(luNewObjectMin < muMaxNumIntervals, "Too many intervals: ");   // :202

        const u32 luNewObjectMax = muNumIntervals;
        muNumIntervals = luNewObjectMax + 1;
        CGS_ASSERT(luNewObjectMax < muMaxNumIntervals, "Too many intervals: ");   // :204

        CGS_ASSERT(rw::math::vpu::IsValid(lMin), "rw::math::IsValid( lMin )");    // :207
        CGS_ASSERT(rw::math::vpu::IsValid(lMax), "rw::math::IsValid( lMax )");    // :208

        // Per-axis extent, asserted non-degenerate. Operand order is the console's
        // `vsubfp vD, vMax, vMin` (0x828B4C48 / 0x828B4CC8 / 0x828B4D44).
        const f32 lfWidth  = lMax.x - lMin.x;
        CGS_ASSERT(!rw::math::fpu::IsZero(lfWidth),  "Bounding box added with zero width");   // :209

        const f32 lfHeight = lMax.y - lMin.y;
        CGS_ASSERT(!rw::math::fpu::IsZero(lfHeight), "Bounding box added with zero height");  // :210

        const f32 lfDepth  = lMax.z - lMin.z;
        CGS_ASSERT(!rw::math::fpu::IsZero(lfDepth),  "Bounding box added with zero depth");   // :211

        // The two endpoint records. Everything but mfXInterval is shared.
        const f32 lfZMin      =  lMin.z;
        const f32 lfYMin      =  lMin.y;
        const f32 lfMinusZMax = -lMax.z;
        const f32 lfMinusYMax = -lMax.y;

        Interval& lrMinInterval = mpaIntervals[luNewObjectMin];
        lrMinInterval.mu16ObjectIndex     = luObjectIndex;
        lrMinInterval.mfXInterval         = lMin.x;
        lrMinInterval.mfYMinInterval      = lfYMin;
        lrMinInterval.mfZMinInterval      = lfZMin;
        lrMinInterval.mfMinusYMaxInterval = lfMinusYMax;
        lrMinInterval.mfMinusZMaxInterval = lfMinusZMax;
        lrMinInterval.mu16Flags           = 0;   // min-role endpoint

        Interval& lrMaxInterval = mpaIntervals[luNewObjectMax];
        lrMaxInterval.mu16ObjectIndex     = luObjectIndex;
        lrMaxInterval.mfXInterval         = lMax.x;
        lrMaxInterval.mfYMinInterval      = lfYMin;
        lrMaxInterval.mfZMinInterval      = lfZMin;
        lrMaxInterval.mfMinusYMaxInterval = lfMinusYMax;
        lrMaxInterval.mfMinusZMaxInterval = lfMinusZMax;
        lrMaxInterval.mu16Flags           = 1;   // max-role endpoint

        // The object -> slot back-reference. `clrlslwi r10,r20,16,2` at 0x828B4EE0 is
        // (u16)luObjectIndex * 4, i.e. sizeof(ObjectToIntervalMap) -- the map is indexed by
        // OBJECT index, never by interval slot.
        ObjectToIntervalMap& lrMap = mpaObjectToIntervalMap[luObjectIndex];
        lrMap.SetIntervalIndex(KI_MIN_INDEX, static_cast<u16>(luNewObjectMin));
        lrMap.SetIntervalIndex(KI_MAX_INDEX, static_cast<u16>(luNewObjectMax));
    }
}
