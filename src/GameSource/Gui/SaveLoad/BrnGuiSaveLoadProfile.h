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
//
// The image is 118064 bytes end to end: BrnProgression::Profile::Serialise (@0x8237C1F0)
// opens with `memset(a2, 0, 118064)` over it, and ProfileManager::Construct's debug print
// names the size against this class ("Size of BrnGuiSaveLoad::Profile 118064"). The tail
// reservation below carries the object out to that width so the whole image is one object.
// ===================================================================================

#include "types.hpp"

namespace BrnGuiSaveLoad
{
    class Profile
    {
    public:
        // The progression save-image version this build understands (X360 cmpwi 0x1C == 28).
        static const s32 KI_VERSION_CURRENT = 28;

        // The full serialised image width (X360 `memset(a2, 0, 118064)` at the head of
        // BrnProgression::Profile::Serialise @0x8237C1F0).
        static const s32 KI_IMAGE_SIZE_BYTES = 118064;

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

        // OUTLINED from BrnProgression::Profile::Serialise @0x8237C1F0 -- the two stores it
        // makes to this image that do NOT come from the live profile:
        //     memset(a2, 0, 118064);   // (prologue) clear the whole progression save image
        //     *a2 = 28;                // (prologue) stamp the save-image version word
        // Serialise is the ONLY writer of the version word -- ProfileManager::Bootup runs
        // ReadProfileData (-> Serialise) before the storage boot-up, which is how a console
        // FIRST boot with no save on the memory unit still presents a version-current image
        // to ValidateProfiles. Reconstructing it as a method keeps that store off a raw
        // offset poke into the byte image.
        void ConstructImage();

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

        // +0x7078 .. +0x1CD2F -- the rest of the serialised image (the split car / livery /
        // rival tables, the freeburn-challenge and mugshot galleries, ...). Un-modelled here;
        // reserved so the object spans the whole 118064-byte image Serialise clears.
        u8 maReservedC[KI_IMAGE_SIZE_BYTES - 0x7078];
    };
}
