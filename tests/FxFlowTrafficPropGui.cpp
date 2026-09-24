// FX-FLOW (crash parity 2026-09-24, G10-D9 caller): the PRODUCTION
// BrnGameModule::BridgeWorldTrafficAndPropDataToGui and BridgeWorldToGui
// (src/GameSource/Game/GameBridgeWorldToGui.cpp) and the PRODUCTION
// CrashModeScoring::DealWithRemovedTraffic (BrnCrashModeScoring.cpp), extracted by
// run_fxflow_traffic_prop_gui.py and run against the real CrashModeScoring layout, the real
// VariableEventQueue<32768,16> (the world output's GUI queue) and the real GUI record types.
// The world output buffer, the GUI input buffer, the mode manager and the module are fixtures:
// the GUI input records what is posted; the mode manager answers IsInProgress from a flag.
//
// Checked against the ARTIST asm of BridgeWorldTrafficAndPropDataToGui @0x823E5560:
//   0x823E55CC..0x823E55D8  GetGuiEventQueue + GetFirstEvent; 0x823E5740..0x823E5750 GetNextEvent
//   208 (0xD0)  mode in progress (this+0x66A520: lwz 0xD98 ; lwz 0x28 ; == 2) -> AddEvent(record,
//               0xD0, 0x290); else a local whose only store is its count word (`stw r27(=0),
//               var_460`, +0x280) -> AddEvent(local, 0xD0, 0x290)           0x823E5640..0x823E569C
//   209 (0xD1)  AddEvent(record, 0xD1, 0x38), then DealWithRemovedTraffic(this+0x66B2F0, record)
//                                                                              0x823E5618..0x823E5638
//   210 (0xD2)  as 208 with 0x410 bytes, count word +0x400 (`stw r27, var_50`) 0x823E56A0..0x823E56FC
//   512 (0x200) AddEvent(record, 0x200, 1)                                     0x823E5724..0x823E5738
//   592 (0x250) AddEvent(record, 0x250, 4)                                     0x823E5710..0x823E5738
//   every other id: nothing.
// BridgeWorldToGui @0x823EDD50: VehicleData (0x823EDD68), Route (0x823EDD74), TrafficAndProp
// (0x823EDD84), Impact (0x823EDD94).
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    DebugPrint* gpDebugPrint = nullptr;   // the [traffic-gui] witnesses stay silent
    StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
}
namespace Message { u64 gxMessageFilterFlags = 0; }
}

// ---- the GUI input-buffer fixture: GetGuiEvents()->AddEvent(event, id, size) records -----------
namespace CgsGui
{
namespace CgsGuiModuleIO
{
    struct PostedEvent
    {
        s32         miType;
        s32         miSize;
        const void* mpSource;
        alignas(16) u8 maBytes[1040];
    };

    struct RecordingQueue
    {
        PostedEvent maPosted[16];
        s32         miCount = 0;

        bool AddEvent(const CgsModule::Event* lpEvent, s32 liType, s32 liSize)
        {
            if (miCount >= 16 || liSize < 0 || liSize > 1040)
            {
                return false;
            }
            PostedEvent& lrPosted = maPosted[miCount++];
            lrPosted.miType   = liType;
            lrPosted.miSize   = liSize;
            lrPosted.mpSource = lpEvent;
            std::memset(lrPosted.maBytes, 0xCD, sizeof(lrPosted.maBytes));
            std::memcpy(lrPosted.maBytes, lpEvent, static_cast<size_t>(liSize));
            return true;
        }
    };

    struct InputBuffer
    {
        RecordingQueue  mQueue;
        RecordingQueue* GetGuiEvents() { return &mQueue; }
    };
}
}

// ---- the world output fixture: the real 32768-byte GUI event queue ------------------------------
namespace BrnWorldIO
{
    struct UpdateOutputBuffer
    {
        CgsModule::VariableEventQueue<32768, 16> mGuiEventQueue;
        const CgsModule::VariableEventQueue<32768, 16>* GetGuiEventQueue() const { return &mGuiEventQueue; }
    };
}

// ---- the module, as far as the code under test reaches it ---------------------------------------
namespace BrnGameState
{
    class ScoringSystem
    {
    public:
        CrashModeScoring  mCrashModeScoring;
        CrashModeScoring* GetCrashScorer() { return &mCrashModeScoring; }
    };

    class ModeManager
    {
    public:
        bool           mbInProgress = false;
        ScoringSystem  mScoringSystem;
        bool           IsInProgress() const { return mbInProgress; }
        ScoringSystem* GetScoringSystem()   { return &mScoringSystem; }
    };

    class GameStateModule
    {
    public:
        ModeManager  mModeManager;
        ModeManager* GetModeManager() { return &mModeManager; }
    };
}

namespace BrnGame
{
    class BrnGameModule
    {
    public:
        BrnGameState::GameStateModule mGameStateModule;
        s32 miVehicleDataAtGuiCount = -1;   // GUI events already posted when each leg ran
        s32 miImpactAtGuiCount      = -1;

        void BridgeWorldToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer,
                              const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer);
        void BridgeWorldTrafficAndPropDataToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
                                                const BrnWorldIO::UpdateOutputBuffer* lpWorldOutput);
        void BridgeWorldVehicleDataToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer,
                                         const BrnWorldIO::UpdateOutputBuffer*)
        {
            miVehicleDataAtGuiCount = lpGuiInputBuffer->GetGuiEvents()->miCount;
        }
        void BridgeWorldImpactInformationToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer,
                                               const BrnWorldIO::UpdateOutputBuffer*)
        {
            miImpactAtGuiCount = lpGuiInputBuffer->GetGuiEvents()->miCount;
        }
    };
}

// The production bodies under test (or the runner's labelled empty stand-ins).
#include "traffic_prop_gui.inc"

using BrnGameState::CrashModeScoring;
using CgsGui::CgsGuiModuleIO::InputBuffer;
using CgsGui::CgsGuiModuleIO::PostedEvent;

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

static s32 ReadS32(const u8* lpBytes, u32 luOffset)
{
    s32 liValue;
    std::memcpy(&liValue, lpBytes + luOffset, sizeof(liValue));
    return liValue;
}

// The world-side records, built as raw bytes at the console offsets.
alignas(16) static u8 gaTargets[656];    // 208: 20 x 32-byte VehicleScoreData, count word +0x280
alignas(16) static u8 gaSigns[1040];     // 210: 32 x 32-byte OverheadSignScore, count word +0x400
alignas(16) static u8 gaRemoved[56];     // 209: 25 x u16, count word +0x34

static void BuildTargets(s32 liCount)
{
    for (u32 luByte = 0; luByte < 640; ++luByte)
        gaTargets[luByte] = static_cast<u8>(luByte * 7 + 3);
    std::memset(gaTargets + 640, 0, 16);
    std::memcpy(gaTargets + 0x280, &liCount, sizeof(liCount));
}

static void BuildSigns(s32 liCount)
{
    for (u32 luByte = 0; luByte < 1024; ++luByte)
        gaSigns[luByte] = static_cast<u8>(luByte * 13 + 1);
    std::memset(gaSigns + 1024, 0, 16);
    std::memcpy(gaSigns + 0x400, &liCount, sizeof(liCount));
}

static void BuildRemoved(const u16* lpuIds, s32 liCount)
{
    std::memset(gaRemoved, 0, sizeof(gaRemoved));
    std::memcpy(gaRemoved, lpuIds, sizeof(u16) * static_cast<size_t>(liCount));
    std::memcpy(gaRemoved + 0x34, &liCount, sizeof(liCount));
}

static void SetRecent(CrashModeScoring& lrScorer, const u16* lpuIds, s32 liCount)
{
    lrScorer.maRecentCrashes.Construct();
    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        CrashModeScoring::RecentCrash* lpCrash = lrScorer.maRecentCrashes.AddNew();
        lpCrash->muTrafficCarIndex = lpuIds[liIndex];
        lpCrash->muCrashChainCount = static_cast<u16>(liIndex);
        lpCrash->mfTimeOfCrash     = static_cast<f32>(liIndex);
    }
}

static bool RecentIs(const CrashModeScoring& lrScorer, const u16* lpuIds, s32 liCount)
{
    if (lrScorer.maRecentCrashes.GetCount() != liCount)
        return false;
    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        if (lrScorer.maRecentCrashes.GetItem(static_cast<u32>(liIndex)).muTrafficCarIndex != lpuIds[liIndex])
            return false;
    }
    return true;
}

static BrnGame::BrnGameModule      gModule;
static BrnWorldIO::UpdateOutputBuffer gWorld;
static InputBuffer                  gGui;

static void Post(s32 liType, const void* lpBytes, s32 liSize)
{
    gWorld.mGuiEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(lpBytes), liType, liSize);
}

int main()
{
    std::memset(&gModule.mGameStateModule, 0, sizeof(gModule.mGameStateModule));
    CrashModeScoring& lrScorer = gModule.mGameStateModule.mModeManager.mScoringSystem.mCrashModeScoring;

    // ---- 1. mode in progress: every record of the five ids forwarded in queue order ----------------
    {
        gWorld.mGuiEventQueue.Construct();
        gGui.mQueue.miCount = 0;
        gModule.mGameStateModule.mModeManager.mbInProgress = true;
        const u16 kauRecent[4] = { 5, 7, 9, 11 };
        SetRecent(lrScorer, kauRecent, 4);

        const s32 kiOther = 0x11223344;
        const u8  kuPoolEmpty = 1;
        const s32 kiRaceCar = 3;
        const u16 kauRemoved[2] = { 5, 9 };
        BuildTargets(3);
        BuildSigns(2);
        BuildRemoved(kauRemoved, 2);
        Post(207, &kiOther, 4);
        Post(512, &kuPoolEmpty, 1);
        Post(208, gaTargets, 656);
        Post(209, gaRemoved, 56);
        Post(210, gaSigns, 1040);
        Post(592, &kiRaceCar, 4);
        Post(31, &kiOther, 4);
        const s32 liWorldLength = gWorld.mGuiEventQueue.GetLength();

        gModule.BridgeWorldTrafficAndPropDataToGui(&gGui, &gWorld);

        const PostedEvent* lp = gGui.mQueue.maPosted;
        const bool lbFive = gGui.mQueue.miCount == 5;
        Check(lbFive && lp[0].miType == 512 && lp[1].miType == 208 && lp[2].miType == 209 &&
              lp[3].miType == 210 && lp[4].miType == 592,
              "in progress: 512, 208, 209, 210, 592 forwarded in queue order (0x823E55FC switch)");
        Check(lbFive && lp[0].miSize == 1 && lp[1].miSize == 656 && lp[2].miSize == 56 &&
              lp[3].miSize == 1040 && lp[4].miSize == 4,
              "sizes 1 / 0x290 / 0x38 / 0x410 / 4 (the AddEvent `li r6` literals)");
        Check(lbFive && lp[0].maBytes[0] == 1, "512: the pool-emptied byte is forwarded");
        Check(lbFive && std::memcmp(lp[1].maBytes, gaTargets, 656) == 0 && ReadS32(lp[1].maBytes, 0x280) == 3,
              "208 in progress: the world's score-target record, byte for byte (count 3 at +0x280)");
        Check(lbFive && std::memcmp(lp[2].maBytes, gaRemoved, 56) == 0,
              "209: the removed-traffic record, byte for byte");
        Check(lbFive && std::memcmp(lp[3].maBytes, gaSigns, 1040) == 0 && ReadS32(lp[3].maBytes, 0x400) == 2,
              "210 in progress: the world's overhead-sign record, byte for byte (count 2 at +0x400)");
        Check(lbFive && ReadS32(lp[4].maBytes, 0) == 3, "592: the race car index word is forwarded");
        const u16 kauAfter[2] = { 7, 11 };
        Check(RecentIs(lrScorer, kauAfter, 2),
              "209 -> CrashModeScoring::DealWithRemovedTraffic: recent crashes {5,7,9,11} lose 5 and 9");
        bool lbOthers = true;
        for (s32 liIndex = 0; liIndex < gGui.mQueue.miCount; ++liIndex)
            lbOthers = lbOthers && lp[liIndex].miType != 207 && lp[liIndex].miType != 31;
        Check(lbOthers, "ids outside the five (207, 31) are not forwarded");
        Check(gWorld.mGuiEventQueue.GetLength() == liWorldLength,
              "the walk does not consume the world's GUI queue");
    }

    // ---- 2. mode not in progress: 208 / 210 go out as empty records, 209 is unaffected ------------
    {
        gWorld.mGuiEventQueue.Construct();
        gGui.mQueue.miCount = 0;
        gModule.mGameStateModule.mModeManager.mbInProgress = false;
        const u16 kauRecent[2] = { 9, 13 };
        SetRecent(lrScorer, kauRecent, 2);
        const u16 kauRemoved[1] = { 9 };
        BuildTargets(3);
        BuildSigns(2);
        BuildRemoved(kauRemoved, 1);
        Post(208, gaTargets, 656);
        Post(210, gaSigns, 1040);
        Post(209, gaRemoved, 56);

        gModule.BridgeWorldTrafficAndPropDataToGui(&gGui, &gWorld);

        const PostedEvent* lp = gGui.mQueue.maPosted;
        const bool lbThree = gGui.mQueue.miCount == 3;
        Check(lbThree && lp[0].miType == 208 && lp[0].miSize == 656 && ReadS32(lp[0].maBytes, 0x280) == 0,
              "208 not in progress: a 0x290-byte record whose count word (+0x280) is 0 (0x823E5688)");
        Check(lbThree && lp[1].miType == 210 && lp[1].miSize == 1040 && ReadS32(lp[1].maBytes, 0x400) == 0,
              "210 not in progress: a 0x410-byte record whose count word (+0x400) is 0 (0x823E56E8)");
        const u16 kauAfter[1] = { 13 };
        Check(lbThree && lp[2].miType == 209 && std::memcmp(lp[2].maBytes, gaRemoved, 56) == 0 &&
              RecentIs(lrScorer, kauAfter, 1),
              "209 has no mode gate: forwarded and DealWithRemovedTraffic drops 9 from {9,13}");
    }

    // ---- 3. an empty world queue posts nothing and leaves the scorer alone ------------------------
    {
        gWorld.mGuiEventQueue.Construct();
        gGui.mQueue.miCount = 0;
        const u16 kauRecent[2] = { 4, 6 };
        SetRecent(lrScorer, kauRecent, 2);
        gModule.BridgeWorldTrafficAndPropDataToGui(&gGui, &gWorld);
        Check(gGui.mQueue.miCount == 0 && RecentIs(lrScorer, kauRecent, 2),
              "empty world queue: nothing posted, recent crashes unchanged");
    }

    // ---- 4. the umbrella runs the leg between the vehicle-data and impact legs --------------------
    {
        gWorld.mGuiEventQueue.Construct();
        gGui.mQueue.miCount = 0;
        const u8 kuPoolEmpty = 0;
        Post(512, &kuPoolEmpty, 1);
        gModule.miVehicleDataAtGuiCount = -1;
        gModule.miImpactAtGuiCount      = -1;
        gModule.BridgeWorldToGui(&gGui, &gWorld);
        Check(gModule.miVehicleDataAtGuiCount == 0 && gModule.miImpactAtGuiCount == 1 &&
              gGui.mQueue.miCount == 1 && gGui.mQueue.maPosted[0].miType == 512,
              "BridgeWorldToGui: VehicleData (0x823EDD68), then TrafficAndProp (0x823EDD84), then Impact (0x823EDD94)");
    }

    Check(gAsserts == 0, "no assert on the way");

    std::printf("FxFlowTrafficPropGui: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
