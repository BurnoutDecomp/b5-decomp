// ===================================================================================
// BrnGuiSaveLoad::Profile  -- implementation
//   class:BrnGuiSaveLoad::Profile
//
//   Construct        (outlined from BrnProgression::Profile::Serialise @0x8237C1F0)
//   ValidateProfile  @0x824EFE30
//
// Reconstructed branch-for-branch from the X360 pseudocode/asm. Member access is by name.
// ===================================================================================
#include "GameSource/Gui/SaveLoad/BrnGuiSaveLoadProfile.h"

#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStreamBase
#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // CgsDev::Log::gpDebugPrint, Message::gxMessageFilterFlags
#include "SharedClasses/Progression/BrnRaceEventData.h"        // BrnProgression::EventJunction (GetID / GetOfflineEvent)

#include <cstring>   // std::memset (the Serialise-prologue image clear)

namespace BrnGuiSaveLoad
{
    // Compile-time pin: the object spans the whole serialised image, so the clear below
    // is a plain object-sized memset rather than an over-length write through a view.
    static_assert(sizeof(Profile) == Profile::KI_IMAGE_SIZE_BYTES,
                  "BrnGuiSaveLoad::Profile must span the 118064-byte serialised image");

    // Outlined from BrnProgression::Profile::Serialise @0x8237C1F0, whose prologue reads:
    //     _savegprlr_14();
    //     memset(a2, 0, 118064);   <- the progression save image
    //     memset(a4, 0,   9800);   <- the DLC1 save image (see ProfileDLC1::ConstructImage)
    //     *a2 = 28;                <- the version word
    // The live-field copy that follows in the X360 body belongs to the Progression TU.
    void Profile::ConstructImage()
    {
        std::memset(this, 0, KI_IMAGE_SIZE_BYTES);
        miVersion = KI_VERSION_CURRENT;
    }

    // @0x824EFE30
    //
    // The X360 body reads the descriptor as `*(a2+28)` (count) / `*(a2+24)` (entry base) and
    // walks the entries `v24 += 4` on a _DWORD* (== 16 bytes), matching `*v24` and testing
    // `v24[1]`. That descriptor is BrnProgression::ProgressionData and those entries are its
    // EventJunction table -- see the header note. Read here through the owning type's own
    // accessors, so no offset arithmetic and no serialised-slot width assumption survives.
    bool Profile::ValidateProfile(const BrnProgression::ProgressionData* lpProgressionData) const
    {
        using namespace CgsDev;

        const s32 liVersion = miVersion;

        // Version word mismatch -> log "expected 28, got <v>" (gated) and fail.
        if (liVersion != KI_VERSION_CURRENT)
        {
            if ((Message::gxMessageFilterFlags & 1) != 0)
            {
                *Log::gpDebugPrint
                    << "Progression Profile version mismatch, expected "
                    << KI_VERSION_CURRENT
                    << ", got "
                    << liVersion
                    << "\n";
            }
            return false;
        }

        // desc+0x1C / desc+0x18. The X360 dereferences the descriptor unconditionally and so
        // does this: on the console the progression data is resolved long before any profile
        // task completes, and the PC boot order is the same (Progression.dat lands during the
        // scripted load, BF_PROFILE's Bootup runs hundreds of lines later). No invented guard.
        const s32 liProfileCount  = muManifestCount;
        const s32 liExpectedCount =
            static_cast<s32>(lpProgressionData->GetEventJunctionCount());

        // [DIAG one-profile] NOT IN THE X360 BINARY. A count mismatch here rejects the whole
        // save and ReportTaskCompleted then silently skips the deserialise -- a boot that
        // looks exactly like a first boot. Print the two numbers so "the save did not load"
        // can be told apart from "the save loaded and was empty".
        if (Log::gpDebugPrint != 0)
        {
            *Log::gpDebugPrint
                << "[profile-save] ValidateProfile: version=" << liVersion
                << " storedEventCount=" << liProfileCount
                << " expectedJunctions=" << liExpectedCount << "\n";
        }

        // The stored and expected manifests must have the same number of entries.
        if (liExpectedCount != liProfileCount)
        {
            return false;
        }

        // No entries -> trivially valid.
        if (liProfileCount <= 0)
        {
            return true;
        }

        // For every stored manifest entry, find the expected entry with the matching id and
        // require its valid flag to be non-zero. A missing id, an empty expected manifest, or
        // a cleared flag fails validation.
        for (s32 liProfileIndex = 0; liProfileIndex < liProfileCount; ++liProfileIndex)
        {
            // X360 guards the expected-entry scan with this empty-manifest check inside the
            // loop (re-tested each profile entry).
            if (liExpectedCount == 0)
            {
                return false;
            }

            const u32 luStoredId = maVersionManifest[liProfileIndex].muId;

            // The scan index never reaches liExpectedCount without returning, so the
            // accessor's own `luIndex < muEventJunctionCount` assert cannot fire here.
            s32 liExpectedIndex = 0;
            while (lpProgressionData
                       ->GetEventJunction(static_cast<u32>(liExpectedIndex))->GetID() != luStoredId)
            {
                ++liExpectedIndex;
                if (liExpectedIndex >= liExpectedCount)
                {
                    return false;   // stored id not present in the junction table
                }
            }

            // X360 `if (!v24[1]) break;` -> the junction's offline-event slot (+0x04).
            if (lpProgressionData
                    ->GetEventJunction(static_cast<u32>(liExpectedIndex))->GetOfflineEvent() == 0)
            {
                return false;       // the matched junction has no offline event
            }
        }

        return true;
    }
}
