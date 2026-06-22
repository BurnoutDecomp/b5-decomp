#pragma once

// ===================================================================================
// BrnGui::GuiEventNetworkGameParams  -- owning header
//   b5-decomp/src/GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h
//
// The network "game parameters" GUI event: a fixed array of per-option records
// followed by a block of scalar option/count fields. Only Construct (@0x824820C8) is
// binary-recovered for this TU; it default-initialises the record array and seeds the
// scalar option block with the game's default online-match parameters.
//
// No prior source carries this struct's member layout (the DecFIGS DWARF forward-
// declares BrnGui::GuiEventNetworkGameParams only). The shape below is recovered from
// the X360 Construct store sequence (anchors r3+0x24 array loop, stride 0x2C, 10 reps;
// scalar stores at r3+0x1B8..0x1DF). Members are named and accessed by name; the
// option record carries explicit reserved padding so the 0x2C (44-byte) stride that
// the X360 loop walks is preserved without raw-offset casting.
// ===================================================================================

#include "types.hpp"

namespace BrnGui
{
    struct GuiEventNetworkGameParams
    {
        // Number of option records the X360 Construct loop initialises (10 reps).
        static const s32 KI_NUM_OPTION_RECORDS = 10;

        // One configurable-option record. The X360 loop writes three words per record
        // (the selected value, a low bound, and a high/flag word) and advances by 0x2C
        // bytes, so the record is 44 bytes; the unwritten tail is preserved as reserved
        // storage so the per-record stride matches the binary.
        struct OptionRecord
        {
            // Asm Construct @0x824820C8 writes (anchor v1 = this record's +4):
            //   *(v1-1)=-1 -> miSelected@+0, *v1=0 -> miFlags@+4, v1[1]=0 -> miValue@+8.
            // Fields ordered to match so Construct's by-name writes hit the right slots.
            s32 miSelected;       // @ rec+0x00 (asm *(v1-1)) -> -1
            s32 miFlags;          // @ rec+0x04 (asm *v1)     -> 0
            s32 miValue;          // @ rec+0x08 (asm v1[1])   -> 0
            u8  maReserved[32];   // unwritten tail; preserves the 0x2C record stride
        };

        // Leading pad up to the option-record array (the X360 array anchor is r3+0x24).
        u8  maHeaderReserved[32];

        OptionRecord maOptionRecords[KI_NUM_OPTION_RECORDS];

        // Scalar option / count block seeded by Construct (X360 r3+0x1B8 .. r3+0x1DF).
        s32 miMaxPlayers;         // r3+0x1B8 -> 10
        s32 miGameMode;           // r3+0x1BC -> 18
        s32 miCountA;             // r3+0x1C0 -> 0
        s32 miCountB;             // r3+0x1C4 -> 0
        s32 miCountC;             // r3+0x1C8 -> 0
        s32 miCountD;             // r3+0x1CC -> 0
        s32 miOptionE;            // r3+0x1D0 -> 1  (word)
        s32 miOptionF;            // r3+0x1D4 -> 9  (word)
        s32 miOptionG;            // r3+0x1D8 -> 3  (word)
        u8  mbFlagH;              // r3+0x1DC -> 1  (byte)
        u8  mbFlagI;              // r3+0x1DD -> 1  (byte)
        u8  mbFlagJ;              // r3+0x1DE -> 1  (byte)
        u8  mbFlagK;              // r3+0x1DF -> 1  (byte)

        // Owned by THIS TU: default-construct the event (@0x824820C8).
        void Construct();
    };
}
