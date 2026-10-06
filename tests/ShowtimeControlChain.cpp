// Complete production aftertouch -> bounce -> velocity-cap bodies, using ARTIST raw-word vectors.
// External force/impulse applications are recorded on both sides of the oracle.
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
#include "GameSource/Math/BrnMathUtils.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int)
{ std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); std::abort(); }
void* EndAssert() { return nullptr; }
} namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} } }

struct ChainInput
{
    f32 steer, pitch, gas, brake, spin, power, dt, airtime, height, pushTimer, uncappedTimer;
    f32 velocity[3], position[3], aim[3], camera[16];
    bool air, force, crashing, showtime, startline, wheel, button, boosting, justBounced, pending, launch, validHeight;
    s32 collisions, worldCollisions;
    s32 numTargets, currentTarget;
    Vector3 targetPositions[8];
    u32 targetIds[8];
    Vector3 envelope;
    u64 airRamMask;
};
struct ChainCall
{
    s32 kind;
    Vector3 v, point;
    s32 spaceImpulse, spacePoint;
    u32 flags;
    f32 factor, decay, limit;
};
struct ChainExpected
{
    ChainInput input;
    std::vector<ChainCall> calls;
    Vector3 velocity, angularVelocity;
    bool flags[11];
    s16 chainCount;
    f32 pushTimer;
    s32 currentTarget;
    Vector3 envelope;
    u64 airRamMask;
};
#include "showtime_chain_data.inc"

namespace BrnPhysics { namespace Vehicle {
namespace vpu = rw::math::vpu;
static PlayerParameters MS;
struct AtGateFixtureCounters
{
    u32 muEnter = 0, mauGateCrashing[2] = {}, mauGateStartLine[2] = {}, mauGateShowtime[2] = {};
    u32 muBodyRan = 0, muChannelsRan = 0, muYawAngImpulse = 0, muLeverRoll = 0, muLeverPitch = 0;
} gsAtGate;
static bool AtGateArmed() { return false; }
struct TargetAssistWitness
{
    s32 miNumTargets=0,miPrevTargetId=0,miBest=0,miBestId=0,miCurrentTargetId=0;
    f32 mfAirTime=0,mfEnvelopeY=0,mfForce=0;
    Vector3 mvAim{};
    bool mbAirGate=false,mbForceFired=false;
};
static bool TargetAssistWitnessArmed() { return false; }
static void NoteTargetAssist(const TargetAssistWitness&) {}
static void ScoreTargetAssistCandidates(TargetAssistWitness&,const Vector3&,const Vector3*,const s32*,f32) {}

struct ShowtimeFixture
{
    bool mbPlayerCarInShowtime = true, mbCrashing = true, mbHasAir = false;
    s32 miNumCollisions = 0;
    s8 mi8NumWorldCollisions = 0;
    struct { bool mbValid; f32 mfVerticalDistance; } mAboveGroundTestResult = {};
    Matrix44Affine mTransform;
    Vector3 velocity, angularVelocity;
    Vector4 mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction;
    std::vector<ChainCall> calls;
    struct { u64 mask=0; void UnSetAll() { mask=0; } } mUsedAirRams;

    bool IsCrashing() const { return mbCrashing; }
    bool IsPlayerVehicleActuallyInShowtime() const { return mbPlayerCarInShowtime; }
    bool IsBounceBoosting() const { return MS.mbBounceBoosting; }
    bool IsPlayerVehicleWithUncappedShowtimeSpeed() const { return MS.mfUncappedSpeedTimer > 0; }
    Vector3 GetLinearVelocity() const { return velocity; }
    Vector3 GetAngularVelocity() const { return angularVelocity; }
    void SetLinearVelocity(Vector3 lv) { velocity = lv; }
    void SetAngularVelocity(Vector3 lv) { angularVelocity = lv; }
    void AddWorldSpaceForce(Vector3 lv) { calls.push_back({0, lv}); }
    void AddWorldSpaceAngularImpulse(Vector3 lv) { calls.push_back({1, lv}); }
    void AddLocalImpulse(Vector3 lv, rw::physics::InputSpace leImpulse, Vector3 lp, rw::physics::InputSpace lePoint)
    { calls.push_back({2, lv, lp, static_cast<s32>(leImpulse), static_cast<s32>(lePoint)}); }
    void AddAirRam(u32 luFlags, f32 lfFactor, f32 lfDecay, Vector3 lv, Vector3 lp, f32 lfLimit)
    { calls.push_back({3, lv, lp, 0, 0, luFlags, lfFactor, lfDecay, lfLimit}); }
    Vector3* ComputeIdealVelocity(Vector3*,Vector3,f32) const;
    void UpdateTargetAssist(const BrnPlayerDriverControls*, VecFloat, VecFloat, Vector3);
    void UpdateAftertouch(const BrnPlayerDriverControls*, const Matrix44Affine*, VecFloat, bool, bool);
    void UpdateShowtimePhysics(const BrnPlayerDriverControls*, const Vector3&, const Vector3&, VecFloat, VecFloat, bool);
    void CapShowtimeVelocities();
};
#include "showtime_chain_methods.inc"
} }

static u32 suChecks = 0, suFailures = 0;
static void Check(bool lbCondition, size_t luCase, const char* lpcMessage)
{
    ++suChecks;
    if (!lbCondition)
    {
        ++suFailures;
        if (suFailures < 18) std::fprintf(stderr, "Case %zu: %s\n", luCase, lpcMessage);
    }
}
static bool Near(f32 lfA, f32 lfB)
{ return std::isfinite(lfA) && std::isfinite(lfB) && std::fabs(lfA-lfB) <= 0.000001f * std::max(1.0f, std::fabs(lfB)); }
static bool Same(Vector3 la, Vector3 lb)
{ return Near(la.x,lb.x) && Near(la.y,lb.y) && Near(la.z,lb.z); }

int main()
{
    using namespace BrnPhysics::Vehicle;
    for (size_t luCase = 0; luCase < saExpected.size(); ++luCase)
    {
        const auto& lrExpected = saExpected[luCase];
        const auto& lrInput = lrExpected.input;
        ShowtimeFixture lCar = {};
        MS = {};
        MS.mbBounceBoosting = lrInput.boosting;
        MS.mbJustBounced = lrInput.justBounced;
        MS.mbBounceBoostPending = lrInput.pending;
        MS.mbLaunchActive = lrInput.launch;
        MS.mfTimeUntilPush = lrInput.pushTimer;
        MS.mfUncappedSpeedTimer = lrInput.uncappedTimer;
        MS.mAimDirection = {lrInput.aim[0],lrInput.aim[1],lrInput.aim[2],0};
        MS.miNumTargets=lrInput.numTargets;
        MS.miCurrentTargetId=lrInput.currentTarget;
        MS.mAssistStrength=lrInput.envelope;
        for(s32 liTarget=0;liTarget<lrInput.numTargets;++liTarget)
        {
            MS.maTargetPositions[liTarget]=lrInput.targetPositions[liTarget];
            MS.maTargetIds[liTarget].muValue=lrInput.targetIds[liTarget];
        }
        lCar.mUsedAirRams.mask=lrInput.airRamMask;
        lCar.mbPlayerCarInShowtime = lrInput.showtime;
        lCar.mbCrashing = lrInput.crashing;
        lCar.mbHasAir = lrInput.air;
        lCar.miNumCollisions = lrInput.collisions;
        lCar.mi8NumWorldCollisions = static_cast<s8>(lrInput.worldCollisions);
        lCar.mAboveGroundTestResult = {lrInput.validHeight,lrInput.height};
        lCar.velocity = {lrInput.velocity[0],lrInput.velocity[1],lrInput.velocity[2],0};
        lCar.angularVelocity = {.25f,.5f,-.25f,0};
        lCar.mTransform.SetIdentity();
        lCar.mTransform.wAxis = {lrInput.position[0],lrInput.position[1],lrInput.position[2],0};
        lCar.mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction = {0,0,lrInput.airtime,0};
        Matrix44Affine lCamera;
        auto* lpRows = &lCamera.xAxis;
        for (u32 luRow = 0; luRow < 4; ++luRow)
            lpRows[luRow] = {lrInput.camera[luRow*4],lrInput.camera[luRow*4+1],lrInput.camera[luRow*4+2],lrInput.camera[luRow*4+3]};
        BrnPlayerDriverControls lControls = {};
        lControls.mfSteering = lrInput.steer;
        lControls.mfForwardSteering = lrInput.pitch;
        lControls.mfRequestedGas = lrInput.gas;
        lControls.mfBrake = lrInput.brake;
        lControls.mfSpin = lrInput.spin;
        lControls.mfAftertouchLevel = lrInput.power;
        lControls.mbBoostBounce = lrInput.button;
        lControls.mbIsOnStartLine = lrInput.startline;
        lControls.mbIsSteeringWheel = lrInput.wheel;
        lCar.UpdateAftertouch(&lControls,&lCamera,VecFloat{lrInput.dt,lrInput.dt,lrInput.dt,lrInput.dt},lrInput.force,false);
        Check(lCar.calls.size() == lrExpected.calls.size(),luCase,"external impulse call count differs from ARTIST");
        for (size_t luCall = 0; luCall < std::min(lCar.calls.size(),lrExpected.calls.size()); ++luCall)
        {
            const auto& lrA = lCar.calls[luCall];
            const auto& lrB = lrExpected.calls[luCall];
            Check(lrA.kind == lrB.kind,luCase,"impulse kind/order differs");
            if(!Same(lrA.v,lrB.v) && suFailures<18)
                std::fprintf(stderr,"Case %zu call %zu: actual (%.9g %.9g %.9g), ARTIST (%.9g %.9g %.9g)\n",
                    luCase,luCall,lrA.v.x,lrA.v.y,lrA.v.z,lrB.v.x,lrB.v.y,lrB.v.z);
            Check(Same(lrA.v,lrB.v),luCase,"impulse vector differs");
            Check(Same(lrA.point,lrB.point),luCase,"application point differs");
            Check(lrA.spaceImpulse == lrB.spaceImpulse && lrA.spacePoint == lrB.spacePoint,luCase,"impulse space differs");
            Check(lrA.flags == lrB.flags && Near(lrA.factor,lrB.factor) && Near(lrA.decay,lrB.decay) && Near(lrA.limit,lrB.limit),
                  luCase,"AirRam parameters differ");
        }
        Check(Same(lCar.velocity,lrExpected.velocity),luCase,"capped linear velocity differs");
        Check(Same(lCar.angularVelocity,lrExpected.angularVelocity),luCase,"capped angular velocity differs");
        const bool laFlags[] = {MS.mbBounceBoosting,MS.mbJustBounced,MS.mbBouncedThisFrame,MS.mbCarBounce,MS.mbGoodImpact,
                               MS.mbShouldBounceBoost,MS.mbBounceBoostPending,MS.mbSixaxisTiltApplied,
                               MS.mbDisableShowtime,MS.mbBounceWasGood,MS.mbLaunchActive};
        for (u32 luFlag = 0; luFlag < 11; ++luFlag)
            Check(laFlags[luFlag] == lrExpected.flags[luFlag],luCase,"bounce latch differs");
        Check(MS.muBounceChainCount == lrExpected.chainCount,luCase,"bounce chain count differs");
        Check(Near(MS.mfTimeUntilPush,lrExpected.pushTimer),luCase,"launch timer differs");
        Check(MS.miCurrentTargetId==lrExpected.currentTarget,luCase,"target selection differs");
        Check(Same(MS.mAssistStrength,lrExpected.envelope) && Near(MS.mAssistStrength.w,lrExpected.envelope.w),
              luCase,"target assistance envelope differs");
        Check(lCar.mUsedAirRams.mask==lrExpected.airRamMask,luCase,"target assistance AirRam clear differs");
    }
    std::printf("Showtime control chain: %s (%zu cases, %u checks, %u failures)\n",
                suFailures ? "FAIL" : "PASS",saExpected.size(),suChecks,suFailures);
    return suFailures ? 1 : 0;
}
