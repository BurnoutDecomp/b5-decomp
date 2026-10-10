#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Numeric/CgsRandom.h"           // CgsNumeric::Random (mRandom)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"     // BrnNetwork::NetworkPlayerID
#include "GameSource/GameState/BrnGameStateSharedIO.h"          // GameStateModuleIO::FlybyData (mFlybyData)

// ===========================================================================
// BrnGameState::FlybyManager -- the base of the offline and online pre-race flyby managers
// (reference home GameSource/GameState/FlybyManager/BrnGameStateFlybyManager.h).
//
// The module constructs both derived managers back to back right after StuntManager::Construct:
// FlybyManager::Construct(gsm+0x2D630, gsm, the ScoringSystem) for the offline one and the same
// call on gsm+0x2D8E0 for the online one. Construct stores the two owner pointers (asserting each)
// and runs the inlined Random::Construct on mRandom.
//
// Members in reference order: the flyby payload, the random generator, the two owner pointers and
// the two rank thresholds. The console's layout puts mFlybyData at +0x10 (OnlineFlybyManager's
// CalculateOnlineRivals prepares this+0x10), mRandom at +0x260 and the owner pointers at
// +0x290/+0x294 (Construct's stores).
// The two reference virtuals (CalculateFlybyRivals, CalculateNumberOfCarsInFlyby) are not
// declared virtual on this build: each derived class declares its own, called by name.
// ===========================================================================

namespace BrnGameState
{
class GameStateModule;
class ScoringSystem;

// Reference BrnGameStateFlybyManager.h. A flyby message's string ids: the singular form (a stat
// of exactly 1) and the plural form.
struct CombinedStringID
{
    const char* mpcSingularStringID;
    const char* mpcPluralStringID;
};

class FlybyManager
{
public:
    // Reference BrnGameStateFlybyManager.h. One rival's flyby weighting.
    struct RivalRating
    {
        bool                        mbHasValidRelationship;
        BrnNetwork::NetworkPlayerID mPlayerID;
        s32                         miRivalWeighting;

        static s32 SortRivalsCallback(const void* lpData0, const void* lpData1);
    };

    void Construct(GameStateModule* lpGameStateModule, ScoringSystem* lpScoringSystem);

    // Asserts the owner was set by Construct and returns it.
    GameStateModule* GetGameStateModule();

    // The flyby payload the managers fill and the GUI reads.
    GameStateModuleIO::FlybyData* GetFlybyData();

    // Reference BrnGameStateFlybyManager.h, header inline.
    CgsNumeric::Random* GetRandom() { return &mRandom; }

    // Reference BrnGameStateFlybyManager.cpp: the string id of the gamertag message every flyby
    // car gets first.
    static const char* KPC_GAMERTAG_STRING_ID;

protected:
    GameStateModuleIO::FlybyData mFlybyData;
    CgsNumeric::Random           mRandom;
    GameStateModule*             mpGameStateModule;
    ScoringSystem*               mpScoringSystem;
    s32                          miRankLeader;
    s32                          miRankOustider;
};
}
