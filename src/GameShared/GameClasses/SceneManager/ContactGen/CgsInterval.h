#pragma once

// CgsSceneManager::OverlappingIntervalPair — a broadphase overlap between two object
// intervals (their indices). Reconstructed from the DecFIGS DWARF.
//
// CgsSceneManager::Interval — one object's swept-axis interval used by the sweep-and-prune
// broadphase (CgsIntervalList / CgsIntervalStack). Field set + offsets recovered from the
// DecFIGS DWARF (CgsInterval.h:155-161). The five interval floats are the per-axis min/max
// bounds packed for the SoA sweep; the trailing u16 object index + flags identify the owner
// and the min/max sentinel role.
#include "types.hpp"

namespace CgsSceneManager
{
    struct OverlappingIntervalPair
    {
        u16 muObjectIndexA;
        u16 muObjectIndexB;
    };

    // Interval — DWARF CgsInterval.h:72. 24 bytes (0x18): five f32 axis bounds + a u16
    // object index + a u16 flags word. The IntervalStack::Push @0x828AA158 reads these by
    // the byte offsets the asm attests:
    //   +0x00  mfXInterval          (lfs 0x00 — not touched by Push)
    //   +0x04  mfZMinInterval       (lfs 0x04(a2) -> stack lane[1])
    //   +0x08  mfMinusZMaxInterval  (lfs 0x08(a2) -> stack lane[0])
    //   +0x0C  mfYMinInterval       (lfs 0x0C(a2) -> stack lane[3])
    //   +0x10  mfMinusYMaxInterval  (lfs 0x10(a2) -> stack lane[2])
    //   +0x14  mu16ObjectIndex      (lhz 0x14(a2) -> mpuIndexData[len])
    //   +0x16  mu16Flags
    struct Interval
    {
        f32 mfXInterval;          // +0x00
        f32 mfZMinInterval;       // +0x04
        f32 mfMinusZMaxInterval;  // +0x08
        f32 mfYMinInterval;       // +0x0C
        f32 mfMinusYMaxInterval;  // +0x10
        u16 mu16ObjectIndex;      // +0x14
        u16 mu16Flags;            // +0x16
    };
}
