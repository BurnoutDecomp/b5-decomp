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

// The build's "expected manifest" is the authored progression data itself -- see the
// ValidateProfile note below. Only pointer/reference use is needed here, but the body reads
// its junction table, so the owning header is included rather than forward-declared.
#include "SharedClasses/Progression/BrnProgressionData.h"   // BrnProgression::ProgressionData

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

        // ⭐⭐ [one-profile wave 2026-08-28] THE "EXPECTED MANIFEST" IS `BrnProgression::
        // ProgressionData`, AND THE LOCAL `ExpectedManifest` FORK IS DELETED. The X360 reads
        // the entry pointer at desc+0x18 and the entry count at desc+0x1C and walks the
        // entries with a 16-byte stride, matching an id word at +0 and requiring +0x04
        // non-zero. Those are, exactly:
        //     ProgressionData::muaEventJunctions   @0x18   (EventJunction[], 16-byte stride)
        //     ProgressionData::muEventJunctionCount@0x1C
        //     EventJunction::muID                  @0x00
        //     EventJunction::muOfflineEventOffset  @0x04   (the "valid" flag == has an offline event)
        // and DecFIGS DWARF spells the parameter outright, three times:
        //     bool ValidateProfile(const BrnProgression::ProgressionData *) const;
        //         (BrnProfile.h:679 / :1582 / :2451)
        // ⛔ WHY THE FORK HAD TO GO, not just be renamed: it modelled the entry array as a
        // native `const Entry*` at +0x18, which is 8 bytes on x64 -- so `miCount` sat at +0x20
        // instead of +0x1C and the entry pointer read two SERIALISED 32-BIT SLOTS as one host
        // pointer. ProgressionData is an 0x50 serialised record whose table words stay 32-bit
        // and are rebased through TableFromSlot (static_assert(sizeof==0x50) guards it), so the
        // cast could only ever have worked against the all-zero stand-in it was actually given.

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

        // @0x824EFE30 - validate the save image against the authored progression data: require
        // the version word to be current and the stored manifest to match the event-junction
        // table entry-for-entry (same count; every stored event id present in the junction
        // table, and that junction carrying an offline event).
        // DWARF shape (BrnProfile.h:679): `bool ValidateProfile(const ProgressionData*) const`.
        bool ValidateProfile(const BrnProgression::ProgressionData* lpProgressionData) const;

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
