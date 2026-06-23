#pragma once

// ===================================================================================
// BrnGuiSaveLoad::Profile  -- owning header
//   b5-decomp/src/GameSource/Gui/SaveLoad/BrnGuiSaveLoadProfile.h
//
// The player's persisted progression save image (the BrnGuiSaveLoad-namespace, on-disk
// view of the live BrnProgression::Profile). On load BrnGui::ProfileManager::
// ValidateProfiles (@0x825073A0) calls ValidateProfile to confirm the save's version and
// its stored per-resource version manifest against the build's expected manifest.
//
// Layout proven from BURNOUT_X360_ARTIST.XEX ValidateProfile (@0x824EFE30):
//   * miVersion         @+0x000  (lwz 0(r3); compared to 28)
//   * muManifestCount   @+0x268  (lwz 0x268(r3); the live entry count, == a1[154])
//   * maVersionManifest @+0x7070 (the per-resource version table; 8-byte stride, the loop
//                                 walks `id` words at +0 and steps +8)
//
// The full save image is very large (the manifest sits at +0x7070) and the bytes between
// the named members are NOT modelled here -- they belong to a dedicated save-image TU.
// They are kept as reserved byte-spans so each named member lands at its proven offset;
// the manifest's true capacity is the save's resource count (unknown from this TU's single
// reference), so it is declared as a one-element head governed by muManifestCount at run
// time rather than fabricating a capacity. All access is by name.
// ===================================================================================

#include "types.hpp"

namespace BrnGuiSaveLoad
{
    class Profile
    {
    public:
        // The progression save-image version this build understands (X360 cmpwi 0x1C == 28).
        static const s32 KI_VERSION_CURRENT = 28;

        // One per-resource version manifest entry (8-byte stride; the X360 loop reads the id
        // word at +0 and advances 8 bytes). Only the id is read on the profile side.
        struct ManifestEntry
        {
            u32 muId;        // +0x00 (resource id)
            u32 muData;      // +0x04 (manifest payload; unread on the profile side)
        };

        // The build's "expected versions" manifest passed to ValidateProfile. The X360 reads
        // the entry pointer at +0x18 and the entry count at +0x1C; each entry is 16 bytes
        // (the scan steps +0x10) with the id at +0 and a non-zero "valid" flag at +0x04.
        struct ExpectedManifest
        {
            struct Entry
            {
                u32 muId;        // +0x00 (resource id, matched against the stored manifest)
                u32 muFlag;      // +0x04 (must be non-zero for the entry to validate)
                u8  maPad[8];    // +0x08 .. +0x17 -> 16-byte stride
            };

            u8           maReserved[0x18];   // +0x00 .. +0x17 (unmodeled descriptor head)
            const Entry* mpEntries;          // +0x18 (entry array pointer)
            s32          miCount;            // +0x1C (entry count)
        };

        // @0x824EFE30 - validate the save image against lrExpected: require the version word
        // to be current and the stored manifest to match the expected manifest entry-for-entry
        // (same count; every stored id present in the expected set with its valid flag set).
        bool ValidateProfile(const ExpectedManifest& lrExpected);

    private:
        s32 miVersion;            // +0x000 (compared to KI_VERSION_CURRENT)

        u8  maReservedA[0x268 - 0x04];     // +0x004 .. +0x267 (unmodeled save-image body)
        s32 muManifestCount;      // +0x268 (live manifest entry count)

        u8  maReservedB[0x7070 - 0x26C];   // +0x26C .. +0x706F (unmodeled save-image body)
        // Per-resource version manifest. Declared as a one-element head: the true on-disk
        // capacity is the save's resource count; the live length is muManifestCount. The
        // dedicated save-image TU must replace this with the real fixed-capacity array.
        ManifestEntry maVersionManifest[1]; // +0x7070
    };
}
