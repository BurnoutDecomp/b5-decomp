#pragma once

// ===================================================================================
// BrnGameState::GameStateModuleIO::TargetEventScore  -- owning header
//   b5-decomp/src/GameSource/GameState/SharedIO/BrnTargetEventScore.h
//
// One persisted "target event score" record (the Burning Route / Stunt Attack target
// results the game modes race against). The progression profile keeps a fixed
// Array<TargetEventScore,49> of these in its X360 DLC-era tail (Profile +118072, count
// word +120032 == 49 * 40); the type is ABSENT from the PS3 DecFIGS DWARF (the whole
// Profile tail is an X360/DLC-era addition), so the X360 asm is the only layout source.
//
// PROMOTED out of the provisional BrnGameStateLeafContainers.h blob record (u8[40]) now
// that the BrnProgression::Profile TU decodes the fields the X360 touches:
//   * Profile::SetTargetEventScore    @0x823714F8 stores a 24-byte by-value block
//     wholesale to +0x00/+0x08/+0x10 (r4..r6 -> three stack qwords -> the record), the
//     event id (r7, 64-bit `std`) to +0x18 and the score word (r8, `stw`) to +0x20.
//   * Profile::GetTargetEvent         @0x82371458  matches records on the CgsID @ +0x18.
//   * Profile::RemoveTargetEventScore @0x82371600  matches records on the CgsID @ +0x18.
//
// The 24-byte head is only ever moved wholesale by the reconstructed functions, so its
// internal fields stay an opaque named block (field names are NOT invented); the X360
// element stride is 40 (every Array<TargetEventScore,49> body strides 40 and reads the
// live-count word at +0x7A8 == 49 * 40).
// ===================================================================================

#include <cstddef>          // offsetof (layout pins)
#include "types.hpp"
#include "BrnCommonTypes.h" // CgsID (u64)

namespace BrnGameState
{
namespace GameStateModuleIO
{
    struct TargetEventScore
    {
        // The 24-byte opaque head Profile::SetTargetEventScore copies in by value (the
        // X360 passes it in r4..r6 and stores it wholesale). Internal layout unrecovered.
        struct OpaqueHead
        {
            u64 maxOpaquePayload[3];   // +0x00 .. +0x17 (24 bytes, moved wholesale only)
        };

        OpaqueHead mHead;      // +0x00 (24 bytes)
        CgsID      mEventId;   // +0x18 (the event this target score belongs to)
        s32        miScore;    // +0x20 (the target score / time word)
        u8         maPad[4];   // +0x24 .. +0x27 -> X360 element stride 40
    };

    // Pointer-free record: X360 offsets == host offsets, so the layout is pinnable.
    static_assert(sizeof(TargetEventScore) == 40,
                  "TargetEventScore must keep the X360 40-byte Array<.,49> element stride");
    static_assert(offsetof(TargetEventScore, mEventId) == 0x18,
                  "TargetEventScore::mEventId must sit at the X360 +0x18 id slot");
    static_assert(offsetof(TargetEventScore, miScore) == 0x20,
                  "TargetEventScore::miScore must sit at the X360 +0x20 score slot");
}
}
