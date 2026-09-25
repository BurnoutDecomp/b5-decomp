// ============================================================================
// GameSource/Director/BrnCrashAnalyser.cpp
//
// BrnDirector::CrashAnalyser::Update @0x82209290 (DWARF BrnCrashAnalyser.cpp:44), reconstructed from
// BURNOUT_X360_ARTIST.XEX. Its one caller is MainDirector::PreSceneQueryUpdate @0x8225BCDC.
//
// WHAT IT DOES. Once per pre-scene pass it snapshots last frame's verdict and re-derives this frame's
// from the director's GameState and the player's published crash record. The event word is only
// ever non-zero on the FIRST frame of a crash (the rising edge of GameState::mbCrashActive):
//     CrashStart                          every crash start
//     | WorldImpact | HardStop            the car hard-stopped against the world
//     | CarImpact   | HardStop            ... or against another car
// and, for a hard stop, which face of the car took it: the contact normal is flattened onto the
// car's ground plane, scaled into "box units" by the reciprocal half extents of the car's AABB
// (so a hit on the long side of a long car is not mistaken for a hit on its nose), normalised,
// and compared against the car's X and Z axes at cos(67.5 deg) -- the angle at which the two
// faces of the box overlap equally. Left / Right come from X, Front / Rear from Z, and the
// "suggest the left of the line of action" verdict is the side the summed face normals point away
// from. MomentHardStop (CrashStart gate, side pick) and the shot selector (event suitability) read
// the result through MainDirector::UpdateMoments' MomentSharedInfo::mpCrashAnalysis.
//
// ASM WALK (@0x82209290):
//   0x822092C4  r27 = lpInput + 0x78E0            -> GetPlayerCrashInfo()     (inlined, no lock test)
//   0x822092C8  bl sub_82207040                   -> GetRaceCarInfo()          (the VehicleInfo[8] base)
//   0x822092CC  mLastAnalysis = mAnalysis         (two word copies, +0/+4 -> +8/+0xC)
//   0x822092EC  mAnalysis.mxEventFlags = 0
//   0x822092F4  mAnalysis.mbIsPlayerCrashing = lpGameState->mbCrashActive   (lbz 0xF9)
//   0x822092F8  if (mbCrashActive && mAnalysis.mbIsPlayerCrashing && !mLastAnalysis.mbIsPlayerCrashing)
//   0x8220931C    flags = CrashStart; mbHardstopVsWall (+0x24) -> = 0x29; mbHardStopVsAI (+0x25) -> |= 0x11
//   0x82209350    if (flags & HardStop)
//                   the box-space face test below; mbSuggestLeftOfLineOfAction; IsZero(normal) -> Clear()
//
// FLAG (PC-platform, numeric -- the rw::math::vpu precedent: vector3_operation.h Divide / Normalize,
// matrix44affine_operation.h MakeRotation*): the console forms the reciprocal half extents with
// vrefp + two Newton steps (0x822093B4..0x822093C8) and the normalisation with vrsqrtefp + two
// Newton steps (0x822093DC..0x822093FC); both are exact 1/x and 1/sqrt(x) here. The console's
// normalisation has NO zero guard (no vcmpeqfp/vsel), and neither does this one: a vertical
// normal gives 0 * inf = NaN on both, every compare fails, and the verdict is the same.
// ============================================================================

#include "GameSource/Director/BrnCrashAnalyser.h"

#include <cmath>                                                        // std::cos / std::sqrt / std::fabs
#include "BrnCommonTypes.h"                                             // Vector3 / Matrix44Affine
#include "rw/math/vpu/vector3_operation.h"                              // Dot, operator-, operator*
#include "GameSource/AttribSys/Enums/CrashEvents.h"                     // AttribSys::Enums::CrashEvents
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"      // DirectorIO::InputBuffer
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"     // GameState::mbCrashActive
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"          // Camera::VehicleInfo / PlayerCrashInfo

namespace BrnDirector
{
namespace
{
    // dbl_820065E8 = 0x3FF2D97C7CC00000 = 1.178097235970199, the argument of the double `cos` call
    // @0x82209414 (`lfd f1` @0x82209398, then `frsp` @0x82209420). It is 67.5 * 0.017453292f folded
    // in double precision (67.5 deg, the angle at which the car box's two faces overlap equally),
    // hence the value just short of 3*pi/8.
    const f64 KD_ANGLE_FOR_EQUAL_OVERLAP = 1.178097235970199;

    // flt_82001770 = 0x34000000 = FLT_EPSILON, the IsZero tolerance @0x8220953C.
    const f32 KF_ZERO_NORMAL_TOLERANCE = 1.1920929e-07f;

    // flt_82001DA0 (0.5f) is what the `vcfsx v12, v0(1), 1` idiom @0x82209390 builds: the half in
    // (max - min) * 0.5.
    const f32 KF_HALF = 0.5f;

    // rw::math::vpu::detail::gIVector rows 0 and 2 (0x82181500 / 0x82181520, x360rd): the car-space
    // X and Z unit axes the left-of-action-line accumulator steps along.
    const Vector3 KV_X_AXIS = { 1.0f, 0.0f, 0.0f, 0.0f };
    const Vector3 KV_Z_AXIS = { 0.0f, 0.0f, 1.0f, 0.0f };
}

// @0x82209290
void CrashAnalyser::Update(const DirectorIO::InputBuffer* lpInput, const GameState* lpGameState,
                           EActiveRaceCarIndex lePlayerCarIndex)
{
    using namespace AttribSys::Enums;   // DWARF cpp:46

    const Camera::PlayerCrashInfo* lpPlayerCrashInfo = lpInput->GetPlayerCrashInfo();             // cpp:47
    const Camera::VehicleInfo* lpPlayerVehicleInfo = &lpInput->GetRaceCarInfo()[lePlayerCarIndex];   // cpp:48

    mLastAnalysis = mAnalysis;
    mAnalysis.mxEventFlags       = CrashEvents::None;
    mAnalysis.mbIsPlayerCrashing = lpGameState->mbCrashActive;

    if (lpGameState->mbCrashActive && mAnalysis.mbIsPlayerCrashing && !mLastAnalysis.mbIsPlayerCrashing)
    {
        mAnalysis.mxEventFlags = CrashEvents::CrashStart;
        if (lpPlayerCrashInfo->mbHardstopVsWall)
            mAnalysis.mxEventFlags = CrashEvents::CrashStart | CrashEvents::WorldImpact | CrashEvents::HardStop;
        if (lpPlayerCrashInfo->mbHardStopVsAI)
            mAnalysis.mxEventFlags |= CrashEvents::CarImpact | CrashEvents::HardStop;

        if ((mAnalysis.mxEventFlags & CrashEvents::HardStop) != 0)
        {
            const Matrix44Affine& lrCarTransform = lpPlayerVehicleInfo->mRaceCarState.mTransform;
            const Vector3&        lrNormal       = lpPlayerCrashInfo->mvCollisionNormal;

            // cpp:167 / :168 -- the car box's half extents and their reciprocal (One / half).
            const Vector3 lHalfExtents = (lpPlayerVehicleInfo->mAABB.mMax - lpPlayerVehicleInfo->mAABB.mMin) * KF_HALF;
            const Vector3 lReciprocalHalfExtents = { 1.0f / lHalfExtents.x, 1.0f / lHalfExtents.y,
                                                     1.0f / lHalfExtents.z, 1.0f / lHalfExtents.w };

            // cpp:170 -- the contact normal with its component along the car's up axis removed.
            const Vector3 lCurrentCollisionNormalNoY =
                lrNormal - lrCarTransform.yAxis * rw::math::vpu::Dot(lrNormal, lrCarTransform.yAxis);

            // cpp:171 -- into box units, then Normalize (no zero guard -- see the banner).
            const Vector3 lAdjustedCollisionNormal =
                rw::math::vpu::Mult(lCurrentCollisionNormalNoY, lReciprocalHalfExtents);
            const f32 lfInverseLength =
                1.0f / std::sqrt(rw::math::vpu::Dot(lAdjustedCollisionNormal, lAdjustedCollisionNormal));
            const Vector3 lCurrentNormalisedAdjustedCollisionNormal = lAdjustedCollisionNormal * lfInverseLength;

            // cpp:172 / :173 / :174.
            const f32 lXAngle = rw::math::vpu::Dot(lCurrentNormalisedAdjustedCollisionNormal, lrCarTransform.xAxis);
            const f32 lZAngle = rw::math::vpu::Dot(lCurrentNormalisedAdjustedCollisionNormal, lrCarTransform.zAxis);
            const f32 lCosAngleForEqualOverlap = static_cast<f32>(std::cos(KD_ANGLE_FOR_EQUAL_OVERLAP));

            // cpp:176.
            Vector3 lCarSpaceLeftOfActionLine;
            lCarSpaceLeftOfActionLine.SetZero();

            if (lXAngle > lCosAngleForEqualOverlap)
            {
                mAnalysis.mxEventFlags |= CrashEvents::LeftImpact;
                lCarSpaceLeftOfActionLine = lCarSpaceLeftOfActionLine + KV_Z_AXIS;
            }
            if (lXAngle < -lCosAngleForEqualOverlap)
            {
                mAnalysis.mxEventFlags |= CrashEvents::RightImpact;
                lCarSpaceLeftOfActionLine = lCarSpaceLeftOfActionLine - KV_Z_AXIS;
            }
            if (lZAngle > lCosAngleForEqualOverlap)
            {
                mAnalysis.mxEventFlags |= CrashEvents::FrontImpact;
                lCarSpaceLeftOfActionLine = lCarSpaceLeftOfActionLine - KV_X_AXIS;
            }
            if (lZAngle < -lCosAngleForEqualOverlap)
            {
                mAnalysis.mxEventFlags |= CrashEvents::RearImpact;
                lCarSpaceLeftOfActionLine = lCarSpaceLeftOfActionLine + KV_X_AXIS;
            }

            mAnalysis.mbSuggestLeftOfLineOfAction =
                !(rw::math::vpu::Dot(lCarSpaceLeftOfActionLine, lCurrentNormalisedAdjustedCollisionNormal) > 0.0f);

            // rw::math::vpu::IsZero(normal): the console tests |x|, |y|, |z| (and |x| again in the w
            // lane, vrlimi128 @0x82209558) with `vcmpgtfp.` against FLT_EPSILON and reads CR6's
            // "no lane true" bit. Written as that predicate, NOT as rw IsZero's `<=`, so a NaN
            // normal counts as zero exactly as it does on the console.
            if (!(std::fabs(lrNormal.x) > KF_ZERO_NORMAL_TOLERANCE
                  || std::fabs(lrNormal.y) > KF_ZERO_NORMAL_TOLERANCE
                  || std::fabs(lrNormal.z) > KF_ZERO_NORMAL_TOLERANCE))
            {
                mAnalysis.Clear();
            }
        }
    }
}

} // namespace BrnDirector
