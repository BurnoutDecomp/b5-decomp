#pragma once

// =============================================================================
// BrnTrafficStopLine.h  (NEW OWNING HEADER)
//
// Home for BrnTraffic::StopLine -- the per-junction stop-line value type of the
// TrafficData traffic-graph (forward-declared as `struct StopLine;` in
// BrnTrafficHull.h, where the hull holds `StopLine* mpaStopLines`).
//
// The X360 retail XEX bakes this exact header path into ConvertToFixed's assert
// string ("...sharedclasses\\traffic\\Junctions/BrnTrafficStopLine.h", line 117),
// which fixes the home directory + file name.
//
// This slice owns ONLY the single attested standalone helper:
//   BrnTraffic::StopLine::ConvertToFixed  @ 0x8274F3C8
//
// ConvertToFixed is a static utility (no `this` use in the asm -- the float comes
// in f1 and the only memory touched is the spill slot for the fctidz result). It
// converts a lane-parameter float in [0, 256) into an 8.8 unsigned fixed-point
// value, the on-disk packing the stop-line records use for their lane parameter.
//
// The record is one u16:
//   * leak references/Feb-2007/BrnEntityModuleUnity/SharedClasses/Traffic/Junctions/
//     BrnTrafficStopLine.h -- `uint16_t muParamFixed;` plus the compile-time
//     `sizeof( StopLine ) == 2` check, and the four inline accessors below;
//   * PS3 DWARF SharedClasses/Traffic/Junctions/BrnTrafficStopLine.h -- the retail
//     type carries the same single private uint16_t muParamFixed at :74;
//   * X360 Hull::GetStopLine @0x82705C20 indexes `mpaStopLines + 2*luIndex`
//     (`slwi r10, r30, 1`), and Section::FindNextStopLineIndex @0x82752A38 reads the
//     element as a bare `lhzx` off Hull::mpaStopLines.
// =============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnTraffic
{
    // BrnTrafficStopLine.h:36 (DWARF) -- per-junction stop-line record, 2 bytes.
    // The lane parameter is 8.8 unsigned fixed point: the high byte is the segment
    // (rung) index along the section, the low byte the fraction within that segment.
    struct StopLine
    {
        // :41 -- the whole packed value (segment.fraction in 8.8).
        u16 GetParameterAlongSection() const { return muParamFixed; }

        // :45 -- the segment (rung) index, i.e. the integer part.
        u16 GetSegmentAlongSection() const { return static_cast<u16>(muParamFixed >> 8); }

        // ConvertToFixed @ 0x8274F3C8
        // Pack a lane parameter (a float, asserted < 256.0f) into 8.8 unsigned fixed
        // point: multiply by 256.0f, truncate toward zero, keep the low 16 bits.
        //   asm: assert(lfFloat < 256.0f);
        //        return (u16)(s64)(lfFloat * 256.0f);   // fmuls + fctidz + low halfword
        static u16 ConvertToFixed(f32 lfFloat)
        {
            CGS_ASSERT(lfFloat < 256.0f, "lfFloat < 256.0f");   // :117
            return static_cast<u16>(lfFloat * 256.0f);
        }

        // :56 -- the inverse. Folded at every X360 call site (DoesParamNeedToStopForStopline
        // @0x82724B58 emits it as `luFixed * 0.00390625`).
        static f32 ConvertToFloat(u16 luFixed) { return static_cast<f32>(luFixed) * (1.0f / 256.0f); }

        // :61 -- declared only; the resource endian-swap TU owns the body.
        void EndianSwap();

    private:
        u16 muParamFixed;   // :74
    };

    // Leak `typedef char __TmpArray[(sizeof( StopLine ) == 2)];` and the X360
    // Hull::GetStopLine stride. Load-bearing: mpaStopLines is indexed by name now.
    static_assert(sizeof(StopLine) == 2, "BrnTraffic::StopLine stride (Hull::GetStopLine)");
}
