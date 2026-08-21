#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace BrnWorld
{

// Breaker @0x822B8D08.  Notice that B5::Prepare is deliberately called twice:
// once with the other embedded strategies, then again after it becomes the
// selected strategy.  That is the retail call order, not a decompiler artifact.
bool BoostManager::Prepare()
{
    mfJustBounceBoostedTimer = 0.0f;
    miCurrentCarBoostLevel = 0;
    miCurrentPlayerBoostLevel = 0;

    mBoostBurnout2.Prepare();
    mBoostBurnout3.Prepare();
    mBoostBurnout5.Prepare();

    // Breaker 0x822B8D70..0x822B8DF8 performs two independently gated
    // writes.  Keep the separate filter reads and the retail text/order.
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        *CgsDev::Log::gpDebugPrint << "leVersion: " << E_BOOSTSTRATEGY_BURNOUT5 << "\n";
    }
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        *CgsDev::Log::gpDebugPrint << "******************************\n";
    }

    mpBoostStrategy = &mBoostBurnout5;
    meBoostStrategy = E_BOOSTSTRATEGY_BURNOUT5;
    mBoostBurnout5.Prepare();
    mpBoostStrategy->SetBoostEarningEnabled(mbBoostEarningEnabled);
    mbBoostEarningEnabled = false;
    return true;
}

// Breaker @0x822A3288.  The selector only carries the three strategies that
// exist in the ARTIST image; ids 1 and 4 are DecFIGS-lineage declarations but
// are rejected by the retail assert.
void BoostManager::SetBoostStrategy(BoostStrategyId leStrategy)
{
    // Breaker 0x822A3294..0x822A3324, before the id store and selector.
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        *CgsDev::Log::gpDebugPrint << "leStrategy: " << leStrategy << "\n";
    }
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        *CgsDev::Log::gpDebugPrint << "******************************\n";
    }

    meBoostStrategy = leStrategy;

    switch (leStrategy)
    {
    case E_BOOSTSTRATEGY_BURNOUT2:
        mpBoostStrategy = &mBoostBurnout2;
        break;
    case E_BOOSTSTRATEGY_BURNOUT3:
        mpBoostStrategy = &mBoostBurnout3;
        break;
    case E_BOOSTSTRATEGY_BURNOUT5:
        mpBoostStrategy = &mBoostBurnout5;
        break;
    default:
        CGS_ASSERT(false, "Unknown boost strategy");
        break;
    }

    mpBoostStrategy->Prepare();
    mpBoostStrategy->SetBoostEarningEnabled(mbBoostEarningEnabled);
}

// These manager wrappers are inlined at their ARTIST callers. The register and
// slot traffic remains visible: HandlePrepareForModeAction @0x8230995C calls
// BoostStrategy::OnModeStart with (mode, strategy-id == 2), while
// HandleCarStatsUpdate @0x822A4774 stores both levels then dispatches slot 43.
void BoostManager::OnModeStart(
    BrnGameState::GameStateModuleIO::EGameModeType leGameModeType)
{
    mpBoostStrategy->OnModeStart(
        leGameModeType, meBoostStrategy == E_BOOSTSTRATEGY_BURNOUT2);
}

void BoostManager::ApplyCarStats(s32 liCurrentCarBoostLevel,
                                 s32 liCurrentPlayerBoostLevel)
{
    miCurrentCarBoostLevel = liCurrentCarBoostLevel;
    miCurrentPlayerBoostLevel = liCurrentPlayerBoostLevel;
    mpBoostStrategy->SetCarStatBoostLevel(
        liCurrentCarBoostLevel, liCurrentPlayerBoostLevel);
}

void BoostManager::ApplyPreviousCarStats()
{
    mpBoostStrategy->SetCarStatBoostLevel(
        miCurrentCarBoostLevel, miCurrentPlayerBoostLevel);
}

// Breaker @0x822A33B0: the selected virtual call precedes the manager store.
void BoostManager::SetBoostEarningEnabled(bool lbEnabled)
{
    mpBoostStrategy->SetBoostEarningEnabled(lbEnabled);
    mbBoostEarningEnabled = lbEnabled;
}

// Breaker @0x822B8E40; BoostStrategy::SetWrecking is the DecFIGS-named inline
// body corresponding exactly to the slot-8 edge callback and +0xBD store.
void BoostManager::SetWrecking(bool lbWrecking, bool lbIsInOnlineGameMode)
{
    mpBoostStrategy->SetWrecking(lbWrecking, lbIsInOnlineGameMode);
}

} // namespace BrnWorld
