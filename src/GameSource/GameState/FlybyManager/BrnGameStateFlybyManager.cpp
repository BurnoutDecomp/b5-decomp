#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"   // CgsNetwork::K_INVALID_PLAYER_ID
#include "GameSource/GameState/FlybyManager/BrnGameStateFlybyManager.h"
#include "GameSource/GameState/FlybyManager/BrnGameStateOnlineFlybyManager.h"
#include "GameSource/GameState/BrnGameStateModule.h"              // GameStateModule::GetLocalPlayerNetworkID

namespace BrnGameState
{

const char* FlybyManager::KPC_GAMERTAG_STRING_ID = "GAMERTAG";

void FlybyManager::Construct(GameStateModule* lpGameStateModule, ScoringSystem* lpScoringSystem)
{
    CGS_ASSERT(lpGameStateModule, "lpGameStateModule");

    mpGameStateModule = lpGameStateModule;

    CGS_ASSERT(lpScoringSystem, "lpScoringSystem");

    mpScoringSystem = lpScoringSystem;
    mRandom.Construct();
}

// X360. Accessor for the owning GameStateModule; asserts it was set by Construct.
GameStateModule* FlybyManager::GetGameStateModule()
{
    CGS_ASSERT(mpGameStateModule, "mpGameStateModule");
    return mpGameStateModule;
}

s32 FlybyManager::RivalRating::SortRivalsCallback(const void* lpData0, const void* lpData1)
{
    const RivalRating* lpRival0 = static_cast<const RivalRating*>(lpData0);
    const RivalRating* lpRival1 = static_cast<const RivalRating*>(lpData1);

    if (lpRival0->mPlayerID == lpRival1->mPlayerID)
    {
        if (lpRival0->mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID
            || lpRival1->mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID)
        {
            CGS_ASSERT((lpRival0->mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID) && (lpRival1->mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID),
                       "( lpRival0->mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID ) && ( lpRival1->mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID )");
        }

        return 0;
    }

    if (lpRival0->mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID)
        return 1;

    if (lpRival1->mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID)
        return -1;

    if (lpRival0->miRivalWeighting < lpRival1->miRivalWeighting)
        return 1;

    if (lpRival0->miRivalWeighting > lpRival1->miRivalWeighting)
        return -1;

    return 0;
}

// X360 @ 0x82358720. Header-inlined accessor: returns the local player's network id by reaching through
// the owning GameStateModule. The X360 null check + verbatim assert ("mpGameStateModule",
// BrnGameStateFlybyManager.h:204) is exactly FlybyManager::GetGameStateModule()'s body, which the
// compiler inlined here; *(mpGameStateModule + 232296) is GameStateModule::mLocalPlayerNetworkID.
BrnNetwork::NetworkPlayerID OnlineFlybyManager::GetLocalPlayerNetworkID()
{
    return GetGameStateModule()->GetLocalPlayerNetworkID();
}

// X360 (DWARF BrnGameStateFlybyManager.h:217). Trivial accessor returning the address of the
// owned FlybyData member (this+0x10 on the console, where OnlineFlybyManager::CalculateOnlineRivals
// prepares it).
GameStateModuleIO::FlybyData* FlybyManager::GetFlybyData()
{
    return &mFlybyData;
}

// OfflineFlybyManager : public FlybyManager (DWARF BrnGameStateOfflineFlybyManager.h:42). Minimal
// slice: only the one method reconstructed by this TU.
class OfflineFlybyManager : public FlybyManager
{
public:
    // X360 @ 0x82364820. const FlybyData* CalculateFlybyRivals().
    const GameStateModuleIO::FlybyData* CalculateFlybyRivals();
};

// X360 @ 0x82364820  ==  addi r3,r3,0x10 ; blr.
// IDA-truncated symbol was 'OfflineFlybyManager::Cal'; full name recovered from DWARF mangled
// symbol _ZN12BrnGameState19OfflineFlybyManager20CalculateFlybyRivalsEv and confirmed by the
// identically-truncated sibling OnlineFlybyManager::Calc @0x82391FA8.
// The Offline override emits no rival-calculation call (that work was inlined into Prepare); the
// surviving vtable entry is the bare 'return &mFlybyData' == return GetFlybyData() (this+0x10).
const GameStateModuleIO::FlybyData* OfflineFlybyManager::CalculateFlybyRivals()
{
    return GetFlybyData();
}
}
