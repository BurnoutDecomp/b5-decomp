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
class Hull;
class Param;
class VehicleTypeRuntime;
struct LaneRung;

// ---------------------------------------------------------------------------
// DWARF BrnTrafficVehicle.h:53 -- `const VecFloat KF_VEHICLE_UPDATE_MATRIX_OLD_UP_FACTOR`.
// The weight the ship-only `lOldUp` argument carries into Vehicle::UpdateMatrix's blended
// up-vector (X360 @0x82757768 loads the whole 16-byte lane register from .data 0x8300D190
// and feeds it to a single vmaddcfp128 -- see the derivation in BrnTrafficVehicle.cpp).
//
// VALUE RECOVERED 2026-08-21 (wave T1 consolidation). The earlier "no writer anywhere"
// claim was the export-scan blind spot: 0x8300D190 is seeded by an UNNAMED dyn-init thunk
// @0x82C67830 (not a function in the DB, invisible to every per-function export), which
// splats flt_820054CC = 20.0f across all four lanes. Defined in BrnTrafficVehicle.cpp.
// ---------------------------------------------------------------------------
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
    // finished vehicle transform (no lane intersection); the parked-car initialiser is its
    // only caller in the wave-1 chain.
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

    // ---- THE parked-car constructor. DWARF BrnTrafficVehicle.h:272, X360 @0x827567F0.
    // Parameter order/width read off the prologue, NOT the pseudocode: r3 this, r4 lpAxles,
    // r5 lOutMatrix, f1 lfRandomVal (its GPR slot r6 is RESERVED AND UNUSED -- the PPC
    // float-arg GPR skip), r7 luVehicleType, r8 lpVehicleTypeRuntime, r9 lpVehicleTypeUpdate,
    // r10 lTransform, then two 8-byte right-justified stack slots: luVehicle and
    // &lVehicleSoaData. Nine parameters, exactly the DWARF list.
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

    // ---- inline state predicates (X360 inlines these flag/species reads at every
    // call site; the bodies below assert against them by NAME). ----
    bool IsAlive() const { return (mxFlags & E_FLAG_ALIVE) != 0; }
    bool IsPhysical() const { return (mxFlags & E_FLAG_PHYSICAL) != 0; }
    bool IsOfTrailerSpecies() const { return (muSpecies & 0xF) == E_SPECIES_TRAILER; }
    // ⭐ ADDED 2026-08-21 (wave T1 round 4, item 2). The predicate BOTH of SetHasEntity's
    // first two asserts name by their own baked strings ("HasEntity() != lbHasEntity" /
    // "lSoaData.mVehiclesWithEntities.IsBitSet( luVehicle ) == HasEntity()",
    // BrnTrafficVehicle.h:979/:980) and the one CreateNewVehicleEntities @0x8272FA30 asserts
    // per candidate ("!lpVehicle->HasEntity()", BrnTrafficEntityModule.cpp:4648). The console
    // inlines it everywhere as `(mxFlags >> 1) & 1` -- e.g. 0x8270EB44 `lbz r11,5(r3) ;
    // extrwi r11,r11,1,30` -- i.e. E_FLAG_HASENTITY, which this enum already carries.
    // StaticVehicles_RemoveDeadParam (wT1_01) already open-codes the same test via
    // GetFlags(); it is left alone rather than churned.
    bool HasEntity() const { return (mxFlags & E_FLAG_HASENTITY) != 0; }
    Manoeuvre GetCurrentManoeuvre() const;

    // ⭐ ADDED 2026-08-21 (wave T1 round 4, item 2). DWARF BrnTrafficVehicle.h:339,
    // X360 @0x8270E4C8 -- 14 instructions:
    //     lbz r11,4(this) ; clrlwi r11,r11,28 ; cmplwi r11,2      -> IsOfTrailerSpecies()
    //     (on mismatch) FireAssert("IsOfTrailerSpecies()", BrnTrafficVehicle.h, 778)
    //     lhz r3,2(this)                                          -> muOtherHalfIndex
    // BrnTrafficEntityModule_Render.cpp:317-319 and WorldLinkStubs.cpp:734 BOTH record a
    // parked leg blocked on "Vehicle::GetCabIndex has no declaration anywhere in the tree".
    // That is now false; the two banners are corrected where they stand.
    // ⚠️ THE RETURN IS 16 BITS (`lhz`, not `lbz`): the sentinel its callers compare against
    // is KU_INVALID_VEHICLE == 0xFFFF, which a u8 could never hold.
    u16 GetCabIndex() const;

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
    s32 GetPhysicalReason() const;
    s32 GetCurrentManoeuvrePhase() const;
    EntityId GetSympatheticCrashTarget() const;
    f32 GetHeadlightWarmth() const;
    f32 GetIndicatorBulbWarmth() const;

    bool IsHornOn() const;
    bool IsAlarmOn() const;
    bool IsRightIndicatorOn() const;
    bool IsIndicatingLeft() const;
    bool IsIndicatingRight() const;
    bool IsFlashingHeadlights() const;
    bool AreHeadlightsFlashed() const;
    bool AreBrakelightsOn() const;
    bool IsCrashing() const;
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
    void StartGiveUpManoeuvre();
    void CopyEffectsFromCab(const Vehicle* lpCab);

    void SetPhysical(s8 liPartsIndex, u32 luVehicle, VehicleSoaData& lSoaData);
    void SetNotPhysical(u32 luVehicle, VehicleSoaData& lSoaData);
    void SetDead(u32 luVehicle, VehicleSoaData& lSoaData);

    // ⭐ ADDED 2026-08-21 (wave T1 round 4, item 2) -- the OTHER half of the scene-entity
    // registration CreateNewVehicleEntities @0x8272FA30 performs, and its ONLY blocker that
    // was a real declaration gap.
    //
    // ⚠️ THE LEDGER ROW IS A CATCH-ALL MISATTRIBUTION, exactly like
    // FindVehicleTypeAttribKey_EXPENSIVE was (round 3): progress/identity.json files
    // Vehicle::SetHasEntity under the GameShared/GameClasses/Development/CgsStrStream.h
    // primary_file, which is why two rounds of "not declared anywhere" parks. It is plainly a
    // Vehicle member: its four baked assert strings all cite
    // GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h (:979/:980/
    // :984/:985) and it reads/writes mxFlags at this+5.
    //
    // SIGNATURE from the DWARF (BrnTrafficVehicle.h:373 `void SetHasEntity(bool, uint32_t,
    // VehicleSoaData &)`) and confirmed register-for-register in the ARTIST prologue:
    //   r3 this, r4 lbHasEntity (`clrlwi r17,r4,24`), r5 luVehicle, r6 &lSoaData.
    // The soa reference is NON-const and that is not a style choice: the body STORES through
    // it (`stdx r10, r11, r15` with r15 == r6+0x50 == &mVehiclesWithEntities). The `_compile`
    // dwarfdump's `const VehicleSoaData &` spelling for this one function is a dumper
    // artifact contradicted both by the header dump above and by rung 1; the sibling setters
    // SetPhysical / SetNotPhysical / SetDead already take it non-const for the same reason.
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

    // Accessors used by BrnReplays::TrafficEntitySerialiser::SetVehicleData @0x82714060
    // (reads mxFlags at this+5 and the physical-parts index when E_FLAG_PHYSICAL is set).
    u8  GetFlags() const { return mxFlags; }

    // Raw byte reads BrnReplays::TrafficVehicleData::SetFromVehicle @0x82713E38 performs on a
    // live traffic vehicle: the vehicle TYPE byte @+0 (`lbz 0`) written verbatim into the
    // replay record's byte 0, and the effect-state byte @+7 (the a2[7] & 2/4/0x10 reads).
    // Both are plain zero-extended byte reads matching the asm (no IsAlive guard inside the
    // getter -- SetFromVehicle asserts IsAlive() explicitly at each use site).
    u8  GetVehicleType() const { return muVehicleType; }  // @+0
    u8  GetEffectState() const { return mxEffectState; }  // @+7 (raw mxEffectState byte)

    // X360 @0x8270F928 asserts, then returns the index byte zero-extended
    // (lbz with no extsb @0x8270F99C): after SetNotPhysical stores -1 the
    // binary hands back 255, not -1.
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

// ⭐ ADDED 2026-08-11 (driver-arms wave). BrnTraffic::GetVehicleSpecies @0x821F4648 (32 insns) --
// the ONE absent callee of PhysicalTrafficManager::UpdateTrafficDriver @0x825CA8A0, landed rather
// than parked because it is a genuine leaf (xrefs_from is the assert triple and nothing else).
//
// It classifies a GLOBAL traffic-vehicle index into the three pools the module lays out
// back-to-back, purely by range -- there is no per-vehicle read at all:
//     [0, 400)    -> E_SPECIES_STANDARD   (TrafficEntityModule::KU_MAX_STANDARD_TRAFFIC)
//     [400, 599)  -> E_SPECIES_STATIC     (400 + KU_MAX_STATIC_TRAFFIC(199) == 599)
//     599         -> E_SPECIES_TRAILER    (the single trailer slot GetTrailerVehicle asserts on)
// The X360 spells the last two arms branchlessly (`li r11,0x257 ; subfc ; subfe ; addi r11,r11,2`
// == `(idx >= 599) ? 2 : 1`); de-optimised back to the range ladder it came from.
//
// The console DEFINES it in this header (its baked assert cites BrnTrafficVehicle.h:578) and the
// compiler ALSO emitted an out-of-line copy at 0x821F4648 -- so header-inline here matches both.
// Returns Vehicle::Species; the console's return type is a plain int and the one call site
// (UpdateTrafficDriver) only tests it against zero.
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
