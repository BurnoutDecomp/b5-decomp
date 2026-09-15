#ifndef GAMESOURCE_SOUND_VEHICLES_BRNAISOUNDDIAG_H
#define GAMESOURCE_SOUND_VEHICLES_BRNAISOUNDDIAG_H

// =============================================================================
// [DIAG] NOT IN THE X360 BINARY
//
// Opt-in witnesses for the AI-vehicle engine audio chain (the rivals' engines),
// enabled by the environment variable BRN_AI_SOUND_DIAG (any value). Nothing here
// exists in BURNOUT_X360_ARTIST.XEX; every call site is guarded by
// AISoundDiagLive() so a build without the variable set does one getenv at startup.
//
// The witnesses are named for exactly what they measure:
//   [ai-sound-load]    every LoadAsset / GetAsset / AddRegistry the AI engine
//                      loader issues, and each meAIEngineLoadingState transition,
//                      with the number of loop content specs constructed per engine
//                      and which of them report loaded.
//   [ai-sound-attach]  an AIVehicleState bound to an active race car: the car
//                      index, the AI engine assignment, the engine name the state
//                      resolved and its attribute key.
//   [ai-sound-detach]  the same state released (range, inactivity, or steal).
//   [ai-sound-engine]  once per second per attached AI car: the simulated engine
//                      RPM / gear / throttle fed to the AEMS graph and the car's
//                      distance to the listener.
//   [ai-sound-passby]  every AI passby / near-miss post and whether it was accepted.
// Every witness is rate-limited (a cap, or one line per second of game time).
// =============================================================================

#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdlib>

namespace BrnSound
{
namespace Vehicles
{

inline bool AISoundDiagEnabled()
{
    static const bool sbEnabled = std::getenv("BRN_AI_SOUND_DIAG") != 0;
    return sbEnabled;
}

// True when the diagnostic is on AND a debug sink exists to print to.
inline bool AISoundDiagLive()
{
    return AISoundDiagEnabled() && CgsDev::Log::gpDebugPrint != 0;
}

} // namespace Vehicles
} // namespace BrnSound

#endif // GAMESOURCE_SOUND_VEHICLES_BRNAISOUNDDIAG_H
