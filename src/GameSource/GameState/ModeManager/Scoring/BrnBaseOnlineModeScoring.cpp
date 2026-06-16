#include "GameSource/GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace
{
// Assert source path baked verbatim into the X360 build for the GetPlayerPosition bounds checks
// (note: it is the .h path, not the .cpp -- the X360 build inlined these accessors from the header).
const char* const KPC_SCORING_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\modemanager\\scoring\\BrnBaseOnlineModeScoring.h";
}

namespace BrnGameState
{
// X360 @ 0x823106F8. Finishing position recorded for active-race-car slot liRaceCarIndex. The two
// bounds checks fire as the build had them: two SEPARATE Begin/Fire/End assert sequences (lower-bound
// then upper-bound), each with its own baked-in message string and source line (343 / 344).
s32 BaseOnlineModeScoring::GetPlayerPosition(s32 liRaceCarIndex)
{
    if (liRaceCarIndex < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liRaceCarIndex >= 0",
            KPC_SCORING_FILE,
            343);
        CgsDev::Assert::EndAssert();
    }
    if (liRaceCarIndex >= KI_MAX_ACTIVE_RACE_CARS)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS",
            KPC_SCORING_FILE,
            344);
        CgsDev::Assert::EndAssert();
    }
    return maiPlayerPositions[liRaceCarIndex];
}

// X360 @ 0x82310770. Store the finishing position liPlayerPosition for active-race-car slot
// liRaceCarIndex. Four independent bounds asserts (the X360 build did NOT fold them into one combined
// check -- each fires its own distinct line number 356/357/359/360); the asserts never early-return,
// so the table write executes unconditionally even on a failed bound.
void BaseOnlineModeScoring::SetPlayerPosition(s32 liRaceCarIndex, s32 liPlayerPosition)
{
    if (liRaceCarIndex < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liRaceCarIndex >= 0",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\modemanager\\scoring\\BrnBaseOnlineModeScoring.h",
            356);
        CgsDev::Assert::EndAssert();
    }
    if (liRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_COUNT)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\modemanager\\scoring\\BrnBaseOnlineModeScoring.h",
            357);
        CgsDev::Assert::EndAssert();
    }
    if (liPlayerPosition < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liPlayerPosition >= 0",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\modemanager\\scoring\\BrnBaseOnlineModeScoring.h",
            359);
        CgsDev::Assert::EndAssert();
    }
    if (liPlayerPosition >= E_ACTIVE_RACE_CAR_INDEX_COUNT)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liPlayerPosition < BrnWorld::KI_MAX_ACTIVE_RACE_CARS",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\modemanager\\scoring\\BrnBaseOnlineModeScoring.h",
            360);
        CgsDev::Assert::EndAssert();
    }

    maiPlayerPositions[liRaceCarIndex] = liPlayerPosition;
}

// X360 @ 0x82314638. Team currently assigned to active-race-car slot liRaceCarIndex (read from the
// per-slot maePlayerTeams[] array). Virtual. Two SEPARATE bounds asserts (lower bound at .cpp line
// 1010, upper bound at line 1011), each with its own verbatim message string and the exact baked-in
// .cpp path (note: this function's asserts use the .cpp path, not the .h path the sibling accessors use).
GameStateModuleIO::EPlayerTeam
BaseOnlineModeScoring::GetCurrentPlayerTeam(s32 liRaceCarIndex)
{
    if (liRaceCarIndex < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liRaceCarIndex >= 0",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.cpp",
            1010);
        CgsDev::Assert::EndAssert();
    }
    if (liRaceCarIndex >= KI_MAX_ACTIVE_RACE_CARS)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.cpp",
            1011);
        CgsDev::Assert::EndAssert();
    }
    return maePlayerTeams[liRaceCarIndex];
}
}
