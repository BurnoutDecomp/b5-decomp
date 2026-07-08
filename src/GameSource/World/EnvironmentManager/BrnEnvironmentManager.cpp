#include "GameSource/World/EnvironmentManager/BrnEnvironmentManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::EnvironmentSettings::EnvironmentManager::UpdateFromTool      @ 0x827B0DA8
//   BrnWorld::EnvironmentSettings::EnvironmentManager::DiscardCurrSeason   @ 0x827B0E50
//
// Both baked asserts fire the plain-string Begin/Fire/End sequence (rendered here as
// CGS_ASSERT); the X360-baked file/line are discarded per project convention. The typo
// "Tyring" is reproduced verbatim from the ARTIST rodata.

namespace BrnWorld
{
namespace EnvironmentSettings
{

// @ 0x827B0DA8. Tool-driven blend/pause transition.
//   miBlendState >= 3  -> already in a blocking op: on unpause (lbPause == false) restore
//                         the saved state (asserting the state really is the reserved 3),
//                         and report the blocking state (true).
//   miBlendState <  3  -> on pause (lbPause == true) stash the current state, enter the
//                         reserved blocking state 3, and reset the tool-update frame gate;
//                         report not-blocking (false).
bool EnvironmentManager::UpdateFromTool(bool lbPause)
{
    if (miBlendState >= 3)
    {
        if (!lbPause)
        {
            CGS_ASSERT(miBlendState == 3, "Tyring to unpause a blocking op");
            miBlendState = miSavedBlendState;
        }
        return true;
    }

    if (lbPause)
    {
        miSavedBlendState        = miBlendState;
        miBlendState             = 3;
        miToolUpdateFrameCounter = 0;
    }
    return false;
}

// @ 0x827B0E50. Commit the pending season swap once the stream-in has finished.
void EnvironmentManager::DiscardCurrSeason()
{
    CGS_ASSERT(meStreamInStage == E_STREAMIN_DONE, "meStreamInStage == E_STREAMIN_DONE");

    const u32 luDiscardSeason = muDiscardSeason;
    muCurrSeasonRef = 0;
    mbCurrSeason    = static_cast<u8>(luDiscardSeason);
}

}
}
