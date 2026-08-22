#pragma once

#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"
#include "GameSource/World/Traffic/BrnVehicleSoaData.h"
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"

namespace CgsNumeric { class Random; }

namespace BrnTraffic
{
struct Hull;   // struct, not class: BrnTrafficHull.h defines it as a struct and MSVC mangles the key
class Param;
class VehicleTypeRuntime;
struct LaneRung;
struct VehicleTraits;

// DWARF BrnTrafficVehicle.h:53. The weight the ship-only `lOldUp` argument carries into
// Vehicle::UpdateMatrix's blended up-vector: X360 @0x82757768 loads the whole 16-byte lane
// register from .data 0x8300D190 and feeds it to one vmaddcfp128. Value 20.0f in all four
// lanes; defined in BrnTrafficVehicle.cpp, which carries the seeding thunk's address.
extern const VecFloat KF_VEHICLE_UPDATE_MATRIX_OLD_UP_FACTOR;

struct Axle
{
    Vector3Plus mPosAndWheelRadius;
    Vector3Plus mUpAndDebug;

    void Initialise()
    {
        mPosAndWheelRadius.SetZero();
        mUpAndDebug = Vector3Plus{ 0.0f, 1.0f, 0.0f, 0.0f };
    }

    bool TryIntersectWithLane(const LaneRung& lRung0, const LaneRung& lRung1);
    void ForceIntersectWithLane(const LaneRung& lRung0, const LaneRung& lRung1);

    void SetUp(Vector3 lUp) { mUpAndDebug.SetVector3(lUp); }
    Vector3 GetUp() const { return mUpAndDebug.GetVector3(); }

    static void _AssertLayout();   // never called; body in the .cpp
};

struct VehicleAxles
{
    Axle mFrontAxle;
    Axle mBackAxle;

    void UpdateRearAxleForRoadCollision(const Param* lpParam, Hull** lpapHulls);

    // DWARF BrnTrafficVehicle.h:148 -- X360 @0x82756738. Seats both axles straight off a
    // finished vehicle transform, with no lane intersection.
    void SetFromVehicleTransform(Matrix44Affine lTransform,
                                 const VehicleTypeRuntime* lpVehicleTypeRuntime,
                                 const VehicleTypeUpdateData* lpVehicleTypeUpdate);

    static void _AssertLayout();   // never called; body in the .cpp
};

class Vehicle
{
public:
    enum Flags
    {
        E_FLAG_ALIVE         = 0x01,
        E_FLAG_HASENTITY     = 0x02,
        E_FLAG_COLLIDABLE    = 0x04,
        E_FLAG_PHYSICAL      = 0x08,
        E_FLAG_FROZEN        = 0x10,
        E_FLAG_ORPHAN        = 0x20,
        E_FLAG_LEFT_SLAMMED  = 0x40
    };

    enum Species
    {
        E_SPECIES_STANDARD = 0,
        E_SPECIES_STATIC   = 1,
        E_SPECIES_TRAILER  = 2
    };

    enum Manoeuvre
    {
        E_MANOEUVRE_NONE = 0,
        E_MANOEUVRE_EXTREME_SWERVE,
        E_MANOEUVRE_3_POINT_TURN,
        E_MANOEUVRE_GIVE_UP,
        E_MANOEUVRE_STUCK_REVERSE,
        E_MANOEUVRE_COUNT
    };

    enum SympatheticCrashState
    {
        E_SYMPATHETIC_NONE = 0,
        E_SYMPATHETIC_HEADON,
        E_SYMPATHETIC_ACCELERATE,
        E_SYMPATHETIC_HANDBRAKE,
        E_SYMPATHETIC_LOCKUP
    };

    void Construct(VehicleAxles* lpAxles, Matrix44Affine& lOutMatrix);

    // The driving-car constructor. DWARF BrnTrafficVehicle.h:259, X360 @0x8275F3A0.
    // Register/stack split from the prologue: r3 this, r4 lpAxles, r5 lOutMatrix, r6 lpParam,
    // f1 lfRandomVal (r7 reserved by the PPC float-arg GPR skip), r8 lpapHulls,
    // r9 luVehicleType, r10 lpVehicleTypeRuntime, then the 8-byte right-justified stack slots
    // arg_54 lpVehicleTypeUpdate, arg_5C lpVehicleTraits, f2 lfDistAcrossLane, f3 lfSpeed,
    // v1 lParamPos, v2 lParamDirection, arg_94 luVehicle, arg_9C lVehicleSoaData,
    // arg_A6 luTrailerIndex. Sixteen parameters, exactly the DWARF list.
    void InitialiseAsStandard(
        VehicleAxles* lpAxles,
        Matrix44Affine& lOutMatrix,
        const Param* lpParam,
        f32 lfRandomVal,
        Hull** lpapHulls,
        u32 luVehicleType,
        const VehicleTypeRuntime* lpVehicleTypeRuntime,
        const VehicleTypeUpdateData* lpVehicleTypeUpdate,
        const VehicleTraits* lpVehicleTraits,
        f32 lfDistAcrossLane,
        f32 lfSpeed,
        Vector3 lParamPos,
        Vector3 lParamDirection,
        u32 luVehicle,
        VehicleSoaData& lVehicleSoaData,
        u16 luTrailerIndex);

    // The parked-car constructor. DWARF BrnTrafficVehicle.h:272, X360 @0x827567F0.
    // Parameter order and width come from the prologue, not the pseudocode: r3 this,
    // r4 lpAxles, r5 lOutMatrix, f1 lfRandomVal (its GPR slot r6 is reserved and unused, the
    // PPC float-arg GPR skip), r7 luVehicleType, r8 lpVehicleTypeRuntime,
    // r9 lpVehicleTypeUpdate, r10 lTransform, then two 8-byte right-justified stack slots,
    // luVehicle and &lVehicleSoaData. Nine parameters, exactly the DWARF list.
    void InitialiseAsStatic(
        VehicleAxles* lpAxles,
        Matrix44Affine& lOutMatrix,
        f32 lfRandomVal,
        u32 luVehicleType,
        const VehicleTypeRuntime* lpVehicleTypeRuntime,
        const VehicleTypeUpdateData* lpVehicleTypeUpdate,
        Matrix44Affine lTransform,
        u32 luVehicle,
        VehicleSoaData& lVehicleSoaData);

    void InitialiseAsTrailer(
        VehicleAxles* lpAxles,
        Matrix44Affine& lOutMatrix,
        const Param* lpParam,
        f32 lfRandomVal,
        Hull** lpapHulls,
        u32 luVehicleType,
        const VehicleTypeRuntime* lpVehicleTypeRuntime,
        const VehicleTypeUpdateData* lpVehicleTypeUpdate,
        const Vehicle* lpCabVehicle,
        Matrix44Affine lCabTransform,
        const VehicleTypeUpdateData* lpCabVehicleTypeUpdate,
        const VehicleTypeRuntime* lpCabVehicleTypeRuntime,
        u32 luVehicle,
        VehicleSoaData& lVehicleSoaData,
        u16 luCabIndex);

    void OnPhysical(BrnPhysics::Vehicle::eCrashTrafficType leCrashTrafficType);

    // DWARF BrnTrafficVehicle.h:302, X360 @0x82756D48. Per-frame headlight-flash / indicator /
    // bulb-warmth tick.
    void UpdateEffects(f32 lfTimeDelta, s32 liBulbWarmthDelta, CgsNumeric::Random* lpRand);

    // ---- inline state predicates (X360 inlines these flag/species reads at every
    // call site; the bodies below assert against them by NAME). ----
    bool IsAlive() const { return (mxFlags & E_FLAG_ALIVE) != 0; }
    bool IsPhysical() const { return (mxFlags & E_FLAG_PHYSICAL) != 0; }
    bool IsOfTrailerSpecies() const { return (muSpecies & 0xF) == E_SPECIES_TRAILER; }
    // DriveTowardsTarget @0x8273E080 / @0x8273E0C4 (`lbz r11,4(this) ; clrlwi r11,r11,28`).
    bool IsOfStandardSpecies() const { return (muSpecies & 0xF) == E_SPECIES_STANDARD; }
    // Named by SetHasEntity's baked assert strings (BrnTrafficVehicle.h:979/:980). The console
    // inlines it as `(mxFlags >> 1) & 1`, e.g. 0x8270EB44 `lbz r11,5(r3) ; extrwi r11,r11,1,30`.
    bool HasEntity() const { return (mxFlags & E_FLAG_HASENTITY) != 0; }
    Manoeuvre GetCurrentManoeuvre() const;

    // DWARF BrnTrafficVehicle.h:339, X360 @0x8270E4C8: IsOfTrailerSpecies() assert (baked at
    // BrnTrafficVehicle.h:778) then `lhz r3,2(this)`.
    // The return is 16 bits (`lhz`, not `lbz`): callers compare it against
    // KU_INVALID_VEHICLE == 0xFFFF, which a u8 could never hold.
    u16 GetCabIndex() const;

    // The UNASSERTED read of the same halfword. ADDITIVE GROW (wave T3 r3): the console reaches
    // it this way where the species is not yet known -- ReturnPhysicalVehicleToTraffic
    // @0x8273DF18 is a bare `lhz r11, 2(r24)` with no assert before it, and the species test
    // comes AFTER. GetCabIndex asserts IsOfTrailerSpecies() and so cannot serve that site.
    // KU_INVALID_VEHICLE (0xFFFF) means "no other half".
    u16 GetOtherHalfIndex() const { return muOtherHalfIndex; }

    // ---- per-frame state / identity accessors (out-of-line, bodied in the .cpp) ----
    VecFloat GetSpeed() const;
    void SetSpeed(VecFloat lfSpeed);
    VecFloat GetSwerveAmount() const;
    VecFloat GetDistAcrossLane() const;
    VecFloat GetSteering() const;
    VecFloat GetWheelRot() const;
    Vector4 GetPitch_Roll_Steering_WheelRot() const;
    Vector3 GetTargetPos() const;
    Vector3 GetLinearVelocity() const;
    f32 GetSwerveTime() const;
    f32 GetRandomVal() const;
    // Vehicle+0x44 / +0x60. DriveTowardsTarget @0x8273E27C reads mfPhysicalTime against 5.0
    // and @0x8273E664 against 3.0; GenerateDriverInputs @0x827492A8 accumulates both.
    f32 GetPhysicalTime() const;
    f32 GetManoeuvreTime() const;
    void AddPhysicalTime(f32 lfDelta);
    void AddManoeuvreTime(f32 lfDelta);
    s32 GetPhysicalReason() const;
    s32 GetCurrentManoeuvrePhase() const;
    EntityId GetSympatheticCrashTarget() const;
    f32 GetHeadlightWarmth() const;
    f32 GetIndicatorBulbWarmth() const;

    bool IsHornOn() const;
    bool IsAlarmOn() const;
    // DWARF BrnTrafficVehicle.h:459. Bit 1 of mxEffectState, the partner of the bit-2
    // IsRightIndicatorOn below. EXPORT HOLE: no per-function JSON; UpdateEffects @0x82756D48
    // calls it by name.
    bool IsLeftIndicatorOn() const;
    bool IsRightIndicatorOn() const;
    bool IsIndicatingLeft() const;
    bool IsIndicatingRight() const;
    bool IsFlashingHeadlights() const;
    bool AreHeadlightsFlashed() const;
    bool AreBrakelightsOn() const;
    bool IsCrashing() const;
    // DWARF BrnTrafficVehicle.h:393. EXPORT HOLE (UpdateEffects @0x82756D48 and
    // GenerateDriverInputs @0x82748E78 call it by name); body reasoned, see the .cpp FLAG.
    bool IsSympatheticallyCrashing() const;
    // E_FLAG_FROZEN, attested by GenerateSceneUpdateEvents @0x8273C17C
    // (`lbz r11,5(this) ; rlwinm r11,r11,0,27,27` == mxFlags & 0x10).
    bool IsFrozen() const { return (mxFlags & E_FLAG_FROZEN) != 0; }
    bool IsRecoveringFromSlam() const;
    bool IsExtremeSwerving() const;
    bool IsBeingChecked() const;
    bool IsNormalPhysical() const;

    void SetSteering(f32 lfValue);
    void SetWheelRot(f32 lfValue);
    void SetSwerveAmount(f32 lfValue);
    void SetSwerveTime(f32 lfValue);
    void SetTargetPos(Vector3 lTargetPos);
    void SetLinearVelocity(Vector3 lLinearVelocity);
    void SetPitch_Roll_Steering_WheelRot(Vector4 lValues);

    void SetHornOn(bool lbOn);
    void SetAlarmOn(bool lbOn);   // @0x8270FC10
    void SetHeadlightsFlashed(bool lbOn);
    void SetLeftIndicatorOn(bool lbOn);
    void SetRightIndicatorOn(bool lbOn);
    void ToggleLeftIndicatorOn();
    void ToggleRightIndicatorOn();
    void SetBrakelightsOn(bool lbOn);
    void SetIndicatingLeft(bool lbOn);
    void SetIndicatingRight(bool lbOn);
    void SetFlashingHeadlights(bool lbOn, CgsNumeric::Random* lpRand);

    void SetPhysicalReason(s8 liReason);
    void SetSympatheticCrashTarget(EntityId lEntityId);
    void SetOrphan();
    void SetFrozen(bool lbFrozen);
    void SetCurrentManoeuvre(Manoeuvre leManoeuvre);
    void SetCurrentManoeuvrePhase(s8 liPhase);
    void SetWantsToExtremeSwerve(bool lbWants);
    // ADDITIVE (wave T3 r3 fix round) -- the two sympathetic-crash fields UpdateExtremeSwerving
    // @0x8273EB08/@0x8273EB14/@0x8273EB24 writes INLINE (`stfs f0,0x4C(r30)` then
    // `stw r10,0x48(r30)`); the console emits no call, so these are header inlines, not
    // out-of-line accessors. No layout change: both members already exist.
    void SetSympCrashTime(f32 lfTime) { mfSympCrashTime = lfTime; }
    void SetSympCrashState(SympatheticCrashState leState) { meSympCrashState = leState; }
    void StartGiveUpManoeuvre();
    void CopyEffectsFromCab(const Vehicle* lpCab);

    void SetPhysical(s8 liPartsIndex, u32 luVehicle, VehicleSoaData& lSoaData);
    void SetNotPhysical(u32 luVehicle, VehicleSoaData& lSoaData);
    void SetDead(u32 luVehicle, VehicleSoaData& lSoaData);

    // X360 @0x8270EB38. progress/identity.json files this under a CgsStrStream.h primary_file,
    // but its four baked assert strings all cite BrnTrafficVehicle.h (:979/:980/:984/:985) and
    // it reads/writes mxFlags at this+5, so it is a Vehicle member.
    //
    // Signature from DWARF BrnTrafficVehicle.h:373, confirmed in the ARTIST prologue:
    //   r3 this, r4 lbHasEntity (`clrlwi r17,r4,24`), r5 luVehicle, r6 &lSoaData.
    // The soa reference must be NON-const: the body stores through it (`stdx r10, r11, r15`
    // with r15 == r6+0x50 == &mVehiclesWithEntities). The `_compile` dwarfdump's
    // `const VehicleSoaData &` for this one function is a dumper artifact.
    void SetHasEntity(bool lbHasEntity, u32 luVehicle, VehicleSoaData& lSoaData);
    Vector3 CalcTowBarPos(Matrix44Affine lTransform,
                          const VehicleTypeRuntime* lpVehicleTypeRuntime) const;
    Vector3 CalcFrontAxlePos(Matrix44Affine lTransform,
                            Vector3 lArticulationPoint,
                            const VehicleTypeRuntime* lpVehicleTypeRuntime) const;
    void UpdateMatrix(const VehicleAxles* lpAxles,
                      Matrix44Affine& lOutMatrix,
                      const VehicleTypeRuntime* lpVehicleTypeRuntime,
                      Vector3 lOldUp);

    // Read by BrnReplays::TrafficEntitySerialiser::SetVehicleData @0x82714060.
    u8  GetFlags() const { return mxFlags; }

    // Raw byte reads BrnReplays::TrafficVehicleData::SetFromVehicle @0x82713E38 performs.
    // Both are plain zero-extended byte reads with no IsAlive guard inside the getter, since
    // SetFromVehicle asserts IsAlive() at each use site.
    u8  GetVehicleType() const { return muVehicleType; }  // @+0
    u8  GetEffectState() const { return mxEffectState; }  // @+7

    // X360 @0x8270F928 asserts, then returns the index byte zero-extended (lbz with no extsb
    // @0x8270F99C): after SetNotPhysical stores -1 the binary hands back 255, not -1.
    s32 GetPhysicalPartsIndex() const
    {
        CGS_ASSERT(IsPhysical(), "IsPhysical()");
        CGS_ASSERT(miPhysicalPartsIndex >= 0, "miPhysicalPartsIndex >= 0");
        return static_cast<u8>(miPhysicalPartsIndex);
    }

    // Never called; defined in the .cpp so offsetof() reaches the private members below.
    static void _AssertLayout();

private:
    u8 muVehicleType;
    u8 muCrashTrafficType;
    u16 muOtherHalfIndex;
    u8 muSpecies;
    u8 mxFlags;
    s8 miPhysicalPartsIndex;
    u8 mxEffectState;
    f32 mfSwerveTime;
    u8 muHeadlightWarmth;
    u8 muIndicatorBulbWarmth;
    u8 muHeadlightFlashPattern;
    u8 muHeadlightFlashState;
    Vector4 mSpeed_DistAcrossLane_SwerveAmount_W;
    Vector4 mPitch_Roll_Steering_WheelRot;
    f32 mfHeadlightTimeToFlash;
    f32 mfIndicatorTimeToFlash;
    s8 miBrakelightState;
    s8 miPhysicalReason;
    s8 miManoeuvre;
    s8 miManoeuvrePhase;
    f32 mfRandomVal;
    EntityId mSympCrashTarget;
    f32 mfPhysicalTime;
    SympatheticCrashState meSympCrashState;
    f32 mfSympCrashTime;
    Vector3 mLinearVelocity;
    f32 mfManoeuvreTime;
    Vector3 mTargetPos;
};

// BrnTraffic::GetVehicleSpecies @0x821F4648. Classifies a GLOBAL traffic-vehicle index into
// the three pools the module lays out back to back, purely by range, with no per-vehicle read:
//     [0, 400)    -> E_SPECIES_STANDARD   (TrafficEntityModule::KU_MAX_STANDARD_TRAFFIC)
//     [400, 599)  -> E_SPECIES_STATIC     (400 + KU_MAX_STATIC_TRAFFIC(199) == 599)
//     599         -> E_SPECIES_TRAILER    (the single trailer slot)
// The X360 spells the last two arms branchlessly (`li r11,0x257 ; subfc ; subfe ; addi
// r11,r11,2`); de-optimised back to the range ladder. The console defines it in this header
// (its baked assert cites BrnTrafficVehicle.h:578) and also emitted an out-of-line copy, so
// header-inline matches both. The console's return type is a plain int.
inline Vehicle::Species GetVehicleSpecies(u32 luIndex)
{
    // BrnTrafficVehicle.h:578 -- the pool-wide bound (TrafficEntityModule::KU_MAX_TOTAL_TRAFFIC).
    CGS_ASSERT(luIndex < 0x258u, "luIndex < KU_MAX_TOTAL_TRAFFIC");

    if (luIndex < 0x190u)        // < 400
        return Vehicle::E_SPECIES_STANDARD;
    if (luIndex < 0x257u)        // < 599
        return Vehicle::E_SPECIES_STATIC;
    return Vehicle::E_SPECIES_TRAILER;
}
}
