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
// LAYOUT (X360 asm authoritative; no DWARF for this type). Derived store-for-store from the
// four bodies; field semantics beyond the asm-attested strides/offsets are NOT recoverable,
// so the regions are modelled as named arrays sized to the asm-proven strides and FLAGGED:
//
//   +0x000  MessageSlot maMessageSlots[3]      3 slots, 128-byte stride (XMemSet 0..0x180;
//                                               EndMessage zeroes byte +0 of each)
//   +0x180  u8          mbMessageStart          StartMessage=1 / EndMessage(no) / Reset=0 /
//                                               SetupStartOfFrame=0
//   +0x181  u8          mbMessageActive         StartMessage=1 / EndMessage=0 / Reset=0
//   +0x182  u8          maReserved0x182[0x0E]   untouched gap up to the per-car block
//   +0x190  CarRecordA  maCarRecordsA[8]        32-byte stride; Reset/SetupStartOfFrame zero
//                                               each (16B vector @+0, word @+0x14, byte @+0x18)
//   +0x290  u64         mauCarRecordsC[8]        8-byte stride; zeroed by Reset/SetupStartOfFrame
//   +0x2D0  u8          maCarFlagsBuffer0[8][16] 16-byte stride; double-buffer 0 (zeroed when the
//                                               frame-parity flag selects it)
//   +0x350  u8          maCarFlagsBuffer1[8][16] 16-byte stride; double-buffer 1
//   +0x3D0  u8          mbFrameParity            SetupStartOfFrame toggles (1 - prev); Reset=0;
//                                               selects which CarFlags buffer is current
//   +0x3D1  u8          maReserved0x3D1[0x0F]   pad the record to 992 bytes (0x3E0)
//
// FLAG (X360-asm-proven, DWARF-silent): every region/field name above is inferred from its
// asm usage, not from DWARF or any recovered source. The 8-element arrays are per active race
// car (the loops assert leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT == 8). The CarRecordA
// element's interior (the 16-byte vector + word + byte the zeroing touches) is modelled as
// opaque storage at the asm offsets; the CarFlags double-buffer is two parallel 8x16 blocks the
// frame-parity flag selects between. Promote names when a reader of these fields is reconstructed.

#include "types.hpp"

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
        u8                   maCarFlagsBuffer0[KI_MAX_ACTIVE_RACE_CARS][16]; // +0x2D0 (8 * 0x10)
        u8                   maCarFlagsBuffer1[KI_MAX_ACTIVE_RACE_CARS][16]; // +0x350 (8 * 0x10)
        u8                   mbFrameParity;                          // +0x3D0
        u8                   maReserved0x3D1[0x3E0 - 0x3D1];         // +0x3D1 (pad to 992)
    };
}
