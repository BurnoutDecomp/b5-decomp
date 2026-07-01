// ============================================================================
// CgsContentSpec.cpp -- CgsSound::Playback::ContentSpec accessors.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   CgsSound::Playback::ContentSpec::GetContentType @ 0x826927C8
//   CgsSound::Playback::ContentSpec::GetPathZone    @ 0x826928C8
//
// Layout is DWARF-authoritative (CgsDataStructures.h:425): ContentSpec : Entity,
// with mpContentType @ +8, mu16PathLength @ +0xC, mu8LoadMethod @ +0xE,
// mu8LoadTime @ +0xF, then the inline full-path buffer (GetPath()/GetPathZone read
// `this + 16`). Every field is addressed BY NAME (no raw-offset arithmetic).
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"
#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h" // ContentType::GetContentClass etc.

#include <cstring>

namespace CgsSound
{
namespace Playback
{

// @ 0x826927C8. Return the resolved ContentType this spec points at.
const ContentType& ContentSpec::GetContentType() const
{
    CGS_ASSERT(mpContentType, "mpContentType");

    // The X360 tests the low tag bit of the pointer; when set the entity is still an
    // unresolved on-disk offset, not a live pointer. The streamed Name in the second
    // assert folds to the joined literal text under the house CGS_ASSERT front-end.
    CGS_ASSERT((reinterpret_cast<uintptr_t>(mpContentType) & 1u) == 0u,
               "This Data Structure is not resolved. (Name  found in a pointer context.)");

    return *mpContentType;
}

// @ 0x826928C8. Copy the au32Zone'th '|'-separated zone of the full path into
// apcPathOut (capacity auMaxLen, NUL-terminated). Returns true on success, false if
// the requested zone doesn't exist or won't fit.
bool ContentSpec::GetPathZone(u32 au32Zone, char* apcPathOut, size_t auMaxLen) const
{
    CGS_ASSERT(apcPathOut, "lpPathOut");

    const char* lpcCursor = GetPath();
    CGS_ASSERT(lpcCursor, "lpFullPath");

    u32 lu32Remaining = mu16PathLength;

    // Skip past au32Zone leading zones, each terminated by SK_PATH_SEPERATOR ('|').
    for (u32 lu32Zone = 0; lu32Zone < au32Zone; )
    {
        const char* lpcSep = strchr(lpcCursor, SK_PATH_SEPERATOR);
        if (!lpcSep)
            return false;

        ++lu32Zone;
        lu32Remaining -= static_cast<u32>((lpcSep + 1) - lpcCursor);
        lpcCursor = lpcSep + 1;
    }

    // The zone runs until the next separator (or the end of the path).
    const char* lpcSep = strchr(lpcCursor, SK_PATH_SEPERATOR);
    if (lpcSep)
        lu32Remaining = static_cast<u32>(lpcSep - lpcCursor);

    if (lu32Remaining > auMaxLen - 1)
        return false;

    strncpy(apcPathOut, lpcCursor, lu32Remaining);
    apcPathOut[lu32Remaining] = 0;
    return true;
}

} // namespace Playback
} // namespace CgsSound
