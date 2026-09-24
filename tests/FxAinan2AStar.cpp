// FX-AINAN2 regression (crash parity 2026-09-24): AStarNodePool::ExtractBestOpenNode @0x82767788,
// extracted verbatim from Route/BrnAStar.cpp by run_fxainan2_astar.py (with GetNode @0x82765530).
//
// Console: the cost-weight assert is `fcmpu w, 0.0 (flt_82001CC0) ; blt -> fire` then
// `fcmpu w, 1.0 (flt_82001C98) ; ble -> skip` (0x827677C4/0x827677CC, 0x827677D8/0x827677DC):
// blt is not taken on an unordered compare and ble is, so a NaN weight skips the assert; the old
// `w >= 0 && w <= 1` fired it. The best-score select is `fcmpu score, best ; bge -> skip`
// (0x82767878/0x8276787C) -- a NaN score never wins, on both spellings (control).
#include "GameSource/World/AI/Route/BrnAStar.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
}
namespace PerfMonCpu {
void StartMonitor(s32) {}
void StopMonitor(s32) {}
}
}

#include "restored_methods.inc"

using namespace BrnAI;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        if (!lbPass) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
    }
    const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

    alignas(16) unsigned char gaPool[sizeof(AStarNodePool)];
    AStarNodePool& Pool() { return *reinterpret_cast<AStarNodePool*>(gaPool); }

    unsigned AssertsFor(f32 lfWeight)
    {
        std::memset(gaPool, 0, sizeof(gaPool));   // empty open set
        gAssertions = 0;
        u16 luBest = 0;
        (void)Pool().ExtractBestOpenNode(&luBest, lfWeight);
        return gAssertions;
    }

    void Open(u16 luNode, f32 lfCost, f32 lfHeuristic)
    {
        AStarNode* lpNode = Pool().GetNode(luNode);
        lpNode->SetCost(lfCost);
        lpNode->SetHeuristic(lfHeuristic);
        lpNode->mbOpen = true;
        Pool().mauOpenNodes[Pool().muOpenNodeCount++] = luNode;
    }
}

int main()
{
    Check(AssertsFor(0.5f) == 0u, "control: weight 0.5 is in range");
    Check(AssertsFor(1.0f) == 0u, "control: weight 1.0 is in range (ble taken)");
    Check(AssertsFor(-0.25f) == 1u, "control: weight -0.25 fires (blt taken)");
    Check(AssertsFor(1.5f) == 1u, "control: weight 1.5 fires (ble not taken)");
    Check(AssertsFor(KF_NAN) == 0u, "a NaN weight falls through blt and takes ble -> no assert");

    // Three open nodes, scores 5 / NaN / 3 at weight 1: the NaN never wins the bge select.
    std::memset(gaPool, 0, sizeof(gaPool));
    Pool().mauNodeCount[0] = 3;
    Open(0, 5.0f, 0.0f);
    Open(1, KF_NAN, 0.0f);
    Open(2, 3.0f, 0.0f);
    u16 luBest = 0xFFFF;
    gAssertions = 0;
    AStarNode* lpBest = Pool().ExtractBestOpenNode(&luBest, 1.0f);
    Check(lpBest == Pool().GetNode(2) && luBest == 2, "control: the lowest ordered score (node 2) wins; NaN (node 1) never does");
    Check(Pool().muOpenNodeCount == 2 && !Pool().GetNode(2)->IsOpen(), "control: the winner is closed and removed");

    std::printf("FxAinan2AStar: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
