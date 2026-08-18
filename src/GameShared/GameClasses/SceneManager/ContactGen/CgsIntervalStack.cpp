#include "GameShared/GameClasses/SceneManager/ContactGen/CgsIntervalStack.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

#include <cstddef>  // offsetof

// Layout pins from the Push asm (0x828AA158): muMaxLen @ +8, muLen @ +0xC, IntervalStackEntry
// stride 16 (slwi r11,r11,4), index array stride 2 (slwi r7,r11,1).
namespace
{
    static_assert(sizeof(CgsSceneManager::IntervalStackEntry) == 16, "IntervalStackEntry must be 16 bytes");
    static_assert(sizeof(CgsSceneManager::Interval) == 0x18, "Interval must be 24 bytes");
}

// CgsSceneManager::IntervalStack::Push @ 0x828AA158
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (semantic parity, asm widths
// matched). The X360 body:
//   lwz   r11,0xC(this)        ; muLen
//   lwz   r10,8(this)          ; muMaxLen
//   cmplw r11,r10; blt ...     ; assert muLen < muMaxLen (CgsIntervalStack.h:118 tripwire)
//   lhz   r9,0x14(rInterval)   ; lrInterval.mu16ObjectIndex (16-bit)
//   lwz   r8,0(this)           ; mpuIndexData
//   slwi  r7,r11,1             ; muLen*2 (u16 stride)
//   lwz   r10,4(this)          ; mpIntervalData
//   slwi  r11,r11,4            ; muLen*16 (IntervalStackEntry stride)
//   add   r11,r11,r10          ; &mpIntervalData[muLen]
//   sthx  r9,r7,r8             ; mpuIndexData[muLen] = mu16ObjectIndex (STH = 16-bit)
//   lfs/stfs x4                ; build { -ZMax, ZMin, -YMax, YMin } on the stack
//   lvx128/stvx128             ; store that 16-byte lane to mpIntervalData[muLen]
//   lwz r11,0xC(this); addi r3,r11,1; stw r3,0xC(this) ; ++muLen; return muLen
//
// The four lanes are assembled, in order, from the source Interval's interval floats:
//   lane[0] <- lfs 0x08 = mfMinusZMaxInterval
//   lane[1] <- lfs 0x04 = mfZMinInterval
//   lane[2] <- lfs 0x10 = mfMinusYMaxInterval
//   lane[3] <- lfs 0x0C = mfYMinInterval
// (mfXInterval @ +0x00 is the sort axis, not stored on the stack.)

namespace CgsSceneManager
{
    u32 IntervalStack::Push(const Interval& lrInterval)
    {
        CGS_ASSERT(muLen < muMaxLen, "muLen < muMaxLen");

        const u32 luIndex = muLen;

        // Parallel object-index store (STH, 16-bit) into mpuIndexData[muLen].
        mpuIndexData[luIndex] = lrInterval.mu16ObjectIndex;

        // Packed Z/Y min/max lane into mpIntervalData[muLen] (one 16-byte SIMD store).
        Vector4 lvLane;
        lvLane.x = lrInterval.mfMinusZMaxInterval;
        lvLane.y = lrInterval.mfZMinInterval;
        lvLane.z = lrInterval.mfMinusYMaxInterval;
        lvLane.w = lrInterval.mfYMinInterval;
        mpIntervalData[luIndex].mvZYMaxMin = lvLane;

        ++muLen;
        return muLen;
    }

    // ================================================================================
    // CgsSceneManager::IntervalStack::Pop  (DWARF CgsIntervalStack.h:70)
    //
    // The X360 build has NO out-of-line symbol for Pop: every caller inlines it. This body
    // is reconstructed from the copy inlined into IntervalList::SweepIntervals
    // (0x828C1440-0x828C1500), which is byte-identical to the two copies inlined into
    // IntervalList::SweepAgainstList (0x828C1668.. and 0x828C17A4..):
    //
    //   0x828C1440  lwz   r9, 0xC(r31)      ; muLen
    //   0x828C1448  lhz   r10, 0x10(r27)    ; lrInterval.mu16ObjectIndex
    //   0x828C144C  cmplwi cr6, r9, 0
    //   0x828C1450  beq   cr6, <assert>     ; empty stack -> the tripwire, NO swap-remove
    //   0x828C1454  lwz   r8, 0(r31)        ; mpuIndexData
    //   0x828C1458  clrlwi r7, r10, 16      ; zero-extend the u16 key
    //   0x828C1460  lhz   r6, 0(r10)        ; mpuIndexData[i]
    //   0x828C1464  cmplw cr6, r6, r7
    //   0x828C1468  beq   cr6, <found>
    //   0x828C1470  addi  r11, r11, 1       ; ++i
    //   0x828C1478  cmplw cr6, r11, r6(muLen re-read)
    //   0x828C147C  blt   cr6, <loop>       ; else fall into the SAME assert
    //   <found @0x828C14CC>
    //   0x828C14E4  addi  r9, r9, -1        ; muLen - 1
    //   0x828C14EC  stw   r9, 0xC(r31)      ; muLen = muLen - 1   (BEFORE the two moves)
    //   0x828C14E8  lhz   r5, -2(r5)        ; mpuIndexData[oldLen-1]
    //   0x828C14F0  sthx  r5, r11, r8       ; mpuIndexData[i] = that
    //   0x828C14F4/F8/FC + the two 8-byte moves at 0x828C1500.. : the 16-byte
    //                                       ; mpIntervalData[i] = mpIntervalData[oldLen-1]
    //
    // ⚠️ FAITHFUL ODDITY, reproduced deliberately: both the "stack was empty" arm and the
    // "index not found" arm fire the SAME assert and then simply fall through -- the console
    // does NOT swap-remove, and does NOT early-return past the rest of the caller's loop body.
    // The assert string/line are baked (CgsIntervalStack.h:156, `li r5,0x9C`).
    u32 IntervalStack::Pop(u32 luObjectIndex)
    {
        if (muLen != 0)
        {
            u32 luEntry = 0;
            while (mpuIndexData[luEntry] != luObjectIndex)
            {
                ++luEntry;
                if (luEntry >= muLen)
                {
                    CGS_ASSERT(false, "Error in IntervalStack::Pop");
                    return muLen;
                }
            }

            // Swap-remove: the last live entry takes the vacated slot in BOTH parallel arrays.
            const u32 luLast = muLen - 1;
            muLen = luLast;
            mpuIndexData[luEntry]   = mpuIndexData[luLast];
            mpIntervalData[luEntry] = mpIntervalData[luLast];
        }
        else
        {
            CGS_ASSERT(false, "Error in IntervalStack::Pop");
        }
        return muLen;
    }

    // ================================================================================
    // CgsSceneManager::IntervalStack::CheckOverlappingAndAddToQueue
    //   (DWARF CgsIntervalStack.h:75 --
    //    void CheckOverlappingAndAddToQueue(const Interval &, OverlapPairQueue *) const)
    //
    // Also inlined at every call site; reconstructed from the copy at SweepIntervals
    // 0x828C13B4-0x828C142C (identical copies inside SweepAgainstList at 0x828C15B4.. and
    // 0x828C1728..).
    //
    // THE INPUT LANE. The opening interval's four bound floats are read as ONE unaligned
    // 16-byte vector starting at `&lrInterval.mfZMinInterval` (the interval's +0x04):
    //     0x828C139C  lvrx  v0,  r21(=16), r27      ; r27 == (u8*)&lrInterval + 4
    //     0x828C13A0  lvlx  v13, r0,       r27
    //     0x828C13AC  vor128 v125, v13, v0
    // so lane {0,1,2,3} == { mfZMinInterval, mfMinusZMaxInterval, mfYMinInterval,
    //                        mfMinusYMaxInterval }.
    // The stacked lane (written by Push) is { -ZMax, ZMin, -YMax, YMin }. Adding the two
    // lane-wise therefore yields exactly the four AABB half-tests:
    //     lane0 = newZMin - openZMax      lane1 = openZMin - newZMax
    //     lane2 = newYMin - openYMax      lane3 = openYMin - newYMax
    // and the pair overlaps in Y and Z iff ALL FOUR are <= 0. (The X axis is handled by the
    // sweep order itself -- this is the "currently open" set.)
    //
    // ⭐ THE ALL-LANES REDUCE, and why it is NOT a lane-0 test. The console spells the
    // reduction as
    //     vcmpgtfp128 v0, v0, v127(=0)   ; lane all-ones iff sum > 0   (NOT overlapping)
    //     vnot        v0, v0             ; lane all-ones iff sum <= 0  (overlapping)
    //     vperm       v0, v0, v0, v7     ; v7 = the vector at .data `unk_8327F110`
    //     vcmpequw128 v0, v0, v126(=-1)
    //     vcmpeqfp128. v0, v0, v127(=0)  ; CR6
    //     mfocrf/extrwi bit 24           ; CR6.LT == "all four lanes equal 0.0"
    //     bne -> skip                    ; i.e. EMIT iff NOT all-zero
    // `unk_8327F110` READS AS 32 ZERO BYTES IN THE IDB (segment .data, perm RW). Taken
    // literally that control would splat byte 0 and the whole test would collapse to lane 0
    // alone. IT DOES NOT: the same constant + the same five-instruction idiom is the
    // compiler's `AllTrue(<vector compare>)` reduce, and its meaning is pinned by an
    // independent, unambiguous user -- BrnAI::IsInsideSectionFast @0x82768680
    // (0x827686E8-0x8276870C) applies the identical sequence to a FOUR-EDGE convex
    // point-in-section test and returns `~CR6.LT & 1`, i.e. "inside" iff all four edge
    // signs agree. A lane-0-only reduce would make that function answer a one-edge
    // question. Five more callers share the constant (BrnCrashTriangleCache x2,
    // BrnDebrisRenderer, RacingLineGenerator x3, cParticleRender, ...). The image byte
    // pattern is therefore a ZERO-INITIALISED .data global written by a dynamic
    // initialiser -- the exact "unrecoverable zero" trap; do not re-derive the semantics
    // from the image bytes.
    //
    // ⚠️ NaN polarity (gotcha 4): the console tests `!(sum > 0)`, so an unordered lane
    // counts as OVERLAPPING. Host `sum <= 0.0f` would answer the opposite for NaN, so the
    // condition below is spelled `!(sum > 0.0f)`.
    void IntervalStack::CheckOverlappingAndAddToQueue(const Interval& lrInterval,
                                                      OverlapPairQueue* lpOverlappingPairQueue) const
    {
        const f32 lfZMin      = lrInterval.mfZMinInterval;       // lane 0
        const f32 lfMinusZMax = lrInterval.mfMinusZMaxInterval;  // lane 1
        const f32 lfYMin      = lrInterval.mfYMinInterval;       // lane 2
        const f32 lfMinusYMax = lrInterval.mfMinusYMaxInterval;  // lane 3

        for (u32 luEntry = 0; luEntry < muLen; ++luEntry)
        {
            const Vector4& lrOpen = mpIntervalData[luEntry].mvZYMaxMin;

            const f32 lfZLow  = lfZMin      + lrOpen.x;  // newZMin - openZMax
            const f32 lfZHigh = lfMinusZMax + lrOpen.y;  // openZMin - newZMax
            const f32 lfYLow  = lfYMin      + lrOpen.z;  // newYMin - openYMax
            const f32 lfYHigh = lfMinusYMax + lrOpen.w;  // openYMin - newYMax

            if (!(lfZLow > 0.0f) && !(lfZHigh > 0.0f) && !(lfYLow > 0.0f) && !(lfYHigh > 0.0f))
            {
                // sub_828B8C60 == BaseEventQueue<OverlappingIntervalPair>::AllocateEvent()
                // (asserts "mpEvents != NULL" / "GetLength() < GetMaxLength()" at
                // CgsBaseEventQueue.h:360/361, element stride 4). The tree spells that member
                // AddEvent() -- see the naming-defect note in CgsBaseEventQueue.h.
                OverlappingIntervalPair& lrPair = lpOverlappingPairQueue->AddEvent();
                lrPair.muObjectIndexA = lrInterval.mu16ObjectIndex;  // sth r10,0(r3)
                lrPair.muObjectIndexB = mpuIndexData[luEntry];       // sth r11,2(r3)
            }
        }
    }
}
