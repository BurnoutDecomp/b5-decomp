// L1 CAMPOOL (owner's list 2026-09-27, "There are still some asserts and crashes"): which pool each
// camera behaviour is allocated from.
//
// The runner (run_campool_behaviour_pools.py) writes campool_sandbox.inc from the revision under test:
//   * one fixture class per console behaviour type, in the REAL BrnDirector::Camera namespace, with
//     the size and alignment the revision's REAL header gives that type on this host (probed);
//   * the revision's own pool table (ConsoleBehaviourPool, when it has one), pool constants and
//     typedefs, the two pool member declarations and BehaviourManager::AllocateBehaviour<>'s body,
//     extracted as text from BrnBehaviourManager.h;
// and this file drives that body the way NewBehaviour<> does. Nothing here re-states the routing
// rule: the pool a type lands in is measured from the two pools' free counters.
//
// The console facts it checks against (ARTIST, each AllocateBehaviour<> sibling's asm):
//   LARGE -- `lwz r11,0x7D30(r30)` (large free count), `mr r4,r30`, h:0x47C (1148):
//            Failsafe @0x82259100, GameplayBumper @0x82253250, GameplayExternal @0x82258E28,
//            IceAnim @0x82263428
//   SMALL -- `lwzx r11,r30,0xFAA0` (small free count), `addi r4,r30,0x7D40`, h:0x471 (1137):
//            the other sixteen, GyroCam @0x822634D8 among them
// and the owner's crash (dumps 40624 / 54124 / 56552 on exe d65db9997047): the director held
// GameplayBumper, GameplayExternal, 2x IceAnim, RotateAboutVehicle, Interpolate (ArbStateCarSelect),
// BystanderCam (MomentBystanderSeesAction), 3x GyroCam (MomentTumbling), AftertouchCrash, GyroCam,
// Interpolate (ArbStateTakedown) and was allocating the take-down's second GyroCam
// (B3ClassicTakedownPlayer::Prepare): large pool 0 free, small pool 15 free, then the assert.
#include "campool_sandbox.inc"

#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

// ---- the assert hooks: record, then THROW so an out-of-slots allocation stops at its first assert
// (the production chain would go on to ObjectPool::operator[](-1) and read ~512 MB past the pool --
// the owner's access violation -- which a test process must not do).
struct CampoolAssert
{
    std::string mText;
};
static std::vector<std::string> gAsserts;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int)
    {
        gAsserts.push_back(lpcText ? lpcText : "");
        throw CampoolAssert{ lpcText ? lpcText : "" };
    }
    void* EndAssert() { return nullptr; }
}
}

namespace BrnDirector
{
namespace Camera
{
    static int giDebugDumps = 0;
    void BehaviourManager::DebugDumpToTTY() const { ++giDebugDumps; }
}
}

using namespace BrnDirector;
using namespace BrnDirector::Camera;

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPassed, const std::string& lrLabel)
{
    ++gChecks;
    if (!lbPassed)
        ++gFailures;
    std::printf("%s  %s\n", lbPassed ? "PASS" : "FAIL", lrLabel.c_str());
}

enum EConsolePool { E_CONSOLE_SMALL, E_CONSOLE_LARGE };

// A fresh manager: the two pools constructed (vptr + ObjectPool's all-free seed) and Prepare()d, as
// BehaviourManager::Prepare @0x8223DBE0 refills them.
static BehaviourManager* NewManager()
{
    static alignas(16) unsigned char saStorage[sizeof(BehaviourManager)];
    std::memset(saStorage, 0xCD, sizeof(saStorage));
    BehaviourManager* lpManager = new (saStorage) BehaviourManager();
    lpManager->mLargeBehaviourPool.Prepare();
    lpManager->mSmallBehaviourPool.Prepare();
    return lpManager;
}

struct Allocation
{
    bool        mbAsserted;
    std::string mAssert;
    int         miLargeTaken;   // how many slots the call took from each pool
    int         miSmallTaken;
    AbstractPoolVoidHandle mHandle;
};

template <typename TBehaviour>
static Allocation Allocate(BehaviourManager& lrManager)
{
    Allocation lResult = {};
    const int liLargeBefore = lrManager.mLargeBehaviourPool.GetNumFreeObjects();
    const int liSmallBefore = lrManager.mSmallBehaviourPool.GetNumFreeObjects();
    try
    {
        lResult.mHandle = lrManager.AllocateBehaviour<TBehaviour>();
    }
    catch (const CampoolAssert& lrAssert)
    {
        lResult.mbAsserted = true;
        lResult.mAssert    = lrAssert.mText;
    }
    lResult.miLargeTaken = liLargeBefore - lrManager.mLargeBehaviourPool.GetNumFreeObjects();
    lResult.miSmallTaken = liSmallBefore - lrManager.mSmallBehaviourPool.GetNumFreeObjects();
    return lResult;
}

template <typename TBehaviour>
static void CheckPool(const char* lpcName, EConsolePool leConsolePool, const char* lpcConsoleAddress)
{
    BehaviourManager& lrManager = *NewManager();
    const Allocation lAllocation = Allocate<TBehaviour>(lrManager);

    const bool lbSmall = !lAllocation.mbAsserted && lAllocation.miSmallTaken == 1 && lAllocation.miLargeTaken == 0;
    const bool lbLarge = !lAllocation.mbAsserted && lAllocation.miLargeTaken == 1 && lAllocation.miSmallTaken == 0;
    const bool lbConsole = (leConsolePool == E_CONSOLE_SMALL) ? lbSmall : lbLarge;
    char lacLabel[256];
    std::snprintf(lacLabel, sizeof(lacLabel), "%-28s allocated from the %s pool (console: %s, AllocateBehaviour<> @%s)",
                  lpcName, lbSmall ? "SMALL" : (lbLarge ? "LARGE" : "??"),
                  leConsolePool == E_CONSOLE_SMALL ? "SMALL" : "LARGE", lpcConsoleAddress);
    Check(lbConsole, lacLabel);

    // The slot it was given holds it: the handle records the object's size, and the object lies
    // wholly inside the pool it was taken from.
    bool lbFits = false;
    if (lbSmall || lbLarge)
    {
        const unsigned char* lpObject = static_cast<const unsigned char*>(lAllocation.mHandle.mpObject);
        const unsigned char* lpBegin  = lbSmall
            ? reinterpret_cast<const unsigned char*>(&lrManager.mSmallBehaviourPool.mObjectPool.maObjectPool[0])
            : reinterpret_cast<const unsigned char*>(&lrManager.mLargeBehaviourPool.mObjectPool.maObjectPool[0]);
        const size_t luBucket = lbSmall ? sizeof(lrManager.mSmallBehaviourPool.mObjectPool.maObjectPool[0])
                                        : sizeof(lrManager.mLargeBehaviourPool.mObjectPool.maObjectPool[0]);
        const size_t luOffset = static_cast<size_t>(lpObject - lpBegin);
        lbFits = lAllocation.mHandle.miSize == static_cast<s32>(sizeof(TBehaviour))
              && luOffset % luBucket == 0
              && sizeof(TBehaviour) <= luBucket;
    }
    std::snprintf(lacLabel, sizeof(lacLabel), "%-28s host sizeof %4zu fits its slot", lpcName, sizeof(TBehaviour));
    Check(lbFits, lacLabel);
}

int main()
{
    // ---- 1. every console type, one at a time, on a fresh manager ---------------------------------
    CheckPool<BehaviourFailsafe>          ("BehaviourFailsafe",           E_CONSOLE_LARGE, "0x82259100");
    CheckPool<BehaviourGameplayBumper>    ("BehaviourGameplayBumper",     E_CONSOLE_LARGE, "0x82253250");
    CheckPool<BehaviourGameplayExternal>  ("BehaviourGameplayExternal",   E_CONSOLE_LARGE, "0x82258E28");
    CheckPool<BehaviourIceAnim>           ("BehaviourIceAnim",            E_CONSOLE_LARGE, "0x82263428");
    CheckPool<BehaviourAftertouchCam>     ("BehaviourAftertouchCam",      E_CONSOLE_SMALL, "0x82263370");
    CheckPool<BehaviourAftertouchCrash>   ("BehaviourAftertouchCrash",    E_CONSOLE_SMALL, "0x82258ED8");
    CheckPool<BehaviourBystanderCam>      ("BehaviourBystanderCam",       E_CONSOLE_SMALL, "0x82259048");
    CheckPool<BehaviourDebugFlyWorld>     ("BehaviourDebugFlyWorld",      E_CONSOLE_SMALL, "0x82230690");
    CheckPool<BehaviourDebugOrbitPlayer>  ("BehaviourDebugOrbitPlayer",   E_CONSOLE_SMALL, "0x822305D8");
    CheckPool<BehaviourFixedCam>          ("BehaviourFixedCam",           E_CONSOLE_SMALL, "0x82259268");
    CheckPool<BehaviourGyroCam>           ("BehaviourGyroCam",            E_CONSOLE_SMALL, "0x822634D8");
    CheckPool<BehaviourHeliCam>           ("BehaviourHeliCam",            E_CONSOLE_SMALL, "0x82230748");
    CheckPool<BehaviourInterpolate>       ("BehaviourInterpolate",        E_CONSOLE_SMALL, "0x82258D70");
    CheckPool<BehaviourLooseAttachment>   ("BehaviourLooseAttachment",    E_CONSOLE_SMALL, "0x822591B0");
    CheckPool<BehaviourPassengerCam>      ("BehaviourPassengerCam",       E_CONSOLE_SMALL, "0x82230800");
    CheckPool<BehaviourRenderMetrics>     ("BehaviourRenderMetrics",      E_CONSOLE_SMALL, "0x8224B770");
    CheckPool<BehaviourRig>               ("BehaviourRig",                E_CONSOLE_SMALL, "0x82258F90");
    CheckPool<BehaviourRoadRunner>        ("BehaviourRoadRunner",         E_CONSOLE_SMALL, "0x8224B6B8");
    CheckPool<BehaviourRotateAboutVehicle>("BehaviourRotateAboutVehicle", E_CONSOLE_SMALL, "0x82259320");
    CheckPool<BehaviourSpirallingDeathcam>("BehaviourSpirallingDeathcam", E_CONSOLE_SMALL, "0x822593D8");

    // ---- 2. the owner's crash: the live set the three dumps hold, then the take-down's second rig ---
    {
        BehaviourManager& lrManager = *NewManager();
        gAsserts.clear();
        giDebugDumps = 0;
        std::vector<Allocation> lAllocations;
        lAllocations.push_back(Allocate<BehaviourGameplayBumper>(lrManager));      // shared gameplay cameras
        lAllocations.push_back(Allocate<BehaviourGameplayExternal>(lrManager));
        lAllocations.push_back(Allocate<BehaviourIceAnim>(lrManager));             // ArbStateCarSelect
        lAllocations.push_back(Allocate<BehaviourIceAnim>(lrManager));
        lAllocations.push_back(Allocate<BehaviourRotateAboutVehicle>(lrManager));
        lAllocations.push_back(Allocate<BehaviourInterpolate>(lrManager));
        lAllocations.push_back(Allocate<BehaviourGyroCam>(lrManager));             // MomentTumbling
        lAllocations.push_back(Allocate<BehaviourBystanderCam>(lrManager));        // MomentBystanderSeesAction
        lAllocations.push_back(Allocate<BehaviourGyroCam>(lrManager));             // MomentTumbling
        lAllocations.push_back(Allocate<BehaviourGyroCam>(lrManager));             // MomentTumbling
        lAllocations.push_back(Allocate<BehaviourAftertouchCrash>(lrManager));     // ArbStateTakedown::Prepare
        lAllocations.push_back(Allocate<BehaviourGyroCam>(lrManager));
        lAllocations.push_back(Allocate<BehaviourInterpolate>(lrManager));
        lAllocations.push_back(Allocate<BehaviourGyroCam>(lrManager));             // B3ClassicTakedownPlayer::Prepare

        const int liLargeFree = lrManager.mLargeBehaviourPool.GetNumFreeObjects();
        const int liSmallFree = lrManager.mSmallBehaviourPool.GetNumFreeObjects();
        std::printf("crash set: large free %d, small free %d, asserts %zu%s%s, debug dumps %d\n",
                    liLargeFree, liSmallFree, gAsserts.size(), gAsserts.empty() ? "" : " -- first: ",
                    gAsserts.empty() ? "" : gAsserts[0].c_str(), giDebugDumps);

        Check(gAsserts.empty(), "the owner's crash set allocates without an out-of-slots assert");
        Check(!lAllocations.back().mbAsserted, "the take-down's second GyroCam (B3ClassicTakedownPlayer::Prepare) gets a slot");
        Check(liLargeFree == 4, "large pool: the two gameplay cameras and the two ICE takes -- 4 of 8 free");
        Check(liSmallFree == 10, "small pool: the ten other behaviours -- 10 of 20 free");
        Check(giDebugDumps == 0, "no DebugDumpToTTY (the out-of-slots pre-check never trips)");
    }

    std::printf("CampoolBehaviourPools: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
