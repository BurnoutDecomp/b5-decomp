#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/vector3_operation.h"   // IsZero / IsValid / Normalize / Cross (ParamTransform)

// Param::Construct / Param::Initialise reach these; all four are leaf headers.
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"        // KU_UNKNOWN_STOPLINE / KU_UNKNOWN_NEIGHBOUR
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h"      // KU_INVALID_HULL
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h" // GetBBoxOffset / GetBBoxHalfSize
#include "SharedClasses/Traffic/BrnTrafficSharedConstants.h"  // KU_INVALID_SECTION, E_DIR_STRAIGHT_ON, E_SIDE_COUNT
#include "SharedClasses/Traffic/BrnTrafficHull.h"             // Hull::GetSection
#include "SharedClasses/Traffic/BrnTrafficSection.h"          // Section::mfSpeed / muRungOffset
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"      // VehicleTypeData::mxVehicleFlags
#include "SharedClasses/Traffic/BrnTrafficVehicleTraits.h"    // VehicleTraits::GetAccelerationModifier

#include <cfloat>   // FLT_MAX (the console's 0x7F7FFFFF seeds)

// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnTraffic::Param::ClearDying              @ 0x82713130
//   BrnTraffic::Param::GetHistoryEntry         @ 0x8274FA08
//   BrnTraffic::Param::IsQueueing              @ 0x82712398
//   BrnTraffic::Param::PushHistory             @ 0x82706AC8
//   BrnTraffic::Param::SetChangedSection       @ 0x82713C68
//   BrnTraffic::Param::SetDyingState           @ 0x82713830
//   BrnTraffic::Param::SetInPurgatory          @ 0x82713CD0
//   BrnTraffic::Param::SetParamAlong           @ 0x827069D0
//   BrnTraffic::Param::SetShouldBeRemoved      @ 0x827134C0
//   BrnTraffic::Param::SetZombie               @ 0x82713528
//   BrnTraffic::Param::ShouldBeIndicatingRight @ 0x82917650
//
// All offsets/bit-fields pinned by name against the store/load order in the asm. The
// SoA membership flips go through CgsContainers::FastBitArray, which inlines to the same
// (field = idx>>6, mask = 1 << (idx & 63)) quadword math the X360 folds in at each call
// site. The X360 also folds a CgsFastBitArray out-of-range StrStream assert (max bits
// 600) around every access; per the project's FastBitArray policy that StrStream
// scaffolding is owned by the caller and is benign, so it is modelled here as a single
// CGS_ASSERT on the index bound rather than reproduced StrStream-for-StrStream.

namespace BrnTraffic
{
void ParamSoaData::Construct()
{
    mAliveParams.Construct();
    mDyingParams.Construct();
    mZombieParams.Construct();
}

// rodata flt_820C0DFC, the scale Construct stores straight into mfMaxAcceleration and
// Initialise multiplies by VehicleTraits::GetAccelerationModifier(). The Feb-2007 source names
// it KF_PARAM_MAX_ACCEL_FORCE (BrnTrafficParam.cpp:139).
static const f32 KF_PARAM_MAX_ACCEL_FORCE = 0.23999999f;

// rodata flt_820C0F68, subtracted from mfBackDist when the vehicle type tows a trailer
// (VehicleTypeData::mxVehicleFlags bit 0). Not named by the leak, which has no such leg.
static const f32 KF_PARAM_CAB_BACK_DIST_EXTENSION = 12.0f;

// The sentinel Construct/Initialise store into both mauNeighbourEndRung slots. The leak
// spells it ~0 over a uint8_t; the asm stores 0xFF.
static const u8 KU_PARAM_INVALID_NEIGHBOUR_END_RUNG = 0xFFu;

// 0x82751B60 — the pool-wide reset Reset @0x8272CDA0 runs 400 times. Store order is the asm's.
void Param::Construct()
{
    muHullIndex         = KU_INVALID_HULL;
    muStartHullIndex    = KU_INVALID_HULL;
    muSectionIndex      = KU_INVALID_SECTION;
    muStartSectionIndex = KU_INVALID_SECTION;

    SetParamAlong(0.0f);

    mfMaxAcceleration         = KF_PARAM_MAX_ACCEL_FORCE;
    mxFlags                   = 0;
    mxEffectAndHistoryState   = 0;
    muCurrentSectionDirection = E_DIR_STRAIGHT_ON;
    miBehaviour               = KI_BEHAVIOUR_INVALID;
    muNextStopLineIndex       = static_cast<u8>(KU_UNKNOWN_STOPLINE);
    muExtraBehaviourFlags     = 0;
    mSympCrashTarget.muValue  = 0xFFFFFFFFu;

    for (u32 luPlan = 0; luPlan < KU_PARAM_NUM_PLANS; ++luPlan)
    {
        maPlans[luPlan].muType = ParamPlan::E_TYPE_NONE;
    }

    for (u32 luSide = E_LEFT; luSide < E_SIDE_COUNT; ++luSide)
    {
        mauNeighbourData[luSide]    = static_cast<u16>(KU_UNKNOWN_NEIGHBOUR);
        mauNeighbourEndRung[luSide] = KU_PARAM_INVALID_NEIGHBOUR_END_RUNG;
    }
}

// 0x82755F40 — bring a free pool slot to life on a lane. The ledger files this row under
// GameShared/GameClasses/Development/CgsStrStream.h; that is the catch-all misattribution the
// baked FastBitArray StrStream asserts cause. The console's own assert strings cite
// BrnTrafficParam.cpp, so this is its real home.
//
// Divergences from the Feb-2007 original, asm-decided: mfSpeedDiff is gone (mfAcceleration and
// mfLastSpeed take its place), mauNeighbourData seeds KU_UNKNOWN_NEIGHBOUR (0xFFFE) rather than
// KU_INVALID_NEIGHBOUR, the alive/dying SoA bit-sets replace the old flag bits, and mfFrontDist /
// mfBackDist are derived from the vehicle type's bbox lanes.
void Param::Initialise(u32 luHullIndex,
                       u32 luSectionIndex,
                       f32 lfParamAlong,
                       f32 lfRandomVal,
                       u32 luVehicleType,
                       const Hull* lpHull,
                       const VehicleTypeData* lpVehicleTypeData,
                       const VehicleTypeRuntime* lpVehicleTypeRuntime,
                       const VehicleTraits* lpVehicleTraits,
                       u32 luParam,
                       ParamSoaData& lParamSoaData)
{
    CGS_ASSERT(lpVehicleTypeData != 0, "lpVehicleTypeData");
    CGS_ASSERT(lpVehicleTypeRuntime != 0, "lpVehicleTypeRuntime");
    CGS_ASSERT(!IsAlive(), "!IsAlive()");
    CGS_ASSERT(!IsDying(), "!IsDying()");
    CGS_ASSERT(luParam < 600, "luParam in range");
    CGS_ASSERT(!lParamSoaData.mAliveParams.IsBitSet(luParam),
               "!lParamSoaData.mAliveParams.IsBitSet( luParam )");
    CGS_ASSERT(!lParamSoaData.mDyingParams.IsBitSet(luParam),
               "!lParamSoaData.mDyingParams.IsBitSet( luParam )");
    CGS_ASSERT(lpHull != 0, "lpHull");

    const Section* lpSection = lpHull->GetSection(luSectionIndex);
    CGS_ASSERT(lpSection != 0, "lpSection");

    muSectionIndex      = static_cast<u8>(luSectionIndex);
    muStartHullIndex    = KU_INVALID_HULL;
    muStartSectionIndex = KU_INVALID_SECTION;
    muHullIndex         = static_cast<u16>(luHullIndex);

    SetParamAlong(lfParamAlong);

    mfRandomVal    = lfRandomVal;
    muVehicleType  = static_cast<u8>(luVehicleType);
    mfSpeed        = lpSection->mfSpeed;
    mfAcceleration = 0.0f;
    mxFlags        = static_cast<u8>(E_FLAG_ALIVE);
    mfLastSpeed    = lpSection->mfSpeed;

    lParamSoaData.mAliveParams.SetBit(luParam);

    mxEffectAndHistoryState =
        static_cast<u8>(E_HISTORY_BORN | E_HISTORY_NEEDS_NEW_PLAN);          // asm: 0x12
    mfNextStopLineParam       = FLT_MAX;
    mSympCrashTarget.muValue  = 0xFFFFFFFFu;
    miBehaviour               = KI_BEHAVIOUR_NORMAL;
    muCurrentSectionDirection = E_DIR_STRAIGHT_ON;
    muNextStopLineIndex       = static_cast<u8>(KU_UNKNOWN_STOPLINE);
    mfMaxAcceleration         = lpVehicleTraits->GetAccelerationModifier() * KF_PARAM_MAX_ACCEL_FORCE;
    muNextHistoryToWrite      = 0;

    const u16 luSegmentId =
        static_cast<u16>(muCurrentSegment + lpSection->muRungOffset);
    for (u32 luHistoryIndex = 0;
         luHistoryIndex < KU_PARAM_NUM_SEGMENTS_TO_REMEMBER;
         ++luHistoryIndex)
    {
        mauHistorySegments[luHistoryIndex] = luSegmentId;
        mauHistoryHulls[luHistoryIndex]    = static_cast<u16>(luHullIndex);
    }

    mfStopDist     = FLT_MAX;
    mfTargetSpeed  = 0.0f;
    mfTimeQueueing = 0.0f;

    // asm: vspltw lane 2 of the runtime's bbox offset and half-size, then vaddfp / vsubfp.
    const f32 lfBBoxOffsetZ   = lpVehicleTypeRuntime->GetBBoxOffset().z;
    const f32 lfBBoxHalfSizeZ = lpVehicleTypeRuntime->GetBBoxHalfSize().z;
    mfFrontDist = lfBBoxOffsetZ + lfBBoxHalfSizeZ;
    mfBackDist  = lfBBoxOffsetZ - lfBBoxHalfSizeZ;
    if ((lpVehicleTypeData->mxVehicleFlags & 1u) != 0)
    {
        mfBackDist -= KF_PARAM_CAB_BACK_DIST_EXTENSION;
    }

    for (u32 luPlan = 0; luPlan < KU_PARAM_NUM_PLANS; ++luPlan)
    {
        maPlans[luPlan].muType = ParamPlan::E_TYPE_NONE;
    }

    for (u32 luSide = E_LEFT; luSide < E_SIDE_COUNT; ++luSide)
    {
        mauNeighbourData[luSide]    = static_cast<u16>(KU_UNKNOWN_NEIGHBOUR);
        mauNeighbourEndRung[luSide] = KU_PARAM_INVALID_NEIGHBOUR_END_RUNG;
    }
}

// 0x827069D0 — stamp the position-along and the derived integer segment.
void Param::SetParamAlong(f32 lfParamAlong)
{
    // X360: RwMath::IsValid (vcmpeqfp self-compare == not-NaN), then range asserts.
    CGS_ASSERT(lfParamAlong == lfParamAlong, "RwMath::IsValid( lfParamAlong )");
    CGS_ASSERT(lfParamAlong >= 0.0f, "lfParamAlong >= 0.0f");
    CGS_ASSERT(lfParamAlong < 256.0f, "lfParamAlong < 256.0f");

    mfParamAlong = lfParamAlong;
    // asm: fctidz (truncate-toward-zero to int64) then store the low byte.
    muCurrentSegment = static_cast<u8>(static_cast<s64>(lfParamAlong));
}

// 0x82712398 — true when the param is crawling (not in a stopping behaviour) and slow.
bool Param::IsQueueing() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");

    // miBehaviour is sign-extended (extsb) before the switch; the retail behaviour enum
    // (renumbered from the Feb-2007 leak) selects 2/4/6 as the "not queueing" cases.
    // FLAG: the full retail Behaviours enum is not recoverable from this TU; the literal
    // comparisons below match the X360 switch exactly.
    const s32 liBehaviour = static_cast<s32>(miBehaviour);
    switch (liBehaviour)
    {
        case 6:
            return false;
        case 2:
            return false;
        case 4:
            return false;
        default:
            break;
    }

    if (mfSpeed >= 5.0f)
    {
        return false;
    }
    return true;
}

// 0x82706AC8 — append (segment, hull) to the history ring unless it duplicates the last.
void Param::PushHistory(u32 luSegmentIndex, u32 luHullIndex)
{
    CGS_ASSERT(muNextHistoryToWrite < KU_PARAM_NUM_SEGMENTS_TO_REMEMBER,
               "muNextHistoryToWrite < KU_PARAM_NUM_SEGMENTS_TO_REMEMBER");

    const u32 luLastEntry =
        (muNextHistoryToWrite + KU_PARAM_NUM_SEGMENTS_TO_REMEMBER - 1) % KU_PARAM_NUM_SEGMENTS_TO_REMEMBER;

    if (luSegmentIndex != mauHistorySegments[luLastEntry] ||
        luHullIndex != mauHistoryHulls[luLastEntry])
    {
        mauHistorySegments[muNextHistoryToWrite] = static_cast<u16>(luSegmentIndex);
        mauHistoryHulls[muNextHistoryToWrite] = static_cast<u16>(luHullIndex);
        muNextHistoryToWrite =
            static_cast<u8>((muNextHistoryToWrite + 1) % KU_PARAM_NUM_SEGMENTS_TO_REMEMBER);
    }
}

// 0x8274FA08 — read the history entry luHistoryIndex steps back from the write cursor.
void Param::GetHistoryEntry(u32 luHistoryIndex, u32* lpOutSegmentIndex, u32* lpOutHullIndex) const
{
    CGS_ASSERT(luHistoryIndex < KU_PARAM_NUM_SEGMENTS_TO_REMEMBER,
               "luHistoryIndex < KU_PARAM_NUM_SEGMENTS_TO_REMEMBER");
    CGS_ASSERT(muNextHistoryToWrite < KU_PARAM_NUM_SEGMENTS_TO_REMEMBER,
               "muNextHistoryToWrite < KU_PARAM_NUM_SEGMENTS_TO_REMEMBER");
    CGS_ASSERT(lpOutSegmentIndex != 0, "lpOutSegmentIndex");
    CGS_ASSERT(lpOutHullIndex != 0, "lpOutHullIndex");

    const u32 luIndexToRead =
        (muNextHistoryToWrite + (2 * KU_PARAM_NUM_SEGMENTS_TO_REMEMBER) - 1 - luHistoryIndex) %
        KU_PARAM_NUM_SEGMENTS_TO_REMEMBER;

    *lpOutSegmentIndex = mauHistorySegments[luIndexToRead];
    *lpOutHullIndex = mauHistoryHulls[luIndexToRead];
}

// 0x82713C68 — flag that this param changed section this frame.
void Param::SetChangedSection()
{
    CGS_ASSERT(IsAlive(), "IsAlive()");

    mxEffectAndHistoryState |= E_HISTORY_CHANGED_SECTION;
}

// 0x827134C0 — request removal (still alive at this point).
void Param::SetShouldBeRemoved()
{
    CGS_ASSERT(IsAlive(), "IsAlive()");

    mxFlags |= E_FLAG_SHOULD_BE_REMOVED;
}

// 0x82736918 (EXPORT HOLE) — divorce a param from its still-alive vehicle.
// FLAG: reconstructed from the sibling StaticTrafficParam::SetDivorced @0x82706CE8
// (assert IsAlive, assert !ShouldBeRemoved, then `mxFlags |= 0x40`). DELETE-WHEN the
// export hole is filled.
void Param::SetDivorced()
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT((mxFlags & E_FLAG_SHOULD_BE_REMOVED) == 0, "!ShouldBeRemoved()");

    mxFlags |= E_FLAG_DIVORCED;
}

// 0x82713CD0 — enter / leave the purgatory list (toggles the 0x80 flag bit).
void Param::SetInPurgatory(bool lbInPurgatory)
{
    if (lbInPurgatory)
    {
        CGS_ASSERT(!IsInPurgatory(), "!IsInPurgatory()");
        mxFlags = static_cast<u8>(mxFlags | E_FLAG_IN_PURGATORY);
    }
    else
    {
        CGS_ASSERT(IsInPurgatory(), "IsInPurgatory()");
        mxFlags = static_cast<u8>(mxFlags & ~E_FLAG_IN_PURGATORY);
    }
}

// 0x82713130 — leave the dying state: clear this param's bit in the dying SoA set and
// clear the E_FLAG_DYING flag.
void Param::ClearDying(u32 luParam, ParamSoaData& lSoaData)
{
    CGS_ASSERT(!IsAlive(), "!IsAlive()");
    CGS_ASSERT(IsDying(), "IsDying()");
    CGS_ASSERT(luParam < 600, "luParam in range");
    CGS_ASSERT(!lSoaData.mAliveParams.IsBitSet(luParam), "!lSoaData.mAliveParams.IsBitSet( luParam )");

    mxFlags = static_cast<u8>(mxFlags & ~E_FLAG_DYING);

    CGS_ASSERT(lSoaData.mDyingParams.IsBitSet(luParam), "lSoaData.mDyingParams.IsBitSet( luParam )");
    lSoaData.mDyingParams.UnSetBit(luParam);
}

// 0x82713528 — mark a still-alive param as a zombie: set its bit in the zombie SoA set
// and set the E_FLAG_ZOMBIE flag.
void Param::SetZombie(u32 luParam, ParamSoaData& lSoaData)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(luParam < 600, "luParam in range");
    CGS_ASSERT(lSoaData.mAliveParams.IsBitSet(luParam), "lSoaData.mAliveParams.IsBitSet( luParam )");

    mxFlags |= E_FLAG_ZOMBIE;

    lSoaData.mZombieParams.SetBit(luParam);
}

// 0x82713830 — transition a live param into the dying state. Asm store order:
//   clear alive SoA bit, clear E_FLAG_ALIVE; mask mxFlags with 0x8F, clear zombie SoA bit;
//   set E_FLAG_DYING, set dying SoA bit; set E_HISTORY_DIED.
void Param::SetDyingState(u32 luParam, ParamSoaData& lSoaData)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(!HasDied(), "!HasDied()");
    CGS_ASSERT(luParam < 600, "luParam in range");
    CGS_ASSERT(lSoaData.mAliveParams.IsBitSet(luParam), "lSoaData.mAliveParams.IsBitSet( luParam )");

    mxFlags = static_cast<u8>(mxFlags & ~E_FLAG_ALIVE);
    lSoaData.mAliveParams.UnSetBit(luParam);

    // asm: andi. r11, r11, 0x8F  (clear bits 0x40 and 0x30 -> drops should-be-removed/zombie)
    mxFlags = static_cast<u8>(mxFlags & 0x8F);
    lSoaData.mZombieParams.UnSetBit(luParam);

    mxFlags |= E_FLAG_DYING;
    lSoaData.mDyingParams.SetBit(luParam);

    mxEffectAndHistoryState |= E_HISTORY_DIED;
}

// 0x82917650 — right-indicator logic: indicating right when the current section turns
// right, or the queued plan(s) commit to a right turn. Matches the X360 branch tree
// (note: the second plan is only inspected when its type is E_TYPE_CHANGE_SECTION).
bool Param::ShouldBeIndicatingRight() const
{
    // E_DIR_STRAIGHT_ON = 0, E_DIR_LEFT = 1, E_DIR_RIGHT = 2 (BrnTrafficSharedConstants.h).
    if (muCurrentSectionDirection == 2) // E_DIR_RIGHT
    {
        return true;
    }
    if (muCurrentSectionDirection != 0) // not E_DIR_STRAIGHT_ON
    {
        return false;
    }

    const u8 luPlan0Type = maPlans[0].muType;
    if (luPlan0Type != ParamPlan::E_TYPE_CHANGE_LANE &&
        luPlan0Type != ParamPlan::E_TYPE_CHANGE_SECTION)
    {
        return false;
    }

    if (maPlans[0].muDirection == 2) // E_DIR_RIGHT
    {
        return true;
    }
    if (maPlans[0].muDirection != 0) // not E_DIR_STRAIGHT_ON
    {
        return false;
    }

    // X360 only tests plan[1].muType == E_TYPE_CHANGE_SECTION here.
    return maPlans[1].muType == ParamPlan::E_TYPE_CHANGE_SECTION &&
           maPlans[1].muDirection == 2; // E_DIR_RIGHT
}

// ===========================================================================
// BrnTraffic::ParamTransform -- per-vehicle orientation/position transform block.
// ===========================================================================

// 0x82712500 -- the render-lerped position (xyz of mLerpedPosAndSpeed; the w lane holds
// speed). The X360 IsValid guard checks only the X/Y/Z lanes, then returns the vector.
Vector3 ParamTransform::GetLerpedPos() const
{
    CGS_ASSERT(rw::math::vpu::IsValid(mLerpedPosAndSpeed.GetVector3()),
               "RwMath::IsValid( mLerpedPosAndSpeed.GetVector3() )");

    return mLerpedPosAndSpeed.GetVector3();
}

// 0x827126A0 -- the vehicle's right axis (mRight @0x20), guarded by an IsValid NaN check.
Vector3 ParamTransform::GetRight() const
{
    CGS_ASSERT(rw::math::vpu::IsValid(mRight), "RwMath::IsValid( mRight )");

    return mRight;
}

// 0x82712770 -- up = Normalize( Cross( GetRight(), GetDirection() ) ). The X360 first asserts
// the direction and right axes are not (near-)equal (a degenerate cross would follow), then
// builds the up axis. The source-level guard is !IsSimilar(dir, right); there is no linkable
// rw::math::vpu::IsSimilar(Vector3,Vector3) in the vendor home, so it is expressed as the
// equivalent !IsZero(dir - right) (asm: vsubfp Direction-Right, vandc fabs, vcmpgtfp |diff| >
// eps). The exact rodata epsilon (unk_820BA240) is un-valued; IsZero's default tolerance stands
// in for this debug-only guard.
Vector3 ParamTransform::CalcUp() const
{
    const Vector3 lDirection = GetDirection();
    const Vector3 lRight     = GetRight();

    CGS_ASSERT(!rw::math::vpu::IsZero(lDirection - lRight),
               "!IsSimilar( GetDirection(), GetRight() )");

    return rw::math::vpu::Normalize(rw::math::vpu::Cross(GetRight(), GetDirection()));
}

// 0x827128E0 -- the lerped speed scalar (the w/plus lane of mLerpedPosAndSpeed), returned
// broadcast across all four lanes as a VecFloat (asm: vspltw lane 3 -> full-register store).
// The IsValid guard validates ONLY that plus lane (asm vspltw v0,v0,3 then vcmpeqfp self-
// compare); modelled as the scalar `lf == lf` NaN self-test (same convention as
// Param::SetParamAlong in this file), since there is no rw::math::vpu::IsValid(float).
VecFloat ParamTransform::GetSpeed() const
{
    const f32 lfSpeed = mLerpedPosAndSpeed.GetPlus();

    CGS_ASSERT(lfSpeed == lfSpeed, "RwMath::IsValid( mLerpedPosAndSpeed.GetPlus() )");

    return VecFloat{ lfSpeed, lfSpeed, lfSpeed, lfSpeed };
}

// 0x82712BA8 -- seed the transform from the spawn basis + speed. Stores (targets/order exact
// from the asm):
//   mPos               = lPos                                      (@0x00)
//   mDirAndAccel       = { lDir.xyz, plus = 0 }                    (@0x10, accel lane cleared)
//   mRight             = lRight                                    (@0x20)
//   mLerpedPosAndSpeed = { (lPos - lDir*lfSpeed*KF_INIT_LERP_STEP).xyz, plus = lfSpeed } (@0x30)
void ParamTransform::Initialise(Vector3 lPos, Vector3 lDir, Vector3 lRight, VecFloat lfSpeed)
{
    CGS_ASSERT(rw::math::vpu::IsValid(lPos),   "RwMath::IsValid( lPos )");
    CGS_ASSERT(rw::math::vpu::IsValid(lDir),   "RwMath::IsValid( lDir )");
    CGS_ASSERT(rw::math::vpu::IsValid(lRight), "RwMath::IsValid( lRight )");
    CGS_ASSERT(lfSpeed.x == lfSpeed.x,         "RwMath::IsValid( lfSpeed )");

    // flt_82004014 = 0.1f (rodata 3D CC CC CD @0x82004014).
    const f32 KF_INIT_LERP_STEP = 0.1f;

    mPos   = lPos;
    mRight = lRight;

    mDirAndAccel.SetVector3(lDir);
    mDirAndAccel.SetPlus(0.0f);

    const f32 lfSpeedScalar = lfSpeed.x;
    const f32 lfStep        = lfSpeedScalar * KF_INIT_LERP_STEP;
    mLerpedPosAndSpeed.SetVector3(Vector3{
        lPos.x - lDir.x * lfStep,
        lPos.y - lDir.y * lfStep,
        lPos.z - lDir.z * lfStep,
        0.0f });
    mLerpedPosAndSpeed.SetPlus(lfSpeedScalar);
}

// 0x82751AF8 -- the pool-wide reset, run 400 times from Reset @0x8272CDA0. The asm loads the
// two SDK basis registers by address: unk_82181520 == (0,0,1,0) and
// rw::math::vpu::detail::gIVector @0x82181500 == (1,0,0,0).
void ParamTransform::Construct()
{
    mPos.SetZero();

    mDirAndAccel.SetVector3(rw::math::vpu::GetVector3_ZAxis());
    mDirAndAccel.SetPlus(0.0f);

    mRight = rw::math::vpu::GetVector3_XAxis();

    mLerpedPosAndSpeed.SetVector3(Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });
    mLerpedPosAndSpeed.SetPlus(0.0f);
}

// 0x82712430 -- EXPORT HOLE (no per-function JSON at that address; UpdateParam_CheckIfNeedToSlow,
// UpdateParams_PrecalcBehaviourParams, GetDeterministicParamPos @0x82714258 and five
// DebugComponent draw walks all call it by name). The body is the byte-identical shape of the
// three sibling accessors: the entry spans 0x82712430..0x827124FF, exactly the 0xD0 bytes
// GetLerpedPos / GetDirection / GetRight each occupy, and each of those is one IsValid guard plus
// a return of its own member. The guard string is the one ParamTransform::Update fires at
// BrnTrafficParam.h:700. DELETE-WHEN 0x82712430 is exported and the body can be read directly.
Vector3 ParamTransform::GetDeterministicPos() const
{
    CGS_ASSERT(rw::math::vpu::IsValid(mPos), "RwMath::IsValid( mPos )");

    return mPos;
}

// 0x827125D0 -- the forward axis (xyz of mDirAndAccel; the w lane holds acceleration).
// Replaces the FLAGged inline stub that used to live in BrnTrafficParam.h.
Vector3 ParamTransform::GetDirection() const
{
    CGS_ASSERT(rw::math::vpu::IsValid(mDirAndAccel.GetVector3()),
               "RwMath::IsValid( mDirAndAccel.GetVector3() )");

    return mDirAndAccel.GetVector3();
}

// 0x82712968 -- advance the render-lerped position one sim step under the stored acceleration.
// asm: speed' = max(speed + accel*dt, 0) (vmaddfp then vmaxfp against vspltisw 0); pos' =
// pos + dir*speed'*dt (vmulfp then vmaddfp); both lanes stored back through vrlimi128, so the
// vector store and the plus store are two separate writes to mLerpedPosAndSpeed.
void ParamTransform::UpdateLerpedPosition(VecFloat lfSimTimeStep)
{
    CGS_ASSERT(rw::math::vpu::IsValid(mLerpedPosAndSpeed.GetVector3()),
               "RwMath::IsValid( mLerpedPosAndSpeed.GetVector3() )");
    CGS_ASSERT(mLerpedPosAndSpeed.GetPlus() == mLerpedPosAndSpeed.GetPlus(),
               "RwMath::IsValid( mLerpedPosAndSpeed.GetPlus() )");
    CGS_ASSERT(rw::math::vpu::IsValid(mDirAndAccel.GetVector3()),
               "RwMath::IsValid( mDirAndAccel.GetVector3() )");
    CGS_ASSERT(mDirAndAccel.GetPlus() == mDirAndAccel.GetPlus(),
               "RwMath::IsValid( mDirAndAccel.GetPlus() )");
    CGS_ASSERT(lfSimTimeStep.x == lfSimTimeStep.x, "RwMath::IsValid( lfSimTimeStep )");

    const f32 lfStep     = lfSimTimeStep.x;
    const f32 lfAccelled = mLerpedPosAndSpeed.GetPlus() + mDirAndAccel.GetPlus() * lfStep;
    const f32 lfSpeed    = (lfAccelled > 0.0f) ? lfAccelled : 0.0f;   // asm: vmaxfp vs zero

    mLerpedPosAndSpeed.SetVector3(mLerpedPosAndSpeed.GetVector3()
                                 + mDirAndAccel.GetVector3() * (lfSpeed * lfStep));
    mLerpedPosAndSpeed.SetPlus(lfSpeed);
}

// 0x82712E28 -- per-decision-frame restamp. The previous deterministic position becomes the
// lerp origin BEFORE mPos takes the new one (the asm loads this+0x00 into v0 before the
// this+0x00 store), and the fifth argument is the acceleration lane, not a blend factor.
void ParamTransform::Update(Vector3 lPos, Vector3 lDir, Vector3 lRight,
                            VecFloat lfSpeed, VecFloat lfAcceleration)
{
    CGS_ASSERT(rw::math::vpu::IsValid(lPos),   "RwMath::IsValid( lPos )");
    CGS_ASSERT(rw::math::vpu::IsValid(lDir),   "RwMath::IsValid( lDir )");
    CGS_ASSERT(rw::math::vpu::IsValid(lRight), "RwMath::IsValid( lRight )");
    CGS_ASSERT(lfSpeed.x == lfSpeed.x,               "RwMath::IsValid( lfSpeed )");
    CGS_ASSERT(lfAcceleration.x == lfAcceleration.x, "RwMath::IsValid( lfAcceleration )");
    CGS_ASSERT(rw::math::vpu::IsValid(mPos), "RwMath::IsValid( mPos )");

    mLerpedPosAndSpeed.SetVector3(mPos);
    mLerpedPosAndSpeed.SetPlus(lfSpeed.x);

    mPos = lPos;

    mDirAndAccel.SetVector3(lDir);
    mDirAndAccel.SetPlus(lfAcceleration.x);

    mRight = lRight;
}

// ===========================================================================
// BrnTraffic::ParamListNode / BrnTraffic::ParamNeedToSlowData
// ===========================================================================

// 0x82751C38 -- Reset @0x8272CDA0 runs this over all 400 nodes.
void ParamListNode::Construct()
{
    mfParamAlong = 0.0f;                                  // rodata flt_82001CC0
    muNextParam  = static_cast<u16>(KU_INVALID_PARAM);
    muPrevParam  = static_cast<u16>(KU_INVALID_PARAM);
}

// Console-inlined, no standalone symbol. The only attested body is the five-store seed
// Reset @0x8272CDA0 emits per param inside the same loop that calls Param::Construct
// (u16 0xFFFF at +0, s8 -1 at +2, FLT_MAX at +4 / +8 / +12).
// FLAG: DWARF declares Construct and Clear separately (BrnTrafficParam.h:452/:456); only one
// body is attested, so Construct delegates. DELETE-WHEN a site that inlines Clear alone turns up.
void ParamNeedToSlowData::Clear()
{
    muParamInFront  = static_cast<u16>(KU_INVALID_PARAM);
    miBehaviour     = -1;
    mfNextParamDist = FLT_MAX;
    mfTargetSpeed   = FLT_MAX;
    mfStopDist      = FLT_MAX;
}

void ParamNeedToSlowData::Construct()
{
    Clear();
}
}
