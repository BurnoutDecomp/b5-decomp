#ifndef BRN_UPDATE_VEHICLES_JOB_H
#define BRN_UPDATE_VEHICLES_JOB_H

// GameSource/Jobs/Traffic/UpdateVehiclesJob.h -- BrnTraffic::UpdateVehiclesJob, the per-vehicle
// kinematic driving worker. TrafficEntityModule::UpdateVehicles @0x82744F58 splits the param
// pool across TrafficJobStubs; each stub runs TrafficJobEntry @0x829172E0 -> TrafficJob::Execute
// @0x829174B0 -> UpdateVehiclesJob::Execute @0x8291DCC8, which walks
// [mpParams->muBeginVehicle, muEndVehicle) and drives each live vehicle one step.
//
// LAYOUT IS ASM-ONLY. The class is absent from the DecFIGS DWARF (on PS3 it is an SPU ELF,
// Traffic.h:52 _binary_Traffic_elf_start). Every member below is pinned by the X360 console
// displacements in Initialise @0x82919AD0 and MoveToNextVehicle @0x8291A1D0:
//   +0x000 mpParams          stw r30,0(r31)                      @0x82919B50
//   +0x004 mpCurrentVehicle             stw r10,4(r31)           @0x8291A4F8
//   +0x008 mpCurrentVehicleAxles        stw r11,8(r31)           @0x8291A564
//   +0x00C mpCurrentVehicleTypeRuntime  stw r10,0xC(r31)         @0x8291A590
//   +0x010 mpCurrentParam               stw r10,0x10(r31)        @0x8291A59C
//   +0x014 mpCurrentParamTransform      stw r11,0x14(r31)        @0x8291A5A8
//   +0x018 muCurrentVehicle  sth (u16!)                          @0x82919B70 / @0x8291A46C
//   +0x020 mCurrentVehicleTransform  four lanes copied in        @0x8291A518
//   +0x060 mEffectRand       6x ld/std from lpParams+0x40        @0x82919B9C
//   +0x090 muEffectTickRate  stb (u8)(mfSimTimeStep * 5000)      @0x82919B94
//   +0x0A0/0x0E0/0x120/0x160 four FuzzyEnvelopeSet4              @0x82919BB4..
//   +0x1A0..+0x210 eight tuning Vector4s                         @0x8291A070..@0x8291A1BC
// HOST-NATIVE: the console is 32-bit, so every cached pointer widens here. No console byte
// offset is reused as a host offset and nothing pins sizeof.
//
// The console's baked assert strings cite GameSource/Jobs/Traffic/UpdateVehiclesJob.cpp; the
// member order and the declaration order below follow those line numbers (GetHull :54,
// GetSection :94, GetCurrentParam :195, GetCurrentParamPlan :209, GetCurrentParamTransform
// :216, GetCurrentVehicle :230/:244, GetCurrentVehicleTransform :258, SetCurrentVehicleTransform
// :270, GetCurrentVehicleAxles :281, GetCurrentVehicleTypeRuntime :295, Initialise :360,
// MoveToNextVehicle :493, WriteBackCurrentVehicle :590, UpdateVehicle :633,
// FindInterestingRaceCar :796, CalcSwerveAmount :826, GetObjectEstimated... :996,
// ProcessSwervingRules :1081, UpdateSwerveState :1123, CalcTargetPos :1283, MoveToTarget :1341,
// PlaceVehicleOnRoad :1517, RequestNewPhysicalVehicle :1668).

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/Jobs/Traffic/TrafficCommon.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficMiscRuntimeClasses.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"
#include "SharedClasses/Traffic/BrnTrafficFuzzyEnvelopeSet.h"
#include "SharedClasses/Traffic/BrnTrafficHull.h"
#include "SharedClasses/Traffic/BrnTrafficSection.h"

namespace BrnTraffic
{

class UpdateVehiclesJob
{
public:
    // The four envelope sets Initialise @0x82919AD0 Constructs at console +0xA0/+0xE0/+0x120/
    // +0x160 and fills with 4/3/2/1 envelopes. Set roles come from which input each one scores
    // in ProcessSwervingRules @0x82918828 (set 2 <- v1 = signed distance, set 0 <- v2 = closing
    // speed) and are corroborated by the corner values and by the CalcScore gate @0x8291D0AC.
    enum SwerveEnvelopeSet
    {
        E_SWERVE_ENVELOPES_CLOSING_SPEED = 0,
        E_SWERVE_ENVELOPES_LANE_POS      = 1,
        E_SWERVE_ENVELOPES_DISTANCE      = 2,
        E_SWERVE_ENVELOPES_ANGLE         = 3,
        E_SWERVE_ENVELOPE_SETS_COUNT     = 4
    };

    // The eight tuning Vector4s Initialise builds at console +0x1A0..+0x210. They are read
    // lane-wise (lvsl/vperm) by MoveToTarget, CalcTargetPos, CalcSwerveAmount,
    // FindInterestingRaceCar and UpdateSwerveState; the lane values are recovered in
    // BrnUpdateVehiclesJob.cpp beside the seeding code.
    enum TuningVector
    {
        E_TUNE_LIMITS       = 0,   // +0x1A0 { MAX_FLOAT, KF_TWO_PI, 4.0, 0.1 }
        E_TUNE_SWERVE       = 1,   // +0x1B0 { 1.25, 0.5, 2.0, 2.0 }
        E_TUNE_LANE         = 2,   // +0x1C0 { 2.5, 0.4, 4.5, 0.0 }
        E_TUNE_ANGLES       = 3,   // +0x1D0 { sin 10deg, sin 35deg, 0.2, 0.025 }
        E_TUNE_LANE_CENTRE  = 4,   // +0x1E0 { 0.7, 0.9, 0.25, 1.3 }
        E_TUNE_FILTERS      = 5,   // +0x1F0 { recip roll speed min, -0.1, dt*dt*360, 0.2 }
        E_TUNE_TIME         = 6,   // +0x200 { 0.95, 0.05, mfSimTimeStep, mfSimTimeSinceLastDecision }
        E_TUNE_MOVE         = 7,   // +0x210 { 0.15, 0.0, 0.125, 15.0 }
        E_TUNE_VECTOR_COUNT = 8
    };

    // @0x8291DCC8. The console passes a third argument in r5 (TrafficJob + 0x300); neither
    // Execute nor Initialise reads it, so it is not modelled.
    void Execute(UpdateVehiclesJobParams* lpParams);

    // @0x829177A0 (:54) / @0x829178A8 (:94)
    Hull* GetHull(u32 luHull) const;
    const Section* GetSection(const Hull* lpHull, u32 luSection) const;

    // @0x82917B18 (:195) / @0x82917C78 (:209) / @0x82917D18 (:216)
    const Param* GetCurrentParam() const;
    const ParamPlan* GetCurrentParamPlan(u32 luPlan) const;
    const ParamTransform* GetCurrentParamTransform() const;

    // @0x82917E78 (:230) and its const overload @0x82917FD8 (:244), which IDA leaves unnamed
    // as sub_82917FD8.
    Vehicle* GetCurrentVehicle();
    const Vehicle* GetCurrentVehicle() const;

    // @0x82919488 (:258) returns BY VALUE through an sret buffer, after asserting
    // IsValid(mCurrentVehicleTransform). @0x829197A8 (:270) asserts IsValid(lTransform).
    Matrix44Affine GetCurrentVehicleTransform() const;
    void SetCurrentVehicleTransform(const Matrix44Affine& lTransform);

    // @0x82918138 (:281) / @0x82918298 (:295, IDA name truncated to GetCurrentVehicleRunti)
    VehicleAxles* GetCurrentVehicleAxles() const;
    const VehicleTypeRuntime* GetCurrentVehicleTypeRuntime() const;

    void Initialise(UpdateVehiclesJobParams* lpParams);   // @0x82919AD0 (:360)
    bool MoveToNextVehicle();                             // @0x8291A1D0 (:493)
    void WriteBackCurrentVehicle();                       // @0x82918468 (:590)

    void UpdateVehicle();                                 // @0x8291D810 (:633)

    // @0x8291B258 (:796). Five out-parameters in r4..r8, in the console's register order.
    // Returns false (and seeds lrfOutDistanceSq with E_TUNE_LIMITS.x == MAX_FLOAT) when the
    // cached race-car list is empty.
    bool FindInterestingRaceCar(Vector3& lrOutPosition,
                                Vector3& lrOutLinearVelocity,
                                Vector3& lrOutDirection,
                                VecFloat& lrOutSpeed,
                                VecFloat& lrOutDistanceSq) const;

    // @0x8291CF18 (:826/:827). The bool out-parameter names are the console's baked assert
    // strings; they arrive in r4 / r5 with the four vectors in v1..v4.
    VecFloat CalcSwerveAmount(Vector3 lRaceCarPosition,
                              Vector3 lRaceCarVelocity,
                              Vector3 lRaceCarDirection,
                              VecFloat lfRaceCarSpeed,
                              bool* lpbOutIsExtreme,
                              bool* lpbOutIsNormalPhysical);

    // @0x8291B508 (:996). Leak key BrnTrafficEntityModule.cpp:5364.
    VecFloat GetObjectEstimatedDistanceToLaneCenterAccordingToParam(Vector3 lObjectPos,
                                                                    Vector3& lrOutPredictedPos) const;

    // @0x82918748 (:1081). r4 is a TWO-element output array; r5 / r6 are the two bools; the
    // four scores arrive in v1..v4 and are matched to maSwerveEnvelopes[2], [0], [1], [3] in
    // that order (0x8291880C..0x82918888).
    void ProcessSwervingRules(VecFloat* lpafOutputs,
                              bool lbIsExtremeSwerving,
                              bool lbAllowsHardcoreSwerving,
                              VecFloat lfDistance,
                              VecFloat lfClosingSpeed,
                              VecFloat lfLanePos,
                              VecFloat lfAngleDot) const;

    // @0x8291B870 (:1123/:1124). The race-car position arrives in v1, the two out-parameters
    // in r4 / r5; both names are baked assert strings.
    void UpdateSwerveState(Vector3 lRaceCarPosition,
                           f32* lpfUpdatedSwerveAmount,
                           bool* lpbUpdatedExtremeSwerve);

    // @0x8291A5E0 (:1283). lfSwerve arrives in v1.
    void CalcTargetPos(VecFloat lfSwerve, Vector3& lrOutTargetPos, bool lbPartialUpdate) const;

    // @0x8291BEE0 (:1341). Bodied on its own in BrnUpdateVehiclesJob_MoveToTarget.cpp.
    // lTargetPos arrives in v1, lbPartialUpdate in r4.
    void MoveToTarget(Vector3 lTargetPos, bool lbPartialUpdate);

    void PlaceVehicleOnRoad();                            // @0x82918A20 (:1517)

    // @0x8291CE48 (:1668). GATED -- no reader of GetNewPhysicalRequests() exists yet.
    void RequestNewPhysicalVehicle(u16 luVehicle, PhysicalReason leReason, EntityId lTargetEntityId);

    // @0x8291A8C0 (:approx 700). EXPORT HOLE -- no per-function JSON exists at that address
    // (the export set jumps 0x8291A5E0 -> 0x8291AC60). Declared so UpdateVehicle can name its
    // gate; the body logs and returns.
    void UpdateEffects(const Param* lpParam);

    // @0x829179A8 (:approx 110). No per-function export exists (the set jumps 0x829178A8 ->
    // 0x82917B18), but the call contract is pinned: the result feeds Section::
    // CalcDistanceAlongSection and CalcParamFromStartParamAndDistanceAlongSection, both of
    // which take the SECTION-LOCAL slice, so the body is the Hull twin.
    const f32* GetRungLengthsForSection(const Hull* lpHull, const Section* lpSection) const;

private:
    UpdateVehiclesJobParams*  mpParams;                     // +0x000
    Vehicle*                  mpCurrentVehicle;             // +0x004
    VehicleAxles*             mpCurrentVehicleAxles;        // +0x008
    const VehicleTypeRuntime* mpCurrentVehicleTypeRuntime;  // +0x00C
    const Param*              mpCurrentParam;               // +0x010
    const ParamTransform*     mpCurrentParamTransform;      // +0x014
    u16                       muCurrentVehicle;             // +0x018 (sth, not stw)
    Matrix44Affine            mCurrentVehicleTransform;     // +0x020
    CgsNumeric::Random        mEffectRand;                  // +0x060
    u8                        muEffectTickRate;             // +0x090
    FuzzyEnvelopeSet4         maSwerveEnvelopes[E_SWERVE_ENVELOPE_SETS_COUNT]; // +0x0A0
    Vector4                   maTuning[E_TUNE_VECTOR_COUNT];                   // +0x1A0
};

} // namespace BrnTraffic

#endif // BRN_UPDATE_VEHICLES_JOB_H
