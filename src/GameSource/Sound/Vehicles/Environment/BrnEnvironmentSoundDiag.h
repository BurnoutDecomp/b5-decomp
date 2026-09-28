#ifndef BRN_SOUND_VEHICLES_ENVIRONMENT_ENVIRONMENT_SOUND_DIAG_H
#define BRN_SOUND_VEHICLES_ENVIRONMENT_ENVIRONMENT_SOUND_DIAG_H

#include "types.hpp"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdlib>

// [FLAG PC witness] Opt-in log lines for the environment sound effects (crash / speed
// stream, in-car stereo, ambience, reverb). Enabled by the BRN_SNDENV_DIAG environment
// variable, read once; every call site owns its own first-N budget counter. Not part of
// the shipped game.
namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

inline bool SndEnvDiagOn()
{
    static const bool sbOn = (getenv("BRN_SNDENV_DIAG") != 0);
    return sbOn;
}

inline bool SndEnvDiagBudget(s32& riLines)
{
    static const s32 KI_MAX_LINES_PER_SITE = 24;
    if (!SndEnvDiagOn() || riLines >= KI_MAX_LINES_PER_SITE)
        return false;
    ++riLines;
    return CgsDev::Log::gpDebugPrint != 0;
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENVIRONMENT_ENVIRONMENT_SOUND_DIAG_H
