#ifndef GAMESOURCE_SOUND_GLOBAL_BRNHUDSOUNDDIAG_H
#define GAMESOURCE_SOUND_GLOBAL_BRNHUDSOUNDDIAG_H

// =============================================================================
// [DIAG] NOT IN THE X360 BINARY
//
// Opt-in witnesses for the GLOBAL HUD / FX sound effects, enabled by the
// environment variable BRN_HUD_SOUND_DIAG (any value but "0"). Nothing here
// exists in BURNOUT_X360_ARTIST.XEX; every call site is guarded by
// HudSoundDiagEnabled() so a build without the variable set does no work beyond
// one getenv at startup.
//
// The witnesses are named for exactly what they measure:
//   [hud-sound-attach]  HUDEffect::Attach binding mHudMessageData -- prints the
//                       number of message->splice mappings the bound
//                       presentationcomponent actually reports. ZERO here means
//                       the attrib collection did not resolve; it does NOT mean
//                       the effect failed to attach.
//   [hud-sound-msg]     every sound message 5 HUDEffect::Notify receives: the
//                       component type / action / additional-info / CgsID of the
//                       GuiAudioEvent, and the VERDICT (mapped / re-trigger /
//                       no-mapping / no-free-voice).
//   [hud-sound-play]    the voice actually created: slot, splice index, mixer
//                       output, choke group.
//   [fx-sound-msg]      every sound message 4 (FXMESSAGE) / 42 (FX volumes)
//                       FxEffect::Notify receives, and what it did with it.
// Each family is capped at 400 lines so a stuck producer cannot flood the log.
// std::printf does NOT reach BrnGame.log on this build; CgsDev::Log::WriteToLog
// is the sink the harness reads (same as the committed [music] witness).
// =============================================================================

#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace BrnSound
{
namespace Logic
{

inline bool HudSoundDiagEnabled()
{
    static int siEnabled = -1;
    if (siEnabled < 0)
    {
        const char* lpcEnv = std::getenv("BRN_HUD_SOUND_DIAG");
        siEnabled = (lpcEnv && lpcEnv[0] && lpcEnv[0] != '0') ? 1 : 0;
    }
    return siEnabled != 0;
}

// Cap a witness family. Returns false once the family has spent its budget.
inline bool HudSoundDiagBudget(u32& aruCount)
{
    if (!HudSoundDiagEnabled())
        return false;
    if (aruCount >= 400u)
        return false;
    ++aruCount;
    return true;
}

inline void HudSoundDiagPrintf(const char* lpcFormat, ...)
{
    char lacMsg[512];
    va_list lArgs;
    va_start(lArgs, lpcFormat);
    std::vsnprintf(lacMsg, sizeof(lacMsg), lpcFormat, lArgs);
    va_end(lArgs);
    CgsDev::Log::WriteToLog(lacMsg);
}

} // namespace Logic
} // namespace BrnSound

#endif // GAMESOURCE_SOUND_GLOBAL_BRNHUDSOUNDDIAG_H
