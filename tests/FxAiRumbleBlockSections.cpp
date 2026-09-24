// FX-AI-RUMBLE (crash parity 2026-09-23), G04-D1: AIModule::OnModeStart @0x82791DB8, the checkpoint
// loop 0x82791E90..0x82791F14 -- every checkpoint's blocked sections into the RouteRequestManager
// slot of the same index. The runner extracts the PRODUCTION loop out of OnModeStart
// (BrnAIModule_Events.cpp), RouteRequestManager::SetBlockSections (BrnRouteRequestManager.cpp) and
// GameModeParams::GetCheckpointCount / GetCheckpointData (BrnGameModeParams.cpp); the checkpoint
// accessor CheckpointData::GetBlockSectionIds is the header's own inline body. A revision with no
// loop runs nothing (the pre-fix behaviour: every slot keeps whatever it held).
//
// Console facts checked here:
//   0x82791E90/0x82791EB4  the count is params+0x260+0x2C0, re-read every pass
//   0x82791EC8             CheckpointData_16__ @0x822AE100 (checked element i)
//   0x82791ED4..0x82791EF8 assert(0 <= i < 16)  (BrnRouteRequestManager.cpp:101)
//   0x82791F04             `stw 0, 0x20(slot)`  -- the slot is EMPTIED first (stale ids go)
//   0x82791F00/0x82791F08  AppendArray<8>(slot, cp + 8)  -- cp + 8 == &mauBlockSectionIds
//   slots past the count are NOT touched here (OnModeEnd's ClearBlockSections empties them).
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/World/AI/BrnRouteRequestManager.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"
#undef protected
#undef private
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include <cstdio>
#include <cstring>

static unsigned gAssertions = 0, gChecks = 0, gFailures = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcText, const char*, int) { ++gAssertions; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
void* EndAssert() { return nullptr; }
} }

#include "restored_methods.inc"

using namespace BrnAI;
using BrnGameState::CheckpointData;
using BrnGameState::GameModeParams;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass) { ++gFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

static void SeedStale(RouteRequestManager& lrManager)
{
    for (u32 luSlot = 0; luSlot < 16; ++luSlot)
    {
        lrManager.mauBlockSectionIds[luSlot].Construct();
        lrManager.mauBlockSectionIds[luSlot].Append(900u + luSlot);
        lrManager.mauBlockSectionIds[luSlot].Append(901u + luSlot);
    }
    lrManager.meDefaultAStarDistanceFunction = static_cast<AStarDistanceFunction>(2);
}

static bool SlotIs(const RouteRequestManager& lrManager, u32 luSlot, const u32* lpauIds, s32 liCount)
{
    const Array<u32, 8u>& lrSlot = lrManager.mauBlockSectionIds[luSlot];
    if (lrSlot.miCount != liCount) return false;
    for (s32 i = 0; i < liCount; ++i)
        if (lrSlot.maElements[i] != lpauIds[i]) return false;
    return true;
}

static void AddCheckpoint(GameModeParams& lrParams, const u32* lpauIds, s32 liCount)
{
    CheckpointData lCheckpoint;
    lCheckpoint.Construct(BrnGameState::LandmarkIndex(), 0);
    for (s32 i = 0; i < liCount; ++i)
        lCheckpoint.AddBlockSectionId(lpauIds[i]);
    lrParams.maCheckpointDataArray.Append(lCheckpoint);
}

int main()
{
    static GameModeParams sParams;
    static BlockSectionHarness sHarness;

    static const u32 kauCheckpoint0[2] = { 11u, 12u };
    static const u32 kauCheckpoint2[8] = { 31u, 32u, 33u, 34u, 35u, 36u, 37u, 38u };

    sParams.maCheckpointDataArray.Construct();
    AddCheckpoint(sParams, kauCheckpoint0, 2);
    AddCheckpoint(sParams, kauCheckpoint0, 0);
    AddCheckpoint(sParams, kauCheckpoint2, 8);
    SeedStale(sHarness.mRouteRequestManager);

    const unsigned luAssertsBefore = gAssertions;
    sHarness.Run(&sParams);

    Check(SlotIs(sHarness.mRouteRequestManager, 0, kauCheckpoint0, 2), "checkpoint 0's two blocked sections land in slot 0");
    Check(SlotIs(sHarness.mRouteRequestManager, 1, kauCheckpoint0, 0), "a checkpoint with none EMPTIES its slot (stw 0, 0x20(slot))");
    Check(SlotIs(sHarness.mRouteRequestManager, 2, kauCheckpoint2, 8), "a full 8-id list lands whole in slot 2");
    bool lbTailKept = true;
    for (u32 luSlot = 3; luSlot < 16; ++luSlot)
    {
        const u32 lauStale[2] = { 900u + luSlot, 901u + luSlot };
        lbTailKept = lbTailKept && SlotIs(sHarness.mRouteRequestManager, luSlot, lauStale, 2);
    }
    Check(lbTailKept, "slots past the checkpoint count are left alone (OnModeEnd clears them)");
    Check(static_cast<s32>(sHarness.mRouteRequestManager.meDefaultAStarDistanceFunction) == 2,
          "the loop does not touch +0x240");
    Check(gAssertions == luAssertsBefore, "three checkpoints raise no assertion");

    // A 16-checkpoint mode fills every slot, in order, with no bounds assert.
    sParams.maCheckpointDataArray.Construct();
    for (u32 luCheckpoint = 0; luCheckpoint < 16; ++luCheckpoint)
    {
        const u32 lauIds[1] = { 100u + luCheckpoint };
        AddCheckpoint(sParams, lauIds, 1);
    }
    SeedStale(sHarness.mRouteRequestManager);
    sHarness.Run(&sParams);
    bool lbAll = true;
    for (u32 luSlot = 0; luSlot < 16; ++luSlot)
    {
        const u32 lauIds[1] = { 100u + luSlot };
        lbAll = lbAll && SlotIs(sHarness.mRouteRequestManager, luSlot, lauIds, 1);
    }
    Check(lbAll, "sixteen checkpoints fill all sixteen slots in index order");
    Check(gAssertions == luAssertsBefore, "sixteen checkpoints raise no assertion (0 <= i < 16)");

    std::printf("FxAiRumbleBlockSections: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
