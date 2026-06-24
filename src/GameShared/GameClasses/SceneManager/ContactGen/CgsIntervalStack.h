#pragma once

// CgsSceneManager::IntervalStack — the LIFO scratch stack the sweep-and-prune broadphase
// (CgsIntervalList::SweepIntervals / SweepAgainstList) uses to hold the set of currently
// "open" intervals while it walks the sorted endpoint list. Each open interval is recorded
// twice: its packed Z/Y min/max bounds (one 16-byte SoA lane) in mpIntervalData, and its
// owning object index (u16) in the parallel mpuIndexData array.
//
// Layout + member names recovered from the DecFIGS DWARF (CgsIntervalStack.h:32/46/79-82).
// The Push body is reconstructed store-for-store from the X360 asm (0x828AA158).
//
//   IntervalStack (0x10 bytes):
//     +0x00  mpuIndexData    (u16*)               — parallel object-index array
//     +0x04  mpIntervalData  (IntervalStackEntry*) — packed Z/Y min/max lanes
//     +0x08  muMaxLen        (u32)                — capacity
//     +0x0C  muLen           (u32)                — live count
//
//   IntervalStackEntry (16 bytes): one Vector4 lane (mvZYMaxMin) holding
//     { -ZMax, ZMin, -YMax, YMin } for the open interval (the four xyzw lanes the
//     Push assembly assembles from the source Interval's four interval floats).

#include "types.hpp"
#include "BrnCommonTypes.h"  // Vector4 (16-byte SIMD lane)
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsInterval.h"  // CgsSceneManager::Interval

namespace CgsSceneManager
{
    // CgsIntervalStack.h:32 (DWARF). One open-interval record: a single packed Vector4 lane.
    struct IntervalStackEntry
    {
        Vector4 mvZYMaxMin;  // { -ZMax, ZMin, -YMax, YMin }
    };

    // CgsIntervalStack.h:46 (DWARF).
    class IntervalStack
    {
    public:
        // Push @ 0x828AA158 — push one open interval. Asserts the stack has room
        // (muLen < muMaxLen, CgsIntervalStack.h:118), then:
        //   * writes the source interval's object index into mpuIndexData[muLen]
        //     (sthx — 16-bit store at index*2 from mpuIndexData base);
        //   * assembles a 16-byte lane { lrInterval.mfMinusZMaxInterval,
        //     lrInterval.mfZMinInterval, lrInterval.mfMinusYMaxInterval,
        //     lrInterval.mfYMinInterval } and stvx128's it into mpIntervalData[muLen]
        //     (stride 16);
        //   * post-increments muLen and returns the new length.
        u32 Push(const Interval& lrInterval);

    private:
        u16*                mpuIndexData;    // +0x00
        IntervalStackEntry* mpIntervalData;  // +0x04
        u32                 muMaxLen;        // +0x08
        u32                 muLen;           // +0x0C
    };
}
