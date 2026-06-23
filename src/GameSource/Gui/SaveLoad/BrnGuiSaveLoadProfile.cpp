// ===================================================================================
// BrnGuiSaveLoad::Profile  -- implementation
//   class:BrnGuiSaveLoad::Profile
//
//   ValidateProfile @0x824EFE30
//
// Reconstructed branch-for-branch from the X360 pseudocode/asm. Member access is by name.
// ===================================================================================
#include "GameSource/Gui/SaveLoad/BrnGuiSaveLoadProfile.h"

#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStreamBase
#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // CgsDev::Log::gpDebugPrint, Message::gxMessageFilterFlags

namespace BrnGuiSaveLoad
{
    // @0x824EFE30
    bool Profile::ValidateProfile(const ExpectedManifest& lrExpected)
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

        const s32 liProfileCount  = muManifestCount;   // +0x268
        const s32 liExpectedCount = lrExpected.miCount; // desc+0x1C

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

            s32 liExpectedIndex = 0;
            while (lrExpected.mpEntries[liExpectedIndex].muId != luStoredId)
            {
                ++liExpectedIndex;
                if (liExpectedIndex >= liExpectedCount)
                {
                    return false;   // stored id not present in the expected manifest
                }
            }

            if (lrExpected.mpEntries[liExpectedIndex].muFlag == 0)
            {
                return false;       // matched entry is not flagged valid
            }
        }

        return true;
    }
}
