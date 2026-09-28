#ifndef GAMESOURCE_SOUND_VEHICLES_TRAFFIC_BRNTRAFFICSOUNDDIAG_H
#define GAMESOURCE_SOUND_VEHICLES_TRAFFIC_BRNTRAFFICSOUNDDIAG_H

// =============================================================================
// [FLAG PC witness] Opt-in witnesses for the traffic sound domain, enabled by the
// environment variable BRN_TRAFFICSND_DIAG (any value but "0"). Nothing here exists
// on the console; every call site is guarded by TrafficSoundDiagTake, so a run without
// the variable does one getenv on the first witness and nothing else.
//
//   [trafficsnd] manager prepared   the manager reached its finished prepare state
//   [trafficsnd] attach             a traffic entity bound to a free TrafficState
//   [trafficsnd] cull               the farthest attached entity released for a nearer one
//   [trafficsnd] voice started      a traffic voice was told to play (engine, horn, skid)
//   [trafficsnd] detach             a slot let its TrafficState go (ok=0: not yet attached)
//   [trafficsnd] voice released     an effect released its voice on detach (with its stage)
//
// Every witness kind is capped by its own counter (first-N lines).
// =============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdlib>

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

inline bool TrafficSoundDiagLive()
{
    static const bool sbEnabled = []()
    {
        const char* lpcValue = std::getenv("BRN_TRAFFICSND_DIAG");
        return lpcValue != 0 && lpcValue[0] != 0 && lpcValue[0] != '0';
    }();
    return sbEnabled && CgsDev::Log::gpDebugPrint != 0;
}

// True (and the counter advanced) while the witness is live and under its cap.
inline bool TrafficSoundDiagTake(u32& ruCount, u32 luCap)
{
    if (!TrafficSoundDiagLive() || ruCount >= luCap)
        return false;
    ++ruCount;
    return true;
}

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound

#endif // GAMESOURCE_SOUND_VEHICLES_TRAFFIC_BRNTRAFFICSOUNDDIAG_H
