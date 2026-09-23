// FX-CRASHMOD (crash parity 2026-09-23), G64-D1: replay the production
// CrashModule::ProcessCrashedRaceCarEvents (ARTIST 0x827CAAB8) and check the crash-duration select
// that opens each RaceCarCrash record (0x827CAE90..0x827CAF00):
//   AI       -> mbFastCrashesForAI ? 2.0f (0x82001D9C) : 4.5f (flt_820CA5B4)
//   network  -> 20.0f (flt_820CA5A8)
//   Showtime -> flt_8300E9B0 == KF_PLAYER_SHOWTIME_CAR_RESET_SECONDS == 15.0f (CRT thunk
//               0x82C6AC28: 1.0 + 3.0 + 10.0 + 1.0, stfs at 0x82C6AC58; read at 0x827CAEE0)
//   online   -> 5.0f (0x8200426C)
//   default  -> mfPlayerCrashTime
// The pre-fix body stored the .bss image value 0.0f in the Showtime arm.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/World/CrashModule/BrnCrashModule.h"
#include "rw/math/vpu/vector3_operation.h"
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <memory>

static unsigned assertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int) { ++assertions; std::fprintf(stderr, "ASSERT: %s\n", message); return 0; }
void* EndAssert() { return nullptr; }
} namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; } }

using namespace BrnWorld;

struct CrashFixture
{
    static constexpr u32 KU_INVALID_CRASH = 0xffffffffu;
    decltype(CrashModule::mRaceCarCrashes)   mRaceCarCrashes;
    decltype(CrashModule::mCrashingRaceCars) mCrashingRaceCars;
    f32  mfPlayerCrashTime    = 3.0f;
    bool mbFastCrashesForAI   = false;
    bool mbIsOnlineGameMode   = false;
    bool mbIsShowtimeGameMode = false;
    CrashFixture() { mRaceCarCrashes.Clear(); mCrashingRaceCars.UnSetAll(); }
    void ProcessCrashedRaceCarEvents(const CrashIO::InputBuffer_PostPhysics*, CrashIO::OutputBuffer_PostPhysics*);
    u32  FindCrashForRaceCar(EActiveRaceCarIndex) const;
};
#include "fxcrashmod_showtime_methods.inc"

int main()
{
    unsigned checks = 0, failures = 0;
    auto Check = [&](bool lbPass, const char* lpcName)
    {
        ++checks;
        if (!lbPass) { ++failures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
    };

    auto input  = std::make_unique<CrashIO::InputBuffer_PostPhysics>();
    auto output = std::make_unique<CrashIO::OutputBuffer_PostPhysics>();
    input->mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForRead);
    auto& queue = input->mVehicleManagerOutputInterface.mRaceCarCrashEventQueue;

    // One crash event for race car `slot`; returns the seconds of the record it opened (-1 if none).
    auto Open = [&](CrashFixture& lrModule, u32 slot, bool ai, bool network) -> f32
    {
        queue.Construct();
        BrnPhysics::Vehicle::RaceCarCrashEvent lEvent{};
        lEvent.mRaceCarVolumeInstanceID.muId = static_cast<u64>(0x01000000u | (slot << 10)) << 32;
        lEvent.mbCarIsAI      = ai;
        lEvent.mbCarIsNetwork = network;
        queue.AddEvent(lEvent);
        const u32 luBefore = lrModule.mRaceCarCrashes.GetLength();
        lrModule.ProcessCrashedRaceCarEvents(input.get(), output.get());
        if (lrModule.mRaceCarCrashes.GetLength() != luBefore + 1)
            return -1.0f;
        return lrModule.mRaceCarCrashes.GetItem(luBefore).mfSecondsBeforeCleanup;
    };

    {   // G64-D1: the player's crash in a Showtime mode (0x827CAED0 lbz 0x152A).
        CrashFixture m; m.mbIsShowtimeGameMode = true;
        Check(Open(m, 0, false, false) == 15.0f, "showtime player crash opens with 15.0 s (flt_8300E9B0)");
        Check(m.mCrashingRaceCars.IsBitSet(0),   "showtime record marks the car crashing");
    }
    {   // Showtime is tested before online (0x827CAED8 beq -> 0x827CAEE8): online Showtime is 15.0 too.
        CrashFixture m; m.mbIsShowtimeGameMode = true; m.mbIsOnlineGameMode = true;
        Check(Open(m, 3, false, false) == 15.0f, "online showtime player crash opens with 15.0 s");
    }
    {   // The AI and network arms come first and are untouched by the Showtime flag.
        CrashFixture m; m.mbIsShowtimeGameMode = true;
        Check(Open(m, 1, true, false) == 4.5f,   "showtime AI crash keeps 4.5 s (flt_820CA5B4)");
        Check(Open(m, 2, false, true) == 20.0f,  "showtime network crash keeps 20.0 s (flt_820CA5A8)");
        m.mbFastCrashesForAI = true;
        Check(Open(m, 4, true, false) == 2.0f,   "rapid-crash AI keeps 2.0 s (0x82001D9C)");
    }
    {
        CrashFixture m; m.mbIsOnlineGameMode = true;
        Check(Open(m, 5, false, false) == 5.0f,  "online (non-showtime) player crash opens with 5.0 s");
    }
    {
        CrashFixture m;
        Check(Open(m, 6, false, false) == 3.0f,  "offline player crash uses mfPlayerCrashTime");
        Check(Open(m, 6, false, false) == -1.0f && m.mRaceCarCrashes.GetLength() == 1,
              "an already-crashing car gets no second record");
    }

    Check(assertions == 0, "valid events raise no assertion");
    std::printf("FxCrashmodShowtimeReset: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
