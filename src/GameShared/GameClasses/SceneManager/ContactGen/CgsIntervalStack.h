#pragma once

// CgsSceneManager::IntervalStack — the LIFO scratch stack the sweep-and-prune broadphase
// (CgsIntervalList::SweepIntervals / SweepAgainstList) uses to hold the set of currently
// "open" intervals while it walks the sorted endpoint list. Each open interval is recorded
// twice: its packed Z/Y min/max bounds (one 16-byte SoA lane) in mpIntervalData, and its
// owning object index (u16) in the parallel mpuIndexData array.
//
// Layout + member names recovered from the DecFIGS DWARF (CgsIntervalStack.h:32/46/79-82).
// The Push body is reconstructed store-for-store from the X360 asm (0x828AA158); Pop and
// CheckOverlappingAndAddToQueue are reconstructed from the two sweep bodies that inline
// them (SweepIntervals @0x828C1328, SweepAgainstList @0x828C1520).
//
//   IntervalStack (0x10 bytes on the console; the two pointers widen on the host):
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
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsInterval.h"  // Interval, OverlapPairQueue

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
        // CgsIntervalStack.h:54 (DWARF). Bind the caller-owned parallel arrays and reset the
        // live count. The X360 build has no out-of-line symbol for this: SceneSweeper::
        // SweepLists @0x828C20F0 builds both stacks IN PLACE on its own frame
        //     stw <&mauDynamicStackIndexMem>,   var_40(r1)   ; mpuIndexData
        //     stw <&maDynamicStackIntervalMem>, var_3C(r1)   ; mpIntervalData
        //     li  r10, 0x7D0 ; stw r10, var_38(r1)           ; muMaxLen  == 2000
        //     stw r11(=0),   var_34(r1)                      ; muLen     == 0
        // (and the same four stores again with 0xBEA == 3050 for the secondary stack), i.e.
        // the DWARF's Init() folded inline. De-inlined here per the project's inlining-reversal
        // rule so SweepLists reads as source rather than as four raw stores.
        void Init(u16* lpuIndexData, IntervalStackEntry* lpIntervalData, u32 luMaxLen)
        {
            mpuIndexData   = lpuIndexData;
            mpIntervalData = lpIntervalData;
            muMaxLen       = luMaxLen;
            muLen          = 0;
        }

        // CgsIntervalStack.h:58 (DWARF). Drop every open interval (the console clears muLen only;
        // the backing arrays are scratch).
        void Clear() { muLen = 0; }

        // CgsIntervalStack.h:62 (DWARF).
        u32 Length() const { return muLen; }

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

        // CgsIntervalStack.h:70 (DWARF): `uint32_t Pop(uint32_t)`. Close the open interval whose
        // owning object index is luObjectIndex, by SWAP-REMOVE with the last live entry. Body
        // reconstructed from the copy inlined at SweepIntervals @0x828C1440-0x828C1500 (and the
        // two identical copies inside SweepAgainstList) — see CgsIntervalStack.cpp.
        u32 Pop(u32 luObjectIndex);

        // CgsIntervalStack.h:75 (DWARF):
        //     void CheckOverlappingAndAddToQueue(const Interval &, OverlapPairQueue *) const;
        // Test one opening interval against every currently-open interval on this stack and
        // queue an OverlappingIntervalPair for each that overlaps in BOTH the Y and Z spans
        // (X is handled by the sweep order). Body reconstructed from the copy inlined at
        // SweepIntervals @0x828C13B4-0x828C142C — see CgsIntervalStack.cpp.
        void CheckOverlappingAndAddToQueue(const Interval& lrInterval,
                                           OverlapPairQueue* lpOverlappingPairQueue) const;

    private:
        u16*                mpuIndexData;    // +0x00
        IntervalStackEntry* mpIntervalData;  // +0x04
        u32                 muMaxLen;        // +0x08
        u32                 muLen;           // +0x0C
    };
}
