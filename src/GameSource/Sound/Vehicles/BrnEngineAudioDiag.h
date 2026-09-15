#ifndef GAMESOURCE_SOUND_VEHICLES_BRNENGINEAUDIODIAG_H
#define GAMESOURCE_SOUND_VEHICLES_BRNENGINEAUDIODIAG_H

// =============================================================================
// [DIAG] NOT IN THE X360 BINARY
//
// Opt-in witnesses for the PLAYER vehicle engine/exhaust audio chain, enabled by
// the environment variable BRN_ENGINE_DIAG (any value). Nothing here exists in
// BURNOUT_X360_ARTIST.XEX; every call site is guarded by EngineAudioDiagEnabled()
// so a build without the variable set does no work beyond one getenv at startup.
//
// The witnesses are named for exactly what they measure:
//   [engine-attach]   the per-car engine/exhaust CgsID names + attribute keys the
//                     PlayerVehicleState copied out of the VehicleListEntry.
//   [engine-asset]    every bundle / resource name PhysicsControl::SetupLoadData
//                     asks the registrar for.
//   [engine-registry] every AddRegistry lookup and whether the handle RESOLVED --
//                     a null handle here is a bank that is not on disk / not in
//                     the bundle, i.e. an engine that falls back to whatever the
//                     playback registry already holds.
//   [engine-param]    the physics -> AEMS parameter feed, once per second: the raw
//                     physics RPM, the mapped unity/normalized RPM, throttle,
//                     gear, speed, and the nine DMix "mixer input" control values
//                     the console's UpdateParams writes.
// Every witness is rate-limited (a cap, or one line per second of game time).
// =============================================================================

#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdlib>

namespace BrnSound
{
namespace Vehicles
{

inline bool EngineAudioDiagEnabled()
{
    static const bool sbEnabled = std::getenv("BRN_ENGINE_DIAG") != 0;
    return sbEnabled;
}

// True when the diagnostic is on AND a debug sink exists to print to.
inline bool EngineAudioDiagLive()
{
    return EngineAudioDiagEnabled() && CgsDev::Log::gpDebugPrint != 0;
}

} // namespace Vehicles
} // namespace BrnSound

#endif // GAMESOURCE_SOUND_VEHICLES_BRNENGINEAUDIODIAG_H
