// GameSource/Jobs/Traffic/UpdateVehiclesJob.cpp -- BrnTraffic::UpdateVehiclesJob, everything
// except MoveToTarget (which owns BrnUpdateVehiclesJob_MoveToTarget.cpp).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The class is absent from the DecFIGS DWARF, so
// the Feb-2007 leak's TrafficEntityModule::UpdateVehicles_* family is the structural key and
// the asm decides every divergence. Every member offset comes from the raw assembly listing,
// never from the pseudocode (Hex-Rays renders this class's member reads in two incompatible
// index forms inside MoveToNextVehicle alone).
//
// PARTIAL. Gated legs, each with its blocker:
//   UpdateEffects                                          @0x8291A8C0  EXPORT HOLE
//   CalcSwerveAmount's intersection refinement             @0x8291D644  needs
//     BrnTraffic::GetLineLineIntersectionParamXZ @0x8291AC60 (BrnTrafficMathsUtils.h, not ours)
//   UpdateVehicle's partial-update latch into Vehicle+4    @0x8291D9E4  needs a Vehicle
//     accessor for muSpecies bit 7 (BrnTrafficVehicle.h, not ours)
//   RequestNewPhysicalVehicle                              @0x8291CE48  GATED (no consumer)

#include "GameSource/Jobs/Traffic/BrnUpdateVehiclesJob.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h"   // KU_INVALID_HULL
#include "SharedClasses/Traffic/BrnTrafficSharedConstants.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/vector4_operation.h"

#include <cmath>
#include <cstdlib>   // getenv

namespace BrnTraffic
{
namespace
{
    // NAMED LEG GATE + diag plumbing, the shape the mounted traffic partfiles use.
    // [DIAG] NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    inline void LogMissingLeg(bool& lrbAlreadyLogged, const char* lpcLegNameAndReason)
    {
        if (lrbAlreadyLogged)
        {
            return;
        }
        lrbAlreadyLogged = true;

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[T2-job-leg] UpdateVehiclesJob leg NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndReason << " [FLAG PC partial gate]\n";
        }
    }

    bool TrafficDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
        return sbEnabled;
    }

    CgsDev::Log::DebugPrint* TrafficDiagStream()
    {
        if (!TrafficDiagEnabled() || CgsDev::Log::gpDebugPrint == 0)
        {
            return 0;
        }
        return CgsDev::Log::gpDebugPrint;
    }

    // RequestNewPhysicalVehicle @0x8291CE48 only appends to mpOutNewPhysicalRequests, and
    // nothing in the tree reads TrafficJobStub::GetNewPhysicalRequests().
    // DELETE-WHEN the physical-traffic wave lands that consumer.
    const bool KB_T2_ALLOW_PHYSICAL_PROMOTION = false;

    // ---- console rodata, dumped out of BURNOUT_X360_ARTIST.XEX.i64 -----------------------
    // Initialise's envelope corners. Every one is a plain .rdata float; the module-side twin
    // Fuzzy::FuzzyBehaviourLogic::ResetToDefaults @0x8275D500 reads the same pool.
    const f32 KF_MINUS_1000_1 = -1000.1f;   // flt_820C26D4
    const f32 KF_MINUS_1000_0 = -1000.0f;   // flt_8200D4F8
    const f32 KF_PLUS_1000_0  =  1000.0f;   // flt_82009E10
    const f32 KF_PLUS_1000_1  =  1000.1f;   // flt_820C26D0

    // Initialise's eight tuning vectors, lane by lane.
    const f32 KF_MAX_FLOAT              = 3.40282347e+38f; // flt_821007CC
    const f32 KF_TWO_PI                 = 6.28318548f;     // flt_821007E0
    const f32 KF_TUNE_LIMITS_Z          = 4.0f;            // flt_82004EF4
    const f32 KF_TUNE_LIMITS_W          = 0.1f;            // flt_82004014
    const f32 KF_TUNE_SWERVE_X          = 1.25f;           // flt_820092CC
    const f32 KF_TUNE_SWERVE_Y          = 0.5f;            // flt_82001DA0
    const f32 KF_TUNE_SWERVE_Z          = 2.0f;            // flt_82100830
    const f32 KF_TUNE_SWERVE_W          = 2.0f;            // flt_82100830
    const f32 KF_TUNE_LANE_X            = 2.5f;            // flt_82100838
    const f32 KF_TUNE_LANE_Y            = 0.4f;            // flt_82100848
    const f32 KF_APPROX_LANE_WIDTH      = 4.5f;            // flt_8210086C
    const f32 KF_ZERO                   = 0.0f;            // flt_82001CC0
    const f32 KF_SWERVE_ANGLE_NARROW    = 0.17453292f;     // sin(dbl_8200D500 == 10 deg)
    const f32 KF_SWERVE_ANGLE_WIDE      = 0.61086523f;     // dbl_82100D60 == 35 deg (sin below)
    const f32 KF_TUNE_ANGLES_Z          = 0.2f;            // flt_82004744
    const f32 KF_TUNE_ANGLES_W          = 0.025f;          // flt_82100834
    const f32 KF_MAX_DIST_ACROSS_LANE   = 0.7f;            // flt_82100814
    const f32 KF_STOPLINE_SIDE_SPACE    = 0.9f;            // flt_8210084C
    const f32 KF_STOPLINE_SIDE_VARIATION= 0.25f;           // flt_82100850
    const f32 KF_MAX_DIST_FROM_LANE_CENTRE = 1.3f;         // flt_82100870
    // flt_831BBC1C is .data seeded by the dyn-init thunk @0x82C71664:
    //   1.0f (flt_82001C98) / (0.44704f (flt_82F31928, mph->m/s) * 10.0f (flt_82004A20))
    // i.e. the reciprocal of 10 mph in m/s -- the leak's KF_VEHICLE_RECIP_ROLL_SPEED_MIN.
    const f32 KF_RECIP_ROLL_SPEED_MIN   = 1.0f / (0.44704f * 10.0f);
    const f32 KF_TUNE_FILTERS_Y         = -0.1f;           // flt_82100858
    const f32 KF_SIM_TIMESTEP_SQ_SCALE  = 360.0f;          // flt_8210085C (== flt_820BA570)
    const f32 KF_TUNE_FILTERS_W         = 0.2f;            // flt_82004744
    const f32 KF_TUNE_TIME_X            = 0.95f;           // flt_82100868
    const f32 KF_TUNE_TIME_Y            = 0.05f;           // flt_82100864
    const f32 KF_TUNE_MOVE_X            = 0.15f;           // flt_82004E58
    const f32 KF_RECIP_LANE_CENTRE_SPEED= 0.125f;          // flt_82004010
    const f32 KF_TUNE_MOVE_W            = 15.0f;           // flt_820047C4

    // Initialise @0x82919B84: (u8)(mfSimTimeStep * 5000.0f), the effect-rand tick rate.
    const f32 KF_EFFECT_TICK_RATE_SCALE = 5000.0f;         // flt_82100880

    // UpdateVehicle @0x8291D810: the squared distance from mBehaviourCentre past which a
    // vehicle updates in PARTIAL mode. flt_82F36B40 == 22500 == 150 m squared.
    const f32 KF_PARTIAL_UPDATE_RADIUS_SQ = 22500.0f;      // flt_82F36B40

    // The default race-car direction FindInterestingRaceCar seeds (unk_82181520 == unit Z).
    inline Vector3 UnitZ() { return Vector3{ 0.0f, 0.0f, 1.0f, 0.0f }; }
    inline Vector3 ZeroV3() { return Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }; }
    inline bool IsValidF(f32 lfValue) { return lfValue == lfValue; }

    inline f32 ClampF(f32 lfValue, f32 lfMin, f32 lfMax)
    {
        return lfValue < lfMin ? lfMin : (lfValue > lfMax ? lfMax : lfValue);
    }

    // rw::math::fpu::Sgn @0x8291D278 / 0x8291B9CC: fsel pair, so exact zero gives zero.
    inline f32 SgnF(f32 lfValue)
    {
        return lfValue == 0.0f ? 0.0f : (lfValue >= 0.0f ? 1.0f : -1.0f);
    }

    // CalcSwerveAmount @0x8291D384 / UpdateSwerveState @0x8291BAC4..0x8291BE60 rodata.
    const f32 KF_SWERVE_RULE_EPSILON     = 0.01f;         // flt_82100A6C
    const f32 KF_SWERVE_REVERSE_DELAY    = -0.2f;         // flt_82020A84
    const f32 KF_SWERVE_HOLD_TIME        = 0.5f;          // flt_82001DA0
    const f32 KF_SWERVE_EPSILON          = 1.1920929e-07f;// flt_82100800
    const f32 KF_SWERVE_LOCKOUT_PER_METRE= 0.0166666675f; // flt_820139F8
}

// ---------------------------------------------------------------------------------------
// Accessors. Each reproduces the console's bounds asserts in asm order; the baked strings
// are quoted verbatim.
// ---------------------------------------------------------------------------------------

// @0x829177A0 (UpdateVehiclesJob.cpp:54/:69)
Hull* UpdateVehiclesJob::GetHull(u32 luHull) const
{
    CGS_ASSERT(luHull < mpParams->muNumHulls, "luHull < mpParams->muNumHulls");
    CGS_ASSERT(mpParams->mpapHulls, "mpParams->mpapHulls");
    return mpParams->mpapHulls[luHull];
}

// @0x829178A8 (:94/:95). The console spells the element step as 48 * luSection + mpaSections.
const Section* UpdateVehiclesJob::GetSection(const Hull* lpHull, u32 luSection) const
{
    CGS_ASSERT(lpHull, "lpHull");
    CGS_ASSERT(luSection < lpHull->muNumSections, "luSection < lpHull->muNumSections");
    return lpHull->mpaSections + luSection;
}

// @0x82917B18 (:195/:196/:201)
const Param* UpdateVehiclesJob::GetCurrentParam() const
{
    CGS_ASSERT(muCurrentVehicle >= mpParams->muBeginVehicle,
               "muCurrentVehicle >= mpParams->muBeginVehicle");
    CGS_ASSERT(muCurrentVehicle < mpParams->muEndVehicle,
               "muCurrentVehicle < mpParams->muEndVehicle");
    CGS_ASSERT(mpCurrentParam, "mpCurrentParam");
    return mpCurrentParam;
}

// @0x82917C78 (:209). The console returns GetCurrentParam() + 6*luPlan + 8, i.e.
// &maPlans[luPlan] with the 6-byte ParamPlan stride.
const ParamPlan* UpdateVehiclesJob::GetCurrentParamPlan(u32 luPlan) const
{
    CGS_ASSERT(luPlan < KU_PARAM_NUM_PLANS, "luPlan < KU_PARAM_NUM_PLANS");
    return GetCurrentParam()->maPlans + luPlan;
}

// @0x82917D18 (:216/:217/:222)
const ParamTransform* UpdateVehiclesJob::GetCurrentParamTransform() const
{
    CGS_ASSERT(muCurrentVehicle >= mpParams->muBeginVehicle,
               "muCurrentVehicle >= mpParams->muBeginVehicle");
    CGS_ASSERT(muCurrentVehicle < mpParams->muEndVehicle,
               "muCurrentVehicle < mpParams->muEndVehicle");
    CGS_ASSERT(mpCurrentParamTransform, "mpCurrentParamTransform");
    return mpCurrentParamTransform;
}

// @0x82917E78 (:230/:231/:236)
Vehicle* UpdateVehiclesJob::GetCurrentVehicle()
{
    CGS_ASSERT(muCurrentVehicle >= mpParams->muBeginVehicle,
               "muCurrentVehicle >= mpParams->muBeginVehicle");
    CGS_ASSERT(muCurrentVehicle < mpParams->muEndVehicle,
               "muCurrentVehicle < mpParams->muEndVehicle");
    CGS_ASSERT(mpCurrentVehicle, "mpCurrentVehicle");
    return mpCurrentVehicle;
}

// @0x82917FD8 (:244/:245/:250) -- the const overload, which IDA leaves as sub_82917FD8.
const Vehicle* UpdateVehiclesJob::GetCurrentVehicle() const
{
    CGS_ASSERT(muCurrentVehicle >= mpParams->muBeginVehicle,
               "muCurrentVehicle >= mpParams->muBeginVehicle");
    CGS_ASSERT(muCurrentVehicle < mpParams->muEndVehicle,
               "muCurrentVehicle < mpParams->muEndVehicle");
    CGS_ASSERT(mpCurrentVehicle, "mpCurrentVehicle");
    return mpCurrentVehicle;
}

// @0x82919488 (:258/:259/:262). Returns by value through an sret buffer.
Matrix44Affine UpdateVehiclesJob::GetCurrentVehicleTransform() const
{
    CGS_ASSERT(muCurrentVehicle >= mpParams->muBeginVehicle,
               "muCurrentVehicle >= mpParams->muBeginVehicle");
    CGS_ASSERT(muCurrentVehicle < mpParams->muEndVehicle,
               "muCurrentVehicle < mpParams->muEndVehicle");
    CGS_ASSERT(rw::math::vpu::IsValid(mCurrentVehicleTransform),
               "IsValid( mCurrentVehicleTransform )");
    return mCurrentVehicleTransform;
}

// @0x829197A8 (:270/:271/:274)
void UpdateVehiclesJob::SetCurrentVehicleTransform(const Matrix44Affine& lTransform)
{
    CGS_ASSERT(muCurrentVehicle >= mpParams->muBeginVehicle,
               "muCurrentVehicle >= mpParams->muBeginVehicle");
    CGS_ASSERT(muCurrentVehicle < mpParams->muEndVehicle,
               "muCurrentVehicle < mpParams->muEndVehicle");
    CGS_ASSERT(rw::math::vpu::IsValid(lTransform), "IsValid( lTransform )");
    mCurrentVehicleTransform = lTransform;
}

// @0x82918138 (:281/:282/:287)
VehicleAxles* UpdateVehiclesJob::GetCurrentVehicleAxles() const
{
    CGS_ASSERT(muCurrentVehicle >= mpParams->muBeginVehicle,
               "muCurrentVehicle >= mpParams->muBeginVehicle");
    CGS_ASSERT(muCurrentVehicle < mpParams->muEndVehicle,
               "muCurrentVehicle < mpParams->muEndVehicle");
    CGS_ASSERT(mpCurrentVehicleAxles, "mpCurrentVehicleAxles");
    return mpCurrentVehicleAxles;
}

// @0x82918298 (:295/:296/:297/:302)
const VehicleTypeRuntime* UpdateVehiclesJob::GetCurrentVehicleTypeRuntime() const
{
    CGS_ASSERT(muCurrentVehicle >= mpParams->muBeginVehicle,
               "muCurrentVehicle >= mpParams->muBeginVehicle");
    CGS_ASSERT(muCurrentVehicle < mpParams->muEndVehicle,
               "muCurrentVehicle < mpParams->muEndVehicle");
    CGS_ASSERT(GetCurrentVehicle()->IsAlive(), "GetCurrentVehicle()->IsAlive()");
    CGS_ASSERT(mpCurrentVehicleTypeRuntime, "mpCurrentVehicleTypeRuntime");
    return mpCurrentVehicleTypeRuntime;
}

// @0x829179A8 (:approx 110). No per-function export, but the call contract pins the body:
// 0x8291B508 hands the result to Section::CalcDistanceAlongSection and
// CalcParamFromStartParamAndDistanceAlongSection, both of which index the SECTION-LOCAL slice.
const f32* UpdateVehiclesJob::GetRungLengthsForSection(const Hull* lpHull,
                                                       const Section* lpSection) const
{
    CGS_ASSERT(lpHull, "lpHull");
    CGS_ASSERT(lpSection, "lpSection");
    return lpHull->GetRungLengthsForSection(lpSection);
}

// ---------------------------------------------------------------------------------------
// Setup + iteration
// ---------------------------------------------------------------------------------------

// @0x82919AD0 (:360). Seeds the whole job: params pointer, the pre-decremented cursor, the
// effect random, the four swerve envelope sets and the eight tuning vectors, then clears the
// five per-vehicle caches. Store order below is the asm's.
void UpdateVehiclesJob::Initialise(UpdateVehiclesJobParams* lpParams)
{
    CGS_ASSERT(lpParams, "lpParams");

    mpParams = lpParams;

    // 0x82919B58..0x82919B70: the cursor starts one BEFORE muBeginVehicle so the first
    // MoveToNextVehicle lands on it. The console stores it as a halfword, so the wrap at
    // muBeginVehicle == 0 is a u16 wrap.
    muCurrentVehicle = static_cast<u16>(lpParams->muBeginVehicle - 1);

    // 0x82919B78..0x82919B94
    muEffectTickRate = static_cast<u8>(lpParams->mfSimTimeStep * KF_EFFECT_TICK_RATE_SCALE);

    // 0x82919B9C: the 6x ld/std copy of the 48-byte Random.
    mEffectRand = lpParams->mEffectRand;

    for (u32 luSet = 0; luSet < E_SWERVE_ENVELOPE_SETS_COUNT; ++luSet)
    {
        maSwerveEnvelopes[luSet].Construct();
    }

    // 0x82919BE0..0x82919FEC. SetEnvelope(index, attackStart, attackStop, decayStart,
    // decayStop) -- the four scalars arrive splatted in v1..v4 in that order.
    // 0x82919C48/0x82919CB4/0x82919D20/0x82919D8C -- all four r3 = r29 = this+0xA0 (set 0).
    FuzzyEnvelopeSet4& lrClosingSpeed = maSwerveEnvelopes[E_SWERVE_ENVELOPES_CLOSING_SPEED];
    lrClosingSpeed.SetEnvelope(0, rw::math::vpu::Splat(-3.0f), rw::math::vpu::Splat(-4.0f), rw::math::vpu::Splat(KF_MINUS_1000_0), rw::math::vpu::Splat(KF_MINUS_1000_1));
    lrClosingSpeed.SetEnvelope(1, rw::math::vpu::Splat(-3.5f), rw::math::vpu::Splat(-2.0f), rw::math::vpu::Splat(-0.5f), rw::math::vpu::Splat(0.5f));
    lrClosingSpeed.SetEnvelope(2, rw::math::vpu::Splat(KF_ZERO), rw::math::vpu::Splat(3.0f), rw::math::vpu::Splat(KF_PLUS_1000_0), rw::math::vpu::Splat(KF_PLUS_1000_1));
    lrClosingSpeed.SetEnvelope(3, rw::math::vpu::Splat(10.0f), rw::math::vpu::Splat(20.0f), rw::math::vpu::Splat(75.0f), rw::math::vpu::Splat(95.0f));

    FuzzyEnvelopeSet4& lrLanePos = maSwerveEnvelopes[E_SWERVE_ENVELOPES_LANE_POS];
    lrLanePos.SetEnvelope(0, rw::math::vpu::Splat(-2.5f), rw::math::vpu::Splat(-1.5f), rw::math::vpu::Splat(1.5f), rw::math::vpu::Splat(2.5f));
    lrLanePos.SetEnvelope(1, rw::math::vpu::Splat(-8.5f), rw::math::vpu::Splat(-7.0f), rw::math::vpu::Splat(7.0f), rw::math::vpu::Splat(8.5f));
    lrLanePos.SetEnvelope(2, rw::math::vpu::Splat(-3.5f), rw::math::vpu::Splat(-2.0f), rw::math::vpu::Splat(2.0f), rw::math::vpu::Splat(3.5f));

    // 0x82919F24/0x82919F80 -- both r3 = r27 = this+0x120 (set 2). Envelope 0 is the whole
    // swerve gate CalcSwerveAmount tests at 0x8291D16C.
    FuzzyEnvelopeSet4& lrDistance = maSwerveEnvelopes[E_SWERVE_ENVELOPES_DISTANCE];
    lrDistance.SetEnvelope(0, rw::math::vpu::Splat(-25.0f), rw::math::vpu::Splat(-15.0f), rw::math::vpu::Splat(80.0f), rw::math::vpu::Splat(110.0f));
    lrDistance.SetEnvelope(1, rw::math::vpu::Splat(-3.0f), rw::math::vpu::Splat(KF_ZERO), rw::math::vpu::Splat(50.0f), rw::math::vpu::Splat(65.0f));

    maSwerveEnvelopes[E_SWERVE_ENVELOPES_ANGLE]
        .SetEnvelope(0, rw::math::vpu::Splat(-10.0f), rw::math::vpu::Splat(-1.0f), rw::math::vpu::Splat(-0.96499997f), rw::math::vpu::Splat(-0.90600002f));

    // 0x8291A070..0x8291A1BC -- the eight tuning vectors, built in three stack blocks and
    // stored at console +0x1A0..+0x210.
    maTuning[E_TUNE_LIMITS] = Vector4{ KF_MAX_FLOAT, KF_TWO_PI, KF_TUNE_LIMITS_Z, KF_TUNE_LIMITS_W };
    maTuning[E_TUNE_SWERVE] = Vector4{ KF_TUNE_SWERVE_X, KF_TUNE_SWERVE_Y,
                                      KF_TUNE_SWERVE_Z, KF_TUNE_SWERVE_W };
    maTuning[E_TUNE_LANE]   = Vector4{ KF_TUNE_LANE_X, KF_TUNE_LANE_Y, KF_APPROX_LANE_WIDTH, KF_ZERO };

    // 0x8291A088/0x8291A09C: two runtime sin() calls on the 35 deg / 10 deg doubles.
    maTuning[E_TUNE_ANGLES] = Vector4{ static_cast<f32>(std::sin(KF_SWERVE_ANGLE_NARROW)),
                                      static_cast<f32>(std::sin(KF_SWERVE_ANGLE_WIDE)),
                                      KF_TUNE_ANGLES_Z, KF_TUNE_ANGLES_W };

    maTuning[E_TUNE_LANE_CENTRE] = Vector4{ KF_MAX_DIST_ACROSS_LANE, KF_STOPLINE_SIDE_SPACE,
                                           KF_STOPLINE_SIDE_VARIATION,
                                           KF_MAX_DIST_FROM_LANE_CENTRE };

    // Lane z is rebuilt from the params every Initialise: mfSimTimeStep^2 * 360.
    maTuning[E_TUNE_FILTERS] = Vector4{ KF_RECIP_ROLL_SPEED_MIN, KF_TUNE_FILTERS_Y,
                                       lpParams->mfSimTimeStep * lpParams->mfSimTimeStep
                                           * KF_SIM_TIMESTEP_SQ_SCALE,
                                       KF_TUNE_FILTERS_W };

    maTuning[E_TUNE_TIME] = Vector4{ KF_TUNE_TIME_X, KF_TUNE_TIME_Y,
                                    lpParams->mfSimTimeStep,
                                    lpParams->mfSimTimeSinceLastDecision };

    maTuning[E_TUNE_MOVE] = Vector4{ KF_TUNE_MOVE_X, KF_ZERO,
                                    KF_RECIP_LANE_CENTRE_SPEED, KF_TUNE_MOVE_W };

    // 0x8291A198..0x8291A1B0
    mpCurrentVehicle            = 0;
    mpCurrentVehicleAxles       = 0;
    mpCurrentVehicleTypeRuntime = 0;
    mpCurrentParam              = 0;
    mpCurrentParamTransform     = 0;
}

// @0x8291A1D0 (:493..:503). Advance the cursor and re-seat the five per-vehicle caches plus
// the copied-in transform. Returns false once the cursor reaches muEndVehicle.
bool UpdateVehiclesJob::MoveToNextVehicle()
{
    CGS_ASSERT(mpParams->mpaVehicles, "mpParams->mpaVehicles");
    CGS_ASSERT(mpParams->mpaVehicleTransforms, "mpParams->mpaVehicleTransforms");
    CGS_ASSERT(mpParams->mpaVehicleAxles, "mpParams->mpaVehicleAxles");
    CGS_ASSERT(mpParams->mpaVehicleRuntimeData, "mpParams->mpaVehicleRuntimeData");
    CGS_ASSERT(mpParams->mpaParams, "mpParams->mpaParams");
    CGS_ASSERT(mpParams->mpaParamTransforms, "mpParams->mpaParamTransforms");

    muCurrentVehicle = static_cast<u16>(muCurrentVehicle + 1);
    CGS_ASSERT(muCurrentVehicle >= mpParams->muBeginVehicle,
               "muCurrentVehicle >= mpParams->muBeginVehicle");

    if (muCurrentVehicle >= mpParams->muEndVehicle)
    {
        return false;
    }

    // The pool arrays are indexed by the GLOBAL vehicle index; the console strides them
    // 128 / 64 / 128 / 64 bytes, which is sizeof of each element on the host too.
    mpCurrentVehicle = mpParams->mpaVehicles + muCurrentVehicle;

    if (!mpCurrentVehicle->IsAlive())
    {
        // A dead slot still advances the cursor and still returns true; only the derived
        // caches are cleared (0x8291A5B4).
        mpCurrentVehicleAxles       = 0;
        mpCurrentVehicleTypeRuntime = 0;
        mpCurrentParam              = 0;
        mpCurrentParamTransform     = 0;
        return true;
    }

    mCurrentVehicleTransform    = mpParams->mpaVehicleTransforms[muCurrentVehicle];
    mpCurrentVehicleAxles       = mpParams->mpaVehicleAxles + muCurrentVehicle;
    mpCurrentVehicleTypeRuntime = mpParams->mpaVehicleRuntimeData
                                      + mpCurrentVehicle->GetVehicleType();
    mpCurrentParam              = mpParams->mpaParams + muCurrentVehicle;
    mpCurrentParamTransform     = mpParams->mpaParamTransforms + muCurrentVehicle;
    return true;
}

// @0x82918468 (:590..:612). Publish the working transform back into the shared pool.
void UpdateVehiclesJob::WriteBackCurrentVehicle()
{
    CGS_ASSERT(mpParams->mpaVehicles, "mpParams->mpaVehicles");
    CGS_ASSERT(mpParams->mpaVehicleTransforms, "mpParams->mpaVehicleTransforms");
    CGS_ASSERT(mpParams->mpaVehicleAxles, "mpParams->mpaVehicleAxles");
    CGS_ASSERT(mpParams->mpaVehicleRuntimeData, "mpParams->mpaVehicleRuntimeData");
    CGS_ASSERT(muCurrentVehicle >= mpParams->muBeginVehicle,
               "muCurrentVehicle >= mpParams->muBeginVehicle");
    CGS_ASSERT(muCurrentVehicle < mpParams->muEndVehicle,
               "muCurrentVehicle < mpParams->muEndVehicle");

    mpParams->mpaVehicleTransforms[muCurrentVehicle] = mCurrentVehicleTransform;
}

// @0x8291DCC8. The whole job: Initialise, then walk the slice.
void UpdateVehiclesJob::Execute(UpdateVehiclesJobParams* lpParams)
{
    Initialise(lpParams);

    u32 luTouched = 0;
    while (MoveToNextVehicle())
    {
        const Vehicle* lpVehicle = GetCurrentVehicle();
        if (lpVehicle->IsAlive() && lpVehicle->HasEntity())
        {
            UpdateVehicle();
            WriteBackCurrentVehicle();
            ++luTouched;
        }
    }

    if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
    {
        // [T2-job] one-shot. DELETE-WHEN-STABLE.
        static bool sbFirst = true;
        if (sbFirst)
        {
            sbFirst = false;
            *lpDiag << "[T2-job] FIRST UpdateVehiclesJob::Execute begin="
                    << static_cast<s32>(lpParams->muBeginVehicle)
                    << " end=" << static_cast<s32>(lpParams->muEndVehicle)
                    << " touched=" << static_cast<s32>(luTouched)
                    << " dispatch=sync\n";
        }
    }

    mpParams = 0;
}

// ---------------------------------------------------------------------------------------
// Per-vehicle driving
// ---------------------------------------------------------------------------------------

// @0x8291D810 (:633/:634). The per-vehicle body the leak spells inline inside
// TrafficEntityModule::UpdateVehicles (BrnTrafficEntityModule.cpp:7030..:7200).
void UpdateVehiclesJob::UpdateVehicle()
{
    Vehicle* lpVehicle = GetCurrentVehicle();
    CGS_ASSERT(lpVehicle->IsAlive(), "lpVehicle->IsAlive()");
    CGS_ASSERT(lpVehicle->HasEntity(), "lpVehicle->HasEntity()");

    const Param* lpParam = GetCurrentParam();

    if (!lpVehicle->IsPhysical())
    {
        // Param+0x14 (mfSpeed) then Param+0x1A bit 0. Bit 0 of mxEffectAndHistoryState has
        // no name in BrnTrafficParam.h's enum (which starts at E_HISTORY_BORN == 0x02); the
        // asm is `lbz r11,26(param) ; clrlwi r11,r11,31`, so the raw mask stands.
        lpVehicle->SetSpeed(rw::math::vpu::Splat(lpParam->mfSpeed));
        lpVehicle->SetBrakelightsOn((lpParam->mxEffectAndHistoryState & 0x01u) != 0);
    }

    Vector3  lRaceCarPosition = ZeroV3();
    Vector3  lRaceCarVelocity = ZeroV3();
    Vector3  lRaceCarDirection = UnitZ();
    VecFloat lfRaceCarSpeed = rw::math::vpu::Splat(0.0f);
    VecFloat lfNearestRaceCarDistSq = rw::math::vpu::Splat(0.0f);

    const bool lbFoundRaceCar = FindInterestingRaceCar(lRaceCarPosition, lRaceCarVelocity,
                                                       lRaceCarDirection, lfRaceCarSpeed,
                                                       lfNearestRaceCarDistSq);

    // 0x8291D9AC..0x8291D9E0: the partial-update test, distance squared from mBehaviourCentre
    // to this vehicle's transform Pos() against 150 m squared.
    const Matrix44Affine lVehicleTransform = GetCurrentVehicleTransform();
    const Vector4& lrCentre = mpParams->mBehaviourCentre;
    const Vector3 lToCentre =
        Vector3{ lrCentre.x, lrCentre.y, lrCentre.z, 0.0f } - lVehicleTransform.Pos();
    const bool lbPartialUpdate =
        rw::math::vpu::MagnitudeSquared(lToCentre) >= KF_PARTIAL_UPDATE_RADIUS_SQ;

    // FLAG the partial-update latch @0x8291D9E4..0x8291DA00: the console writes lbPartialUpdate
    // into bit 0x80 of Vehicle+4 (muSpecies, low nibble is the species). muSpecies is private
    // and BrnTrafficVehicle.h is not this cluster's file, so no accessor exists to set it.
    // Nothing in the tree reads the bit yet. DELETE-WHEN Vehicle gains the setter.
    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateVehicle's partial-update latch @0x8291D9E4 -- needs a Vehicle accessor for "
            "muSpecies bit 0x80 (Vehicle+4), and BrnTrafficVehicle.h belongs to another cluster. "
            "The in-function behaviour is unaffected; no consumer of the bit exists yet");
    }

    f32  lfSwerveAmount = 0.0f;
    bool lbIsExtreme = false;
    bool lbIsNormalPhysical = false;

    if (!lpVehicle->IsSympatheticallyCrashing())
    {
        // 0x8291DA34..0x8291DAA8
        if (!lbPartialUpdate && mpParams->mbGameModeAllowsSwerving && lbFoundRaceCar)
        {
            lfSwerveAmount = CalcSwerveAmount(lRaceCarPosition, lRaceCarVelocity,
                                              lRaceCarDirection, lfRaceCarSpeed,
                                              &lbIsExtreme, &lbIsNormalPhysical).x;
            UpdateSwerveState(lRaceCarPosition, &lfSwerveAmount, &lbIsExtreme);
        }

        // 0x8291DAB0: either swerve flag makes CalcTargetPos treat the car as swerving.
        const bool lbSwerveActive = lbIsExtreme || lbIsNormalPhysical;

        Vector3 lTargetPos = ZeroV3();
        CalcTargetPos(rw::math::vpu::Splat(lfSwerveAmount), lTargetPos, lbSwerveActive);
        lpVehicle->SetTargetPos(lTargetPos);

        if (!lpVehicle->IsPhysical())
        {
            MoveToTarget(lTargetPos, lbPartialUpdate);

            if (!mpParams->mbDEBUGStopTrafficMoving)
            {
                // 0x8291DA?? -- promote to a physical car when the swerve state says so.
                // miLocalPlayerIndex == -1 means there is no local player to target.
                const s8 liLocalPlayer = mpParams->miLocalPlayerIndex;
                if (lbIsExtreme && liLocalPlayer != -1)
                {
                    EntityId lTarget;
                    lTarget.muValue = (static_cast<u32>(liLocalPlayer) << 10) | 0x1000000u;
                    RequestNewPhysicalVehicle(muCurrentVehicle,
                                              E_PHYSICALREASON_SWERVING, lTarget);
                }
                else if (lbIsNormalPhysical)
                {
                    EntityId lTarget;
                    lTarget.muValue = (static_cast<u32>(liLocalPlayer) << 10) | 0x1000000u;
                    RequestNewPhysicalVehicle(muCurrentVehicle,
                                              E_PHYSICALREASON_NORMAL, lTarget);
                }
            }
        }
    }

    if (!lbPartialUpdate)
    {
        UpdateEffects(lpParam);
    }

    // 0x8291DB?? -- a live param whose behaviour byte is 0 (Param+0x1B) and whose vehicle is
    // neither physical nor already sympathetically crashing asks for promotion, carrying its
    // own crash target (Param+0x48 == mSympCrashTarget).
    if (lpParam->IsAlive() && lpParam->miBehaviour == 0 && !lpVehicle->IsPhysical()
        && !lpVehicle->IsSympatheticallyCrashing())
    {
        RequestNewPhysicalVehicle(muCurrentVehicle, E_PHYSICALREASON_SYMPATHETIC_CRASHING,
                                  lpParam->mSympCrashTarget);
    }
}

// @0x8291B258 (:336/:796). Walk the cached race-car list and keep the nearest one. The
// accumulators are SIMD-selected, so every list entry is read and the winner is chosen
// lane-wise; the scalar form below is behaviour-identical.
bool UpdateVehiclesJob::FindInterestingRaceCar(Vector3& lrOutPosition,
                                               Vector3& lrOutLinearVelocity,
                                               Vector3& lrOutDirection,
                                               VecFloat& lrOutSpeed,
                                               VecFloat& lrOutDistanceSq) const
{
    const RaceCarStateData* lpRaceCarState = mpParams->mpRaceCarState;
    const Matrix44Affine lVehicleTransform = GetCurrentVehicleTransform();
    const Vector3 lVehiclePos = lVehicleTransform.Pos();

    // 0x8291B2?? -- the running minimum starts at E_TUNE_LIMITS lane 0 == MAX_FLOAT, and the
    // direction accumulator starts at unk_82181520 == unit Z.
    f32 lfMinDistSq = maTuning[E_TUNE_LIMITS].x;

    if (lpRaceCarState->mRaceCarPositions.GetLength() == 0)
    {
        lrOutDistanceSq = rw::math::vpu::Splat(lfMinDistSq);
        return false;
    }

    Vector3 lPosition = ZeroV3();
    Vector3 lLinearVelocity = ZeroV3();
    Vector3 lDirection = UnitZ();
    f32     lfSpeed = 0.0f;
    bool    lbFoundRaceCar = false;

    const u32 luCount = lpRaceCarState->mRaceCarPositions.GetLength();
    for (u32 luRaceCar = 0; luRaceCar < luCount; ++luRaceCar)
    {
        const Vector3 lRaceCarPos = lpRaceCarState->mRaceCarPositions.GetItem(luRaceCar);
        const f32 lfDistSq = rw::math::vpu::MagnitudeSquared(lRaceCarPos - lVehiclePos);

        if (lfDistSq < lfMinDistSq)
        {
            lbFoundRaceCar  = true;
            lfMinDistSq     = lfDistSq;
            lPosition       = lRaceCarPos;
            lLinearVelocity = lpRaceCarState->mRaceCarLinearVelocities.GetItem(luRaceCar);
            lDirection      = lpRaceCarState->mRaceCarXZVelocityDirs.GetItem(luRaceCar);
            lfSpeed         = lpRaceCarState->mRaceCarSpeeds.GetItem(luRaceCar).x;
        }
    }

    CGS_ASSERT(lbFoundRaceCar, "lbFoundRaceCar");

    lrOutPosition       = lPosition;
    lrOutLinearVelocity = lLinearVelocity;
    lrOutDirection      = lDirection;
    lrOutSpeed          = rw::math::vpu::Splat(lfSpeed);
    lrOutDistanceSq     = rw::math::vpu::Splat(lfMinDistSq);
    return true;
}

// @0x8291A5E0 (:1283/:1284). Leak key: TrafficEntityModule::UpdateVehicles_CalcTargetPos,
// BrnTrafficEntityModule.cpp:7407. Ship divergences, all asm-attested:
//  - the lane-centring weight comes from Vehicle::GetSpeed() * 0.125 clamped to [0,1]; the
//    leak's Section::mfSpeed reciprocal is gone (the ship calls no Get{Hull,Section}).
//  - a LEFT neighbour biases the centre line NEGATIVE and a RIGHT one positive
//    (0x8291A7C4 vxor of the sign mask on the left arm); the leak has the opposite sign.
//  - a partial update returns the raw swerved lane offset with no centring at all.
void UpdateVehiclesJob::CalcTargetPos(VecFloat lfSwerve, Vector3& lrOutTargetPos,
                                      bool lbPartialUpdate) const
{
    CGS_ASSERT(GetCurrentVehicle()->IsAlive(), "GetCurrentVehicle()->IsAlive()");
    CGS_ASSERT(IsValidF(lfSwerve.x), "IsValid( lfSwerve )");

    const Param* lpParam = GetCurrentParam();
    const ParamTransform* lpParamTransform = GetCurrentParamTransform();
    const Vehicle* lpVehicle = GetCurrentVehicle();

    const f32 lfRandomVal = lpVehicle->GetRandomVal();
    const f32 lfSpeed     = lpVehicle->GetSpeed().x;

    f32 lfDistAcrossLane = lpVehicle->GetDistAcrossLane().x
                           + lfSwerve.x * maTuning[E_TUNE_LANE].z;   // KF_APPROX_LANE_WIDTH

    f32 lfDistAcross;
    if (lbPartialUpdate)
    {
        lfDistAcross = lfDistAcrossLane;
    }
    else
    {
        const f32 lfMaxAcrossLane = maTuning[E_TUNE_LANE_CENTRE].x;
        lfDistAcrossLane = ClampF(lfDistAcrossLane, -lfMaxAcrossLane, lfMaxAcrossLane);

        const f32 lfSideSpace = maTuning[E_TUNE_LANE_CENTRE].y;
        f32 lfCentreLine = 0.0f;
        // 0x8291A798 / 0x8291A7E8 `cmplwi 0xFFFE ; bge` -- a BAND, so both the 0xFFFE cache
        // sentinel and Section::FindNeighbourForRung's 0xFFFF "none" count as no neighbour.
        if (lpParam->mauNeighbourData[E_LEFT] < KU_UNKNOWN_NEIGHBOUR)
        {
            lfCentreLine = -lfSideSpace;
        }
        else if (lpParam->mauNeighbourData[E_RIGHT] < KU_UNKNOWN_NEIGHBOUR)
        {
            lfCentreLine = lfSideSpace;
        }

        const f32 lfSideVariation = maTuning[E_TUNE_LANE_CENTRE].z;
        lfCentreLine += (lfRandomVal * lfSideVariation * 2.0f) - lfSideVariation;

        const f32 lfSpeedScale =
            ClampF(lfSpeed * maTuning[E_TUNE_MOVE].z, 0.0f, 1.0f);   // 1/8 m/s per unit

        lfDistAcross = lfCentreLine
                       + (lfDistAcrossLane - lfCentreLine) * (lfSpeedScale * lfSpeedScale);

        const f32 lfMaxFromCentre = maTuning[E_TUNE_LANE_CENTRE].w;
        lfDistAcross = ClampF(lfDistAcross, -lfMaxFromCentre, lfMaxFromCentre);
    }

    // 0x8291A8A0: ParamTransform::GetLerpedPositionAcross, inlined -- mLerpedPos + mRight*d.
    lrOutTargetPos = lpParamTransform->GetLerpedPos()
                     + lpParamTransform->GetRight() * lfDistAcross;
}

// @0x82918A20 (:1517). Drop both axles onto the lane the param remembers driving over, walking
// the history ring until one intersects. When the back axle finds no rung it inherits the
// front axle's up vector (the vrlimi at 0x82918B84 keeps the back axle's own w).
void UpdateVehiclesJob::PlaceVehicleOnRoad()
{
    VehicleAxles* lpAxles = GetCurrentVehicleAxles();
    const Param* lpParam = GetCurrentParam();
    CGS_ASSERT(lpParam, "lpParam");

    const u32 KU_HISTORY_ENTRIES_TO_TRY = 5;   // `cmplwi cr6, r30, 5` @0x82918B00

    u32 luHistory = 0;
    u32 luSegment = 0;
    u32 luHull    = 0;
    bool lbFrontHit = false;

    for (; luHistory < KU_HISTORY_ENTRIES_TO_TRY; ++luHistory)
    {
        lpParam->GetHistoryEntry(luHistory, &luSegment, &luHull);
        const Hull* lpHull = GetHull(luHull);
        const LaneRung* lpRung = lpHull->mpaRungs + luSegment;
        if (lpAxles->mFrontAxle.TryIntersectWithLane(lpRung[0], lpRung[1]))
        {
            lbFrontHit = true;
            break;
        }
    }

    if (!lbFrontHit)
    {
        return;
    }

    for (;;)
    {
        const Hull* lpHull = GetHull(luHull);
        const LaneRung* lpRung = lpHull->mpaRungs + luSegment;
        if (lpAxles->mBackAxle.TryIntersectWithLane(lpRung[0], lpRung[1]))
        {
            return;
        }

        ++luHistory;
        lpParam->GetHistoryEntry(luHistory, &luSegment, &luHull);
        if (luHistory >= KU_HISTORY_ENTRIES_TO_TRY)
        {
            break;
        }
    }

    lpAxles->mBackAxle.SetUp(lpAxles->mFrontAxle.GetUp());
}

// @0x8291CE48 (:1668). Body fully reconstructed; GATED because nothing reads
// TrafficJobStub::GetNewPhysicalRequests().
void UpdateVehiclesJob::RequestNewPhysicalVehicle(u16 luVehicle, PhysicalReason leReason,
                                                  EntityId lTargetEntityId)
{
    if (!KB_T2_ALLOW_PHYSICAL_PROMOTION)
    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "RequestNewPhysicalVehicle @0x8291CE48 -- body reconstructed below; no reader of "
            "TrafficJobStub::GetNewPhysicalRequests() exists yet");
        return;
    }

    CGS_ASSERT(mpParams->mpOutNewPhysicalRequests, "mpParams->mpOutNewPhysicalRequests");

    if (mpParams->mpOutNewPhysicalRequests->GetLength() != 25)
    {
        PhysicalRequestInfo lInfo;
        lInfo.Construct(luVehicle, leReason, lTargetEntityId);
        mpParams->mpOutNewPhysicalRequests->Append(lInfo);
    }
}

// ---------------------------------------------------------------------------------------
// Gated legs
// ---------------------------------------------------------------------------------------

// @0x8291A8C0. EXPORT HOLE.
void UpdateVehiclesJob::UpdateEffects(const Param* lpParam)
{
    static bool sbLogged = false;
    LogMissingLeg(sbLogged,
        "UpdateEffects @0x8291A8C0 -- EXPORT HOLE: the ARTIST export set jumps 0x8291A5E0 -> "
        "0x8291AC60, so no pseudocode or asm exists for it. Cost: no indicator / headlight / "
        "brakelight warmth animation on driving traffic (mEffectRand and muEffectTickRate are "
        "seeded and unread)");
    (void)lpParam;
}

// @0x8291B508 (:996). Leak key BrnTrafficEntityModule.cpp:5364. Ship divergences: the console
// takes the SECTION-LOCAL rung slice through GetRungLengthsForSection, reads the lane position
// from ParamTransform::GetLerpedPos, and spells CalcDistanceFromEndOfSection inline as
// Section::mfLength - CalcDistanceAlongSection.
VecFloat UpdateVehiclesJob::GetObjectEstimatedDistanceToLaneCenterAccordingToParam(
    Vector3 lObjectPos, Vector3& lrOutPredictedPos) const
{
    const Matrix44Affine lVehicleTransform = GetCurrentVehicleTransform();
    const Param* lpParam = GetCurrentParam();
    const Hull* lpHull = GetHull(lpParam->muHullIndex);
    const Section* lpSection = GetSection(lpHull, lpParam->muSectionIndex);
    const ParamTransform* lpParamTransform = GetCurrentParamTransform();

    // Signed by Dot(diff, At) so a race car behind the vehicle reads negative.
    const Vector3 lDiffPosition = lObjectPos - lpParamTransform->GetLerpedPos();
    const f32 lfMagnitude = rw::math::vpu::Magnitude(lDiffPosition);
    f32 lfDistanceToObject = rw::math::vpu::Dot(lDiffPosition, lVehicleTransform.At()) > 0.0f
                                 ? lfMagnitude
                                 : -lfMagnitude;

    f32 lfParamAlong   = lpParam->mfParamAlong;
    u32 luSegmentAlong = lpParam->muCurrentSegment;

    f32 lfDistanceToEndOfSection =
        lpSection->mfLength
        - lpSection->CalcDistanceAlongSection(lfParamAlong, luSegmentAlong,
                                              GetRungLengthsForSection(lpHull, lpSection));

    if (lfDistanceToObject > 0.0f && lfDistanceToObject > lfDistanceToEndOfSection)
    {
        for (u32 luPlan = 0; luPlan < KU_PARAM_NUM_PLANS; ++luPlan)
        {
            const ParamPlan* lpPlan = GetCurrentParamPlan(luPlan);
            CGS_ASSERT(lpPlan, "lpPlan");

            if (lpPlan->muType == ParamPlan::E_TYPE_CHANGE_SECTION
                && lpPlan->mChangeSectionData.muNewHull != KU_INVALID_HULL)
            {
                lpHull    = GetHull(lpPlan->mChangeSectionData.muNewHull);
                lpSection = GetSection(lpHull, lpPlan->mChangeSectionData.muNewSection);

                lfParamAlong        = 0.0f;
                luSegmentAlong      = 0;
                lfDistanceToObject -= lfDistanceToEndOfSection;

                lfDistanceToEndOfSection =
                    lpSection->mfLength
                    - lpSection->CalcDistanceAlongSection(
                          lfParamAlong, luSegmentAlong,
                          GetRungLengthsForSection(lpHull, lpSection));

                if (lfDistanceToObject <= lfDistanceToEndOfSection)
                {
                    break;
                }
            }
            else if (lpPlan->muType == ParamPlan::E_TYPE_NONE)
            {
                break;
            }
        }
    }

    const f32 lfEstimatedObjectParam =
        lpSection->CalcParamFromStartParamAndDistanceAlongSection(
            lfParamAlong, lfDistanceToObject, GetRungLengthsForSection(lpHull, lpSection));

    Vector3 lEstimatedPosition  = ZeroV3();
    Vector3 lEstimatedDirection = ZeroV3();
    Vector3 lEstimatedRight     = ZeroV3();
    lpSection->CalcTransformAtParameter(lpHull->mpaRungs,
                                        rw::math::vpu::Splat(lfEstimatedObjectParam),
                                        static_cast<u32>(lfEstimatedObjectParam),
                                        lEstimatedPosition, lEstimatedDirection,
                                        lEstimatedRight);

    lrOutPredictedPos = lEstimatedPosition;
    return rw::math::vpu::Splat(
        rw::math::vpu::Dot(lObjectPos - lEstimatedPosition, lEstimatedRight));
}

// @0x82918748 (:1081). Four fuzzy envelope sets scored against the four inputs, then the lanes
// min-combined into two rule outputs. Console set/argument pairing (0x8291880C..0x82918888):
// job+0x120 (set 2) sees lfDistance, +0xA0 (set 0) lfClosingSpeed, +0xE0 (set 1) lfLanePos,
// +0x160 (set 3) lfAngleDot.
//
// The rule combination @0x82918924..0x829189AC draws rule 0's selected lane from SET 0 and its
// last min term from SET 2 -- crossed over from the sets the per-input scoring uses first.
void UpdateVehiclesJob::ProcessSwervingRules(VecFloat* lpafOutputs,
                                             bool lbIsExtremeSwerving,
                                             bool lbAllowsHardcoreSwerving,
                                             VecFloat lfDistance,
                                             VecFloat lfClosingSpeed,
                                             VecFloat lfLanePos,
                                             VecFloat lfAngleDot) const
{
    CGS_ASSERT(lpafOutputs, "lpafOutputs");

    const Vector4 lDistanceScores = maSwerveEnvelopes[E_SWERVE_ENVELOPES_DISTANCE].CalcScores(lfDistance);
    const Vector4 lClosingScores  = maSwerveEnvelopes[E_SWERVE_ENVELOPES_CLOSING_SPEED].CalcScores(lfClosingSpeed);
    const Vector4 lLanePosScores  = maSwerveEnvelopes[E_SWERVE_ENVELOPES_LANE_POS].CalcScores(lfLanePos);
    const Vector4 lAngleScores    = maSwerveEnvelopes[E_SWERVE_ENVELOPES_ANGLE].CalcScores(lfAngleDot);

    // 0x82918924 / 0x82918950 / 0x8291896C: both selected lanes are set 0 (closing speed) --
    // hardcore takes envelope 2 ("approaching at all"), otherwise envelope 3 ("approaching fast").
    const f32 lfClosingLane = lbAllowsHardcoreSwerving ? lClosingScores.z : lClosingScores.w;

    // 0x8291899C: an already-extreme vehicle gets an E_TUNE_LIMITS.w bonus on rule 0.
    const f32 lfExtremeBonus = lbIsExtremeSwerving ? maTuning[E_TUNE_LIMITS].w : 0.0f;

    // 0x82918984 / 0x829189A0 / 0x829189A8, in that order.
    f32 lfRule0 = lAngleScores.x;
    lfRule0 = lfRule0 < lfClosingLane     ? lfRule0 : lfClosingLane;
    lfRule0 = lfRule0 < lLanePosScores.z  ? lfRule0 : lLanePosScores.z;
    lfRule0 = lfRule0 < lDistanceScores.y ? lfRule0 : lDistanceScores.y;
    lfRule0 += lfExtremeBonus;

    // 0x8291898C: min(set2.x, set1.y).
    f32 lfRule1 = lDistanceScores.x < lLanePosScores.y ? lDistanceScores.x : lLanePosScores.y;

    // 0x829189B4..0x829189FC: both rules are offset by the crash slider.
    const f32 lfCrashSlider = mpParams->mfCrashSliderFinalValue;
    lpafOutputs[0] = rw::math::vpu::Splat(lfRule0 - lfCrashSlider);
    lpafOutputs[1] = rw::math::vpu::Splat(lfRule1 - lfCrashSlider);
}

// @0x8291CF18 (:826..:928). The ship fuzzy swerve controller. The Feb-2007 twin
// (BrnTrafficEntityModule.cpp:7354) is a 43-line predecessor and only names the outer shape.
//
// PARTIAL -- one gated leg:
//   FLAG the predicted-intersection refinement @0x8291D644. It calls
//   BrnTraffic::GetLineLineIntersectionParamXZ @0x8291AC60, declared nowhere in this tree; its
//   owning header BrnTrafficMathsUtils.h is not this cluster's file. Cost: inside the extreme
//   arm the swerve direction keeps the lane-offset sign. DELETE-WHEN that helper lands.
VecFloat UpdateVehiclesJob::CalcSwerveAmount(Vector3 lRaceCarPosition,
                                             Vector3 lRaceCarVelocity,
                                             Vector3 lRaceCarDirection,
                                             VecFloat lfRaceCarSpeed,
                                             bool* lpbOutIsExtreme,
                                             bool* lpbOutIsNormalPhysical)
{
    CGS_ASSERT(lpbOutIsExtreme, "lpbOutIsExtreme");
    CGS_ASSERT(lpbOutIsNormalPhysical, "lpbOutIsNormalPhysical");

    const Vehicle* lpVehicle = GetCurrentVehicle();

    // 0x8291D020: an orphaned half keeps whatever it was already doing.
    if ((lpVehicle->GetFlags() & Vehicle::E_FLAG_ORPHAN) != 0)
    {
        *lpbOutIsExtreme = false;
        return lpVehicle->GetSwerveAmount();
    }

    const Matrix44Affine lTransform = GetCurrentVehicleTransform();
    const Vector3 lPos   = lTransform.Pos();
    const Vector3 lAt    = lTransform.At();
    const Vector3 lRight = lTransform.Right();

    const Vector3 lDiff = lRaceCarPosition - lPos;
    const VecFloat lfAngleDot = rw::math::vpu::Splat(rw::math::vpu::Dot(lRaceCarDirection, lAt));

    const f32 lfDistSq = rw::math::vpu::MagnitudeSquared(lDiff);
    const f32 lfDist   = rw::math::vpu::Magnitude(lDiff);
    const f32 lfSignedDist = rw::math::vpu::Dot(lDiff, lAt) > 0.0f ? lfDist : -lfDist;

    // 0x8291D16C: envelope 0 of the distance set is the whole gate -- out of band, no swerve.
    if (maSwerveEnvelopes[2].CalcScore(0, rw::math::vpu::Splat(lfSignedDist)).x <= 0.0f)
    {
        return rw::math::vpu::Splat(0.0f);
    }

    const Vector3 lUnitToRaceCar = lfDistSq > 0.0f ? lDiff * (1.0f / lfDist) : ZeroV3();
    const Vector3 lOwnVelocity = lAt * lpVehicle->GetSpeed().x;
    const VecFloat lfClosingSpeed =
        rw::math::vpu::Splat(rw::math::vpu::Dot(lOwnVelocity - lRaceCarVelocity, lUnitToRaceCar));

    Vector3 lPredictedPos = ZeroV3();
    const f32 lfRCLanePos =
        GetObjectEstimatedDistanceToLaneCenterAccordingToParam(lRaceCarPosition, lPredictedPos).x;
    CGS_ASSERT(IsValidF(lfRCLanePos), "RwMath::fpu::IsValid( lfRCLanePos )");

    const f32 lfSwerveDirection = -SgnF(lfRCLanePos);
    CGS_ASSERT(IsValidF(lfSwerveDirection), "RwMath::fpu::IsValid( lfSwerveDirection )");

    const VecFloat lfAbsLanePos =
        rw::math::vpu::Splat(lfRCLanePos >= 0.0f ? lfRCLanePos : -lfRCLanePos);

    VecFloat laOutputs[2];
    ProcessSwervingRules(laOutputs,
                         lpVehicle->GetCurrentManoeuvre() == Vehicle::E_MANOEUVRE_EXTREME_SWERVE,
                         mpParams->mbGameModeAllowsHardcoreSwerving,
                         rw::math::vpu::Splat(lfSignedDist), lfClosingSpeed,
                         lfAbsLanePos, lfAngleDot);

    bool lbIsExtreme = false;
    f32  lfSwerveAmount;

    // 0x8291D3B8: rule 0 wins ties by KF_SWERVE_RULE_EPSILON.
    if (laOutputs[0].x + KF_SWERVE_RULE_EPSILON > laOutputs[1].x)
    {
        lfSwerveAmount = laOutputs[0].x * maTuning[E_TUNE_SWERVE].x;

        if (laOutputs[0].x > 0.0f)
        {
            lbIsExtreme = true;

            // 0x8291D490: close enough to aim at where the race car will be.
            if (maTuning[E_TUNE_MOVE].w > lfDist)
            {
                const f32 lfAlongDot = rw::math::vpu::Dot(lAt, lDiff);
                const f32 lfOwnSpeed = lpVehicle->GetSpeed().x;
                const f32 lfSumSpeed = lfOwnSpeed + lfRaceCarSpeed.x;
                const f32 lfRelativeSpeed =
                    lfSumSpeed == 0.0f ? 0.0f : (lfOwnSpeed / lfSumSpeed);
                CGS_ASSERT(IsValidF(lfRelativeSpeed), "RwMath::IsValid( lfRelativeSpeed )");

                lPredictedPos = lPos + (lAt * lfRelativeSpeed) * lfAlongDot;
                CGS_ASSERT(rw::math::vpu::IsValid(lPredictedPos),
                           "RwMath::IsValid( lPredictedPos )");

                static bool sbLogged = false;
                LogMissingLeg(sbLogged,
                    "CalcSwerveAmount's predicted-intersection refinement @0x8291D644 -- "
                    "BrnTraffic::GetLineLineIntersectionParamXZ @0x8291AC60 is declared nowhere "
                    "in the tree and its owning header BrnTrafficMathsUtils.h belongs to another "
                    "cluster. The swerve direction keeps the lane-offset sign");
                (void)lRight;
            }
        }
    }
    else
    {
        lfSwerveAmount = laOutputs[1].x * maTuning[E_TUNE_LIMITS].z;
    }

    // 0x8291D794: Param+0x1B == 2 is one of the six unattested miBehaviour enumerators; such a
    // param always goes normal-physical at full swerve.
    if (GetCurrentParam()->miBehaviour == 2)
    {
        *lpbOutIsExtreme = false;
        *lpbOutIsNormalPhysical = true;
        lfSwerveAmount = 1.0f;
    }
    else
    {
        *lpbOutIsExtreme = lbIsExtreme;
    }

    return rw::math::vpu::Splat(lfSwerveDirection * lfSwerveAmount);
}

// @0x8291B870 (:1123/:1124). The swerve timer / hysteresis over the amount CalcSwerveAmount
// just produced.
void UpdateVehiclesJob::UpdateSwerveState(Vector3 lRaceCarPosition,
                                          f32* lpfUpdatedSwerveAmount,
                                          bool* lpbUpdatedExtremeSwerve)
{
    CGS_ASSERT(lpfUpdatedSwerveAmount, "lpfUpdatedSwerveAmount");
    CGS_ASSERT(lpbUpdatedExtremeSwerve, "lpbUpdatedExtremeSwerve");

    const f32 lfRequested = *lpfUpdatedSwerveAmount;
    f32 lfLatched = lfRequested;

    Vehicle* lpVehicle = GetCurrentVehicle();
    const f32 lfSimTimeStep = mpParams->mfSimTimeStep;

    if (*lpbUpdatedExtremeSwerve)
    {
        if (lpVehicle->GetCurrentManoeuvre() == Vehicle::E_MANOEUVRE_EXTREME_SWERVE)
        {
            // 0x8291B9A4: wind the negative countdown up toward 0, never past it.
            const f32 lfWound = lpVehicle->GetSwerveTime() + lfSimTimeStep;
            lpVehicle->SetSwerveTime(lfWound >= 0.0f ? 0.0f : lfWound);

            if (SgnF(lfRequested) != SgnF(lpVehicle->GetSwerveAmount().x))
            {
                if (lpVehicle->GetSwerveTime() < 0.0f)
                {
                    lfLatched = lpVehicle->GetSwerveAmount().x;
                    *lpfUpdatedSwerveAmount = lfLatched;
                }
                else
                {
                    lpVehicle->SetSwerveTime(KF_SWERVE_REVERSE_DELAY);
                }
            }
        }
        else
        {
            lpVehicle->SetWantsToExtremeSwerve(true);
            lpVehicle->SetSwerveTime(KF_SWERVE_REVERSE_DELAY);
        }

        lpVehicle->SetSwerveAmount(lfLatched);
        return;
    }

    // 0x8291BAC4: an extreme-swerving car whose countdown is still negative restarts its hold
    // at E_TUNE_SWERVE.y.
    if (lpVehicle->GetCurrentManoeuvre() == Vehicle::E_MANOEUVRE_EXTREME_SWERVE
        && lpVehicle->GetSwerveTime() < 0.0f)
    {
        lpVehicle->SetSwerveTime(maTuning[E_TUNE_SWERVE].y);
    }

    if (lpVehicle->GetSwerveTime() > 0.0f)
    {
        const f32 lfDown = lpVehicle->GetSwerveTime() - lfSimTimeStep;
        lpVehicle->SetSwerveTime(lfDown >= 0.0f ? lfDown : 0.0f);
    }
    else if (lpVehicle->GetSwerveTime() < 0.0f)
    {
        lpVehicle->SetSwerveTime(lpVehicle->GetSwerveTime() + lfSimTimeStep);
        if (lpVehicle->GetSwerveTime() >= 0.0f)
        {
            lpVehicle->SetSwerveTime(KF_SWERVE_HOLD_TIME);
            lpVehicle->SetSwerveAmount(lfLatched);
        }
    }

    if (lpVehicle->GetCurrentManoeuvre() == Vehicle::E_MANOEUVRE_EXTREME_SWERVE)
    {
        if (lpVehicle->GetSwerveTime() > 0.0f)
        {
            *lpbUpdatedExtremeSwerve = true;
            *lpfUpdatedSwerveAmount = lpVehicle->GetSwerveAmount().x;
            return;
        }
        lpVehicle->SetWantsToExtremeSwerve(false);
    }

    // 0x8291BBAC: IsZero against FLT_EPSILON, not against 0.
    if (lfRequested <= KF_SWERVE_EPSILON && lfRequested >= -KF_SWERVE_EPSILON)
    {
        lpVehicle->SetSwerveAmount(0.0f);
        lpVehicle->SetSwerveTime(0.0f);
        *lpfUpdatedSwerveAmount = 0.0f;
        return;
    }

    // 0x8291BC10: a vehicle that is not already swerving starts a fresh hold.
    const f32 lfCurrent = lpVehicle->GetSwerveAmount().x;
    const f32 lfAbsCurrent = lfCurrent >= 0.0f ? lfCurrent : -lfCurrent;
    if (!(lfAbsCurrent > KF_SWERVE_EPSILON))
    {
        lpVehicle->SetSwerveTime(KF_SWERVE_HOLD_TIME);
        lpVehicle->SetSwerveAmount(lfLatched);
    }

    if (lpVehicle->GetSwerveTime() > 0.0f)
    {
        // Holding: only a same-sign request gets through.
        if (SgnF(lfRequested) != SgnF(lpVehicle->GetSwerveAmount().x))
        {
            *lpfUpdatedSwerveAmount = lpVehicle->GetSwerveAmount().x;
        }
        return;
    }

    if (SgnF(lfRequested) == SgnF(lpVehicle->GetSwerveAmount().x))
    {
        lpVehicle->SetSwerveAmount(lfLatched);
        lpVehicle->SetSwerveTime(0.0f);
        return;
    }

    // 0x8291BDD4: a sign flip costs a negative lockout scaled by the race-car distance, clamped
    // into [E_TUNE_LIMITS.w, 1].
    const Matrix44Affine lTransform = GetCurrentVehicleTransform();
    const f32 lfDistance = rw::math::vpu::Magnitude(lRaceCarPosition - lTransform.Pos());
    f32 lfLockout = lfDistance * KF_SWERVE_LOCKOUT_PER_METRE;
    if (lfLockout < maTuning[E_TUNE_LIMITS].w)
    {
        lfLockout = maTuning[E_TUNE_LIMITS].w;
    }
    if (lfLockout > 1.0f)
    {
        lfLockout = 1.0f;
    }
    lpVehicle->SetSwerveTime(-lfLockout);
}

} // namespace BrnTraffic
