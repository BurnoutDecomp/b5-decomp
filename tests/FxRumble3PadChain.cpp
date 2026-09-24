// FX-RUMBLE3 (crash parity 2026-09-24, G10-D4): the input module end of the rumble chain, run end to end
// on the PRODUCTION bodies -- CgsInput::InputModule::PreWorldUpdate @0x82903328 and ProcessRumbleRequests
// @0x828FFE50 (extracted from CgsInputModule.cpp), with the revision's whole CgsInputPads.cpp,
// CgsInputDeviceX360Pad.cpp, CgsInputModuleIO.cpp and CgsInputProcessRumbleQueues.cpp compiled beside it
// (run_fxrumble3_pad_rumble.py), the real CgsModule::IOBuffer and CgsDev::StrStream. The XDK motor
// entry points (XInputSetState / XInputFF*) and the PC pad scan (InputPadsPC::UpdatePadDevices) are
// RECORDING stand-ins: the chain's observable output is exactly what reaches them. Expected values
// from the ARTIST asm:
//   PreWorldUpdate            output write lock around everything, pre-world read lock around
//                             ProcessRumbleRequests; `if (lbUpdatePads) InputPads::Update` (the PC seat:
//                             UpdatePadDevices); the module's bind / unbind results Appended into the output
//                             buffer (0x82903384..0x829033A0) and the module queues emptied
//   ProcessRumbleRequests     jolt, STOP, PLAY, volume (a stop and a play of one id in one step leave the
//                             rumble playing; a volume change after its play applies); the step is the GAME
//                             timer's base x multiplier (0x8290007C..0x82900094); pause = +0x394 ||
//                             !lbUpdatePads; UpdateRumble inlined (0x829000C4..0x829000F0)
//   InputPads::BindPlayerToPort @0x828DBEF0  3 / 4 past the bounds (unsigned), OK when the pair already
//                             holds, 1 player already bound, 2 port already bound, else both sides stored
//   DeviceX360Pad::SetRumble  @0x828E78D0  "IsConnected()" assert (:679) and the RAW +0x10 gate; pad arm
//                             left = (f32)pow(left, 1.5 (0x820FA3D0)) x 65535 (0x820F78F0), right = right x
//                             65535, fctidz -> u16, XInputSetState(mePort, &cached); any failure stores the
//                             PREVIOUS LEFT into both halves (0x828E7A98); wheel arm x 655350 (0x82F3471C)
//                             fsel-Clamp'd to [0, 65535], nothing sent while +0xF8 == 997, 997 / 1167 quiet,
//                             anything else "WHEEL ERROR " (:699)
// The motor numbers are float32 arithmetic on binary fractions (see the runner's docstring for the table).
#include "GameShared/GameClasses/Input/CgsInputModule.h"
#include "GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.h"
#include "GameShared/GameClasses/System/CgsHardwareInit.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static char     gLastAssert[256];

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        std::snprintf(gLastAssert, sizeof(gLastAssert), "%s", lpcMessage ? lpcMessage : "");
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }   // the [DIAG] lines stay silent
}

bool CgsSystem::HardwareInit::mbHasDetectedAutomaticTestingFile = false;

// ---- the recording XDK leaves -----------------------------------------------------------------------
struct MotorCall { u32 muPort; u16 muLeft; u16 muRight; };
static MotorCall gMotor[64];
static s32       gMotorCalls = 0;
static u32       gSetStateResult = 0;

extern "C" u32 XInputSetState(u32 dwUserIndex, void* pVibration)
{
    const u16* lpu16 = static_cast<const u16*>(pVibration);
    if (gMotorCalls < 64)
    {
        gMotor[gMotorCalls].muPort  = dwUserIndex;
        gMotor[gMotorCalls].muLeft  = lpu16[0];
        gMotor[gMotorCalls].muRight = lpu16[1];
    }
    ++gMotorCalls;
    return gSetStateResult;
}

static MotorCall gWheel;
static s32       gWheelCalls = 0;
static u32       gWheelResult = 0;

extern "C" u32 XInputFFSetRumble(u32 dwUserIndex, void* pVibration, void*)
{
    const u16* lpu16 = static_cast<const u16*>(pVibration);
    gWheel.muPort = dwUserIndex; gWheel.muLeft = lpu16[0]; gWheel.muRight = lpu16[1];
    ++gWheelCalls;
    return gWheelResult;
}
extern "C" u32 XInputFFResetDevice(u32, void*)                            { return 0; }
extern "C" u32 XInputFFSetDeviceGain(u32, u32, void*)                     { return 0; }
extern "C" u32 XInputFFEnableMotors(u32, s32, void*)                      { return 0; }
extern "C" u32 XInputFFSetEffect(u32, const void*, u32, void*)            { return 0; }
extern "C" u32 XInputFFEffectOperation(u32, const void*, u32, u32, void*) { return 0; }
extern "C" u32 XInputFFUpdateEffect(u32, const void*, u32, void*)         { return 0; }

// ---- the recording PC pad scan (the console's InputPads::Update seat) ---------------------------------
static s32 gPadScans = 0;
void CgsInput::InputPadsPC::UpdatePadDevices(InputPads*) { ++gPadScans; }

// The production InputModule bodies under test, extracted verbatim (see the runner).
#include "fxrumble3_pad_chain.inc"

using namespace CgsInput;
using namespace CgsInput::InputIO;

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

static JoltEnvelope Envelope(f32 lfAttack, f32 lfDecay, f32 lfSustain, f32 lfRelease, f32 lfPeak, f32 lfLevel)
{
    JoltEnvelope lEnvelope;
    lEnvelope.mfAttackTime = lfAttack; lEnvelope.mfDecayTime = lfDecay; lEnvelope.mfSustainTime = lfSustain;
    lEnvelope.mfReleaseTime = lfRelease; lEnvelope.mfPeakSpeedValue = lfPeak; lEnvelope.mfSustainSpeedValue = lfLevel;
    return lEnvelope;
}

static JoltEffect Effect(const JoltEnvelope& lrLow, const JoltEnvelope& lrHigh)
{
    JoltEffect lEffect;
    lEffect.mLowFreqJoltData = lrLow;
    lEffect.mHighFreqJoltData = lrHigh;
    return lEffect;
}

static bool SameEffect(const JoltEffect& lrA, const JoltEffect& lrB)
{
    return std::memcmp(&lrA, &lrB, sizeof(JoltEffect)) == 0;
}

// The module is never constructed as an object (its vtable needs the whole ModuleSingleBuffered base,
// which is not under test): raw storage, the production InputPads::Construct, and qualified calls.
alignas(16) static unsigned char gModuleStorage[sizeof(InputModule)];
static InputModule* Module() { return reinterpret_cast<InputModule*>(gModuleStorage); }
static InputPads&   Pads()   { return Module()->mControllers; }

static PreWorldInputBuffer             gPre;
static OutputBuffer                    gOut;
static CgsSystem::TimerStatusInterface gTimer;

// Jolt A: low {0.125, 0.25, 0.375, 0.25, peak 0.75, level 0.5}, high {0, 0, 0.5, 0, 0.875, 0.625}.
static const JoltEffect& JoltA()
{
    static const JoltEffect slEffect = Effect(Envelope(0.125f, 0.25f, 0.375f, 0.25f, 0.75f, 0.5f),
                                              Envelope(0.0f, 0.0f, 0.5f, 0.0f, 0.875f, 0.625f));
    return slEffect;
}
// Rumble R2: a sustained {level 0.5 low, 0.25 high} for 0.5 s.
static const JoltEffect& RumbleR2()
{
    static const JoltEffect slEffect = Effect(Envelope(0.0f, 0.0f, 0.5f, 0.0f, 1.0f, 0.5f),
                                              Envelope(0.0f, 0.0f, 0.5f, 0.0f, 1.0f, 0.25f));
    return slEffect;
}

static void FreshModule()
{
    std::memset(gModuleStorage, 0xCD, sizeof(gModuleStorage));
    Pads().Construct();                                   // 0x828EFAF0 -> InitializePads -> 4 x DeviceX360Pad::Construct
    Module()->mOutputBindResultQueue.Construct();
    Module()->mOutputBindResultQueue.Clear();
    Module()->mOutputUnbindResultQueue.Construct();
    Module()->mOutputUnbindResultQueue.Clear();
    gSetStateResult = 0;
    gWheelResult    = 0;
}

// Player 0 on port 0 with a standard pad there -- the PC's standing assignment.
static void BindPad0()
{
    Pads().BindPlayerToPort(0, 0);
    Pads().maPads[0].BindToPort(0, Device::E_PAD_DEVICE_TYPE);
}

typedef void (*PostFn)(PreWorldInputBuffer&);

// One DoUpdate_InputPreWorld-shaped step: the bridge's writes into a fresh pre-world buffer under its
// write lock, then the module's PreWorldUpdate (qualified: no vtable).
static void Step(bool lbUpdatePads, PostFn lpfPost = nullptr, bool lbPause = false, bool lbEnable = true)
{
    gPre.Construct();
    gOut.Construct();
    gPre.LockForWrite();
    if (lpfPost != nullptr)
    {
        lpfPost(gPre);
    }
    gPre.SetTimerStatusInterface(gTimer);
    gPre.SetRumblePaused(lbPause);
    gPre.SetRumbleEnabled(lbEnable);
    gPre.SetWheelForceFeedbackEnabled(true);
    gPre.UnlockForWrite();
    gMotorCalls = 0;
    gPadScans   = 0;
    Module()->InputModule::PreWorldUpdate(nullptr, nullptr, &gPre, &gOut, lbUpdatePads);
}

static bool OneMotorCall(u32 luPort, u16 luLeft, u16 luRight)
{
    return gMotorCalls == 1 && gMotor[0].muPort == luPort && gMotor[0].muLeft == luLeft && gMotor[0].muRight == luRight;
}

static bool Cached(const DeviceX360Pad& lrPad, u16 luLeft, u16 luRight)
{
    return lrPad.mCachedRumble.muLeftMotorSpeed == luLeft && lrPad.mCachedRumble.muRightMotorSpeed == luRight;
}

int main()
{
    std::memset(&gTimer, 0, sizeof(gTimer));
    gTimer.mGameTimerStatus.mfBaseTimeStep       = 0.25f;   // the GAME step the rumble ages by
    gTimer.mGameTimerStatus.mfTimeStepMultiplier = 1.0f;
    gTimer.mSimTimerStatus.mfBaseTimeStep        = 0.125f;  // a different SIM step, which must not be used
    gTimer.mSimTimerStatus.mfTimeStepMultiplier  = 1.0f;

    // ---- InputPads::Construct @0x828EFAF0 with the real DeviceX360Pad::Construct @0x828DC578 --------------
    FreshModule();
    {
        bool lbSeeded = Pads().mbRumbleEnabled && Pads().mbWheelFFEnabled && !Pads().mbRumblePaused;
        for (s32 liPort = 0; liPort < 4; ++liPort)
        {
            lbSeeded = lbSeeded && Pads().maiJoltEffectPriorities[liPort][0] == -1 && Pads().maiRumbleIds[liPort][1] == -1
                    && !Pads().maPlayers[liPort].mbBound && Pads().maiPortToPlayer[liPort] == -1
                    && Pads().maPads[liPort].mePort == -1 && Pads().maPads[liPort].mbConnected == 0
                    && Cached(Pads().maPads[liPort], 0, 0);
        }
        Check(lbSeeded, "Construct: rumble on / unpaused, every slot free, nobody bound, every pad unbound (port -1) with cached {0,0}");
    }

    // ---- InputPads::BindPlayerToPort @0x828DBEF0 --------------------------------------------------------
    {
        const unsigned luBefore = gAsserts;
        Check(Pads().BindPlayerToPort(0, 0) == E_BINDRESULTOK && Pads().maPlayers[0].mbBound && Pads().maPlayers[0].miPort == 0
              && Pads().maiPortToPlayer[0] == 0 && gAsserts == luBefore,
              "BindPlayerToPort(0, 0): OK, player {bound, port 0} and port -> player 0 (stb 1 / stw port / stwx player)");
        Check(Pads().BindPlayerToPort(0, 0) == E_BINDRESULTOK && gAsserts == luBefore,
              "BindPlayerToPort(0, 0) again: OK with no assert (the pair already holds, 0x828DBF44)");
        Check(Pads().BindPlayerToPort(0, 1) == E_BINDRESULTPLAYERALREADYBOUND && gAsserts == luBefore + 1
              && Pads().maiPortToPlayer[1] == -1,
              "player 0 onto port 1: 1 (PLAYERALREADYBOUND) + the :448 assert, nothing stored");
        Check(Pads().BindPlayerToPort(1, 0) == E_BINDRESULTPORTALREADYBOUND && gAsserts == luBefore + 2 && !Pads().maPlayers[1].mbBound,
              "player 1 onto port 0: 2 (PORTALREADYBOUND) + the :453 assert");
        Check(Pads().BindPlayerToPort(4, 0) == E_BINDRESULTINVALIDPLAYER && Pads().BindPlayerToPort(-1, 0) == E_BINDRESULTINVALIDPLAYER
              && gAsserts == luBefore + 4,
              "player 4 or -1 (cmplwi, unsigned): 3 (INVALIDPLAYER) + the :432 assert each");
        Check(Pads().BindPlayerToPort(1, 4) == E_BINDRESULTINVALIDPORT && gAsserts == luBefore + 5,
              "port 4: 4 (INVALIDPORT) + the :437 assert");
    }

    // ---- one jolt, pad to motor -------------------------------------------------------------------------
    FreshModule();
    BindPad0();
    Check(Pads().maPads[0].mePort == 0 && Pads().maPads[0].meType == Device::E_PAD_DEVICE_TYPE && Pads().maPads[0].mbConnected == 1,
          "DeviceX360Pad::BindToPort(0, pad): port 0, type 1, connected (0x828DC8E8)");
    {
        const unsigned luBefore = gAsserts;
        Step(true, [](PreWorldInputBuffer& lrBuffer) { lrBuffer.PostPlayJoltEffectByPlayer(0, 1010, JoltA()); });
        Check(gAsserts == luBefore, "a whole PreWorldUpdate step raises no assert (every lock taken and released)");
        Check(OneMotorCall(0, 42566, 40959),
              "the posted jolt reaches the motor: XInputSetState(0, {(f32)pow(0.75,1.5) x 65535 = 42566, 0.625 x 65535 = 40959})");
        Check(Cached(Pads().maPads[0], 42566, 40959), "...and the pad caches what it sent (+0xF4 / +0xF6)");
        Check(gPadScans == 1, "lbUpdatePads: the pads are updated (the PC seat of InputPads::Update runs once)");
        Check(Pads().mafJoltTime[0][0] == 0.25f, "the jolt ages by the GAME timer step (0.25), not the sim's (0.125)");
        Check(!gPre.IsBufferLocked() && !gOut.IsBufferLocked(), "both buffers are unlocked again after the step");

        Step(false);
        Check(OneMotorCall(0, 0, 0), "!lbUpdatePads (the disk-error path): rumble paused, the motors are stopped");
        Check(gPadScans == 0, "...and the pads are not updated");
        Check(Pads().mafJoltTime[0][0] == 0.25f, "...and the jolt does not age");

        Step(true, nullptr, true);
        Check(OneMotorCall(0, 0, 0) && Pads().mafJoltTime[0][0] == 0.25f, "the buffer's pause flag (+0x394): stopped, not ageing");

        Step(true, nullptr, false, false);
        Check(OneMotorCall(0, 0, 0) && Pads().mafJoltTime[0][0] == 0.5f, "the buffer's enable flag off (+0x395): stopped, still ageing");

        Step(true);
        Check(OneMotorCall(0, 23170, 0), "t=0.5: low sustain 0.5 -> (f32)pow(0.5,1.5) x 65535 = 23170, high over -> 0");
    }

    // ---- ProcessRumbleRequests' order: jolt, stop, play, volume -----------------------------------------
    FreshModule();
    BindPad0();
    Step(true, [](PreWorldInputBuffer& lrBuffer)
    {
        lrBuffer.PostPlayRumbleEffectByPlayer(0, 50, JoltA(), 5, 1.0f);
        lrBuffer.PostStopRumbleEffectByPlayer(0, 5);
        lrBuffer.PostChangeVolumeRumbleEffectByPlayer(0, RumbleR2(), 5, 0.5f);
    });
    Check(Pads().maiRumbleIds[0][0] == 5 && Pads().maiRumbleEffectPriorities[0][0] == 50,
          "a stop and a play of id 5 in one step: the STOP is processed first, so the rumble plays");
    Check(Pads().mafRumbleVolume[0][0] == 0.5f && SameEffect(Pads().maRumbleEffects[0][0], RumbleR2()),
          "...and the volume change is processed after the play: volume 0.5 with its envelope");
    Check(OneMotorCall(0, 8191, 8191), "...the motors: 0.5 x {0.5, 0.25} -> (f32)pow(0.25,1.5) x 65535 = 8191, 0.125 x 65535 = 8191");

    // ---- SetRumble's failure restore --------------------------------------------------------------------
    gSetStateResult = 1167;   // ERROR_DEVICE_NOT_CONNECTED
    Step(true, [](PreWorldInputBuffer& lrBuffer) { lrBuffer.PostChangeVolumeRumbleEffectByPlayer(0, RumbleR2(), 5, 1.0f); });
    Check(OneMotorCall(0, 23170, 16383), "volume 1.0: the pad is handed {23170, 16383}");
    Check(Cached(Pads().maPads[0], 8191, 8191),
          "...the send fails, and BOTH cached halves take the PREVIOUS LEFT speed (sth r26 x2, 0x828E7A98)");
    gSetStateResult = 0;

    // ---- the device gates -------------------------------------------------------------------------------
    Pads().maPads[0].mbConnected = 0;
    Step(true);
    Check(gMotorCalls == 0, "a pad that is not connected is never sent anything");
    {
        const unsigned luBefore = gAsserts;
        gMotorCalls = 0;
        Pads().maPads[0].SetRumble(0.5f, 0.5f);
        Check(gAsserts == luBefore + 1 && std::strcmp(gLastAssert, "IsConnected()") == 0 && gMotorCalls == 0,
              "SetRumble on it fires \"IsConnected()\" (:679) and returns before the XDK call");
        CgsSystem::HardwareInit::mbHasDetectedAutomaticTestingFile = true;
        Pads().maPads[0].SetRumble(0.5f, 0.5f);
        Check(gAsserts == luBefore + 1 && gMotorCalls == 0,
              "under automated testing the assert is quiet but the RAW +0x10 flag still gates the send (0x828E7938)");
        CgsSystem::HardwareInit::mbHasDetectedAutomaticTestingFile = false;
    }

    // ---- the wheel arm ------------------------------------------------------------------------------------
    {
        DeviceX360Pad& lrWheel = Pads().maPads[2];
        lrWheel.mePort = 2; lrWheel.meType = Device::E_WHEEL_DEVICE_TYPE; lrWheel.mbConnected = 1;
        lrWheel.mFFOverlappedRumble.muStatus = 0;
        lrWheel.mCachedRumble.muLeftMotorSpeed = 0; lrWheel.mCachedRumble.muRightMotorSpeed = 0;
        const unsigned luBefore = gAsserts;
        gWheelCalls = 0; gMotorCalls = 0;
        lrWheel.SetRumble(0.0625f, 0.03125f);
        Check(gWheelCalls == 1 && gMotorCalls == 0 && gWheel.muPort == 2 && gWheel.muLeft == 40959 && gWheel.muRight == 20479
              && Cached(lrWheel, 40959, 20479),
              "wheel: XInputFFSetRumble(2, {0.0625 x 655350 = 40959, 0.03125 x 655350 = 20479})");
        lrWheel.SetRumble(1.0f, 0.5f);
        Check(gWheel.muLeft == 65535 && gWheel.muRight == 65535, "wheel: 655350 x motor saturates at 65535 (fsel Clamp)");
        lrWheel.SetRumble(0.01f, 0.0f);
        Check(Cached(lrWheel, 6553, 0), "wheel: 0.01 -> 6553, 0 -> 0");
        lrWheel.mFFOverlappedRumble.muStatus = 997;   // the previous send still pending
        gWheelCalls = 0;
        lrWheel.SetRumble(0.0625f, 0.03125f);
        Check(gWheelCalls == 0 && Cached(lrWheel, 6553, 6553),
              "wheel with the overlapped still pending (+0xF8 == 997): nothing sent, both halves = the previous left");
        lrWheel.mFFOverlappedRumble.muStatus = 0;
        lrWheel.SetRumble(0.01f, 0.0f);
        gWheelResult = 997;
        lrWheel.SetRumble(0.0625f, 0.03125f);
        Check(gAsserts == luBefore && Cached(lrWheel, 6553, 6553), "wheel send answering 997: quiet, previous left restored");
        gWheelResult = 5;
        lrWheel.SetRumble(0.0625f, 0.03125f);
        Check(gAsserts == luBefore + 1 && std::strncmp(gLastAssert, "WHEEL ERROR 5", 13) == 0 && Cached(lrWheel, 6553, 6553),
              "wheel send answering 5: \"WHEEL ERROR 5\" (:699), previous left restored");
        gWheelResult = 0;
    }

    // ---- PreWorldUpdate hands the module's bind results over --------------------------------------------
    FreshModule();
    {
        BindResult lBind;
        lBind.miPlayer = 0; lBind.miPort = 0; lBind.meResultCode = E_BINDRESULTOK;
        Module()->mOutputBindResultQueue.AddEvent(lBind);
        UnBindResult lUnbind;
        std::memset(&lUnbind, 0, sizeof(lUnbind));
        lUnbind.miPlayer = 3; lUnbind.miPort = 2;
        Module()->mOutputUnbindResultQueue.AddEvent(lUnbind);
        Step(true);
        const OutputBuffer& lrOut = gOut;   // the read-lock (const) accessors, as the game's readers use them
        const unsigned luBefore = gAsserts;
        lrOut.LockForRead();
        Check(lrOut.GetBindResultQueue()->GetLength() == 1 && lrOut.GetBindResultQueue()->GetEvent(0).miPlayer == 0
              && lrOut.GetUnbindResultQueue()->GetLength() == 1 && lrOut.GetUnbindResultQueue()->GetEvent(0).miPlayer == 3
              && gAsserts == luBefore,
              "the module's bind / unbind results are Appended into the output buffer (0x82903384..0x829033A0)");
        lrOut.UnlockForRead();
        Check(Module()->mOutputBindResultQueue.GetLength() == 0 && Module()->mOutputUnbindResultQueue.GetLength() == 0,
              "...and the module's two queues are emptied");
    }

    std::printf("FxRumble3PadChain: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
