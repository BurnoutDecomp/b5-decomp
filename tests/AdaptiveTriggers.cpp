#include "pc/input/AdaptiveTriggers.h"
#include "pc/input/DualSenseEffects.h"
#include <cstdio>
#include <limits>
#include <chrono>

using namespace CgsInput::AdaptiveTriggers;
static unsigned guChecks = 0, guFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++guChecks;
    if (!lbPass) { ++guFailures; std::printf("FAIL: %s\n", lpcName); }
}
static Sample Driving()
{
    Sample lSample;
    lSample.muVehicle = 1;
    lSample.mbActive = true;
    lSample.muGroundedWheels = 4;
    lSample.miGear = 2;
    lSample.mfGas = 1;
    lSample.mfSpeedMPH = 70;
    lSample.mfRPM = 4500;
    return lSample;
}
static unsigned Force(const Effect& lrEffect)
{
    unsigned luSum = 0;
    for (auto luStrength : lrEffect.mauStrength) luSum += luStrength;
    return luSum;
}
static void TestEncoding()
{
    Effect lEffect;
    Check(Encode(lEffect) == TriggerBytes{5,0,0,0,0,0,0,0,0,0,0}, "explicit neutral command");
    lEffect.meMode = Mode::Feedback;
    lEffect.mauStrength.fill(8);
    Check(Encode(lEffect) == TriggerBytes{0x21,0xff,3,0xff,0xff,0xff,0x3f,0,0,0,0}, "ten max-force zones encode to independent known vector");
    lEffect.mauStrength.fill(1);
    lEffect.meMode = Mode::Vibration;
    lEffect.muFrequency = 45;
    Check(Encode(lEffect) == TriggerBytes{0x26,0xff,3,0,0,0,0,0,0,45,0}, "vibration amplitude one is active with zero packed strengths");
    lEffect.muFrequency = 0;
    Check(Encode(lEffect)[0] == 5, "zero frequency clears vibration");
    lEffect.meMode = Mode::Feedback;
    lEffect.mauStrength.fill(255);
    Check(Encode(lEffect)[6] == 0x3f, "invalid zone strengths clamp without overflowing reserved bits");
    lEffect.meMode = static_cast<Mode>(0xfc);
    Check(Encode(lEffect)[0] == 5, "unknown firmware modes cannot reach device");
    Output lOutput;
    lOutput.mLeft.meMode = Mode::Feedback;
    lOutput.mLeft.mauStrength[9] = 4;
    auto lPacket = Encode(lOutput);
    Check(lPacket.size() == 47 && lPacket[0] == 0x0c && lPacket[10] == 5 && lPacket[21] == 0x21,
        "SDL payload size, trigger flags and independent left/right placement");
    bool lbUnrelatedClear = true;
    for (unsigned i = 1; i < lPacket.size(); ++i)
        if (i < 10 || i >= 32) lbUnrelatedClear &= lPacket[i] == 0;
    Check(lbUnrelatedClear, "rumble, LEDs and audio are untouched");
}
static void TestDriving()
{
    Controller lController;
    auto lSample = Driving();
    lController.Submit(lSample, 100);
    auto lOutput = lController.Resolve(100);
    Check(lOutput.mRight.meCue == Cue::Engine && lOutput.mRight.meMode == Mode::Vibration, "continuous engine feedback");
    Check(lOutput.mLeft.meCue == Cue::Brake && lOutput.mLeft.mauStrength[0] == 0 &&
        lOutput.mLeft.mauStrength[9] >= lOutput.mLeft.mauStrength[2], "brake travel has bite and progressive resistance");
    auto luLowFrequency = lOutput.mRight.muFrequency;
    auto luLightBrake = Force(lOutput.mLeft);
    lSample.mfRPM = 7500; lSample.mfBrake = 1;
    lController.Submit(lSample, 116);
    lOutput = lController.Resolve(116);
    Check(lOutput.mRight.muFrequency > luLowFrequency, "engine texture follows RPM");
    Check(Force(lOutput.mLeft) > luLightBrake, "heavy braking increases pedal resistance");
    lSample.mfBrakeSkid = 0.8f;
    lController.Submit(lSample, 132);
    Check(lController.Resolve(132).mLeft.meCue == Cue::BrakeSkid, "actual braking skid produces chatter");
    lSample.mfBrake = lSample.mfBrakeSkid = 0;
    lSample.mfDrivenSlip = 0.9f;
    for (unsigned t = 148; t <= 340; t += 16) lController.Submit(lSample, t);
    Check(lController.Resolve(340).mRight.meCue == Cue::Wheelspin, "driven-wheel slip produces wheelspin texture");
    lSample.mfDrivenSlip = 0;
    bool lbRecovered = false;
    for (unsigned t = 356; t < 700; t += 16)
    {
        lController.Submit(lSample, t);
        lbRecovered |= lController.Resolve(t).mRight.meCue == Cue::GripRecovery;
    }
    Check(lbRecovered, "grip recovery produces transition before smooth engine return");
    lSample.mbBoosting = true;
    lController.Submit(lSample, 710);
    Check(lController.Resolve(710).mRight.meCue == Cue::BoostStart, "boost starts even without remapping its button");
    lController.Submit(lSample, 910);
    Check(lController.Resolve(910).mRight.meCue == Cue::Boost, "boost sustain has its own texture");
    lSample.mbBoosting = false;
    lController.Submit(lSample, 926);
    Check(lController.Resolve(926).mRight.meCue == Cue::BoostEnd, "boost end transition");
    lController.Submit(lSample, 1080);
    Check(lController.Resolve(1080).mRight.meCue == Cue::Engine, "boost ends back at engine baseline");
    lSample.miGear = 3;
    lController.Submit(lSample, 1100);
    Check(lController.Resolve(1100).mRight.meCue == Cue::Shift && lController.Resolve(1100).mRight.IsOff(), "gear shift begins with unloading");
    Check(lController.Resolve(1150).mRight.meCue == Cue::Shift && !lController.Resolve(1150).mRight.IsOff(), "gear shift returns a short kick");
    lController.Submit(lSample, 1250);
    Check(lController.Resolve(1250).mRight.meCue == Cue::Engine, "held gear never retriggers the shift");
    lSample.mbDrifting = true; lSample.mfDrift = 0.7f;
    lController.Submit(lSample, 1270);
    Check(lController.Resolve(1270).mLeft.meCue == Cue::DriftStart, "drift breakaway reaches brake pedal");
    lController.Submit(lSample, 1450);
    Check(lController.Resolve(1450).mRight.meCue == Cue::Drift, "sustained drift texture");
    lSample.mbBoosting = true;
    lController.Submit(lSample, 1470);
    Check(lController.Resolve(1470).mRight.meCue == Cue::Drift, "drift grip priority survives simultaneous boost");
    lSample.mbDrifting = lSample.mbBoosting = false;
    lSample.mbLimiter = true;
    lController.Submit(lSample, 1700);
    Check(lController.Resolve(1700).mRight.meCue == Cue::Limiter, "explicit limiter signal has distinct chatter");
    lSample.mbLimiter = false; lSample.mbReverse = true; lSample.miGear = 0;
    lController.Submit(lSample, 1900);
    Check(lController.Resolve(1900).mLeft.meCue == Cue::Reverse && lController.Resolve(1900).mRight.IsOff(), "reverse transfers accelerator role to L2");
}
static void TestContactsAndEvents()
{
    Controller lController;
    auto lSample = Driving();
    lController.Submit(lSample, 100);
    lSample.mfSurface = 0.8f;
    lController.Submit(lSample, 120);
    Check(lController.Resolve(120).mRight.meCue == Cue::Surface, "surface rumble creates grounded road texture");
    lSample.muGroundedWheels = 0; lSample.mfAirTime = 0.15f; lSample.mfVerticalSpeed = -8;
    lController.Submit(lSample, 140);
    Check(lController.Resolve(140).mRight.meCue == Cue::Takeoff && lController.Resolve(140).mLeft.IsOff(), "takeoff unloads both triggers");
    lController.Submit(lSample, 250);
    Check(lController.Resolve(250).mRight.meCue == Cue::Airborne && lController.Resolve(250).mLeft.IsOff(), "airtime suppresses surface and brake loads");
    lSample.muGroundedWheels = 4; lSample.mfAirTime = 0; lSample.mfBrake = 1;
    lController.Submit(lSample, 270);
    Check(lController.Resolve(270).mRight.meCue == Cue::Landing && lController.Resolve(270).mLeft.meCue == Cue::Landing, "landing wins over braking on both pedals");
    lController.Submit(lSample, 520);
    Check(lController.Resolve(520).mLeft.meCue == Cue::Brake, "landing settles back into current braking");
    lSample.mfScrape = 0.6f;
    lController.Submit(lSample, 540);
    Check(lController.Resolve(540).mRight.meCue == Cue::Scrape, "wall scrape is continuous contact texture");
    lSample.muEvents = E_EVENT_IMPACT; lSample.mfImpact = 0.8f;
    lController.Submit(lSample, 560);
    Check(lController.Resolve(560).mLeft.meCue == Cue::Impact, "impact overrides ongoing scrape");
    lSample.muEvents = 0; lSample.mfImpact = 0;
    lController.Submit(lSample, 760);
    Check(lController.Resolve(760).mRight.meCue == Cue::Scrape, "impact returns to active scrape");
    lSample.mbCrashing = true;
    lController.Submit(lSample, 780);
    Check(lController.Resolve(780).mRight.meCue == Cue::Wreck, "new wreck creates one shudder");
    lController.Submit(lSample, 1100);
    Check(lController.Resolve(1100).mRight.IsOff() && lController.Resolve(1100).mLeft.IsOff(), "wreck releases after shudder");
    lSample = Driving(); lSample.mbShowtime = true; lSample.mbCrashing = true;
    lController.Submit(lSample, 1120);
    lSample.muEvents = E_EVENT_BOUNCE;
    lController.Submit(lSample, 1140);
    Check(lController.Resolve(1140).mRight.meCue == Cue::Bounce, "Showtime uses bounce event while wreck physics is active");
    lSample.muEvents |= E_EVENT_BOOSTED_BOUNCE;
    lController.Submit(lSample, 1380);
    Check(lController.Resolve(1380).mRight.meCue == Cue::BoostedBounce, "boosted bounce has its own cue");
    lController.Reset(); lSample = Driving();
    lController.Submit(lSample, 2000);
    lSample.muEvents = E_EVENT_CHAIN;
    lController.Submit(lSample, 2020);
    Check(lController.Resolve(2020).mRight.meCue == Cue::Chain && !lController.Resolve(2020).mRight.IsOff(), "burnout chain first kick");
    Check(lController.Resolve(2120).mRight.IsOff() && !lController.Resolve(2210).mRight.IsOff(), "burnout chain separates its two kicks");
    lSample.muEvents = 0; lController.Submit(lSample, 2300);
    Check(lController.Resolve(2300).mRight.meCue == Cue::Engine, "consumed award is not repeated by polling");
    lSample.muEvents = E_EVENT_NEAR_MISS;
    lController.Submit(lSample, 2320);
    Check(lController.Resolve(2320).mRight.meCue == Cue::Reward, "optional near-miss tick is perceptible over baseline");
    Settings lSettings; lSettings.mbArcadeCues = false; lController.Configure(lSettings);
    lSample.muEvents = 0; lController.Submit(lSample, 2400);
    lSample.muEvents = E_EVENT_NEAR_MISS; lController.Submit(lSample, 2420);
    Check(lController.Resolve(2420).mRight.meCue == Cue::Engine, "arcade layer independently disables");
}
static void TestLifecycleAndProfiles()
{
    Controller lController;
    auto lSample = Driving();
    lSample.muEvents = E_EVENT_IMPACT;
    lController.Submit(lSample, 100);
    Check(lController.Resolve(100).mRight.meCue == Cue::Engine, "first snapshot does not replay old events");
    Check(lController.Resolve(601).mRight.IsOff(), "stale snapshots release the actuator");
    Check(lController.Resolve(90).mRight.IsOff(), "clock regression releases output");
    lSample.mbActive = false; lController.Submit(lSample, 120);
    Check(lController.Resolve(120).mLeft.IsOff(), "inactive player clears both pedals");
    lSample = Driving(); lController.Submit(lSample, 140);
    lSample.muVehicle = 2; lSample.muEvents = E_EVENT_IMPACT; lController.Submit(lSample, 160);
    Check(lController.Resolve(160).mRight.meCue == Cue::Engine, "car replacement drops old transients");
    lSample.mbReset = true; lController.Submit(lSample, 180);
    Check(lController.Resolve(180).mRight.IsOff(), "respawn/reset explicitly clears");
    Settings lSettings; lSettings.mePreset = Preset::Off; lController.Configure(lSettings);
    lSample = Driving(); lController.Submit(lSample, 200);
    Check(lController.Resolve(200).mLeft.IsOff() && lController.Resolve(200).mRight.IsOff(), "off preset is neutral");
    lSettings.mePreset = Preset::Light; lController.Configure(lSettings); lController.Submit(lSample, 220);
    Check(!lController.Resolve(220).mRight.IsOff(), "light preset survives actuator quantization");
    lSettings.mfResistance = lSettings.mfTexture = 0; lController.Configure(lSettings); lController.Submit(lSample, 240);
    Check(lController.Resolve(240).mLeft.IsOff() && lController.Resolve(240).mRight.IsOff(), "both strength sliders zero clears output");
    lSettings = {}; lSettings.mfTexture = 0; lController.Configure(lSettings); lController.Submit(lSample, 260);
    Check(lController.Resolve(260).mRight.meMode == Mode::Feedback, "texture off retains smooth pedal resistance");
    std::array<std::uint8_t, 3> laFrequencies{};
    for (unsigned i = 0; i < 3; ++i)
    {
        lController.Configure(Settings{}); lSample = Driving(); lSample.meCharacter = static_cast<Character>(i);
        lController.Submit(lSample, 300); laFrequencies[i] = lController.Resolve(300).mRight.muFrequency;
    }
    Check(laFrequencies[0] != laFrequencies[1] && laFrequencies[1] != laFrequencies[2] && laFrequencies[0] != laFrequencies[2], "three vehicle profiles have different engine signatures");
    lSample = Driving(); lSample.mfRPM = std::numeric_limits<float>::infinity();
    lSample.mfGas = std::numeric_limits<float>::quiet_NaN(); lSample.mfImpact = -100;
    lController.Submit(lSample, 320);
    const auto lBytes = Encode(lController.Resolve(320));
    Check(lBytes[0] == 0x0c, "non-finite input produces bounded valid output");
    const auto lStart = std::chrono::steady_clock::now();
    volatile unsigned luChecksum = 0;
    lSample = Driving();
    for (unsigned i = 0; i < 100000; ++i)
    {
        lController.Submit(lSample, 1000 + i * 16);
        luChecksum += Encode(lController.Resolve(1000 + i * 16))[10];
    }
    const auto lfTime = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - lStart).count();
    std::printf("Resolver + encoding: 100000 driving updates in %.2f ms (checksum %u)\n", lfTime, luChecksum);
}
int main()
{
    TestEncoding(); TestDriving(); TestContactsAndEvents(); TestLifecycleAndProfiles();
    std::printf("Adaptive triggers: %u/%u checks passed\n", guChecks - guFailures, guChecks);
    return guFailures ? 1 : 0;
}
