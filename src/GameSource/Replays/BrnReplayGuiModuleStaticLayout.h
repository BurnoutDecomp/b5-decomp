#pragma once

// Canonical home for BrnReplays::GuiModuleStaticLayout.
//
// The GUI module's replay "static layout" record: the 992-byte (0x3E0) block the
// GuiModuleSerialiser persists/restores per replay (BrnReplayGuiModuleSerialiser.cpp:
// Construct passes nStaticSize == 992; Read/Write transfer 992 bytes). This TU owns the
// per-frame/lifecycle mutators the rest of the GUI/replay code calls on that block:
//   Reset             @ 0x8264CDF0  full reset (recording first-use; called by Write())
//   SetupStartOfFrame @ 0x8264CEB0  per-frame double-buffer flip + current-buffer clear
//   StartMessage      @ 0x8264CF70  raise the in-game-message active/start flags
//   EndMessage        @ 0x8264CF80  drop the message flags + clear the 3 message slots
//
// LAYOUT (ARTIST assembly; fields without a recovered source name remain projections).
// Message slots0..17F, start/active bytes180/181, partial per-car records190..28F,
// and the eight whole 64-bit values290..2CF retain their existing owners and widths.
// Native Update82529984..825299A0 writes actual CgsNetwork::PlayerName objects
// into the two 16-byte-stride name buffers2D0..3CF. The existing AboveCar assertion
// attests the original field name maaGamertags. Frame parity remains one byte3D0.
// The record stays pointer-free and 992 bytes; no Apt or host-pointer layout is inferred.

#include "types.hpp"
#include "GameSource/GameState/BrnCgsPlayerName.h" // actual 16-byte name owner

namespace BrnReplays
{
    // ---- per-active-race-car 32-byte record (zeroed by Reset/SetupStartOfFrame) ----
    // The zeroing touches: a 16-byte block @+0x00 (stvx128), a word @+0x14, a byte @+0x18.
    // Interior field identities are unrecovered -> opaque storage at the asm-attested width.
    struct GuiModuleCarRecordA
    {
        u8 maStorage[0x20];   // 32 bytes (X360 loop stride 0x20)
    };

    // ---- in-game-message slot (128-byte stride; EndMessage zeroes its leading byte) ----
    // Only byte +0 is asm-attested (EndMessage clears it; XMemSet zeroes the whole block in
    // Reset). The remaining 127 bytes are opaque message payload.
    struct GuiModuleMessageSlot
    {
        u8 maStorage[0x80];   // 128 bytes (X360 EndMessage stride 0x80)
    };

    // Reconstructed from BURNOUT_X360_ARTIST.XEX. 992-byte (0x3E0) GUI replay static layout.
    struct GuiModuleStaticLayout
    {
        static const s32 KI_NUM_MESSAGE_SLOTS    = 3;   // EndMessage loops 3 (stride 0x80)
        static const s32 KI_MAX_ACTIVE_RACE_CARS = 8;   // == E_ACTIVE_RACE_CAR_INDEX_COUNT

        // ---- owned mutators (this TU) ----
        GuiModuleStaticLayout* Reset();                 // X360 0x8264CDF0
        GuiModuleStaticLayout* SetupStartOfFrame();     // X360 0x8264CEB0
        void                   StartMessage();          // X360 0x8264CF70
        GuiModuleStaticLayout* EndMessage();            // X360 0x8264CF80

        static void _AssertLayout();

        // ---- layout (X360 asm authoritative) ----
        GuiModuleMessageSlot maMessageSlots[KI_NUM_MESSAGE_SLOTS];   // +0x000 (3 * 0x80 = 0x180)
        u8                   mbMessageStart;                         // +0x180
        u8                   mbMessageActive;                        // +0x181
        u8                   maReserved0x182[0x190 - 0x182];         // +0x182 (gap)
        GuiModuleCarRecordA  maCarRecordsA[KI_MAX_ACTIVE_RACE_CARS]; // +0x190 (8 * 0x20 = 0x100)
        u64                  mauCarRecordsC[KI_MAX_ACTIVE_RACE_CARS];// +0x290 (8 * 0x08 = 0x40)
        CgsNetwork::PlayerName maaGamertags[2][KI_MAX_ACTIVE_RACE_CARS]; // +0x2D0/+0x350
        u8                   mbFrameParity;                          // +0x3D0
        u8                   maReserved0x3D1[0x3E0 - 0x3D1];         // +0x3D1 (pad to 992)
    };
}
