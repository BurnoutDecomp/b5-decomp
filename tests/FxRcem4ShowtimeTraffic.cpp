// FX-RCEM4 (crash parity 2026-09-24): RaceCarEntityModule::PostSceneUpdate's Showtime traffic publish
// (ARTIST 0x822FE4BC..0x822FE554). run_fxrcem4_showtime_traffic.py extracts VERBATIM:
//   RaceCarEntityModule::PostSceneUpdate                  (BrnRaceCarEntityModule_CrashExit.cpp)
//   CrashPlayManager::IsPlayerInShowtimeOnGround          (BrnCrashPlayManager.cpp; DWARF :160, inlined
//                                                          at 0x822FE4C4..0x822FE510)
//   RaceCarToTrafficInterface::SetFlag / SetShowtimeTrafficDensityScale (the header inlines, :151 / :160)
// A piece the pre-fix source lacks is replayed as an empty stub.
//   0x822FE4C4  lbz +0x14D (IsInShowtime) ; 0x822FE4D0 lfs +0x140 > 0.0 (IsBounceBoosting) ;
//   0x822FE4F8  lfs +0x118 > flt_82CDB540 (1.0, KF_TIME_ON_GROUND_NO_PENALTY) -> r29
//   0x822FE524  interface +0x6A0: ori 2 / rlwinm clear-bit-1 ; 0x822FE554 stfs +0x6A4 (density scale)
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

static unsigned guAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { ++guAssertions; std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

namespace Fixture {
static std::vector<std::string> gaCalls;
static const f32 KF_TIME_ON_GROUND_NO_PENALTY = 1.0f;   // 0x82CDB540 (x360rd), BrnCrashPlayManager.cpp's static

struct CrashPlayManager {
    bool mbIsInShowtime = false;
    f32  mfBounceBoostTimer = 0.0f;       // +0x140
    f32  mfTimeSinceLastInAir = 0.0f;     // +0x118
    f32  mfDensityScale = 0.7f;
    bool IsInShowtime() const { return mbIsInShowtime; }
    bool IsBounceBoosting() const { return mfBounceBoostTimer > 0.0f; }
    f32  GetShowtimeTrafficDensityScale() const { gaCalls.push_back("density"); return mfDensityScale; }
    bool IsPlayerInShowtimeOnGround() const;
};
#include "fxrcem4_st_onground.inc"

namespace RaceCarEntityModuleIO {
struct RaceCarToTrafficInterface {
    enum Flag : s32 { E_FLAG_PLAYER_IS_POWER_PARKING = 0, E_FLAG_PLAYER_IS_IN_SHOWTIME_ON_GROUND = 1, E_FLAG_COUNT = 2 };
    u32 muFlags = 0;
    f32 mfShowtimeTrafficDensityScale = 1.0f;   // RaceCarToTrafficInterface::Construct
    bool IsFlagSet(Flag leFlag) const { return (muFlags & (1u << static_cast<u32>(leFlag))) != 0; }
    f32  GetShowtimeTrafficDensityScale() const { return mfShowtimeTrafficDensityScale; }
#include "fxrcem4_st_setters.inc"
};
struct InputBuffer_PostScene { void LockForRead() { gaCalls.push_back("lockR"); } void UnlockForRead() { gaCalls.push_back("unlockR"); } };
struct OutputBuffer_PostScene {
    RaceCarToTrafficInterface mRaceCarToTraffic;
    void LockForWrite() { gaCalls.push_back("lockW"); }
    void UnlockForWrite() { gaCalls.push_back("unlockW"); }
    RaceCarToTrafficInterface* GetRaceCarToTrafficInterface() { gaCalls.push_back("publish"); return &mRaceCarToTraffic; }
};
}
typedef u32 BrnUpdateSet;
inline bool CrashExitDiagEnabled() { return false; }

struct RaceCarEntityModule {
    CrashPlayManager mCrashPlayManager;
    void UpdateTrafficAndRaceCarNearMisses(RaceCarEntityModuleIO::InputBuffer_PostScene*) { gaCalls.push_back("nearmiss"); }
    void ProcessRaceCarCrashCompleteEvents(RaceCarEntityModuleIO::InputBuffer_PostScene*) { gaCalls.push_back("crashcomplete"); }
    void ProcessLeapedAndStompedCars(RaceCarEntityModuleIO::InputBuffer_PostScene*, RaceCarEntityModuleIO::OutputBuffer_PostScene*) { gaCalls.push_back("stomp"); }
    void SendResetOnTrackRequests(RaceCarEntityModuleIO::OutputBuffer_PostScene*) { gaCalls.push_back("reset"); }
    void CheckForResetOnTrackConditions() { gaCalls.push_back("watchdog"); }
    void PostSceneUpdate(RaceCarEntityModuleIO::InputBuffer_PostScene* lpInput,
                         RaceCarEntityModuleIO::OutputBuffer_PostScene* lpOutput, BrnUpdateSet lUpdateSet);
};
#include "fxrcem4_st_postscene.inc"
}   // namespace Fixture

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static int IndexOf(const char* lpc) {
    for (size_t i = 0; i < Fixture::gaCalls.size(); ++i) if (Fixture::gaCalls[i] == lpc) return static_cast<int>(i);
    return -1;
}

int main() {
    using namespace Fixture;
    const f32 lfNan = std::numeric_limits<f32>::quiet_NaN();

    // ---- IsPlayerInShowtimeOnGround: the inlined predicate --------------------------------------------
    {
        CrashPlayManager c;
        c.mfTimeSinceLastInAir = 2.0f;
        Check(!c.IsPlayerInShowtimeOnGround(), "not in Showtime (lbz +0x14D == 0) -> false");
        c.mbIsInShowtime = true;
        Check(c.IsPlayerInShowtimeOnGround(), "Showtime, not bounce-boosting, grounded 2.0 s > 1.0 -> true");
        c.mfBounceBoostTimer = 0.1f;
        Check(!c.IsPlayerInShowtimeOnGround(), "bounce-boosting (+0x140 > 0) -> false");
        c.mfBounceBoostTimer = lfNan;
        Check(c.IsPlayerInShowtimeOnGround(), "a NaN bounce timer is not boosting (bgt not taken) -> true");
        c.mfBounceBoostTimer = 0.0f; c.mfTimeSinceLastInAir = 1.0f;
        Check(!c.IsPlayerInShowtimeOnGround(), "exactly KF_TIME_ON_GROUND_NO_PENALTY (1.0) is not > -> false");
        c.mfTimeSinceLastInAir = lfNan;
        Check(!c.IsPlayerInShowtimeOnGround(), "a NaN time since last in air -> false (bgt not taken)");
    }

    // ---- PostSceneUpdate publishes both, every frame, between the stomp leg and the reset pump --------
    {
        RaceCarEntityModule lModule; RaceCarEntityModuleIO::InputBuffer_PostScene lIn; RaceCarEntityModuleIO::OutputBuffer_PostScene lOut;
        lModule.mCrashPlayManager.mbIsInShowtime = true; lModule.mCrashPlayManager.mfTimeSinceLastInAir = 3.0f;
        lModule.mCrashPlayManager.mfDensityScale = 0.55f;
        lOut.mRaceCarToTraffic.muFlags = 1u;   // the power-parking bit belongs to ProcessPowerParking
        gaCalls.clear();
        lModule.PostSceneUpdate(&lIn, &lOut, 0u);
        Check(lOut.mRaceCarToTraffic.muFlags == 3u,
              "on ground in Showtime -> flag bit 1 set (ori r11, r11, 2 @0x822FE52C), bit 0 untouched");
        Check(lOut.mRaceCarToTraffic.mfShowtimeTrafficDensityScale == 0.55f,
              "density scale = CrashPlayManager::GetShowtimeTrafficDensityScale (stfs f31, 0x6A4 @0x822FE554)");
        const int liStomp = IndexOf("stomp"), liPublish = IndexOf("publish"), liReset = IndexOf("reset");
        Check(liStomp >= 0 && liPublish > liStomp && liReset > liPublish && IndexOf("density") > liStomp,
              "the publish sits after ProcessLeapedAndStompedCars (0x822FE4B8) and before SendResetOnTrackRequests (0x822FE5A4)");

        lModule.mCrashPlayManager.mfBounceBoostTimer = 0.5f; lModule.mCrashPlayManager.mfDensityScale = 0.4f;
        lModule.PostSceneUpdate(&lIn, &lOut, 0u);
        Check(lOut.mRaceCarToTraffic.muFlags == 1u && lOut.mRaceCarToTraffic.mfShowtimeTrafficDensityScale == 0.4f,
              "bounce-boosting -> bit 1 cleared (rlwinm r11, r11, 0, 31, 29 @0x822FE534); the scale is republished every frame");

        RaceCarEntityModule lFree; RaceCarEntityModuleIO::OutputBuffer_PostScene lFreeOut;
        lFree.mCrashPlayManager.mfDensityScale = 1.0f;
        lFreeOut.mRaceCarToTraffic.muFlags = 2u; gaCalls.clear();
        lFree.PostSceneUpdate(&lIn, &lFreeOut, 1u);
        Check(lFreeOut.mRaceCarToTraffic.muFlags == 0u && IndexOf("publish") >= 0,
              "outside Showtime the flag is published clear; the publish is unconditional (also on a catch-up update)");
    }

    Check(guAssertions == 0, "no assertions");
    std::printf("FxRcem4ShowtimeTraffic: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
