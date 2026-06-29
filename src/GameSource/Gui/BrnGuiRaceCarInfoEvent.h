#pragma once

// ===================================================================================
// BrnGui::GuiRaceCarInfoEvent  -- owning header
//   b5-decomp/src/GameSource/Gui/BrnGuiRaceCarInfoEvent.h
//
// The per-frame "race car info" GUI event: a fixed set of E_ACTIVE_RACE_CAR_INDEX_COUNT
// (8) entries, one per active race car, that BrnGameModule::BridgeWorldVehicleDataToGui
// fills from the world vehicle data. Each entry carries a screen-space position vector
// plus an identity qword and three per-entry flag bytes.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct    @ 0x823A7658 -- zero-init all 8 entries + the count.  [BODIED]
//   DoWorstCase  @ 0x823A6BB8 -- per-entry VMX position transform.     [BLOCKED, declared-only]
//
// The X360 lays the event out as parallel per-entry arrays (the Construct zero-init
// loop / DoWorstCase per-index stores attest the offsets and store widths):
//   maPosition[8]   @0x00..0x7F  (16-byte VMX vectors; Construct stvx128 zero, stride 16)
//   maIdentity[8]   @0x80..0xBF  (8-byte; Construct std 0, stride 8)
//   miNumEntries    @0xC0        (word; Construct stw 0; DoWorstCase sets 8)
//   maFlagA[8]      @0xC4..0xCB  (byte; Construct stb 0, stride 1; DoWorstCase sets 1)
//   maFlagB[8]      @0xCC..0xD3  (byte; Construct stb 0, stride 1)
//   maFlagC[8]      @0xD4..0xDB  (byte; Construct stb 0, stride 1)
//
// DoWorstCase is a per-entry VMX lowering (per-entry vmaddfp128 fused multiply-add of the
// source position lane against an input vector + a {0,0.1,0.25,0} weighting vector). It is
// reconstructed here by scalarizing the lane math (following the committed
// GuiEventUpdateSatNav::DoWorstCase precedent in BrnGuiEventTypeDefs.cpp): the X360 used VMX
// (lvx128/vmaddfp128/stvx128) to move/transform the 16-byte position lanes; reproduced as
// scalar Vector4 lane assignments (semantic parity, endian-independent). The weight
// immediates {0,0.1,0.25,0} and the 2.0 input scale are function-baked float-pool constants,
// recovered from the asm.
//
// LAYOUT NOTE: DoWorstCase writes FIVE per-entry flag-byte arrays (X360 byte offsets
// 0xC4/0xCC/0xD4/0xDC/0xE4); Construct only zero-inits the first three. The header is grown
// to model all five at their X360-proven offsets so DoWorstCase can write them and the struct
// is correctly sized.
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                 // Vector4
#include "GameSource/BurnoutConstants.h"    // E_ACTIVE_RACE_CAR_INDEX_COUNT

namespace BrnGui
{
    class GuiRaceCarInfoEvent
    {
    public:
        // Entry count == number of active race cars (BurnoutConstants.h).
        static const s32 KI_NUM_ENTRIES = E_ACTIVE_RACE_CAR_INDEX_COUNT; // 8

        // @ 0x823A7658 -- zero every entry's position, identity and flag bytes, and clear
        // the entry count. Returns `this`.
        GuiRaceCarInfoEvent* Construct();

        // @ 0x823A6BB8 -- recompute every entry's "worst case" screen position. For each
        // entry index 0..7 (skipping liNumActive, whose entry is the source/template), the
        // entry position is set to a per-lane fused-multiply-add of the input vector lvInput,
        // the source position maPosition[liNumActive] and the {0,0.1,0.25,0} weighting; the
        // identity is copied from entry 0; flag arrays A/D are set to 1, B/C/E to 0. Finally
        // the entry count is set to 8. Returns `this`. (X360 passes lvInput in a VMX register
        // and liNumActive in a GPR.)
        GuiRaceCarInfoEvent* DoWorstCase(Vector4 lvInput, s32 liNumActive);

    private:
        Vector4 maPosition[KI_NUM_ENTRIES];  // @0x00 -- per-entry screen-space position
        u64     maIdentity[KI_NUM_ENTRIES];  // @0x80 -- per-entry identity qword
        s32     miNumEntries;                // @0xC0 -- active entry count
        u8      maFlagA[KI_NUM_ENTRIES];     // @0xC4 (Construct stb 0; DoWorstCase stb 1)
        u8      maFlagB[KI_NUM_ENTRIES];     // @0xCC (Construct stb 0; DoWorstCase stb 0)
        u8      maFlagC[KI_NUM_ENTRIES];     // @0xD4 (Construct stb 0; DoWorstCase stb 0)
        u8      maFlagD[KI_NUM_ENTRIES];     // @0xDC (DoWorstCase stb 1; not touched by Construct)
        u8      maFlagE[KI_NUM_ENTRIES];     // @0xE4 (DoWorstCase stb 0; not touched by Construct)
    };
}
