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
}
