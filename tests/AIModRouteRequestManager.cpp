// FX-AIMOD (crash parity 2026-09-22), G04-D1 / G04-D3: RouteRequestManager's per-checkpoint block
// sections and its Construct @0x8278A3B0. The runner extracts the PRODUCTION Construct,
// SetBlockSections, ClearBlockSections and the file-static mRandom from BrnRouteRequestManager.cpp.
//
// Console facts checked here:
//   Construct 0x8278A3B0..0x8278A434 -- mRandom (0x8300D570) == Random::Construct() folded: ring
//     3F800000 3FE43E6C 3F98B09C 3FDA23E0 3FE21EDC 3FDDEB96 3F9C9A72 3F923D76, seed
//     0xB5E330D02EC654DA, index 0; 0x8278A438..0x8278A450 every slot COUNT (+0x20) = 0;
//     0x8278A454 +0x240 = 0.
//   SetBlockSections (inlined, AIModule::OnModeStart 0x82791ED4..0x82791F08) -- assert :101,
//     count = 0, AppendArray<8>.
//   ClearBlockSections (inlined, AIModule::OnModeEnd 0x8277BB8C..0x8277BBC8) -- 16 counts = 0.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/World/AI/BrnRouteRequestManager.h"
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

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass) { ++gFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

int main()
{
    static RouteRequestManager sManager;
    std::memset(&sManager, 0xCD, sizeof(sManager));   // a Construct must not rely on zeroed storage
    std::memset(&mRandom, 0, sizeof(mRandom));

    sManager.Construct();

    bool lbCountsZero = true;
    for (s32 liSlot = 0; liSlot < 16; ++liSlot)
    {
        lbCountsZero = lbCountsZero && sManager.mauBlockSectionIds[liSlot].miCount == 0;
    }
    Check(lbCountsZero, "Construct zeroes every block-section COUNT (0x8278A438..0x8278A450)");
    Check(static_cast<s32>(sManager.meDefaultAStarDistanceFunction) == 0, "Construct zeroes the default A* function (0x8278A454)");

    static const u32 kauRing[8] = { 0x3F800000u, 0x3FE43E6Cu, 0x3F98B09Cu, 0x3FDA23E0u,
                                    0x3FE21EDCu, 0x3FDDEB96u, 0x3F9C9A72u, 0x3F923D76u };
    bool lbRing = true;
    for (s32 liSlot = 0; liSlot < 8; ++liSlot)
    {
        lbRing = lbRing && mRandom.mauIntegerBuffer[liSlot] == kauRing[liSlot];
    }
    Check(lbRing, "Construct seeds the file-static Random ring to the console's 0x8300D570 words");
    Check(mRandom.muSeed == 0xB5E330D02EC654DAull, "Construct seeds the Random's LCG (insrdi at 0x8278A418)");
    Check(mRandom.muOldestBufferIndex == 0u, "Construct leaves the Random's index at 0 (0x8278A434)");

    Array<u32, 8u> laIds;
    laIds.Construct();
    laIds.Append(0x1111u);
    laIds.Append(0x2222u);
    laIds.Append(0x3333u);

    sManager.SetBlockSections(3, &laIds);
    Check(sManager.mauBlockSectionIds[3].GetLength() == 3
          && sManager.mauBlockSectionIds[3].GetItem(0) == 0x1111u
          && sManager.mauBlockSectionIds[3].GetItem(2) == 0x3333u,
          "SetBlockSections copies the checkpoint's ids into its slot");
    sManager.SetBlockSections(3, &laIds);
    Check(sManager.mauBlockSectionIds[3].GetLength() == 3, "SetBlockSections resets the slot before appending (stw 0,0x20)");
    Check(sManager.mauBlockSectionIds[4].GetLength() == 0, "SetBlockSections touches only its own checkpoint");

    sManager.SetBlockSections(15, &laIds);
    sManager.ClearBlockSections();
    bool lbCleared = true;
    for (s32 liSlot = 0; liSlot < 16; ++liSlot)
    {
        lbCleared = lbCleared && sManager.mauBlockSectionIds[liSlot].GetLength() == 0;
    }
    Check(lbCleared, "ClearBlockSections empties all 16 checkpoints");

    Check(gAssertions == 0, "valid calls raise no assertion");
    std::printf("AIModRouteRequestManager: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
