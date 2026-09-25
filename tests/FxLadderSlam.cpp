// FX-LADDER slam commit (crash parity 2026-09-25): VehiclePhysics::AddSlam @0x825D4870,
// VehiclePhysics::UpdateSlam @0x825D4950 and VehicleManager::CalculateSlamData @0x825C7568 -- their NaN
// arms and their fused roundings. run_fxladder_slam.py extracts the three production bodies (and
// CalculateSlamData's tables and SignOrZero) and this fixture runs them on the REAL VehicleManager /
// RaceCarPhysics structs against a model of the ARTIST asm written here:
//   * after fcmpu an unordered result sets only UN, so bgt / blt are NOT taken on a NaN;
//   * `fsel d, t, x, y` = (t >= 0) ? x : y takes y on a NaN -- the clamps are rwmath's Clamp / Max
//     (a NaN value comes back as the clamp's MAX, and Max(g, 0.9) as 0.9);
//   * fmadds / fnmsubs round ONCE (== std::fmaf).
// Every NaN / rounding check carries its address; each one is RED on the pre-fix bodies (the checks
// marked "discriminates" confirm the chosen input rounds differently fused and unfused).
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/fpu/scalar_operation.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <limits>

#include "restored_methods.inc"

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int)
{
    std::fprintf(stderr, "Assertion: %s\n", lpcMessage); std::abort();
}
void* EndAssert() { return nullptr; }
} }

namespace CgsDev {
StrStreamBase::StrStreamBase() : mePrintMode(E_PRINTMODE_DECIMAL) {}
StrStreamBase& StrStreamBase::operator<<(s32) { return *this; }
StrStreamBase& StrStreamBase::operator<<(u32) { return *this; }
StrStreamBase& StrStreamBase::operator<<(u64) { return *this; }
StrStreamBase& StrStreamBase::operator<<(f32) { return *this; }
StrStreamBase& StrStreamBase::operator<<(void*) { return *this; }
StrStreamBase& StrStreamBase::operator<<(PrintMode) { return *this; }
void StrStreamBase::AppendFormat(const char*, ...) {}
namespace Log {
DebugPrint* gpDebugPrint = nullptr;
StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
} }

// The fixture constructs the real manager and its contained cars; these vtable entries must link but
// never run here (the same set tests/FxLadderReplay.cpp carries).
namespace CgsDev {
DebugComponent::DebugComponent() {}
void DebugComponent::Update() { std::abort(); }
void DebugComponent::RenderWorld(Debug3DImmediateRender*) { std::abort(); }
void DebugComponent::RenderHUD(Debug2DImmediateRender*) { std::abort(); }
const char* DebugComponent::GetPath() const { std::abort(); }
const char* DebugComponent::GetName() const { std::abort(); }
bool DebugComponent::IsSimple() const { std::abort(); }
void DebugComponent::OnActivate() { std::abort(); }
void DebugComponent::OnRegister() { std::abort(); }
namespace DebugUI {
Window::Window() {}
void Window::Update(f32, InputEvent) { std::abort(); }
void Window::Render(Debug2DImmediateRender*) { std::abort(); }
void Window::OnGetFocus() { std::abort(); }
void Window::OnLostFocus() { std::abort(); }
void Window::GetMenuPath(char*, s32) { std::abort(); }
void Window::GetSelectedItemString(char*, s32) const { std::abort(); }
} }

using namespace BrnPhysics::Vehicle;

namespace BrnPhysics { namespace Vehicle {
VecFloat SimpleVehiclePhysics::GetSteeringAngle() const { std::abort(); }
void SimpleVehiclePhysics::ClearCrashing() { std::abort(); }
void SimpleVehiclePhysics::SetCrashing() { std::abort(); }
void VehiclePhysics::Update(VecFloat, VecFloat, const Matrix44Affine*, const BrnPlayerDriverControls*,
    bool, bool, bool, CgsNumeric::Random&) { std::abort(); }
void VehiclePhysics::UpdateSuspension(VecFloat) { std::abort(); }
void VehiclePhysics::SetCrashing() { std::abort(); }
void VehiclePhysics::ClearCrashing() { std::abort(); }
VecFloat VehiclePhysics::GetSteeringAngle() const { std::abort(); }
bool RaceCarPhysics::IsCrashingNormally() const { std::abort(); }
bool RaceCarPhysics::Prepare(Matrix44Affine, Vector3, Vector3, Vector3, Vector3,
    const CgsGeometric::AxisAlignedBox&, rw::physics::Inertia, VehicleAttribs*, const Vector3*, const f32*, u8)
{ std::abort(); }
void RaceCarPhysics::SetCrashing() { std::abort(); }
void RaceCarPhysics::Update(VecFloat, VecFloat, const Matrix44Affine*, const BrnPlayerDriverControls*,
    bool, bool, bool, CgsNumeric::Random&) { std::abort(); }
f32 RaceCarPhysics::GetShowtimePlayerCarStrength() const { std::abort(); }
bool RaceCarPhysics::IsPlayerVehicleInShowtime() const { std::abort(); }
void RaceCarPhysics::UpdateAftertouch(const BrnPlayerDriverControls*, const Matrix44Affine*, VecFloat, bool, bool)
{ std::abort(); }
void TrafficPhysics::Update(f32, f32, const Matrix44Affine*, const BrnPlayerDriverControls*, bool, bool, bool)
{ std::abort(); }
const char* PhysicalTrafficManagerDebugComponent::GetName() const { std::abort(); }
const char* VehicleManagerDebugComponent::GetName() const { std::abort(); }
const char* DebugComponent::GetName() const { std::abort(); }
const char* DebugComponent::GetPath() const { std::abort(); }
} }

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcWhat)
    {
        ++guChecks;
        if (!lbPass) { ++guFailures; }
        std::printf("  %s %s\n", lbPass ? "ok  " : "FAIL", lpcWhat);
    }
    bool Same(f32 lfA, f32 lfB)   // bit-exact, NaN == NaN
    {
        u32 luA, luB;
        std::memcpy(&luA, &lfA, 4); std::memcpy(&luB, &lfB, 4);
        return luA == luB;
    }
    const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

    VehicleManager gManager;
    RaceCarPhysics& Car(s32 liIndex) { return gManager.maRaceCarVehicles[liIndex]; }

    // ---- AddSlam ---------------------------------------------------------------------------------
    // Arms a slam of duration 0.8, steer 0.25, recovery 2.0 by car 3 on car 0 from the given state.
    void AddSlam(f32 lfLife, f32 lfTotal, bool lbTaper, f32 lfAirTime)
    {
        RaceCarPhysics& lrCar = Car(0);
        lrCar.mSlamEffect.mfSlamLife = lfLife;
        lrCar.mSlamEffect.mfTotalSlamTime = lfTotal;
        lrCar.mSlamEffect.mfSteering = 9.0f;
        lrCar.mSlamEffect.mfOriginalSteering = 9.0f;
        lrCar.mSlamEffect.mi8SlamNumber = 0;
        lrCar.mfSpeedMPH = VecFloat{ lfAirTime, lfAirTime, lfAirTime, lfAirTime };
        lrCar.AddSlam(lbTaper, 0.8f, 0.25f, 2.0f, 3);
    }

    // ---- UpdateSlam ------------------------------------------------------------------------------
    BrnPlayerDriverControls gControls;
    void UpdateSlam(f32 lfLife, f32 lfTotal, f32 lfOriginal, bool lbAI, f32 lfSteer, f32 lfGas)
    {
        RaceCarPhysics& lrCar = Car(1);
        lrCar.mSlamEffect.mfSlamLife = lfLife;
        lrCar.mSlamEffect.mfTotalSlamTime = lfTotal;
        lrCar.mSlamEffect.mfOriginalSteering = lfOriginal;
        lrCar.mSlamEffect.mfSteering = 0.0f;
        lrCar.mSlamEffect.mfRecoveryTime = 2.0f;
        lrCar.mSlamEffect.mi8SlamNumber = 1;
        gControls = BrnPlayerDriverControls{};
        gControls.meDriverType = lbAI ? E_DRIVER_TYPE_AI : E_DRIVER_TYPE_PLAYER;
        gControls.mfSteering = lfSteer;
        gControls.mfGas = lfGas;
        gControls.mfBrake = 0.7f;
        gControls.mfHandBrake = 0.6f;
        lrCar.UpdateSlam(&gControls, VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f });   // dt 0: life is the input
    }
    // The console's envelope: 0x825D49A4 fdivs, 0x825D49B4 fnmsubs (one rounding), 0x825D49B0/B8 fmuls.
    f32 ConsoleEnvTerm(f32 lfLife, f32 lfTotal, f32 lfOriginal)
    {
        const f32 lfR = lfLife / lfTotal;
        return -std::fmaf(lfR, lfR, -lfR) * (lfOriginal * 2.0f);
    }
    f32 TwoRoundingEnvTerm(f32 lfLife, f32 lfTotal, f32 lfOriginal)
    {
        const f32 lfR = lfLife / lfTotal;
        const f32 lfSq = lfR * lfR;
        return -(lfSq - lfR) * (lfOriginal * 2.0f);
    }

    // ---- CalculateSlamData -----------------------------------------------------------------------
    f32 SlamDuration(f32 lfAggressorSteer, f32 lfAggressorMass, f32 lfVictimMass, f32 lfVictimSteer,
                     f32* lpfCounter)
    {
        RaceCarPhysics& lrVictim = Car(2);
        RaceCarPhysics& lrAggressor = Car(3);
        lrAggressor.mfSlamSteering = lfAggressorSteer;
        lrVictim.mfSlamSteering = lfVictimSteer;
        lrAggressor.mfMass = VecFloat{ lfAggressorMass, lfAggressorMass, lfAggressorMass, lfAggressorMass };
        lrVictim.mfMass = VecFloat{ lfVictimMass, lfVictimMass, lfVictimMass, lfVictimMass };
        lrVictim.mSlamEffect.mi8SlamNumber = 0;
        lrVictim.mTransform.SetIdentity();
        gManager.maeRaceCarTypes[3] = BrnWorld::E_RACE_CAR_TYPE_AI;   // no offline x0.7 softening
        gManager.meCurrentGameModeType = 0;
        VehicleManager::RaceCarResponseInfo lInfo = {};
        lInfo.meVictimActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(2);
        lInfo.meAggressorActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(3);
        lInfo.meImpactSitutation = static_cast<EImpactSituation>(2);   // situation scale 1.0 (0x82F2A1A8[2])
        f32 lfDuration = 0.0f, lfCounter = 0.0f, lfSteerDirection = 0.0f, lfRecovery = 0.0f, lfVulnerable = 0.0f;
        Vector3 lvDirection;
        u8 lu8Score = 0;
        gManager.CalculateSlamData(&lInfo, &lfDuration, &lfCounter, &lfSteerDirection, &lfRecovery, &lvDirection,
                                   &lu8Score, &lfVulnerable);
        if (lpfCounter) *lpfCounter = lfCounter;
        return lfDuration;
    }
    // The console's ease: Min(|s|, 1.2) (fsel 0x825C771C), fsubs, fmuls, then 0x825C7740 fnmsubs.
    f32 ConsoleEase(f32 lfAbs)
    {
        const f32 lfCap = rw::math::fpu::Min(lfAbs, 1.2f);
        const f32 lfSq = (1.0f - lfCap) * (1.0f - lfCap);
        return -std::fmaf(lfSq, lfSq, -1.0f);
    }
}

int main()
{
    std::printf("AddSlam @0x825D4870 -- the rate limit (bgt 0x825D4888, blt 0x825D48B0) and the taper (fsel 0x825D48F0/FC):\n");
    AddSlam(KF_NAN, 1.0f, false, 0.0f);
    Check(Car(0).mSlamEffect.mfSlamLife == 0.8f && Car(0).mSlamEffect.mfSteering == 1.0f,
          "NaN life: bgt not taken -> the slam re-arms (life 0.8, steering 4.0 * 0.25)");
    AddSlam(0.3f, KF_NAN, false, 0.0f);
    Check(Car(0).mSlamEffect.mfSlamLife == 0.8f, "life 0.3, NaN total: blt not taken -> re-arms");
    AddSlam(0.3f, 0.5f, false, 0.0f);
    Check(Car(0).mSlamEffect.mfSlamLife == 0.3f && Car(0).mSlamEffect.mfSteering == 9.0f,
          "life 0.3, 0.2 s in (< 0.5): the running slam is kept");
    AddSlam(0.3f, 0.9f, false, 0.0f);
    Check(Car(0).mSlamEffect.mfSlamLife == 0.8f, "life 0.3, 0.6 s in (>= 0.5): re-arms");
    AddSlam(0.0f, 0.9f, false, 0.0f);
    Check(Car(0).mSlamEffect.mfSlamLife == 0.8f, "life 0.0: re-arms");
    AddSlam(0.0f, 0.0f, true, KF_NAN);
    Check(Car(0).mSlamEffect.mfSteering == 1.0f, "taper, NaN air time: Clamp(NaN, 0, 1) = 1.0 -> scale 4.0");
    AddSlam(0.0f, 0.0f, true, 75.0f);
    Check(Car(0).mSlamEffect.mfSteering == 0.5f, "taper, air time 75: 75 / 150 = 0.5 -> scale 2.0");
    AddSlam(0.0f, 0.0f, true, -10.0f);
    Check(Car(0).mSlamEffect.mfSteering == 0.0f, "taper, negative ratio -> 0");

    std::printf("UpdateSlam @0x825D4950 -- the alive gate (bgt 0x825D4988), the fsel clamps, the fused roundings:\n");
    UpdateSlam(KF_NAN, 1.0f, 0.3f, false, 0.2f, 0.5f);
    Check(Same(gControls.mfSteering, 0.2f) && Same(gControls.mfGas, 0.5f) && Same(gControls.mfBrake, 0.7f),
          "NaN life: bgt not taken -> the dead branch; the controls are untouched");
    Check(std::isnan(Car(1).mSlamEffect.mfSlamLife) && Car(1).mSlamEffect.mi8SlamNumber == 1,
          "... and bgelr (0x825D4AB4) returns on unordered: the slam is not cleared");
    UpdateSlam(0.5f, 1.0f, 0.3f, true, KF_NAN, 0.5f);
    Check(Same(gControls.mfSteering, 0.0025f + 0.15f * 0.005f),
          "AI, NaN steer: Clamp(NaN, -0.0025, 0.0025) = +0.0025 (fsel 0x825D49EC/F4), plus the slam term");
    UpdateSlam(0.5f, 1.0f, KF_NAN, true, 0.1f, 0.5f);
    Check(Same(gControls.mfSteering, 0.0025f + 1.0f),
          "AI, NaN slam term: Clamp(NaN, -1, 1) = +1.0 (fsel 0x825D4A1C/24)");
    UpdateSlam(0.5f, 1.0f, 0.3f, false, KF_NAN, 0.5f);
    Check(Same(gControls.mfSteering, 0.95f), "player, NaN steer: Clamp(NaN, -0.95, 0.95) = +0.95 (fsel 0x825D4A70/80)");
    UpdateSlam(0.5f, 1.0f, 0.3f, false, 0.1f, KF_NAN);
    Check(Same(gControls.mfSteering, 0.95f) && Same(gControls.mfGas, 0.9f),
          "player, NaN gas: steer +0.95, and Max(NaN, 0.9) = 0.9 (fsel 0x825D4A90)");
    {
        const f32 lfLife = 0.09047619f, lfTotal = 1.0f, lfOrig = 0.5f;
        const f32 lfConsole = ConsoleEnvTerm(lfLife, lfTotal, lfOrig);
        Check(!Same(lfConsole, TwoRoundingEnvTerm(lfLife, lfTotal, lfOrig)), "(the envelope input discriminates)");
        UpdateSlam(lfLife, lfTotal, lfOrig, true, 0.0f, 0.5f);
        Check(Same(Car(1).mSlamEffect.mfSteering, lfConsole), "envelope r - r^2 is ONE fnmsubs (0x825D49B4), bit for bit");
    }
    {
        const f32 lfLife = 0.5f, lfTotal = 1.0f, lfOrig = -1.0f, lfGas = 0.3f, lfSteer = 0.1f;
        const f32 lfEnvTerm = ConsoleEnvTerm(lfLife, lfTotal, lfOrig);   // -0.5, exact either way
        const f32 lfConsole = std::fmaf(std::fmaf(lfGas, 0.1f, 0.89999998f), lfEnvTerm, lfSteer);
        const f32 lfTwo = ((lfGas * 0.1f) + 0.89999998f) * lfEnvTerm + lfSteer;
        Check(!Same(lfConsole, lfTwo), "(the steer input discriminates)");
        UpdateSlam(lfLife, lfTotal, lfOrig, false, lfSteer, lfGas);
        Check(Same(gControls.mfSteering, lfConsole), "player steer is two fmadds (0x825D4A5C/60), bit for bit");
        Check(Same(gControls.mfGas, 0.89999998f) && gControls.mfBrake == 0.0f && gControls.mfHandBrake == 0.0f,
              "gas floored at 0.9, brake and handbrake cleared");
    }
    UpdateSlam(-3.0f, 1.0f, 0.3f, false, 0.1f, 0.5f);
    Check(Car(1).mSlamEffect.mi8SlamNumber == -1 && Car(1).mSlamEffect.mfSlamLife == 0.0f,
          "life -3 < -recovery 2: the slam is cleared");
    UpdateSlam(0.5f, 1.0f, 40.0f, false, 0.1f, 1.0f);
    Check(Same(gControls.mfSteering, 0.95f), "a large envelope clamps at +0.95");

    std::printf("CalculateSlamData @0x825C7568 -- the mass-ratio fsel clamp (0x825C7764/70) and the ease fnmsubs (0x825C7740):\n");
    {
        const f32 lfDuration = SlamDuration(0.4f, KF_NAN, 1500.0f, 0.0f, nullptr);
        Check(Same(lfDuration, ConsoleEase(0.4f) * 1.1f * 1.0f),
              "NaN aggressor mass: Clamp(NaN, 0.9, 1.1) = 1.1 -> duration = ease * 1.1");
        Check(Same(SlamDuration(0.4f, 3000.0f, 1500.0f, 0.0f, nullptr), ConsoleEase(0.4f) * 1.1f),
              "mass ratio 2.0 -> 1.1");
        Check(Same(SlamDuration(0.4f, 750.0f, 1500.0f, 0.0f, nullptr), ConsoleEase(0.4f) * 0.9f),
              "mass ratio 0.5 -> 0.9");
    }
    {
        const f32 lfAbs = 0.05f;
        const f32 lfCap = lfAbs;
        const f32 lfSq = (1.0f - lfCap) * (1.0f - lfCap);
        Check(!Same(ConsoleEase(lfAbs), 1.0f - lfSq * lfSq), "(the ease input discriminates)");
        Check(Same(SlamDuration(lfAbs, 1500.0f, 1500.0f, 0.0f, nullptr), ConsoleEase(lfAbs) * 1.0f),
              "ease 1 - (1 - |s|)^4 ends in ONE fnmsubs, bit for bit");
    }
    {
        f32 lfCounter = 0.0f;
        SlamDuration(0.4f, 1500.0f, 1500.0f, -3.0f, &lfCounter);
        Check(lfCounter == 1.0f, "counter: |-3 * 0.5| = 1.5 clamps to 1.0");
        SlamDuration(0.4f, 1500.0f, 1500.0f, -0.12f, &lfCounter);
        Check(lfCounter == 0.1f, "counter: |-0.12 * 0.5| = 0.06 clamps to 0.1");
    }

    std::printf("FxLadderSlam: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
