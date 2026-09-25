// FX-GATE item 4: BrnGameState::ModeManager::GetNumberOfCarsInFlyby (X360 0x82311E38), the body
// EXTRACTED from src/GameSource/GameState/ModeManager/BrnModeManager_Accessors.cpp by
// run_fxgate_flyby_car_count.py, compiled against a minimal ModeManager / GameStateModule /
// ScoringSystem that expose only what the body names.
//
// The console: 0x82311E50 bl GameStateModule::IsOnlineGameMode, then the flyby manager's vtable slot 1:
//   offline: OfflineFlybyManager vtable 0x820CE774 slot 1 = 0x827E2F38 `li r3, 0 ; blr`         -> 0
//   online:  OnlineFlybyManager vtable 0x820CFF94 slot 1 = CalculateNumberOfCarsInFlyby @0x82357F08:
//            n = GetNumberOfNetworkPlayersStillConnected() - 1 (0x82357FA0 / 0x82357FA4);
//            n > 0 (0x82357FA8 cmpwi ; bgt) ? (n >= 3 (0x82357FBC cmpwi ; blt keep) ? 3 : n) : 0
// on the scoring system both managers are constructed with, gsm + 0x1DD0 == the ModeManager's own
// mScoringSystem (GameStateModule::Construct 0x82380614 / 0x82380628 / 0x82380644).
// The consumer, GameMode::GetIntroDurationSeconds @0x82315A88, turns it into (n + 1) * 4.0 s.
#include <cstdio>

typedef int          s32;
typedef unsigned int u32;

namespace
{
    int giFires = 0;
}

// What the pre-fix body referenced: CGS_ASSERT and the one-shot parked log.
#define CGS_ASSERT(condition, msg) do { if (!(condition)) { ++giFires; } } while (0)
namespace CgsDev
{
    namespace Message { u32 gxMessageFilterFlags = 0; }
    namespace Log
    {
        struct Stream
        {
            template <typename T> Stream& operator<<(const T&) { return *this; }
        };
        Stream  gStream;
        Stream* gpDebugPrint = &gStream;
    }
}

struct ScoringSystem
{
    s32 miStillConnected = 0;
    s32 miCalls          = 0;
    s32 GetNumberOfNetworkPlayersStillConnected() const
    {
        ++const_cast<ScoringSystem*>(this)->miCalls;
        return miStillConnected;
    }
};

struct GameStateModule
{
    bool mbOnline = false;
    bool IsOnlineGameMode() { return mbOnline; }
};

struct ModeManager
{
    GameStateModule* mpGameStateModule = nullptr;
    ScoringSystem    mScoringSystem;
    ScoringSystem* GetScoringSystem() { return &mScoringSystem; }   // ModeManager_gUI_00.cpp:64
    s32 GetNumberOfCarsInFlyby();
};

#include "extracted.inc"

namespace
{
    int giChecks   = 0;
    int giFailures = 0;

    // The console's two slot-1 leaves.
    s32 Console(bool lbOnline, s32 liStillConnected)
    {
        if (!lbOnline)
        {
            return 0;                                   // 0x827E2F38
        }
        const s32 liRivals = liStillConnected - 1;      // 0x82357FA4
        if (!(liRivals > 0))
        {
            return 0;                                   // 0x82357FAC bgt not taken -> li r3, 0
        }
        return (liRivals < 3) ? liRivals : 3;           // 0x82357FC0 blt keep, else li 3
    }

    void Expect(bool lbOnline, s32 liStillConnected)
    {
        GameStateModule lModule;
        lModule.mbOnline = lbOnline;
        ModeManager lManager;
        lManager.mpGameStateModule = &lModule;
        lManager.mScoringSystem.miStillConnected = liStillConnected;

        giFires = 0;
        const s32 liActual   = lManager.GetNumberOfCarsInFlyby();
        const s32 liExpected = Console(lbOnline, liStillConnected);
        const float lfIntroSeconds = (static_cast<float>(liExpected) + 1.0f) * 4.0f;   // 0x82315AE8..0x82315B10
        ++giChecks;
        if (liActual != liExpected || giFires != 0)
        {
            ++giFailures;
            std::printf("FAIL: %s, %d player(s) still connected: %d car(s) in the flyby, console %d "
                        "(intro %.1f s), %d assert(s)\n",
                        lbOnline ? "online" : "offline", liStillConnected, liActual, liExpected,
                        lfIntroSeconds, giFires);
        }
        // Offline, the console never reads the scoring system (slot 1 is `li r3, 0`).
        ++giChecks;
        if (!lbOnline && lManager.mScoringSystem.miCalls != 0)
        {
            ++giFailures;
            std::printf("FAIL: offline, the scoring system was read %d time(s); the console's offline leaf "
                        "0x827E2F38 reads nothing\n", lManager.mScoringSystem.miCalls);
        }
    }
}

int main()
{
    for (s32 liConnected = 0; liConnected <= 8; ++liConnected)
    {
        Expect(false, liConnected);   // every offline mode: 0
        Expect(true, liConnected);    // online: clamp(connected - 1, 0, 3)
    }
    Expect(true, -1);                 // the count is a signed word; nothing below 1 gives rivals

    std::printf("%d/%d checks passed\n", giChecks - giFailures, giChecks);
    return giFailures == 0 ? 0 : 1;
}
