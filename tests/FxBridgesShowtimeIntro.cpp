// FX-BRIDGES (crash parity 2026-09-24) CC-9: the PRODUCTION body of BrnGame::BrnGameModule::BridgeControllerToWorld
// @0x823CD890 (src/GameSource/Game/GameBridgeControllerToX.cpp, extracted verbatim by run_fxbridges_showtime_intro.py)
// compiled against the REAL pad record (CgsInput::InputIO::PadOutputInformation / ActionInfo) and the REAL
// BrnWorld::PlayerVehicleControls, with small fixtures for the world input buffer, the input module's output
// buffer and the two GameStateModule getters the override calls.
//
// Checked against the ARTIST asm:
//   0x823CD8CC..0x823CDB7C  the pad -> controls copy: axes +0x00/+0x04/+0x08/+0x0C = pad +0x08/+0x00/+0x0C/+0x04,
//                           sensors 0, accel/brake/handbrake = maActionInfo[0/1/2].mfValue, steering =
//                           sign(LX)*|LX|^1.0 (exponent @0x82035228 == 1.0f) or the wheel axis pad+0x10 when
//                           meControllerState == 2, spin = action[55] - action[54], eight status bytes;
//   0x823CDB80..0x823CDBCC  IsInShowtimeIntro() -> brake 0.0, accel 1.0, handbrake 1.0 (flt_82001C98),
//                           steering GetShowtimeIntroSteering(), the four axes and four sensors 0.0;
//                           spin and the bools stay as read from the pad;
//   0x823CDBD8              ONE SetPlayerVehicleControls per call, after the override.
#include "types.hpp"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"                            // the REAL pad record
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h"  // the REAL controls
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                            // CgsModule::Event
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                  // the [showtime-intro] witness (silent here)
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
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
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
}

using CgsInput::InputIO::PadOutputInformation;
using BrnWorld::PlayerVehicleControls;

namespace BrnWorldIO
{
    typedef BrnWorld::PlayerVehicleControls PlayerVehicleControls;   // the typedef BrnWorldModuleIO.h carries
    struct GameActionQueue
    {
        s32 miEvents;
        void AddEvent(const CgsModule::Event*, s32, s32) { ++miEvents; }
    };
    struct DebugController { u8 maBytes[172]; };
}

namespace BrnGame
{
    // The file-scope action ids of GameBridgeControllerToX.cpp (the change-car / replay gesture).
    static const s32 KI_GUI_ACTION_OPTION2   = 53;
    static const s32 KI_GUI_ACTION_RSHOULDER = 55;

    // The 172-byte debug-controller image (only copied through here).
    struct DebugControllerImage { u8 maBytes[172]; };

    // The RETIRED bridge-local image, byte-identical to the pre-CC-9 header, so that the pre-fix body
    // compiles against this fixture (and then fails the override checks). The fixed body does not use it.
    struct WorldVehicleControlsImage
    {
        f32 mfAxis08, mfStickLX, mfAxis0C, mfStickLY;
        f32 mafZeroed[4];
        f32 mfAxis18, mfAxis20, mfAxis28, mfSteeringCurved, mfDistance;
        u8  mabStatus[8];
    };

    struct WorldInputFixture
    {
        PlayerVehicleControls       mControls;
        s32                         miSets;
        BrnWorldIO::GameActionQueue mQueue;
        BrnWorldIO::DebugController mDebug;
        void SetPlayerVehicleControls(const PlayerVehicleControls* lpControls) { mControls = *lpControls; ++miSets; }
        BrnWorldIO::GameActionQueue* GetGameActionQueue() { return &mQueue; }
        BrnWorldIO::DebugController* GetDebugController() { return &mDebug; }
    };

    struct InputOutputFixture
    {
        PadOutputInformation maPads[2];
        const PadOutputInformation* GetPadInfo(s32 liPort) const { return &maPads[liPort]; }
    };

    struct GameStateFixture
    {
        bool mbInIntro;
        f32  mfIntroSteering;
        mutable s32 miSteeringReads;
        bool IsInShowtimeIntro() const { return mbInIntro; }
        f32  GetShowtimeIntroSteering() const { ++miSteeringReads; return mfIntroSteering; }
    };

    struct GameModuleFixture
    {
        GameStateFixture mGameStateModule;
        s32              miSecondaryControllerPort;
        bool             mbNoPad;

        const PadOutputInformation* GetPadInfoForPlayer0(const InputOutputFixture* lpBuffer, s32* lpiPort)
        {
            *lpiPort = 0;
            return mbNoPad ? nullptr : lpBuffer->GetPadInfo(0);
        }
        void MapActionInfoToDebugController(DebugControllerImage*, const CgsInput::InputIO::ActionInfo*) {}

        // The body under test, under its production parameter names.
        void BridgeControllerToWorld(WorldInputFixture* lpWorldInput, const InputOutputFixture* lpInputOutputBuffer)
#include "fxbridges_showtime_intro_body.inc"
    };
}

using namespace BrnGame;

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

static GameModuleFixture  gModule;
static InputOutputFixture gInputs;
static WorldInputFixture  gWorld;

static void Reset(f32 lfStickLX, u32 luControllerState, bool lbIntro, f32 lfIntroSteering)
{
    std::memset(&gInputs, 0, sizeof(gInputs));
    std::memset(&gWorld, 0, sizeof(gWorld));
    PadOutputInformation& lrPad = gInputs.maPads[0];
    lrPad.mfStickLX = lfStickLX;
    lrPad.mfStickLY = 0.25f;
    lrPad.mfStickRX = -0.5f;
    lrPad.mfStickRY = 0.75f;
    lrPad.mfAxis10  = 0.375f;                    // the wheel's steering axis
    lrPad.maActionInfo[0].mfValue  = 0.8f;       // accelerate
    lrPad.maActionInfo[1].mfValue  = 0.6f;       // brake
    lrPad.maActionInfo[2].mfValue  = 0.4f;       // handbrake
    lrPad.maActionInfo[54].mfValue = 0.125f;
    lrPad.maActionInfo[55].mfValue = 0.625f;     // spin = 0.625 - 0.125 = 0.5
    lrPad.maActionInfo[13].muStatus = 1;         // horn
    lrPad.maActionInfo[3].muStatus  = 3;         // boost held + pressed (boost bounce)
    lrPad.maActionInfo[8].muStatus  = 2;         // start pressed
    lrPad.meControllerState = luControllerState;
    gModule.mGameStateModule.mbInIntro       = lbIntro;
    gModule.mGameStateModule.mfIntroSteering = lfIntroSteering;
    gModule.mGameStateModule.miSteeringReads = 0;
    gModule.miSecondaryControllerPort        = 1;
    gModule.mbNoPad                          = false;
}

static bool PadFlagsKept(const PlayerVehicleControls& c)
{
    return c.mfSpin == 0.5f && c.mbHorn && !c.mbChangeView && c.mbStart && !c.mbReset && !c.mbToggle
        && c.mbBoost && c.mbBoostBounce;
}

int main()
{
    Check(sizeof(PlayerVehicleControls) == 60 && offsetof(PlayerVehicleControls, mfAcceleration) == 0x20
          && offsetof(PlayerVehicleControls, mfBraking) == 0x24 && offsetof(PlayerVehicleControls, mfHandBrake) == 0x28
          && offsetof(PlayerVehicleControls, mfSteering) == 0x2C && offsetof(PlayerVehicleControls, mfSpin) == 0x30
          && offsetof(PlayerVehicleControls, mbHorn) == 0x34 && offsetof(PlayerVehicleControls, mbIsWheel) == 0x3A,
          "the published record is the console's 60-byte controls layout (var_140..var_105)");

    // 1. Driving: the pad goes straight through.
    Reset(0.5f, 1u, false, 0.0f);
    gModule.BridgeControllerToWorld(&gWorld, &gInputs);
    const PlayerVehicleControls& c = gWorld.mControls;
    Check(gWorld.miSets == 1, "one SetPlayerVehicleControls per call (0x823CDBD8)");
    Check(c.mfXAxis1 == -0.5f && c.mfXAxis0 == 0.5f && c.mfYAxis1 == 0.75f && c.mfYAxis0 == 0.25f
          && c.mfXSensor == 0.0f && c.mfYSensor == 0.0f && c.mfZSensor == 0.0f && c.mfGSensor == 0.0f,
          "axes +0x00..+0x0C = pad +0x08/+0x00/+0x0C/+0x04, sensors 0");
    Check(c.mfAcceleration == 0.8f && c.mfBraking == 0.6f && c.mfHandBrake == 0.4f && c.mfSteering == 0.5f
          && PadFlagsKept(c) && !c.mbIsWheel,
          "not in the intro: accel/brake/handbrake from actions 0/1/2, steering |LX|^1.0, spin + bools from the pad");
    Check(gModule.mGameStateModule.miSteeringReads == 0, "outside the intro the intro steering is never read");

    // 2. The curve keeps the sign; the wheel takes its own axis.
    Reset(-0.25f, 1u, false, 0.0f);
    gModule.BridgeControllerToWorld(&gWorld, &gInputs);
    Check(gWorld.mControls.mfSteering == -0.25f, "a left stick steers negative (fmuls by flt_820037C8 -1.0)");
    Reset(-0.25f, 2u, false, 0.0f);
    gModule.BridgeControllerToWorld(&gWorld, &gInputs);
    Check(gWorld.mControls.mfSteering == 0.375f && gWorld.mControls.mbIsWheel,
          "controller state 2 (wheel): steering = pad+0x10, mbIsWheel set");

    // 3. The Showtime intro: the pad is overridden.
    Reset(0.5f, 1u, true, -1.0f);
    gModule.BridgeControllerToWorld(&gWorld, &gInputs);
    Check(c.mfAcceleration == 1.0f && c.mfBraking == 0.0f && c.mfHandBrake == 1.0f,
          "in the intro: full throttle + full handbrake (flt_82001C98 1.0), no brake (0x823CDB94..0x823CDBA4)");
    Check(c.mfSteering == -1.0f && gModule.mGameStateModule.miSteeringReads == 1,
          "in the intro: steering = GetShowtimeIntroSteering() (0x823CDBA8), read once");
    Check(c.mfXAxis0 == 0.0f && c.mfXAxis1 == 0.0f && c.mfYAxis0 == 0.0f && c.mfYAxis1 == 0.0f
          && c.mfXSensor == 0.0f && c.mfYSensor == 0.0f && c.mfZSensor == 0.0f && c.mfGSensor == 0.0f,
          "in the intro: the four stick axes and four sensors are zeroed (0x823CDBB0..0x823CDBCC)");
    Check(PadFlagsKept(c) && gWorld.miSets == 1, "in the intro: spin and the eight bools stay as read from the pad");
    Reset(0.5f, 2u, true, 1.0f);
    gModule.BridgeControllerToWorld(&gWorld, &gInputs);
    Check(c.mfSteering == 1.0f && c.mbIsWheel, "the intro overrides the wheel's steering too (+1.0 latched)");

    // 4. No pad bound to player 0: nothing is published.
    Reset(0.5f, 1u, true, 1.0f);
    gModule.mbNoPad = true;
    gModule.BridgeControllerToWorld(&gWorld, &gInputs);
    Check(gWorld.miSets == 0, "no pad: early return before any publish (0x823CD8C8)");

    std::printf("FxBridgesShowtimeIntro: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
