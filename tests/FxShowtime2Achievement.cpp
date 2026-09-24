// FX-SHOWTIME2 (crash parity 2026-09-24): the PRODUCTION AchievementManagerBase::OnShowTimeMultiplier,
// extracted from src/GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.cpp by
// run_fxshowtime2_achievement.py and run on the REAL AchievementManagerBase declaration through a
// recording leaf (the two protected virtuals are the console's vtable slots 0 and 1).
//
// Checked against the ARTIST asm -- the body is inlined at the tail of
// GameStateModule::UpdateShowtimeMode @0x82380EF8:
//     0x82381130  lwz  r29, 0x20D4(this)           the multiplier (CrashModeScoring+0x2E4)
//     0x82381134  li   r4, 0xD ; slot 1 bctrl      IsAchievementEarnt(13)
//     0x82381150  clrlwi ; cmplwi ; bne -> out     already earnt: nothing more
//     0x8238115C  cmpwi r29, 0xA ; blt -> out      SIGNED: fewer than ten: nothing more
//     0x82381168  li   r4, 0xD ; slot 0 bctrl      AchievementEarnt(13)
// so: slot 1 is asked exactly once per call, slot 0 fires only for an unearnt id with mult >= 10.
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h"
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
}

namespace BrnGameState
{
namespace
{
    // E_CONSOLE_ACHIEVEMENT_SHOWTIME_MULTIPLIER / KI_SHOWTIME_MULTIPLIER_FOR_ACHIEVEMENT, extracted.
#include "achievement_constants.inc"
}
#include "achievement_methods.inc"
}

namespace
{
    // A leaf that records every slot-1 / slot-0 call instead of touching a profile.
    class RecordingAchievementManager : public BrnGameState::AchievementManagerBase
    {
    public:
        s32  miAskCount    = 0;
        s32  miAskedId     = -1;
        s32  miEarnCount   = 0;
        s32  miEarnedId    = -1;
        bool mbAlreadyEarnt = false;

    protected:
        void AchievementEarnt(EAchievement leAchievement) override
        {
            ++miEarnCount;
            miEarnedId = static_cast<s32>(leAchievement);
        }
        bool IsAchievementEarnt(EAchievement leAchievement) override
        {
            ++miAskCount;
            miAskedId = static_cast<s32>(leAchievement);
            return mbAlreadyEarnt;
        }
    };
}

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

int main()
{
    {
        RecordingAchievementManager lManager;
        lManager.OnShowTimeMultiplier(9);
        Check(lManager.miAskCount == 1 && lManager.miAskedId == 13,
              "x9: IsAchievementEarnt(13) asked once (li r4, 0xD ; slot 1)");
        Check(lManager.miEarnCount == 0, "x9: below ten -> no AchievementEarnt (cmpwi 0xA ; blt)");
    }
    {
        RecordingAchievementManager lManager;
        lManager.OnShowTimeMultiplier(10);
        Check(lManager.miEarnCount == 1 && lManager.miEarnedId == 13,
              "x10: AchievementEarnt(13) fires (>= 10, slot 0)");
    }
    {
        RecordingAchievementManager lManager;
        lManager.OnShowTimeMultiplier(37);
        Check(lManager.miEarnCount == 1 && lManager.miEarnedId == 13, "x37: AchievementEarnt(13) fires");
    }
    {
        RecordingAchievementManager lManager;
        lManager.mbAlreadyEarnt = true;
        lManager.OnShowTimeMultiplier(12);
        Check(lManager.miAskCount == 1 && lManager.miEarnCount == 0,
              "already earnt: asked once, never re-awarded (bne -> out)");
    }
    {
        RecordingAchievementManager lManager;
        lManager.OnShowTimeMultiplier(-20);
        Check(lManager.miEarnCount == 0, "x-20: a SIGNED compare -- negative is below ten");
    }
    {
        RecordingAchievementManager lManager;
        lManager.OnShowTimeMultiplier(0);
        Check(lManager.miAskCount == 1 && lManager.miEarnCount == 0,
              "x0: the earnt test still runs first (slot 1 precedes the compare)");
    }
    Check(gAsserts == 0, "no assert fired");

    std::printf("FxShowtime2Achievement: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
