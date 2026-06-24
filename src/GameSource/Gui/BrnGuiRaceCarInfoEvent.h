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
// DoWorstCase is a heavy VMX lowering (per-entry vmaddfp128 fused multiply-add transform
// of the position lane against an input vector + a {0,0.1,0.25,0} weighting vector); it
// is left declared-only and the TU is BLOCKED on it (an honest VMX-math boundary). The
// scalar constructor is reconstructed here.
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

        // @ 0x823A6BB8 -- recompute the per-entry "worst case" screen positions. Declared-
        // only: the body is a per-entry VMX vmaddfp128 transform (BLOCKED -- see banner).
        GuiRaceCarInfoEvent* DoWorstCase(s32 liNumActive);

    private:
        Vector4 maPosition[KI_NUM_ENTRIES];  // @0x00 -- per-entry screen-space position
        u64     maIdentity[KI_NUM_ENTRIES];  // @0x80 -- per-entry identity qword
        s32     miNumEntries;                // @0xC0 -- active entry count
        u8      maFlagA[KI_NUM_ENTRIES];     // @0xC4
        u8      maFlagB[KI_NUM_ENTRIES];     // @0xCC
        u8      maFlagC[KI_NUM_ENTRIES];     // @0xD4
    };
}
