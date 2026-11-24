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
#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/CgsDataStructures.h"
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
// The original body is a header inline in CgsDataStructures.h (asserts h:1883/h:1886); the
// locals are the DWARF's (lpFullPath h:1885, luLen h:1888, lpNext h:1889, lu32I h:1891). Each
// skipped zone steps lpNext past its separator before shortening luLen (0x8269295C..0x82692970).
bool ContentSpec::GetPathZone(u32 lu32Zone, char* lpPathOut, size_t luMaxLen) const
{
    CGS_ASSERT(lpPathOut, CGS_ASSERT_MESSAGE_AT("lpPathOut", CGS_ARTIST_FILE, 1883));

    const char* lpFullPath = GetPath();
    CGS_ASSERT(lpFullPath, CGS_ASSERT_MESSAGE_AT("lpFullPath", CGS_ARTIST_FILE, 1886));

    size_t luLen = mu16PathLength;
    const char* lpNext;

    // Skip past lu32Zone leading zones, each terminated by SK_PATH_SEPERATOR ('|').
    for (u32 lu32I = 0; lu32I < lu32Zone; ++lu32I)
    {
        lpNext = strchr(lpFullPath, SK_PATH_SEPERATOR);
        if (!lpNext)
            return false;

        ++lpNext;
        luLen -= lpNext - lpFullPath;
        lpFullPath = lpNext;
    }

    // The zone runs until the next separator (or the end of the path).
    lpNext = strchr(lpFullPath, SK_PATH_SEPERATOR);
    if (lpNext)
        luLen = lpNext - lpFullPath;

    if (luLen > luMaxLen - 1)
        return false;

    strncpy(lpPathOut, lpFullPath, luLen);
    lpPathOut[luLen] = 0;
    return true;
}

} // namespace Playback
} // namespace CgsSound

#undef CGS_ARTIST_FILE
