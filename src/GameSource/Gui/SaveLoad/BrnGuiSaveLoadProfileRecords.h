#pragma once

// ===================================================================================
// BrnGuiSaveLoad save-image record twins
//   b5-decomp/src/GameSource/Gui/SaveLoad/BrnGuiSaveLoadProfileRecords.h
//
// The on-disk (save-load namespace) twins of the live BrnProgression progression records.
// On save, BrnProgression::Profile::Serialise splits each live progression array into a
// "base game" run and a "DLC" run via BrnProgression::SplitArray<TSrc, TDst>, copying each
// live record verbatim into its save-image twin. The split functions copy the record
// bytewise (whole-record qword/dword copies proven in BURNOUT_X360_ARTIST.XEX), so only the
// per-record STRIDE is load-bearing on the destination side.
//
// Strides are X360-proven by the four SplitArray instantiations:
//   * SplitArray<CarData,     BrnGuiSaveLoad::CarData>     @0x823696F0 -> 0x18 (3 qwords)
//   * SplitArray<LiveryData,  BrnGuiSaveLoad::LiveryData>  @0x823697C0 -> 0x18 (3 qwords)
//   * SplitArray<RivalData,   BrnGuiSaveLoad::RivalData>   @0x82369890 -> 0x38 (7 qwords)
//   * SplitArray<ProfileEvent,BrnGuiSaveLoad::ProfileEvent>@0x82369988 -> 0x08 (2 dwords)
//
// The internal field layout of the save-image twins is NOT modelled here (it belongs to a
// dedicated save-image TU); each twin is a correctly-sized opaque copy target so the split
// functions land each record at its proven stride without fabricating field semantics.
// ===================================================================================

#include "types.hpp"

namespace BrnGuiSaveLoad
{
    // Save-image twin of BrnProgression::CarData (0x18 stride, three-qword copy).
    struct CarData
    {
        u8 maRaw[0x18];
    };
    static_assert(sizeof(CarData) == 0x18, "BrnGuiSaveLoad::CarData must be the 0x18 SplitArray stride");

    // Save-image twin of BrnProgression::LiveryData (0x18 stride, three-qword copy).
    struct LiveryData
    {
        u8 maRaw[0x18];
    };
    static_assert(sizeof(LiveryData) == 0x18, "BrnGuiSaveLoad::LiveryData must be the 0x18 SplitArray stride");

    // Save-image twin of BrnProgression::RivalData (0x38 stride, seven-qword copy).
    struct RivalData
    {
        u8 maRaw[0x38];
    };
    static_assert(sizeof(RivalData) == 0x38, "BrnGuiSaveLoad::RivalData must be the 0x38 SplitArray stride");

    // Save-image twin of BrnProgression::ProfileEvent (0x08 stride, two-dword copy).
    struct ProfileEvent
    {
        u8 maRaw[0x08];
    };
    static_assert(sizeof(ProfileEvent) == 0x08, "BrnGuiSaveLoad::ProfileEvent must be the 0x08 SplitArray stride");
}
