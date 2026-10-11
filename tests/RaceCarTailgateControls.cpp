#include <cmath>
#include <cstdio>
#include <limits>
using f32 = float;
using s32 = int;
using EActiveRaceCarIndex = int;
constexpr int E_ACTIVE_RACE_CAR_INDEX_0 = 0;
constexpr int E_ACTIVE_RACE_CAR_INDEX_COUNT = 8;
constexpr int E_ACTIVE_RACE_CAR_INDEX_INVALID = -1;
static int giChecks = 0, giFailures = 0;
static void Check(bool lbValue, const char* lpcName)
{
    ++giChecks;
    if (!lbValue) { ++giFailures; std::printf("FAIL %s\n", lpcName); }
}
#define CGS_ASSERT(condition, message) Check(condition, message)
struct Vector3
{
    f32 x = 0, y = 0, z = 0, w = 0;
    Vector3 operator-() const { return {-x, -y, -z, -w}; }
    Vector3 operator+(Vector3 lOther) const { return {x+lOther.x, y+lOther.y, z+lOther.z, w+lOther.w}; }
    Vector3 operator-(Vector3 lOther) const { return {x-lOther.x, y-lOther.y, z-lOther.z, w-lOther.w}; }
    Vector3 operator*(f32 lfScale) const { return {x*lfScale, y*lfScale, z*lfScale, w*lfScale}; }
};
namespace rw::math::vpu
{
    f32 Dot(Vector3 lA, Vector3 lB) { return lA.x*lB.x+lA.y*lB.y+lA.z*lB.z; }
    Vector3 Normalize(Vector3 lValue) { return lValue*(1.0f/std::sqrt(Dot(lValue,lValue))); }
}
namespace BrnPhysics::Vehicle
{
    struct RaceCarState { Vector3 mLinearVelocity; f32 mfSpeedMPH = 40.0f; };
}
namespace BrnWorld
{
    extern f32 KF_Z_OFFSET_FROM_CAR_CENTRE_TO_CONE_APEX;
    extern f32 KF_TAILGATING_CONE_HALF_ANGLE_RADS;
    extern f32 KF_TAILGATING_CONE_DEPTH;
    extern f32 KF_MIN_TAILGATE_DURATION;
    extern f32 KF_TAILGATING_MAX_RELATIVE_VELOCITY_MAGNITUDE;
    extern f32 KF_MIN_TAILGATEE_SPEED;
    struct ActiveRaceCar
    {
        Vector3 mPosition, mDirection{0,0,1,0};
        BrnPhysics::Vehicle::RaceCarState mPhysicsState;
        bool mbActive = false;
        Vector3 GetPosition() const { return mPosition; }
        Vector3 GetDirection() const { return mDirection; }
        bool IsAttached() const { return true; }
        bool IsActive() const { return mbActive; }
        const BrnPhysics::Vehicle::RaceCarState* GetPhysicsState() const { return &mPhysicsState; }
    };
    struct BoostManager
    {
        bool mbTailgating = false;
        int miCar = -1;
        void SetTailgating(bool lbValue, int liCar) { mbTailgating=lbValue; miCar=liCar; }
    };
    struct RaceCarEntityModule
    {
        int meIndexOfCarPlayerIsTailgating = -1;
        f32 mfCurrentTailgateDuration = 0;
        BoostManager mBoostManager;
        bool IsPlayerCarTailgatingOtherRaceCars(EActiveRaceCarIndex, const ActiveRaceCar*);
        void UpdateTailgateBoost();
    };
}
#include "race_car_tailgate_controls.inc"
int main()
{
    using namespace BrnWorld;
    RaceCarEntityModule lModule;
    ActiveRaceCar laCars[8];
    laCars[1].mbActive = true;
    laCars[1].mPosition = {0,0,10,0};
    auto IsTailgating = [&] { return lModule.IsPlayerCarTailgatingOtherRaceCars(0,laCars); };
    Check(IsTailgating(), "shipped defaults detect the car ahead");
    Check(lModule.meIndexOfCarPlayerIsTailgating==1, "detected slot is published");
    KF_TAILGATING_CONE_DEPTH = 10.0f;
    Check(!IsTailgating(), "shrinking menu cone depth changes detection");
    Check(lModule.meIndexOfCarPlayerIsTailgating==-1, "miss clears the slot");
    KF_TAILGATING_CONE_DEPTH = 12.0f;
    Check(IsTailgating(), "cone depth includes the boundary");
    KF_TAILGATING_CONE_DEPTH = 20.0f;
    laCars[0].mPosition.x = 8.0f;
    Check(!IsTailgating(), "shipped narrow cone rejects an off-axis car");
    KF_TAILGATING_CONE_HALF_ANGLE_RADS = 1.0f;
    Check(IsTailgating(), "widening menu angle changes detection");
    laCars[0].mPosition.x = 0.0f;
    KF_TAILGATING_CONE_HALF_ANGLE_RADS = 0.346f;
    KF_MIN_TAILGATEE_SPEED = 41.0f;
    Check(!IsTailgating(), "raising menu minimum speed changes detection");
    KF_MIN_TAILGATEE_SPEED = 40.0f;
    Check(IsTailgating(), "minimum speed includes the boundary");
    KF_MIN_TAILGATEE_SPEED = 30.0f;
    laCars[1].mPhysicsState.mLinearVelocity.z = 20.0f;
    KF_TAILGATING_MAX_RELATIVE_VELOCITY_MAGNITUDE = 19.0f;
    Check(!IsTailgating(), "lowering menu relative speed changes detection");
    KF_TAILGATING_MAX_RELATIVE_VELOCITY_MAGNITUDE = 20.0f;
    Check(IsTailgating(), "relative speed includes the boundary");
    KF_TAILGATING_MAX_RELATIVE_VELOCITY_MAGNITUDE = std::numeric_limits<f32>::quiet_NaN();
    Check(IsTailgating(), "PPC ble accepts unordered relative speed comparison");
    KF_TAILGATING_MAX_RELATIVE_VELOCITY_MAGNITUDE = 40.2336f;
    laCars[1].mPhysicsState.mLinearVelocity.z = std::numeric_limits<f32>::quiet_NaN();
    Check(IsTailgating(), "PPC ble accepts unordered velocity difference");
    laCars[1].mPhysicsState.mLinearVelocity.z = 20.0f;
    KF_Z_OFFSET_FROM_CAR_CENTRE_TO_CONE_APEX = 15.0f;
    Check(!IsTailgating(), "cone reference offset uses the shared renderer variable");
    KF_Z_OFFSET_FROM_CAR_CENTRE_TO_CONE_APEX = 2.0f;
    laCars[1].mbActive = false;
    Check(!IsTailgating(), "inactive cars are excluded");
    laCars[7] = laCars[1]; laCars[7].mbActive = true;
    Check(IsTailgating() && lModule.meIndexOfCarPlayerIsTailgating==7, "last car slot participates");
    lModule.mfCurrentTailgateDuration = 0.5f;
    lModule.meIndexOfCarPlayerIsTailgating = 7;
    lModule.UpdateTailgateBoost();
    Check(!lModule.mBoostManager.mbTailgating, "default boost threshold requires more than one second");
    KF_MIN_TAILGATE_DURATION = 0.25f;
    lModule.UpdateTailgateBoost();
    Check(lModule.mBoostManager.mbTailgating && lModule.mBoostManager.miCar==7,
          "lowering menu duration grants boost credit to the detected car");
    KF_MIN_TAILGATE_DURATION = 0.5f;
    lModule.UpdateTailgateBoost();
    Check(!lModule.mBoostManager.mbTailgating, "duration boundary is strict");
    KF_MIN_TAILGATE_DURATION = 2.0f;
    lModule.mfCurrentTailgateDuration = 1.5f;
    lModule.UpdateTailgateBoost();
    Check(!lModule.mBoostManager.mbTailgating, "raising menu duration delays boost credit");
    std::printf("RaceCarTailgateControls: %d checks, %d failures\n",giChecks,giFailures);
    return giFailures ? 1 : 0;
}
