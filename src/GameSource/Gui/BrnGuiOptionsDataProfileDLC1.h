#ifndef BRN_GUI_OPTIONS_DATA_PROFILE_DLC1_H
#define BRN_GUI_OPTIONS_DATA_PROFILE_DLC1_H

#include "types.hpp"   // s32, u8

// BrnGui::OptionsDataProfileDLC1 - the first DLC pack's slice of a player's
// persisted options/settings profile. Stored in the save profile; on load the
// profile manager calls ValidateProfile to migrate / sanity-check the version.
//
// Layout proven from BURNOUT_X360_ARTIST.XEX @ 0x824F0C38 (ValidateProfile):
//   +0x00  miVersion   s32  (`lwz r29, 0(r31)`; compared to 0x7FFFFFFF and 1)
//   +0x04  miReserved  s32  (`stw r11, 4(r31)` -> 0 on reset)
//   +0x08  maFlags[8]  u8   (`stb r11, 8..0xF(r31)` -> 0 on reset)
// The 0x7FFFFFFF sentinel marks an un-upgraded (never DLC1-initialised) profile;
// the only currently-valid concrete version is 1. Member access is by name.
namespace BrnGui
{
    class OptionsDataProfileDLC1
    {
    public:
        // Sentinel stored in miVersion for a profile that predates DLC1 and has
        // never been initialised by it. (`lis r11,0x7FFF; ori r11,0xFFFF`.)
        static const s32 KI_VERSION_UNINITIALISED = 0x7FFFFFFF;

        // The one concrete DLC1 profile version this build understands.
        static const s32 KI_VERSION_CURRENT = 1;

        // 0x824F0C38 - validate/migrate the DLC1 profile after load. If the
        // version is the uninitialised sentinel, warn and reset to defaults
        // (version 1, all flags clear) and report success. If it is already the
        // current version, succeed. Any other version is an unrecognised
        // mismatch: log "expected 1, got <v>" and fail.
        bool ValidateProfile();

    private:
        s32 miVersion;       // +0x00
        s32 miReserved;      // +0x04
        u8  maFlags[8];      // +0x08 .. +0x0F
    };
}

#endif
