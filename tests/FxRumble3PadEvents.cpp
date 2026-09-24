// FX-RUMBLE3 (crash parity 2026-09-24, G10-D4): the pads' rumble engine -- the PRODUCTION
// CgsInput::InputPads event handlers and envelope walk (PlayJoltEvent @0x828DC128, PlayRumbleEvent
// @0x828DC208, ChangeVolumeRumbleEvent @0x828DC318, StopRumbleEvent @0x828DC3C0, UpdateJoltEnvelope
// @0x828E7618, UpdatePadRumble @0x828EFC58 and the envelope-duration helpers), extracted from
// CgsInputPads.cpp by run_fxrumble3_pad_rumble.py and compiled against the revision's own
// CgsInputPads.h / CgsInputDeviceX360Pad.h. DeviceX360Pad::SetRumble is a RECORDING stand-in here (the
// device end is FxRumble3PadChain.cpp's), so this program runs on the old bodies too and shows which
// behaviours they got wrong. Expected values from the ARTIST asm:
//   ChangeVolumeRumbleEvent  volume store (0x828DC3A4) AND memcpy(&maRumbleEffects[port][slot],
//                            &event.mJoltEffect, 0x30) (0x828DC398..0x828DC3B8)
//   UpdatePadRumble          0x828EFEB8..0x828EFEE4: the connected gate is DeviceX360Pad::IsConnected()
//                            inlined -- `lbz byte_83085F80 (HardwareInit::mbHasDetectedAutomaticTestingFile)
//                            ; bne connected ; lbz 0x10(pad)` -- so an automated-testing run drives every
//                            pad's motors; jolts EXPIRE strictly past the longer envelope (`ble keep`),
//                            rumbles WRAP their time to 0; pad arm: SetRumble(0,0) when stopped or
//                            disabled, else the fsel-Clamp'd [0,1] motors
//   UpdateJoltEnvelope       t < attack -> peak; t < decay+attack -> lerp peak->level; t < sustain+decay+
//                            attack -> level; t >= total -> 0; else release lerp level->0
// Every time and level below is a binary fraction, so each expected motor value is exact.
#include "GameShared/GameClasses/System/Input/CgsInputPads.h"
#include "GameShared/GameClasses/System/CgsHardwareInit.h"
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
namespace Log { DebugPrint* gpDebugPrint = nullptr; }   // UpdatePadRumble's [DIAG] line stays silent
}

// The automated-testing byte (X360 byte_83085F80). Its home is CgsHardwareInitPC.cpp, not under test.
bool CgsSystem::HardwareInit::mbHasDetectedAutomaticTestingFile = false;

static CgsInput::InputPads gPads;

// ---- the recording device end ------------------------------------------------------------------------
struct MotorCall { s32 miPort; f32 mfLeft; f32 mfRight; };
static MotorCall gCalls[64];
static s32 gNumCalls = 0;

void CgsInput::DeviceX360Pad::SetRumble(f32 lfLeftMotor, f32 lfRightMotor)
{
    if (gNumCalls < 64)
    {
        gCalls[gNumCalls].miPort  = static_cast<s32>(this - &gPads.maPads[0]);
        gCalls[gNumCalls].mfLeft  = lfLeftMotor;
        gCalls[gNumCalls].mfRight = lfRightMotor;
    }
    ++gNumCalls;
}

// The production bodies, extracted verbatim (see the runner).
#include "fxrumble3_pad_events.inc"

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

// The state InputPads::Construct @0x828EFAF0 leaves for every field these bodies read (Construct itself
// is FxRumble3PadChain.cpp's, with the real DeviceX360Pad): nobody bound, every slot free, rumble on.
static void Fresh()
{
    std::memset(&gPads, 0, sizeof(gPads));
    for (s32 liPort = 0; liPort < 4; ++liPort)
    {
        gPads.maPlayers[liPort].mbBound = false;
        gPads.maPlayers[liPort].miPort  = -1;
        gPads.maiPortToPlayer[liPort]   = -1;
        for (s32 li = 0; li < 2; ++li)
        {
            gPads.maiJoltEffectPriorities[liPort][li]   = -1;
            gPads.mafJoltTime[liPort][li]               = 0.0f;
            gPads.maiRumbleEffectPriorities[liPort][li] = -1;
            gPads.maiRumbleIds[liPort][li]              = -1;
            gPads.mafRumbleTime[liPort][li]             = 0.0f;
        }
        gPads.maPads[liPort].mePort      = -1;
        gPads.maPads[liPort].mbConnected = 0;
        gPads.maPads[liPort].meType      = 0;
    }
    gPads.mbRumblePaused   = false;
    gPads.mbRumbleEnabled  = true;
    gPads.mbWheelFFEnabled = true;
    CgsSystem::HardwareInit::mbHasDetectedAutomaticTestingFile = false;
    gNumCalls = 0;
}

// Player 0 on port 1, a connected standard pad there.
static void BindPlayer0ToPort1()
{
    gPads.maPlayers[0].mbBound = true;
    gPads.maPlayers[0].miPort  = 1;
    gPads.maiPortToPlayer[1]   = 0;
    gPads.maPads[1].mePort      = 1;
    gPads.maPads[1].mbConnected = 1;
    gPads.maPads[1].meType      = 1;   // Device::E_PAD_DEVICE_TYPE
}

static PlayJoltEffectEvent JoltEvent(s32 liPlayer, s32 liPort, s32 liPriority, const JoltEffect& lrEffect)
{
    PlayJoltEffectEvent lEvent;
    lEvent.miPlayer = liPlayer; lEvent.miPort = liPort; lEvent.miRumblePriority = liPriority; lEvent.mJoltEffect = lrEffect;
    return lEvent;
}

static PlayRumbleEffectEvent RumbleEvent(s32 liPlayer, s32 liPriority, const JoltEffect& lrEffect, s32 liId, f32 lfVolume)
{
    PlayRumbleEffectEvent lEvent;
    lEvent.miPlayer = liPlayer; lEvent.miPort = -1; lEvent.miRumblePriority = liPriority; lEvent.mJoltEffect = lrEffect;
    lEvent.miRumbleId = liId; lEvent.mfRumbleVolume = lfVolume;
    return lEvent;
}

static ChangeVolumeRumbleEffectEvent VolumeEvent(s32 liPlayer, const JoltEffect& lrEffect, s32 liId, f32 lfVolume)
{
    ChangeVolumeRumbleEffectEvent lEvent;
    lEvent.miPlayer = liPlayer; lEvent.miPort = -1; lEvent.mJoltEffect = lrEffect;
    lEvent.miRumbleId = liId; lEvent.mfRumbleVolume = lfVolume;
    return lEvent;
}

static StopRumbleEffectEvent StopEvent(s32 liPlayer, s32 liId)
{
    StopRumbleEffectEvent lEvent;
    lEvent.miPlayer = liPlayer; lEvent.miPort = -1; lEvent.miRumbleId = liId;
    return lEvent;
}

// One UpdatePadRumble of port 1; true when exactly one SetRumble(left, right) reached the pad.
static bool TickPort1(f32 lfLeft, f32 lfRight, f32 lfStep = 0.25f)
{
    gNumCalls = 0;
    gPads.UpdatePadRumble(1, 0, lfStep);
    return gNumCalls == 1 && gCalls[0].miPort == 1 && gCalls[0].mfLeft == lfLeft && gCalls[0].mfRight == lfRight;
}

int main()
{
    // Jolt A: low {attack 0.125, decay 0.25, sustain 0.375, release 0.25, peak 0.75, level 0.5} (total 1.0),
    //         high {0, 0, sustain 0.5, 0, peak 0.875, level 0.625} (total 0.5).
    const JoltEffect lJoltA = Effect(Envelope(0.125f, 0.25f, 0.375f, 0.25f, 0.75f, 0.5f),
                                     Envelope(0.0f, 0.0f, 0.5f, 0.0f, 0.875f, 0.625f));

    // ---- PlayJoltEvent @0x828DC128 ---------------------------------------------------------------------
    Fresh();
    BindPlayer0ToPort1();
    gPads.PlayJoltEvent(JoltEvent(0, -1, 1010, lJoltA));
    Check(gPads.maiJoltEffectPriorities[1][0] == 1010 && gPads.mafJoltTime[1][0] == 0.0f
          && SameEffect(gPads.maJoltEffects[1][0], lJoltA),
          "PlayJoltEvent: port -1 resolves player 0 -> port 1; slot 0 takes {effect, time 0, priority 1010}");
    gPads.PlayJoltEvent(JoltEvent(2, -1, 1010, lJoltA));
    Check(gPads.maiJoltEffectPriorities[0][0] == -1 && gPads.maiJoltEffectPriorities[2][0] == -1
          && gPads.maiJoltEffectPriorities[3][0] == -1 && gPads.maiJoltEffectPriorities[1][1] == -1,
          "PlayJoltEvent: an unbound player's jolt is dropped");
    gPads.PlayJoltEvent(JoltEvent(0, -1, 1000, lJoltA));
    Check(gPads.maiJoltEffectPriorities[1][0] == 1010 && gPads.maiJoltEffectPriorities[1][1] == 1000,
          "PlayJoltEvent: a lower priority skips the busier slot (1000 <= 1010) and takes the free one");
    gPads.PlayJoltEvent(JoltEvent(0, -1, 900, lJoltA));
    Check(gPads.maiJoltEffectPriorities[1][0] == 1010 && gPads.maiJoltEffectPriorities[1][1] == 1000,
          "PlayJoltEvent: with no slot below its priority the jolt is dropped");
    gPads.PlayJoltEvent(JoltEvent(3, 2, 5, lJoltA));
    Check(gPads.maiJoltEffectPriorities[2][0] == 5, "PlayJoltEvent: an explicit port bypasses the player bind");

    // ---- UpdatePadRumble @0x828EFC58: one jolt through its whole envelope, 0.25 s per tick ---------------
    Fresh();
    BindPlayer0ToPort1();
    gPads.PlayJoltEvent(JoltEvent(0, -1, 1010, lJoltA));
    Check(TickPort1(0.75f, 0.625f), "jolt t=0: attack -> peak 0.75 (low); high sustain level 0.625");
    Check(gPads.mafJoltTime[1][0] == 0.25f, "...the jolt time advances by the step");
    Check(TickPort1(0.625f, 0.625f), "jolt t=0.25: decay lerp 0.75 -> 0.5 half way = 0.625");
    Check(TickPort1(0.5f, 0.0f), "jolt t=0.5: low sustain 0.5; high at its total (t >= 0.5) -> 0");
    Check(TickPort1(0.5f, 0.0f), "jolt t=0.75: low release starts at the level (0.5)");
    Check(gPads.maiJoltEffectPriorities[1][0] == 1010 && gPads.mafJoltTime[1][0] == 1.0f,
          "...time 1.0 == the total: kept (the expiry is strictly past it, `ble keep`)");
    Check(TickPort1(0.0f, 0.0f), "jolt t=1.0: time 1.25 > 1.0 -> expired this tick, motors stopped");
    Check(gPads.maiJoltEffectPriorities[1][0] == -1 && gPads.mafJoltTime[1][0] == 0.0f,
          "...expiry frees the slot: priority -1, time 0");

    // ---- rumbles: volume scaling and the time WRAP -------------------------------------------------------
    // Rumble R: low {0, 0, 0.5, 0, 1.0, 0.75}, high {0, 0, 0.5, 0, 1.0, 0.5} (total 0.5), volume 0.5.
    const JoltEffect lRumbleR  = Effect(Envelope(0.0f, 0.0f, 0.5f, 0.0f, 1.0f, 0.75f), Envelope(0.0f, 0.0f, 0.5f, 0.0f, 1.0f, 0.5f));
    const JoltEffect lRumbleR2 = Effect(Envelope(0.0f, 0.0f, 0.5f, 0.0f, 1.0f, 0.5f), Envelope(0.0f, 0.0f, 0.5f, 0.0f, 1.0f, 0.25f));
    Fresh();
    BindPlayer0ToPort1();
    gPads.PlayRumbleEvent(RumbleEvent(0, 50, lRumbleR, 7, 0.5f));
    Check(gPads.maiRumbleEffectPriorities[1][0] == 50 && gPads.maiRumbleIds[1][0] == 7 && gPads.mafRumbleVolume[1][0] == 0.5f
          && gPads.mafRumbleTime[1][0] == 0.0f && SameEffect(gPads.maRumbleEffects[1][0], lRumbleR),
          "PlayRumbleEvent @0x828DC208: {effect, time 0, priority 50, id 7, volume 0.5}");
    Check(TickPort1(0.375f, 0.25f), "rumble t=0: volume x level = 0.5 x 0.75 / 0.5 x 0.5");
    Check(TickPort1(0.375f, 0.25f), "rumble t=0.25: sustained");
    Check(TickPort1(0.0f, 0.0f), "rumble t=0.5: at its total the envelope reads 0");
    Check(gPads.mafRumbleTime[1][0] == 0.0f && gPads.maiRumbleEffectPriorities[1][0] == 50,
          "...time 0.75 > 0.5 WRAPS to 0 and the rumble stays (no expiry)");
    Check(TickPort1(0.375f, 0.25f), "rumble loops: t=0 again");

    // ---- ChangeVolumeRumbleEvent @0x828DC318 -------------------------------------------------------------
    gPads.ChangeVolumeRumbleEvent(VolumeEvent(0, lRumbleR2, 7, 0.25f));
    Check(gPads.mafRumbleVolume[1][0] == 0.25f, "ChangeVolumeRumbleEvent: the slot with id 7 takes volume 0.25 (0x828DC3A4)");
    Check(SameEffect(gPads.maRumbleEffects[1][0], lRumbleR2),
          "ChangeVolumeRumbleEvent: ...AND the event's envelope (memcpy 0x30 @0x828DC398..0x828DC3B8)");
    Check(TickPort1(0.125f, 0.0625f), "...the next tick plays the NEW envelope at the new volume (0.25 x 0.5 / 0.25 x 0.25)");
    gPads.ChangeVolumeRumbleEvent(VolumeEvent(0, lRumbleR, 8, 1.0f));
    Check(gPads.mafRumbleVolume[1][0] == 0.25f && SameEffect(gPads.maRumbleEffects[1][0], lRumbleR2),
          "ChangeVolumeRumbleEvent: an unknown id changes nothing");

    // ---- StopRumbleEvent @0x828DC3C0 ---------------------------------------------------------------------
    gPads.StopRumbleEvent(StopEvent(0, 7));
    Check(gPads.mafRumbleTime[1][0] == 0.0f && gPads.mafRumbleVolume[1][0] == 0.0f
          && gPads.maiRumbleEffectPriorities[1][0] == -1 && gPads.maiRumbleIds[1][0] == -1,
          "StopRumbleEvent: the id's slot is cleared (time 0, volume 0, priority -1, id -1)");
    Check(TickPort1(0.0f, 0.0f), "...and the motors stop");

    // ---- the gates -------------------------------------------------------------------------------------
    Fresh();
    BindPlayer0ToPort1();
    gPads.PlayJoltEvent(JoltEvent(0, -1, 1010, lJoltA));
    gPads.mbRumblePaused = true;
    Check(TickPort1(0.0f, 0.0f), "paused: both walks are skipped, SetRumble(0, 0)");
    Check(gPads.mafJoltTime[1][0] == 0.0f, "...and the jolt's time does not advance");
    gPads.mbRumblePaused  = false;
    gPads.mbRumbleEnabled = false;
    Check(TickPort1(0.0f, 0.0f), "disabled: SetRumble(0, 0)");
    Check(gPads.mafJoltTime[1][0] == 0.25f, "...but the jolt keeps ageing");
    gPads.mbRumbleEnabled = true;

    Fresh();
    BindPlayer0ToPort1();
    gPads.PlayJoltEvent(JoltEvent(0, -1, 1010, lJoltA));
    gPads.maPads[1].mbConnected = 0;
    gNumCalls = 0;
    gPads.UpdatePadRumble(1, 0, 0.25f);
    Check(gNumCalls == 0, "a disconnected pad gets no SetRumble");
    Check(gPads.mafJoltTime[1][0] == 0.25f, "...while its jolt still ages");
    CgsSystem::HardwareInit::mbHasDetectedAutomaticTestingFile = true;
    Check(TickPort1(0.625f, 0.625f),
          "automated testing (byte_83085F80, IsConnected @0x828DC7E0 inlined): the same pad IS driven");
    CgsSystem::HardwareInit::mbHasDetectedAutomaticTestingFile = false;

    // ---- the fsel Clamp and the wheel arm --------------------------------------------------------------
    Fresh();
    BindPlayer0ToPort1();
    gPads.PlayJoltEvent(JoltEvent(0, -1, 1010, Effect(Envelope(0.5f, 0.0f, 0.0f, 0.0f, 1.5f, 0.0f),
                                                      Envelope(0.5f, 0.0f, 0.0f, 0.0f, 0.25f, 0.0f))));
    Check(TickPort1(1.0f, 0.25f), "a motor above 1 is Clamp'd to 1.0 (fsel pair, 0x828EFF0C..)");

    Fresh();
    BindPlayer0ToPort1();
    gPads.maPads[1].meType = 2;   // Device::E_WHEEL_DEVICE_TYPE
    gPads.PlayJoltEvent(JoltEvent(0, -1, 1010, lJoltA));
    gPads.mbWheelFFEnabled = false;
    Check(TickPort1(0.0f, 0.0f), "wheel with force feedback off: SetRumble(0, 0) despite a running jolt");
    Check(gPads.maPads[1].mfRumbleX == 1.0f && gPads.maPads[1].mfRumbleY == 0.0f,
          "...and the neutral spring {1.0, 0.0} is published");

    std::printf("FxRumble3PadEvents: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
