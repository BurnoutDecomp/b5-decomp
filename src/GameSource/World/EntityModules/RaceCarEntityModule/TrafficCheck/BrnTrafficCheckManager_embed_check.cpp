#include "GameSource/World/EntityModules/RaceCarEntityModule/TrafficCheck/BrnTrafficCheckManager.h"

#include <cstddef>   // offsetof

// Embed/layout check for BrnWorld::TrafficCheckManager. Proves the two-member layout the
// X360 Update body addresses (+0 s32 chain, +4 f32 timer) and exercises the bodied Update.

namespace
{
    using BrnWorld::TrafficCheckManager;

    void Exercise(TrafficCheckManager* lpManager,
                  const TrafficCheckManager::GameEventQueue* lpInput,
                  TrafficCheckManager::GameEventQueue* lpOutput)
    {
        TrafficCheckManager::_AssertLayout();   // proves +0 chain / +4 timer / sizeof 8
        lpManager->Update(lpInput, lpOutput, /*lbLocalPlayerCrashing*/ false, /*lfTimeStep*/ 0.033f);
        lpManager->Update(lpInput, lpOutput, /*lbLocalPlayerCrashing*/ true,  /*lfTimeStep*/ 0.033f);
    }
}

extern "C" int BrnTrafficCheckManager_embed_check_anchor(void) { return 0; }
