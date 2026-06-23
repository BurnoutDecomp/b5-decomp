#pragma once

// ===================================================================================
// BrnGuiSaveLoad::ProfileDLC1  -- owning header
//   b5-decomp/src/GameSource/Gui/SaveLoad/BrnGuiSaveLoadProfileDLC1.h
//
// The DLC-pack-1 slice of the player's persisted save-load profile image. Stored inside
// the save profile; on load BrnGui::ProfileManager::ValidateProfiles (@0x825073A0) calls
// ValidateProfile to version-check / migrate it. This is the on-disk SAVE-LOAD twin of
// the live progression DLC1 data (the BrnGuiSaveLoad namespace is the save image's view
// of the BrnProgression types; see the BrnProgression::SplitArray<T, BrnGuiSaveLoad::T>
// fan-out the loader runs).
//
// Layout proven from BURNOUT_X360_ARTIST.XEX:
//   * Construct       @0x824FF400 - resets the profile to defaults: clears the version
//       word and a fixed set of save-image fields. The X360 stores are:
//         stw 0 @+0x7B0, stw 0 @+0xAC8, std 0 @+0x2628, std 0 @+0x2630,
//         XMemSet(+0x2638,0,4), (+0x263C,0,4), (+0x2640,0,4), (+0x2644,0,4).
//       (Note: the +0 version word is NOT written by Construct; ValidateProfile sets it
//        implicitly by treating a freshly-Constructed profile as version-current.)
//   * IsDLCCarId      @0x824EFF30 - static: true when a car id's top 14 bits == 0x2957
//       (the DLC marker). Reads the 64-bit id at the start of a CarData record.
//   * ValidateProfile @0x824FF480 - version-check after load: if the version word is the
//       uninitialised sentinel 0x7FFFFFFF, warn + Construct (upgrade) and succeed; else if
//       it is not the current version 6, Construct (reset) + log "expected 6, got <v>";
//       otherwise succeed. Always returns true.
//
// The full save-image body is large and belongs to a dedicated TU; only the fields the
// reconstructed functions touch are named here, with reserved byte-spans between them so
// each named member lands at its proven offset. All access is by name.
// ===================================================================================

#include "types.hpp"
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"  // BrnProgression::CarData (IsDLCCarId)

namespace BrnGuiSaveLoad
{
    class ProfileDLC1
    {
    public:
        // Stored in miVersion for a profile that predates DLC1 and has never been
        // initialised by it (X360 lis 0x7FFF; ori 0xFFFF).
        static const s32 KI_VERSION_UNINITIALISED = 0x7FFFFFFF;

        // The one concrete DLC1 save-image version this build understands (X360 cmpwi 6).
        static const s32 KI_VERSION_CURRENT = 6;

        // @0x824EFF30 - true when lrCar's id marks it as a DLC car. The X360 keeps the id as
        // a 64-bit packed value whose top 14 bits hold the DLC marker 0x2957.
        static bool IsDLCCarId(const BrnProgression::CarData& lrCar);

        // @0x824FF400 - reset the DLC1 save image to defaults (clear the tracked fields).
        void Construct();

        // @0x824FF480 - validate / migrate the DLC1 profile after load. Always returns true.
        bool ValidateProfile();

    private:
        // The DLC marker stored in a DLC car id's top 14 bits (X360 li 0x2957; sldi 50).
        static const u64 KU_DLC_CAR_ID_MARKER = 0x2957ull;

        s32 miVersion;            // +0x00 (version word; compared in ValidateProfile)

        u8  maReservedA[0x7B0 - 0x04];     // +0x04 .. +0x7AF  (unmodeled save-image body)
        u32 muField_7B0;          // +0x7B0  (cleared by Construct)

        u8  maReservedB[0xAC8 - 0x7B4];    // +0x7B4 .. +0xAC7
        u32 muField_AC8;          // +0xAC8  (cleared by Construct)

        u8  maReservedC[0x2628 - 0xACC];   // +0xACC .. +0x2627
        u64 muField_2628;         // +0x2628 (cleared by Construct, 8 bytes)
        u64 muField_2630;         // +0x2630 (cleared by Construct, 8 bytes)
        u32 muField_2638;         // +0x2638 (XMemSet 0)
        u32 muField_263C;         // +0x263C (XMemSet 0)
        u32 muField_2640;         // +0x2640 (XMemSet 0)
        u32 muField_2644;         // +0x2644 (XMemSet 0)
    };
}
