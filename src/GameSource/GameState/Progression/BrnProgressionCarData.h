#pragma once

// ===================================================================================
// BrnProgression::CarData  -- minimal owning slice
//   b5-decomp/src/GameSource/GameState/Progression/BrnProgressionCarData.h
//
// One persisted "owned car" record in the player's progression profile. The save loader
// splits the live progression car list into base vs DLC arrays via
// BrnProgression::SplitArray<CarData, BrnGuiSaveLoad::CarData> (@0x823696F0), keyed on
// BrnGuiSaveLoad::ProfileDLC1::IsDLCCarId(carRecord).
//
// Layout proven from BURNOUT_X360_ARTIST.XEX:
//   * SplitArray @0x823696F0 copies three 8-byte words per record (sizeof == 24, 0x18
//     stride) and IsDLCCarId reads the 64-bit id at offset 0.
// Only the id field needed by the reconstructed functions is named; the remaining 16
// bytes are reserved (the full record belongs to the Progression Profile/CarData TU).
// All access is by name.
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID (GetId return)

namespace BrnProgression
{
    struct CarData
    {
        // ADDITIVE GROW (declare-only; bodies in the Progression CarData TU).
        // DriveThruManager body/paint shops read this record's car id and write its colour/palette.
        //   GetId()          -> muCarId (the packed car id at +0x00).
        //   SetColourIndex() -> X360 0x82354890. SetPaletteIndex() -> X360 0x823548F0.
        CgsID GetId() const;
        void  SetColourIndex(s32 liColour);
        void  SetPaletteIndex(s32 liPalette);

        // The 64-bit packed car id (X360 ld r11, 0(r3) in IsDLCCarId). Its top 14 bits hold
        // the DLC marker; the rest identify the specific car.
        u64 muCarId;          // +0x00

        // Remaining record payload (unmodeled here; 0x18-byte stride proven by SplitArray's
        // three-qword copy). Replace with the real named members in the CarData TU.
        u8  maReserved[16];   // +0x08 .. +0x17  -> sizeof(CarData) == 24
    };

    static_assert(sizeof(CarData) == 0x18, "CarData must be the 0x18 SplitArray stride");
}
