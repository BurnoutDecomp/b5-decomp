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
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/fpu/scalar_operation.h"     // rw::math::fpu::IsZero (scalar box-extent tests)
#include "rw/math/vpu/vector3_operation.h"    // rw::math::vpu::IsValid (Vector3)

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
}
