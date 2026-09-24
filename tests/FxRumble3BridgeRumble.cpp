// FX-RUMBLE3 (crash parity 2026-09-24, G10-D4): the rumble hand-over from the game state to the input
// module -- the PRODUCTION BrnGameState::RumbleManager::Construct / BridgeRumbleToInput
// (BrnRumbleManager.cpp) and the CgsInput::InputIO::PreWorldInputBuffer bodies it posts through
// (CgsInputModuleIO.cpp, CgsInputProcessRumbleQueues.cpp), extracted by run_fxrumble3_bridge_rumble.py
// and compiled against the revision's own BrnRumbleManager.h / CgsInputModuleIO.h, with the real
// CgsModule::IOBuffer (CgsIOBuffer.cpp) and CgsDev::StrStream. Expected values from the ARTIST asm:
//   PreWorldInputBuffer::Construct @0x828F8500   status "constructed" (stb 1,0(this)); the four queues
//        empty (stw 0 -> +0xC/+0x108/+0x224/+0x330); +0x395 enable = 1 and +0x396 force feedback = 1
//        (0x828F8544/48); +0x394 pause = 0 (0x828F855C)
//   PreWorldInputBuffer::Destruct  @0x828EF358   the four lengths 0, then IOBuffer::Destruct
//   PostPlayJoltEffectByPlayer @0x828EF370, PostPlayRumbleEffectByPlayer @0x828EF4B0,
//   PostChangeVolumeRumbleEffectByPlayer @0x828EF608, PostStopRumbleEffectByPlayer @0x828EF758:
//        "Not locked for writing\n" and "Player must be greater than -1\n" asserts (non-gating), then
//        the event {player, port -1 (`li -1 ; stw 0x74`), ...} appended to its queue
//   RumbleManager::BridgeRumbleToInput @0x82364978: every queued jolt (length read once, 0x82364998),
//        stop (0x823649D8), play (0x82364A18) and volume change (0x82364AE8) posted in queue order
//        through the four Post*ByPlayer with the event's own fields (its port is NOT carried -- the
//        posts write -1); SetTimerStatusInterface (0x82364B9C); +0x394 = mbRumblePaused ||
//        mbInPictureParadise, +0x395 = mbRumbleEnabled, +0x396 = mbWheelForceFeedback
//        (0x82364BA0..0x82364BD0); the four manager lengths 0 (0x82364BD4..0x82364BE0).
#include "GameSource/GameState/RumbleManager/BrnRumbleManager.h"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }   // the bridge's [DIAG] line stays silent
}

// The production bodies, extracted verbatim (see the runner).
#include "fxrumble3_bridge_rumble.inc"

using namespace CgsInput::InputIO;
using BrnGameState::RumbleManager;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

static unsigned char Byte(const bool& lrFlag)
{
    unsigned char luByte = 0;
    std::memcpy(&luByte, &lrFlag, 1);
    return luByte;
}

// Twelve distinct lanes per effect, so a swapped or partial copy is visible.
static JoltEffect MakeEffect(f32 lfBase)
{
    JoltEffect lEffect;
    f32* lpfLanes = &lEffect.mLowFreqJoltData.mfAttackTime;
    for (s32 li = 0; li < 12; ++li)
    {
        lpfLanes[li] = lfBase + 0.125f * static_cast<f32>(li);
    }
    return lEffect;
}

static bool SameEffect(const JoltEffect& lrA, const JoltEffect& lrB)
{
    return std::memcmp(&lrA, &lrB, sizeof(JoltEffect)) == 0;
}

static PlayJoltEffectEvent Jolt(s32 liPlayer, s32 liPort, s32 liPriority, const JoltEffect& lrEffect)
{
    PlayJoltEffectEvent lEvent;
    lEvent.miPlayer = liPlayer; lEvent.miPort = liPort; lEvent.miRumblePriority = liPriority; lEvent.mJoltEffect = lrEffect;
    return lEvent;
}

static StopRumbleEffectEvent Stop(s32 liPlayer, s32 liPort, s32 liId)
{
    StopRumbleEffectEvent lEvent;
    lEvent.miPlayer = liPlayer; lEvent.miPort = liPort; lEvent.miRumbleId = liId;
    return lEvent;
}

static PlayRumbleEffectEvent Play(s32 liPlayer, s32 liPort, s32 liPriority, const JoltEffect& lrEffect, s32 liId, f32 lfVolume)
{
    PlayRumbleEffectEvent lEvent;
    lEvent.miPlayer = liPlayer; lEvent.miPort = liPort; lEvent.miRumblePriority = liPriority; lEvent.mJoltEffect = lrEffect;
    lEvent.miRumbleId = liId; lEvent.mfRumbleVolume = lfVolume;
    return lEvent;
}

static ChangeVolumeRumbleEffectEvent Volume(s32 liPlayer, s32 liPort, const JoltEffect& lrEffect, s32 liId, f32 lfVolume)
{
    ChangeVolumeRumbleEffectEvent lEvent;
    lEvent.miPlayer = liPlayer; lEvent.miPort = liPort; lEvent.mJoltEffect = lrEffect;
    lEvent.miRumbleId = liId; lEvent.mfRumbleVolume = lfVolume;
    return lEvent;
}

static RumbleManager       gManager;
static PreWorldInputBuffer gBuffer;

static void FreshManager()
{
    std::memset(&gManager, 0xCD, sizeof(gManager));   // Construct seeds, not the harness
    gManager.Construct();
}

static void FreshBuffer()
{
    std::memset(&gBuffer, 0xCD, sizeof(gBuffer));
    gBuffer.Construct();
}

static bool BufferEmpty()
{
    return gBuffer.mPlayJoltEffectEventQueue.GetLength() == 0 && gBuffer.mPlayRumbleEffectEventQueue.GetLength() == 0
        && gBuffer.mChangeVolumeRumbleEffectEventQueue.GetLength() == 0 && gBuffer.mStopRumbleEffectEventQueue.GetLength() == 0;
}

static bool ManagerEmpty()
{
    return gManager.mPlayJoltEffectEventQueue.GetLength() == 0 && gManager.mPlayRumbleEffectEventQueue.GetLength() == 0
        && gManager.mChangeVolumeRumbleEffectEventQueue.GetLength() == 0 && gManager.mStopRumbleEffectEventQueue.GetLength() == 0;
}

// One bridge call under the write lock, the way DoUpdate_InputPreWorld makes it (0x823C56DC..0x823C56FC).
static void Bridge(const CgsSystem::TimerStatusInterface& lrTimer)
{
    gBuffer.LockForWrite();
    gManager.BridgeRumbleToInput(&gBuffer, &lrTimer);
    gBuffer.UnlockForWrite();
}

int main()
{
    // ---- PreWorldInputBuffer::Construct @0x828F8500 ------------------------------------------------
    FreshBuffer();
    Check(BufferEmpty(), "Construct: the four queues start empty (stw 0 -> +0xC/+0x108/+0x224/+0x330)");
    Check(Byte(gBuffer.mbEnableRumble) == 1, "Construct: +0x395 mbEnableRumble = 1 (0x828F8544)");
    Check(Byte(gBuffer.mbForceFeedback) == 1, "Construct: +0x396 mbForceFeedback = 1 (0x828F8548)");
    Check(Byte(gBuffer.mbPauseRumble) == 0, "Construct: +0x394 mbPauseRumble = 0 (0x828F855C)");
    {
        const unsigned luBefore = gAsserts;
        gBuffer.LockForWrite();   // asserts unless the IOBuffer status is exactly "constructed"
        gBuffer.UnlockForWrite();
        Check(gAsserts == luBefore, "Construct: IOBuffer::Construct ran (the lock pair raises no assert)");
    }

    // ---- the four posts --------------------------------------------------------------------------
    {
        const JoltEffect lA = MakeEffect(0.5f), lB = MakeEffect(2.0f), lC = MakeEffect(7.0f);
        gBuffer.LockForWrite();
        const unsigned luBefore = gAsserts;
        gBuffer.PostPlayJoltEffectByPlayer(2, 1010, lA);
        gBuffer.PostPlayRumbleEffectByPlayer(1, 1000, lB, 7, 0.75f);
        gBuffer.PostChangeVolumeRumbleEffectByPlayer(3, lC, 7, 0.25f);
        gBuffer.PostStopRumbleEffectByPlayer(0, 9);
        Check(gAsserts == luBefore, "Post*ByPlayer under the write lock with a player >= 0 raise no assert");

        const PlayJoltEffectEvent& lrJ = gBuffer.mPlayJoltEffectEventQueue.GetEvent(0);
        Check(gBuffer.mPlayJoltEffectEventQueue.GetLength() == 1 && lrJ.miPlayer == 2 && lrJ.miPort == -1
              && lrJ.miRumblePriority == 1010 && SameEffect(lrJ.mJoltEffect, lA),
              "PostPlayJoltEffectByPlayer @0x828EF370: {player, port -1, priority, 0x30-byte jolt}");
        const PlayRumbleEffectEvent& lrP = gBuffer.mPlayRumbleEffectEventQueue.GetEvent(0);
        Check(gBuffer.mPlayRumbleEffectEventQueue.GetLength() == 1 && lrP.miPlayer == 1 && lrP.miPort == -1
              && lrP.miRumblePriority == 1000 && SameEffect(lrP.mJoltEffect, lB) && lrP.miRumbleId == 7
              && lrP.mfRumbleVolume == 0.75f,
              "PostPlayRumbleEffectByPlayer @0x828EF4B0: {player, -1, priority, jolt, id, volume}");
        const ChangeVolumeRumbleEffectEvent& lrV = gBuffer.mChangeVolumeRumbleEffectEventQueue.GetEvent(0);
        Check(gBuffer.mChangeVolumeRumbleEffectEventQueue.GetLength() == 1 && lrV.miPlayer == 3 && lrV.miPort == -1
              && SameEffect(lrV.mJoltEffect, lC) && lrV.miRumbleId == 7 && lrV.mfRumbleVolume == 0.25f,
              "PostChangeVolumeRumbleEffectByPlayer @0x828EF608: {player, -1, jolt, id, volume}");
        const StopRumbleEffectEvent& lrS = gBuffer.mStopRumbleEffectEventQueue.GetEvent(0);
        Check(gBuffer.mStopRumbleEffectEventQueue.GetLength() == 1 && lrS.miPlayer == 0 && lrS.miPort == -1 && lrS.miRumbleId == 9,
              "PostStopRumbleEffectByPlayer @0x828EF758: {player, -1, id}");

        const unsigned luBeforeNegative = gAsserts;
        gBuffer.PostStopRumbleEffectByPlayer(-1, 1);
        Check(gAsserts == luBeforeNegative + 1, "a player of -1 fires \"Player must be greater than -1\" (:260)");
        gBuffer.UnlockForWrite();

        const unsigned luBeforeUnlocked = gAsserts;
        gBuffer.PostPlayJoltEffectByPlayer(0, 1, lA);
        Check(gAsserts == luBeforeUnlocked + 1, "a post outside the write lock fires \"Not locked for writing\" (:114)");
    }

    // ---- PreWorldInputBuffer::Destruct @0x828EF358 -------------------------------------------------
    gBuffer.Destruct();
    Check(BufferEmpty(), "Destruct: the four queue lengths are 0 (words 3 / 66 / 137 / 204)");

    // ---- RumbleManager::BridgeRumbleToInput @0x82364978 --------------------------------------------
    CgsSystem::TimerStatusInterface lTimer;
    std::memset(&lTimer, 0x5A, sizeof(lTimer));
    lTimer.mGameTimerStatus.mfBaseTimeStep       = 1.0f / 60.0f;
    lTimer.mGameTimerStatus.mfTimeStepMultiplier = 0.5f;

    const JoltEffect lE0 = MakeEffect(1.0f), lE1 = MakeEffect(3.0f), lE2 = MakeEffect(5.0f);
    const JoltEffect lE3 = MakeEffect(9.0f), lE4 = MakeEffect(11.0f);
    {
        FreshManager();
        FreshBuffer();
        Check(Byte(gManager.mbRumbleEnabled) == 1 && Byte(gManager.mbWheelForceFeedback) == 1 && ManagerEmpty(),
              "RumbleManager::Construct @0x82378A70 seeds enable / force feedback on and empty queues");

        gManager.mPlayJoltEffectEventQueue.AddEvent(Jolt(0, 3, 1010, lE0));   // port 3: must NOT be carried
        gManager.mPlayJoltEffectEventQueue.AddEvent(Jolt(1, 0, 1000, lE1));
        gManager.mStopRumbleEffectEventQueue.AddEvent(Stop(0, 2, 11));
        gManager.mPlayRumbleEffectEventQueue.AddEvent(Play(0, 1, 1003, lE2, 11, 0.5f));
        gManager.mPlayRumbleEffectEventQueue.AddEvent(Play(2, -1, 1004, lE3, 12, 1.0f));
        gManager.mChangeVolumeRumbleEffectEventQueue.AddEvent(Volume(3, 0, lE4, 12, 0.125f));
        gManager.mbRumblePaused       = false;
        gManager.mbInPictureParadise  = true;
        gManager.mbRumbleEnabled      = false;
        gManager.mbWheelForceFeedback = false;

        const unsigned luBefore = gAsserts;
        Bridge(lTimer);
        Check(gAsserts == luBefore, "the bridge under the write lock raises no assert");

        gBuffer.LockForRead();
        const PreWorldInputBuffer::PlayJoltEffectEventQueue* lpJolts = gBuffer.GetPlayJoltEffectEventQueue();
        Check(lpJolts->GetLength() == 2, "jolts: both queued jolts are posted (0x82364998 loop)");
        Check(lpJolts->GetLength() == 2 && lpJolts->GetEvent(0).miPlayer == 0 && lpJolts->GetEvent(0).miPort == -1
              && lpJolts->GetEvent(0).miRumblePriority == 1010 && SameEffect(lpJolts->GetEvent(0).mJoltEffect, lE0),
              "jolt 0: player 0, priority 1010, its envelope; port -1 (the manager's port 3 is not carried)");
        Check(lpJolts->GetLength() == 2 && lpJolts->GetEvent(1).miPlayer == 1 && lpJolts->GetEvent(1).miPort == -1
              && lpJolts->GetEvent(1).miRumblePriority == 1000 && SameEffect(lpJolts->GetEvent(1).mJoltEffect, lE1),
              "jolt 1: player 1, priority 1000, its envelope -- queue order kept");

        const PreWorldInputBuffer::StopRumbleEffectEventQueue* lpStops = gBuffer.GetStopRumbleEffectEventQueue();
        Check(lpStops->GetLength() == 1 && lpStops->GetEvent(0).miPlayer == 0 && lpStops->GetEvent(0).miPort == -1
              && lpStops->GetEvent(0).miRumbleId == 11,
              "stops: {player 0, port -1, id 11} (0x823649D8 loop -> PostStopRumbleEffectByPlayer)");

        const PreWorldInputBuffer::PlayRumbleEffectEventQueue* lpPlays = gBuffer.GetPlayRumbleEffectEventQueue();
        Check(lpPlays->GetLength() == 2, "plays: both queued rumbles are posted (0x82364A18 loop)");
        Check(lpPlays->GetLength() == 2 && lpPlays->GetEvent(0).miPlayer == 0 && lpPlays->GetEvent(0).miPort == -1
              && lpPlays->GetEvent(0).miRumblePriority == 1003 && SameEffect(lpPlays->GetEvent(0).mJoltEffect, lE2)
              && lpPlays->GetEvent(0).miRumbleId == 11 && lpPlays->GetEvent(0).mfRumbleVolume == 0.5f,
              "play 0: player, priority, envelope, id 11 (ev+0x3C), volume 0.5 (ev+0x40, f1)");
        Check(lpPlays->GetLength() == 2 && lpPlays->GetEvent(1).miPlayer == 2 && lpPlays->GetEvent(1).miRumblePriority == 1004
              && SameEffect(lpPlays->GetEvent(1).mJoltEffect, lE3) && lpPlays->GetEvent(1).miRumbleId == 12
              && lpPlays->GetEvent(1).mfRumbleVolume == 1.0f,
              "play 1: player 2, priority 1004, id 12, volume 1.0 -- queue order kept");

        const PreWorldInputBuffer::ChangeVolumeRumbleEffectEventQueue* lpVolumes = gBuffer.GetChangeVolumeRumbleEffectEventQueue();
        Check(lpVolumes->GetLength() == 1 && lpVolumes->GetEvent(0).miPlayer == 3 && lpVolumes->GetEvent(0).miPort == -1
              && SameEffect(lpVolumes->GetEvent(0).mJoltEffect, lE4) && lpVolumes->GetEvent(0).miRumbleId == 12
              && lpVolumes->GetEvent(0).mfRumbleVolume == 0.125f,
              "volumes: {player 3, -1, envelope (ev+8), id 12 (ev+0x38), volume 0.125 (ev+0x3C)} (0x82364AE8 loop)");

        Check(std::memcmp(gBuffer.GetTimerStatusInt(), &lTimer, sizeof(lTimer)) == 0,
              "SetTimerStatusInterface (0x82364B9C): the game timer snapshot is copied whole (48 bytes)");
        Check(gBuffer.GetTimerStatusInt()->GetGameTimerStatus()->GetCurrentTimeStep() == (1.0f / 60.0f) * 0.5f,
              "the published game time step is the timer's base step x multiplier");
        gBuffer.UnlockForRead();

        Check(Byte(gBuffer.mbPauseRumble) == 1, "+0x394 pause = mbRumblePaused || mbInPictureParadise (Picture Paradise alone pauses)");
        Check(Byte(gBuffer.mbEnableRumble) == 0, "+0x395 enable = mbRumbleEnabled (0 published over Construct's 1)");
        Check(Byte(gBuffer.mbForceFeedback) == 0, "+0x396 force feedback = mbWheelForceFeedback (0 published over Construct's 1)");
        Check(ManagerEmpty(), "the four manager queue lengths are reset (0x82364BD4..0x82364BE0)");
    }
    {
        // The pause flag's other operand, and the enable / force-feedback 1 path.
        FreshManager();
        FreshBuffer();
        gManager.mbRumblePaused       = true;
        gManager.mbInPictureParadise  = false;
        gManager.mbRumbleEnabled      = true;
        gManager.mbWheelForceFeedback = true;
        Bridge(lTimer);
        Check(Byte(gBuffer.mbPauseRumble) == 1, "+0x394 pause = 1 from mbRumblePaused alone");
        Check(Byte(gBuffer.mbEnableRumble) == 1 && Byte(gBuffer.mbForceFeedback) == 1, "+0x395 / +0x396 publish 1 / 1");
        Check(BufferEmpty(), "an empty manager posts nothing");

        FreshManager();
        FreshBuffer();
        gManager.mbRumblePaused      = false;
        gManager.mbInPictureParadise = false;
        gBuffer.mbPauseRumble        = true;   // a stale value: the bridge must overwrite it
        Bridge(lTimer);
        Check(Byte(gBuffer.mbPauseRumble) == 0, "+0x394 pause = 0 when neither is set (the store is unconditional)");
    }
    {
        // A full manager queue: all four jolts cross (the length is the loop bound, capacity 4).
        FreshManager();
        FreshBuffer();
        for (s32 li = 0; li < 4; ++li)
        {
            gManager.mPlayJoltEffectEventQueue.AddEvent(Jolt(0, -1, 1000 + li, MakeEffect(static_cast<f32>(li))));
        }
        Bridge(lTimer);
        gBuffer.LockForRead();
        const PreWorldInputBuffer::PlayJoltEffectEventQueue* lpJolts = gBuffer.GetPlayJoltEffectEventQueue();
        bool lbOrdered = lpJolts->GetLength() == 4;
        for (s32 li = 0; lbOrdered && li < 4; ++li)
        {
            lbOrdered = lpJolts->GetEvent(li).miRumblePriority == 1000 + li;
        }
        gBuffer.UnlockForRead();
        Check(lbOrdered, "four queued jolts -> four posted, in order");
        Check(ManagerEmpty(), "...and the manager's jolt queue is empty again (the next PlayJolt is accepted)");
    }

    std::printf("FxRumble3BridgeRumble: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
