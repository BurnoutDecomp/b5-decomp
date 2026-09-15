#ifndef CGS_SOUND_STREAM_DIAG_H
#define CGS_SOUND_STREAM_DIAG_H

// =============================================================================
// [DIAG] NOT IN THE X360 BINARY.
//
// Opt-in witness for the STREAMED-AUDIO path (BRN_STREAM_DIAG=1, default OFF).
// Header-only so it needs no new mount line.
//
// It measures exactly what its name says and nothing adjacent: the streaming
// request ring (post / accept / refuse / stop), the StreamingEffect voice
// bring-up (content spec -> voice ident -> VoiceWrapper update stage -> the gain
// actually written to the send), and the playback module's 3 stream-buffer
// records (which record a stream takes and which state it is left in). Each
// witness prints the ContentSpec it is about, so a line can never be read as
// being about a different stream.
//
// Every call site is rate limited: state-change edges only, plus a hard cap on
// the total number of lines this TU set may emit in one run (an assert/witness
// storm starves the harness).
// =============================================================================

#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace CgsSound
{
namespace Diag
{

inline bool StreamDiagEnabled()
{
    static int siEnabled = -1;
    if (siEnabled < 0)
    {
        const char* lpcEnv = std::getenv("BRN_STREAM_DIAG");
        siEnabled = (lpcEnv && lpcEnv[0] && lpcEnv[0] != '0') ? 1 : 0;
    }
    return siEnabled != 0;
}

// Hard line cap shared by every [stream] witness (see the storm note above).
inline unsigned& StreamDiagLineCount()
{
    static unsigned suLines = 0;
    return suLines;
}

inline void StreamDiagPrintf(const char* lpcFormat, ...)
{
    if (!StreamDiagEnabled())
        return;
    unsigned& lruLines = StreamDiagLineCount();
    if (lruLines >= 3000u)
        return;
    ++lruLines;

    char lacMsg[512];
    std::va_list lArgs;
    va_start(lArgs, lpcFormat);
    std::vsnprintf(lacMsg, sizeof(lacMsg), lpcFormat, lArgs);
    va_end(lArgs);
    CgsDev::Log::WriteToLog(lacMsg);
}

} // namespace Diag
} // namespace CgsSound

#endif // CGS_SOUND_STREAM_DIAG_H
