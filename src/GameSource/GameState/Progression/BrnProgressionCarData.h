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
        // DWARF BrnProfile.h:92 (UnlockType enum).
        enum UnlockType
        {
            E_UNLOCK_TYPE_UNLOCK        = 0,
            E_UNLOCK_TYPE_GIFT          = 1,
            E_UNLOCK_TYPE_TROPHY        = 2,
            E_UNLOCK_TYPE_SHUTDOWN_RIVAL = 3,
            E_UNLOCK_TYPE_GOLD_SILVER   = 4,
            E_UNLOCK_TYPE_SPONSOR       = 5
        };

        // The full member layout is now named (proven by Profile::AddCar @0x82366C80, which
        // inlines the per-car init: std id @+0x00, stb 0xFF @+0x08/+0x09, stb 0 @+0x0A,
        // stfs 0.0 @+0x0C, stw unlockType @+0x10). Construct(CgsID) is the X360 CarData
        // initialiser the Progression TU owns; AddCar open-codes the same stores.
        void Construct(CgsID lId);

        // GetId()          -> mId (the packed car id at +0x00).
        // SetColourIndex() -> X360 0x82354890. SetPaletteIndex() -> X360 0x823548F0.
        CgsID GetId() const;
        void  SetColourIndex(s32 liColour);
        void  SetPaletteIndex(s32 liPalette);

        //   IsHiddenFromUnlockSequence() / WasUnlockSequenceAlreadyShown() -> mbUnlockSequenceAlreadyShown @+0x0A.
        //   GetUnlockType() / SetUnlockType()  -> meUnlockType @+0x10.
        //   GetUnlockDeformationAmount() / SetUnlockDeformationAmount() -> mfUnlockDeformedAmount @+0x0C.
        bool       IsHiddenFromUnlockSequence() const;
        bool       WasUnlockSequenceAlreadyShown() const;
        void       SetUnlockSequenceAlreadyShown();
        UnlockType GetUnlockType() const;
        void       SetUnlockType(UnlockType leType);
        f32        GetUnlockDeformationAmount() const;
        void       SetUnlockDeformationAmount(f32 lfAmount);

        // Layout (DWARF BrnProfile.h:151..158, offsets X360-proven by AddCar). All access by name.
        CgsID      mId;                            // +0x00  packed car id (8 bytes)
        u8         mu8ColourIndex;                 // +0x08  (AddCar inits 0xFF)
        u8         mu8PaletteIndex;                // +0x09  (AddCar inits 0xFF)
        bool       mbUnlockSequenceAlreadyShown;   // +0x0A
        // +0x0B: one byte trailing pad (mfUnlockDeformedAmount is 4-aligned at +0x0C)
        u8         mPad0B;                          // +0x0B
        f32        mfUnlockDeformedAmount;         // +0x0C
        UnlockType meUnlockType;                   // +0x10  (4 bytes)
        // +0x14: 4 bytes trailing pad -> 0x18 (SplitArray three-qword stride)
        u32        mPad14;                          // +0x14 .. +0x17
    };

    static_assert(sizeof(CarData) == 0x18, "CarData must be the 0x18 SplitArray stride");
}
