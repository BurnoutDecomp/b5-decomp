// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.cpp
//
// BrnDirector::Camera::BehaviourRoadRunner -- THE DJ FLY-BY CAMERA BEHAVIOUR.
//
// Bodied here:
//   BehaviourRoadRunner::Construct           @0x8222BCE0   (full field sweep)
//   BehaviourRoadRunner::Prepare             @0x8220F748   (full float sweep; returns true)
//   BehaviourRoadRunner::Reverse             (BrnBehaviourRoadRunner.h:331, inlined on console)
//   BehaviourRoadRunner::GetName             @0x821FB130
//   BehaviourRoadRunner::GetParameters / SetParameters
//   BehaviourRoadRunner::Update              @0x82247E98   -- ⚠️ the un-prepared leg only (see below)
//   BehaviourRoadRunner::PostCollisionUpdate @0x8220F850   -- ⚠️ gated
//   TrafficLaneTruck::GetTransform           @0x821F53D8
//   TrafficLaneTruck::GetLocalAngularVelocity@0x821F5470
//   TrafficLaneTruck::GetLinearVelocity      @0x821F54E0
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.h"
#include <cstddef>   // offsetof

// The lane graph the truck walks (TrafficLaneTruck::Prepare / CalcTransformFromLanePosition).
// BrnTrafficSection.h MUST precede BrnTrafficHull.h: it sets BRNTRAFFIC_SECTION_DEFINED so the
// real 48-byte Section wins over Hull.h's placeholder (the same ordering BrnDirectorWorldMap.cpp
// documents).
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"   // BrnTraffic::TrafficData::GetHull
#include "SharedClasses/Traffic/BrnTrafficSection.h"            // BrnTraffic::Section::CalcDirectionAtParameter
#include "SharedClasses/Traffic/BrnTrafficHull.h"               // BrnTraffic::Hull::GetSection / mpaRungs
#include "rw/math/vpu/vector3_operation.h"                      // operator+ (lane point + direction)
#include "rw/math/vpu/matrix44affine_operation.h"               // SLerp / Mult / InverseOfMatrixWithOrthonormal3x3
#include "rw/math/fpu/scalar_operation.h"                       // rw::math::fpu::IsZero (the timestep guard)
#include <cmath>                                                // std::floor (the lane walkers)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // the lane-seated diagnostic

namespace BrnDirector
{
namespace Camera
{

// Pin the asm-attested member offsets the accessors sample. Every member of the truck is
// size-stable (no pointers), so these hold exactly on the host too.
static_assert(offsetof(TrafficLaneTruck, mTransform)            == 0x20, "transform @ +0x20");
static_assert(offsetof(TrafficLaneTruck, mLocalAngularVelocity) == 0x60, "angular velocity @ +0x60");
static_assert(offsetof(TrafficLaneTruck, mLinearVelocity)       == 0x70, "linear velocity @ +0x70");
static_assert(offsetof(TrafficLaneTruck, mfSpeed)               == 0x80, "speed @ +0x80");
static_assert(offsetof(TrafficLaneTruck, mbPrepared)            == 0x8C, "mbPrepared @ +0x8C");

// ----------------------------------------------------------------------------
// TrafficLaneTruck::CalcTransformFromLanePosition @0x8222A640
//
// Build the truck's world transform from its current lane position: sample the lane's forward
// direction at the current (section, rung, parameter) and make a look-at frame from the lane
// point toward point + direction.
//
//   assert(worldMap.meLoadingState == E_LOADING_STATE_LOADED)   // BrnDirectorWorldMap.h:93
//   hull    = trafficData->mpapHulls[mLanePosition.muHullIndex] // *(4*(this+24) + *(td+12))
//   section = Hull::GetSection(hull, mLanePosition.muSection)   // *(this+28)
//   section->CalcDirectionAtParameter(hull->mpaRungs /*hull+20*/,
//                                     broadcast(mLanePosition.mfParamAlongSection /*this+20*/),
//                                     mLanePosition.muRung /*this+29*/, lDirection)
//   mTransform /*this+32*/ = Utils::CreateLookAt(mLanePosition.mPosition,
//                                                mLanePosition.mPosition + lDirection)
//     (the vaddfp of v1 = *this+0 and v0 = lDirection feeds CreateLookAt's second vector arg;
//      its 4-row sret is then copied row-by-row into this+0x20..0x50)
// ----------------------------------------------------------------------------
void TrafficLaneTruck::CalcTransformFromLanePosition(const BrnDirector::WorldMap& lrWorldMap)
{
    const BrnTraffic::TrafficData* lpTrafficData = lrWorldMap.GetTrafficData();
    const BrnTraffic::Hull*        lpHull        = lpTrafficData->GetHull(mLanePosition.muHullIndex);
    const BrnTraffic::Section*     lpSection     = lpHull->GetSection(mLanePosition.muSection);

    // ::VecFloat -- the GLOBAL broadcast-lane type BrnTraffic::Section takes. Unqualified
    // `VecFloat` in this namespace would bind to BrnDirector::VecFloat (the Timestep register),
    // which is a different type.
    const f32        lfParam = mLanePosition.mfParamAlongSection;
    const ::VecFloat lParam  = { lfParam, lfParam, lfParam, lfParam };

    rw::math::vpu::Vector3 lDirection;
    lpSection->CalcDirectionAtParameter(lpHull->mpaRungs, lParam, mLanePosition.muRung, lDirection);

    // The look-at build (asm 0x8222A6CC..0x8222A70C): v1 = *this+0 == mLanePosition.mPosition,
    // v0 = the sampled lane direction, `vaddfp v2, v1, v0` makes the target, and the four rows
    // of CreateLookAt's sret are copied into this+0x20..0x50.
    mTransform = Utils::CreateLookAt(mLanePosition.mPosition,
                                     rw::math::vpu::Add(mLanePosition.mPosition, lDirection));

    // [diagnostic, one-shot] the lane sample is REAL and is the end-to-end proof that the
    // ported lane graph walks: hull -> section -> rung table -> a lane tangent -> a frame.
    {
        static bool sbLogged = false;
        if (!sbLogged)
        {
            sbLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[RoadRunner] lane seated: hull=" << (s32)mLanePosition.muHullIndex
                    << " section=" << (s32)mLanePosition.muSection
                    << " rung=" << (s32)mLanePosition.muRung
                    << " pos=(" << mLanePosition.mPosition.x << "," << mLanePosition.mPosition.y
                    << "," << mLanePosition.mPosition.z << ")"
                    << " dir=(" << lDirection.x << "," << lDirection.y << "," << lDirection.z << ")"
                    << " frameX=(" << mTransform.xAxis.x << "," << mTransform.xAxis.y
                    << "," << mTransform.xAxis.z << ")"
                    << " frameY=(" << mTransform.yAxis.x << "," << mTransform.yAxis.y
                    << "," << mTransform.yAxis.z << ")\n";
            }
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficLaneTruck::Prepare @0x82247A08 -- seat the truck on the lane nearest lPoint.
//
//   mLanePosition = worldMap.GetLanePositionNearestPoint(lPoint);   // 4 QWORD copies (32B)
//   if (!mLanePosition.mbValid /*this+30*/) return false;           // li r3,0
//   CalcTransformFromLanePosition(worldMap);
//   mLocalAngularVelocity /*this+96*/  = 0;      // stvx128 v0 (vspltisw 0)
//   mLinearVelocity       /*this+112*/ = 0;      // stvx128 v0
//   mfSpeed               /*this+128*/ = 20.0f;
//   mfBlendDistance       /*this+136*/ = 20.0f;
//   mfTransformBlendAmount/*this+132*/ = 0.1f;
//   mbPrepared            /*this+140*/ = true;
//   return true;
//
// ⭐ THIS IS THE FUNCTION THE WHOLE LANE-DATA CAMPAIGN WAS FOR. Its one gate is
// mLanePosition.mbValid, which GetLanePositionNearestPoint only raises when the world map's
// TrafficData is really loaded -- which it now is (WorldMap::LoadData is live).
// ----------------------------------------------------------------------------
bool TrafficLaneTruck::Prepare(const BrnDirector::WorldMap& lrWorldMap,
                               rw::math::vpu::Vector3 lPoint)
{
    mLanePosition = lrWorldMap.GetLanePositionNearestPoint(lPoint);

    if (!mLanePosition.mbValid)
        return false;

    CalcTransformFromLanePosition(lrWorldMap);

    mLocalAngularVelocity  = rw::math::vpu::Vector3();
    mLinearVelocity        = rw::math::vpu::Vector3();
    mfSpeed                = 20.0f;
    mfBlendDistance        = 20.0f;
    mfTransformBlendAmount = 0.1f;
    mbPrepared             = true;
    return true;
}

// ============================================================================
// ⭐ THE LANE MOTION.  PickSplitToTake / MoveAlongTrafficLane{,Forwards,Backwards} /
// TrafficLaneTruck::Update -- the chain that actually MOVES the fly-by camera.
//
// All four walkers are STATIC on the console: the first argument register holds the preferred
// DIRECTION, not a `this` (asm 0x82247B98 `li r3, 1` right before the call). They take the
// lane position and the transform as out-parameters instead of mutating members, which is why
// Update can run two independent walks (one at the frame's distance, one further ahead) off
// the same starting position.
// ============================================================================

// ----------------------------------------------------------------------------
// PickSplitToTake @0x821FAE58 -- which way to continue when a section runs out.
//
//   assert(lePreferredDirection < E_DIRECTIONS_COUNT)   (:161)
//   assert(lpOutSection) (:162)  assert(lpOutHull) (:163)  assert(lpOutDirection) (:164)
//   leDir = lePreferredDirection
//   if (mauForwardSections[leDir] == 0xFF) { leDir = E_DIR_LEFT;
//   if (mauForwardSections[leDir] == 0xFF) { leDir = E_DIR_RIGHT;
//   if (mauForwardSections[leDir] == 0xFF) { leDir = E_DIR_STRAIGHT_ON;
//   if (mauForwardSections[leDir] == 0xFF) { *lpOutSection = 0xFF; *lpOutHull = 0xFFFF; return; }}}}
//   *lpOutSection   = mauForwardSections[leDir];
//   *lpOutHull      = mauForwardHulls[leDir];       // asm `(leDir+4)*2` off the section base
//   *lpOutDirection = leDir;
//
// The asm walks the fallbacks in the order 1, 2, 0 regardless of what was preferred (it
// re-tests the preferred one if it happens to be 1 or 2 -- harmless, and reproduced).
// ----------------------------------------------------------------------------
void TrafficLaneTruck::PickSplitToTake(const BrnTraffic::Section& lrSection,
                                       BrnTraffic::Directions lePreferredDirection,
                                       u8* lpOutSection, u16* lpOutHull, u8* lpOutDirection)
{
    CGS_ASSERT(lePreferredDirection < BrnTraffic::E_DIRECTIONS_COUNT,
               "lePreferredDirection < BrnTraffic::E_DIRECTIONS_COUNT");
    CGS_ASSERT(lpOutSection != 0, "lpOutSection");
    CGS_ASSERT(lpOutHull != 0, "lpOutHull");
    CGS_ASSERT(lpOutDirection != 0, "lpOutDirection");

    u32 luDirection = static_cast<u32>(lePreferredDirection);
    if (lrSection.mauForwardSections[luDirection] == 0xFF)
    {
        luDirection = BrnTraffic::E_DIR_LEFT;
        if (lrSection.mauForwardSections[luDirection] == 0xFF)
        {
            luDirection = BrnTraffic::E_DIR_RIGHT;
            if (lrSection.mauForwardSections[luDirection] == 0xFF)
            {
                luDirection = BrnTraffic::E_DIR_STRAIGHT_ON;
                if (lrSection.mauForwardSections[luDirection] == 0xFF)
                {
                    *lpOutSection = 0xFF;
                    *lpOutHull    = 0xFFFF;
                    return;
                }
            }
        }
    }

    *lpOutSection   = lrSection.mauForwardSections[luDirection];
    *lpOutHull      = lrSection.mauForwardHulls[luDirection];
    *lpOutDirection = static_cast<u8>(luDirection);
}

// ----------------------------------------------------------------------------
// PickSplitToTakeBackwards @0x821FAF90 -- the identical machine over the BACKWARD tables
// (mauBackwardSections @+0x17 / mauBackwardHulls @+0x0E, the asm's `(leDir+7)*2`). Assert
// lines :207/:208/:209/:210.
// ----------------------------------------------------------------------------
void TrafficLaneTruck::PickSplitToTakeBackwards(const BrnTraffic::Section& lrSection,
                                                BrnTraffic::Directions lePreferredDirection,
                                                u8* lpOutSection, u16* lpOutHull,
                                                u8* lpOutDirection)
{
    CGS_ASSERT(lePreferredDirection < BrnTraffic::E_DIRECTIONS_COUNT,
               "lePreferredDirection < BrnTraffic::E_DIRECTIONS_COUNT");
    CGS_ASSERT(lpOutSection != 0, "lpOutSection");
    CGS_ASSERT(lpOutHull != 0, "lpOutHull");
    CGS_ASSERT(lpOutDirection != 0, "lpOutDirection");

    u32 luDirection = static_cast<u32>(lePreferredDirection);
    if (lrSection.mauBackwardSections[luDirection] == 0xFF)
    {
        luDirection = BrnTraffic::E_DIR_LEFT;
        if (lrSection.mauBackwardSections[luDirection] == 0xFF)
        {
            luDirection = BrnTraffic::E_DIR_RIGHT;
            if (lrSection.mauBackwardSections[luDirection] == 0xFF)
            {
                luDirection = BrnTraffic::E_DIR_STRAIGHT_ON;
                if (lrSection.mauBackwardSections[luDirection] == 0xFF)
                {
                    *lpOutSection = 0xFF;
                    *lpOutHull    = 0xFFFF;
                    return;
                }
            }
        }
    }

    *lpOutSection   = lrSection.mauBackwardSections[luDirection];
    *lpOutHull      = lrSection.mauBackwardHulls[luDirection];
    *lpOutDirection = static_cast<u8>(luDirection);
}

namespace
{
    // flt_82008984 == 0.99900001. The walkers never let the local parameter reach the far rung
    // exactly; a segment is "used up" at 0.999 of the way across it. Dumped from the shipped
    // image (headless IDA 9.3), not guessed.
    const f32 KF_SEGMENT_END_PARAM = 0.99900001f;

    // flt_82001CC0 == 0.0f -- the dispatch threshold in MoveAlongTrafficLane and the
    // `lfDistToMove >= 0.0f` assert's comparand.
    const f32 KF_ZERO = 0.0f;

    // The 0xFF / 0xFFFF sentinels PickSplitToTake writes into a dead end.
    const u8  KU_INVALID_SECTION = 0xFF;
    const u16 KU_INVALID_HULL_ID = 0xFFFF;

    // The lane walkers' shared "we ran off the end" exit (asm 0x8222B050 / 0x8222BB90):
    // hand back an identity frame whose translation row is the lane position we started from,
    // and invalidate the lane position so the caller stops walking it.
    void EndOfLane(BrnDirector::WorldMap::LanePosition* lpPositionInOut,
                   rw::math::vpu::Matrix44Affine* lpTransformOut)
    {
        lpTransformOut->SetIdentity();
        lpTransformOut->wAxis = lpPositionInOut->mPosition;   // lvx128 r19 -> stvx128 r6+0x30
        lpPositionInOut->mbValid = false;                      // stb r26, 0x1E(r19)
    }
}

// ----------------------------------------------------------------------------
// MoveAlongTrafficLaneForwards @0x8222A728
//
// Consume lfDistToMove one lane segment at a time, taking a split whenever the section runs
// out, then sample the reached parameter into lpTransformOut.
//
//   assert(lpPositionInOut->mbValid)                              (:275)
//   lpHull   = lWorldMap.GetTrafficHullData(lpPositionInOut->muHullIndex)
//   lpSection= lpHull->GetSection(lpPositionInOut->muSection)
//   lfParamAlong  = lpPositionInOut->mfParamAlongSection          (lfs 0x14(r19))
//   luCurrentRung = lpPositionInOut->muRung                       (lbz 0x1D(r19))
//   lpaCumulativeLengths = lpHull->GetRungLengthsForSection(lpSection)
//   lfSegmentLength = lpaCumulativeLengths[rung+1] - lpaCumulativeLengths[rung]
//   luNumSegments   = lpSection->GetNumSegments()
//   assert(lfDistToMove >= 0.0f)                                  (:286)
//   for (;;) {
//       assert(luCurrentRung == (u32)lfParamAlong)                (:290) "Current rung got out of sync: r=..., p=..."
//       lfLocalParam = lfParamAlong - Floor(lfParamAlong)          (:293)  the two-fsel PPC floor
//       lfRemainingInCurrentSegment = (0.999f - lfLocalParam) * lfSegmentLength   (:295)
//       if (lfRemainingInCurrentSegment > lfDistToMove) break;
//       lfDistToMove -= lfRemainingInCurrentSegment;
//       ++luCurrentRung;
//       lfParamAlong = Floor(lfParamAlong + 1.0f);
//       assert(luCurrentRung == (u32)lfParamAlong)                (:328) "...out of sync(2)..."
//       if (luCurrentRung >= luNumSegments) {                      (:333-:336)
//           PickSplitToTake(*lpSection, lePreferredDirection, &luNewSection, &luNewHull, &luNewDirection);
//           if (luNewSection == 0xFF || luNewHull == 0xFFFF) { <identity + invalidate>; return; }
//           lfParamAlong = 0.0f; luCurrentRung = 0;
//           lpPositionInOut->muSection = luNewSection;             (stb 0x1C)
//           if (luNewHull != lpPositionInOut->muHullIndex) {       (stw 0x18)
//               lpPositionInOut->muHullIndex = luNewHull;
//               assert(luNewHull < lWorldMap.GetNumTrafficHulls())  (:360) "New hull is out of bounds:"
//               lpHull = lWorldMap.GetTrafficHullData(lpPositionInOut->muHullIndex);
//           }
//           assert(luNewSection < lpHull->muNumSections)            (:365) "New section is out of bounds:"
//           lpSection = lpHull->GetSection(luNewSection);
//           lpaCumulativeLengths = lpHull->GetRungLengthsForSection(lpSection);
//           luNumSegments = lpSection->GetNumSegments();
//       }
//       lfSegmentLength = lpaCumulativeLengths[rung+1] - lpaCumulativeLengths[rung];
//   }
//   lfLocalParamDelta = lfDistToMove / lfSegmentLength;            (:300)
//   lfNewParam        = lfLocalParamDelta + lfParamAlong;          (:301)
//   lpSection->CalcTransformAtParameter(lpHull->mpaRungs, lfNewParam, luCurrentRung,
//                                       lNewPosition, lNewAt, lNewRight);            (:303-:305)
//   assert(lfNewParam < (f32)lpSection->GetNumSegments())          (:315)
//   *lpTransformOut = Utils::CreateLookAt(lNewPosition, lNewPosition + lNewAt);
//   assert(lfNewParam >= 0.0f)                                     (:375)
//   lpPositionInOut->mfParamAlongSection = lfNewParam;             (stfs 0x14)
//   lpPositionInOut->muRung              = (u8)lfNewParam;         (fctidz + stb 0x1D)
//
// NOTE (faithful, and load-bearing for the camera): the walker does NOT write
// lpPositionInOut->mPosition. The reached point leaves through lpTransformOut's translation
// row; mPosition stays at whatever seated the lane. Update below reads the transform, never
// the stale mPosition.
// FLAG (VMX->portable): the console open-codes floor() as the classic PPC two-fsel magic-
// constant dance (dbl_82001CA0/CA8/CB0/CB8 == +/-2^52 and the 0/1 correction pair); that IS
// rw::math::fpu::Floor<float>, which the DWARF names in this exact spot, so it is written as
// std::floor.
// ----------------------------------------------------------------------------
void TrafficLaneTruck::MoveAlongTrafficLaneForwards(
        BrnTraffic::Directions lePreferredDirection,
        const BrnDirector::WorldMap& lrWorldMap,
        f32 lfDistToMove,
        BrnDirector::WorldMap::LanePosition* lpPositionInOut,
        rw::math::vpu::Matrix44Affine* lpTransformOut)
{
    CGS_ASSERT(lpPositionInOut->mbValid, "lpPositionInOut->mbValid");

    const BrnTraffic::Hull*    lpHull    = lrWorldMap.GetTrafficHullData(lpPositionInOut->muHullIndex);
    const BrnTraffic::Section* lpSection = lpHull->GetSection(lpPositionInOut->muSection);

    f32        lfParamAlong         = lpPositionInOut->mfParamAlongSection;
    u32        luCurrentRung        = lpPositionInOut->muRung;
    const f32* lpaCumulativeLengths = lpHull->GetRungLengthsForSection(lpSection);
    f32        lfSegmentLength      = lpaCumulativeLengths[luCurrentRung + 1]
                                    - lpaCumulativeLengths[luCurrentRung];
    u32        luNumSegments        = lpSection->GetNumSegments();

    CGS_ASSERT(lfDistToMove >= KF_ZERO, "lfDistToMove >= 0.0f");

    for (;;)
    {
        CGS_ASSERT(luCurrentRung == static_cast<u32>(static_cast<s32>(lfParamAlong)),
                   "Current rung got out of sync: r=");

        const f32 lfLocalParam = lfParamAlong - std::floor(lfParamAlong);
        const f32 lfRemainingInCurrentSegment =
            (KF_SEGMENT_END_PARAM - lfLocalParam) * lfSegmentLength;

        if (lfRemainingInCurrentSegment > lfDistToMove)
            break;

        lfDistToMove -= lfRemainingInCurrentSegment;
        ++luCurrentRung;
        lfParamAlong = std::floor(lfParamAlong + 1.0f);

        CGS_ASSERT(luCurrentRung == static_cast<u32>(static_cast<s32>(lfParamAlong)),
                   "Current rung got out of sync(2): r=");

        if (luCurrentRung >= luNumSegments)
        {
            u8  luNewSection   = 0;
            u16 luNewHull      = 0;
            u8  luNewDirection = 0;
            PickSplitToTake(*lpSection, lePreferredDirection,
                            &luNewSection, &luNewHull, &luNewDirection);

            if (luNewSection == KU_INVALID_SECTION || luNewHull == KU_INVALID_HULL_ID)
            {
                EndOfLane(lpPositionInOut, lpTransformOut);
                return;
            }

            lfParamAlong  = KF_ZERO;
            luCurrentRung = 0;
            lpPositionInOut->muSection = luNewSection;

            if (luNewHull != lpPositionInOut->muHullIndex)
            {
                lpPositionInOut->muHullIndex = luNewHull;
                CGS_ASSERT(luNewHull < lrWorldMap.GetNumTrafficHulls(),
                           "New hull is out of bounds:");
                lpHull = lrWorldMap.GetTrafficHullData(lpPositionInOut->muHullIndex);
            }

            CGS_ASSERT(luNewSection < lpHull->muNumSections, "New section is out of bounds:");
            lpSection            = lpHull->GetSection(luNewSection);
            lpaCumulativeLengths = lpHull->GetRungLengthsForSection(lpSection);
            luNumSegments        = lpSection->GetNumSegments();
        }

        lfSegmentLength = lpaCumulativeLengths[luCurrentRung + 1]
                        - lpaCumulativeLengths[luCurrentRung];
    }

    const f32 lfLocalParamDelta = lfDistToMove / lfSegmentLength;
    const f32 lfNewParam        = lfLocalParamDelta + lfParamAlong;

    rw::math::vpu::Vector3 lNewPosition;
    rw::math::vpu::Vector3 lNewAt;
    rw::math::vpu::Vector3 lNewRight;
    {
        const ::VecFloat lParam = { lfNewParam, lfNewParam, lfNewParam, lfNewParam };
        lpSection->CalcTransformAtParameter(lpHull->mpaRungs, lParam, luCurrentRung,
                                            lNewPosition, lNewAt, lNewRight);
    }

    CGS_ASSERT(lfNewParam < static_cast<f32>(lpSection->GetNumSegments()),
               "lfParamAlong < (float32_t) lpSection->GetNumSegments()");

    *lpTransformOut = Utils::CreateLookAt(lNewPosition,
                                          rw::math::vpu::Add(lNewPosition, lNewAt));

    CGS_ASSERT(lfNewParam >= KF_ZERO, "lfParamAlong >= 0.0f");

    lpPositionInOut->mfParamAlongSection = lfNewParam;
    lpPositionInOut->muRung              = static_cast<u8>(static_cast<s64>(lfNewParam));
}

// ----------------------------------------------------------------------------
// MoveAlongTrafficLaneBackwards @0x8222B100 -- the mirror walk. Same skeleton with three
// sign flips, all read off the asm:
//   * a segment is used up from its START, so the remaining distance is
//     lfLocalParam * lfSegmentLength (no 0.999 term -- that is the forward end guard);
//   * the rung DECREMENTS and the parameter floors DOWN one; running out is rung == 0
//     (the `lbOffEnd` local DWARF names at :448), not rung >= numSegments;
//   * the split comes from PickSplitToTakeBackwards and the walk resumes at the LAST segment
//     of the new section, so the parameter restarts at numSegments - 1 rather than 0;
//   * the sampled point looks BACKWARD: CreateLookAt(pos, pos - at) (DWARF's call list for
//     this function names rw::math::vpu::operator- where the forward twin names operator+).
// Assert lines :394 (mbValid), :409/:450 (the two out-of-sync messages), :476/:481 (the two
// out-of-bounds messages), :434 / :494 (the parameter range pair).
//
// ⚠️ REACHABILITY: on this build the road runner's speed is positive and the frame timestep is
// positive, so MoveAlongTrafficLane always dispatches FORWARDS. This arm is reconstructed for
// completeness (BehaviourRoadRunner::Reverse negates the speed, which is what reaches it) but
// it has NOT been exercised at runtime yet.
// ----------------------------------------------------------------------------
void TrafficLaneTruck::MoveAlongTrafficLaneBackwards(
        BrnTraffic::Directions lePreferredDirection,
        const BrnDirector::WorldMap& lrWorldMap,
        f32 lfDistToMove,
        BrnDirector::WorldMap::LanePosition* lpPositionInOut,
        rw::math::vpu::Matrix44Affine* lpTransformOut)
{
    CGS_ASSERT(lpPositionInOut->mbValid, "lpPositionInOut->mbValid");

    const BrnTraffic::Hull*    lpHull    = lrWorldMap.GetTrafficHullData(lpPositionInOut->muHullIndex);
    const BrnTraffic::Section* lpSection = lpHull->GetSection(lpPositionInOut->muSection);

    f32        lfParamAlong         = lpPositionInOut->mfParamAlongSection;
    u32        luCurrentRung        = lpPositionInOut->muRung;
    const f32* lpaCumulativeLengths = lpHull->GetRungLengthsForSection(lpSection);
    f32        lfSegmentLength      = lpaCumulativeLengths[luCurrentRung + 1]
                                    - lpaCumulativeLengths[luCurrentRung];

    CGS_ASSERT(lfDistToMove >= KF_ZERO, "lfDistToMove >= 0.0f");

    for (;;)
    {
        CGS_ASSERT(luCurrentRung == static_cast<u32>(static_cast<s32>(lfParamAlong)),
                   "Current rung got out of sync: r=");

        const f32 lfLocalParam                = lfParamAlong - std::floor(lfParamAlong);
        const f32 lfRemainingInCurrentSegment = lfLocalParam * lfSegmentLength;

        if (lfRemainingInCurrentSegment > lfDistToMove)
            break;

        lfDistToMove -= lfRemainingInCurrentSegment;

        const bool lbOffEnd = (luCurrentRung == 0);
        if (lbOffEnd)
        {
            u8  luNewSection   = 0;
            u16 luNewHull      = 0;
            u8  luNewDirection = 0;
            PickSplitToTakeBackwards(*lpSection, lePreferredDirection,
                                     &luNewSection, &luNewHull, &luNewDirection);

            if (luNewSection == KU_INVALID_SECTION || luNewHull == KU_INVALID_HULL_ID)
            {
                EndOfLane(lpPositionInOut, lpTransformOut);
                return;
            }

            lpPositionInOut->muSection = luNewSection;

            if (luNewHull != lpPositionInOut->muHullIndex)
            {
                lpPositionInOut->muHullIndex = luNewHull;
                CGS_ASSERT(luNewHull < lrWorldMap.GetNumTrafficHulls(),
                           "New hull is out of bounds:");
                lpHull = lrWorldMap.GetTrafficHullData(lpPositionInOut->muHullIndex);
            }

            CGS_ASSERT(luNewSection < lpHull->muNumSections, "New section is out of bounds:");
            lpSection            = lpHull->GetSection(luNewSection);
            lpaCumulativeLengths = lpHull->GetRungLengthsForSection(lpSection);

            luCurrentRung = lpSection->GetNumSegments() - 1;
            lfParamAlong  = static_cast<f32>(luCurrentRung) + 1.0f;
        }
        else
        {
            --luCurrentRung;
            lfParamAlong = std::floor(lfParamAlong);
        }

        CGS_ASSERT(luCurrentRung == static_cast<u32>(static_cast<s32>(lfParamAlong - 1.0f)),
                   "Current rung got out of sync(2): r=");

        lfSegmentLength = lpaCumulativeLengths[luCurrentRung + 1]
                        - lpaCumulativeLengths[luCurrentRung];
    }

    const f32 lfLocalParamDelta = lfDistToMove / lfSegmentLength;
    const f32 lfNewParam        = lfParamAlong - lfLocalParamDelta;

    rw::math::vpu::Vector3 lNewPosition;
    rw::math::vpu::Vector3 lNewAt;
    rw::math::vpu::Vector3 lNewRight;
    {
        const ::VecFloat lParam = { lfNewParam, lfNewParam, lfNewParam, lfNewParam };
        lpSection->CalcTransformAtParameter(lpHull->mpaRungs, lParam, luCurrentRung,
                                            lNewPosition, lNewAt, lNewRight);
    }

    CGS_ASSERT(lfNewParam < static_cast<f32>(lpSection->GetNumSegments()),
               "lfParamAlong < (float32_t) lpSection->GetNumSegments()");

    *lpTransformOut = Utils::CreateLookAt(lNewPosition,
                                          rw::math::vpu::Subtract(lNewPosition, lNewAt));

    CGS_ASSERT(lfNewParam >= KF_ZERO, "lfParamAlong >= 0.0f");

    lpPositionInOut->mfParamAlongSection = lfNewParam;
    lpPositionInOut->muRung              = static_cast<u8>(static_cast<s64>(lfNewParam));
}

// ----------------------------------------------------------------------------
// MoveAlongTrafficLane @0x8222BC48 -- the dispatcher, four statements:
//   assert(lpPositionInOut->mbValid)                               (:517)
//   if (lfDistToMove >= 0.0f) MoveAlongTrafficLaneForwards (dir, map,  lfDistToMove, pos, xf);
//   else                      MoveAlongTrafficLaneBackwards(dir, map, -lfDistToMove, pos, xf);
// ----------------------------------------------------------------------------
void TrafficLaneTruck::MoveAlongTrafficLane(BrnTraffic::Directions lePreferredDirection,
                                            const BrnDirector::WorldMap& lrWorldMap,
                                            f32 lfDistToMove,
                                            BrnDirector::WorldMap::LanePosition* lpPositionInOut,
                                            rw::math::vpu::Matrix44Affine* lpTransformOut)
{
    CGS_ASSERT(lpPositionInOut->mbValid, "lpPositionInOut->mbValid");

    if (lfDistToMove >= KF_ZERO)
        MoveAlongTrafficLaneForwards(lePreferredDirection, lrWorldMap, lfDistToMove,
                                     lpPositionInOut, lpTransformOut);
    else
        MoveAlongTrafficLaneBackwards(lePreferredDirection, lrWorldMap, -lfDistToMove,
                                      lpPositionInOut, lpTransformOut);
}

// ----------------------------------------------------------------------------
// ⭐⭐ TrafficLaneTruck::Update @0x82247AC0 -- THE FRAME ADVANCE.
//
// DWARF (BrnBehaviourRoadRunner.cpp:93) gives every local by name; the console body is 245
// instructions and reads statement for statement as:
//
//   assert(mbPrepared)                                             (:95)
//   if (!mLanePosition.mbValid) return;
//   lfDistToMove    = mfSpeed * lfTimestep;                        (:105)
//   lfBlendDistance = (lfDistToMove < 0.0f) ? -mfBlendDistance : mfBlendDistance;   (:107)
//   lLanePositionA = lLanePositionB = mLanePosition;               (:119/:120, 4 QWORD copies each)
//   MoveAlongTrafficLane(E_DIR_LEFT, lWorldMap, lfDistToMove,                    &lLanePositionA, &lTransformA);
//   MoveAlongTrafficLane(E_DIR_LEFT, lWorldMap, lfDistToMove + lfBlendDistance,  &lLanePositionB, &lTransformB);
//   lNewTransform = SLerp(mTransform,                                             (:126)
//                         Utils::CreateLookAt(lTransformA.GetW(), lTransformB.GetW()),
//                         mfTransformBlendAmount, &lUnusedAngle);                (:125)
//   if (!rw::math::fpu::IsZero(lfTimestep))
//   {
//       lFramesPerSec   = 1.0f / lfTimestep;                        (:130)
//       mLinearVelocity = (lNewTransform.GetW() - mTransform.GetW()) * lFramesPerSec;
//       lRelative       = Mult(lNewTransform, InverseOfMatrixWithOrthonormal3x3(mTransform));
//       lNewAngularVelocity = EulerAnglesZXYFromMatrix44Affine(lRelative, 0, 0.01f)
//                             * lFramesPerSec;                      (:135)
//       mLocalAngularVelocity = Lerp(mLocalAngularVelocity, lNewAngularVelocity,
//                                    mfTransformBlendAmount * 0.5f);
//   }
//   assert(IsValid(mLocalAngularVelocity))                          (:139)
//   mTransform    = lNewTransform;
//   mLanePosition = lLanePositionA;
//
// ⚠️ ORDERING IS LOAD-BEARING and easy to get wrong from the pseudocode: the slerped frame is
// held in a STACK slot for the whole velocity block and only stored into mTransform at the
// very end (asm 0x82247E4C..0x82247E58), so every `mTransform` read inside the block is the
// PREVIOUS frame's -- which is exactly what makes the velocities finite differences. The two
// direction arguments really are the literal `1` at 0x82247B98 / 0x82247BC0 == E_DIR_LEFT.
// The constants: flt_82001C98 = 1.0f, flt_82001DA0 = 0.5f, flt_82002138 = 0.01f (the Euler
// near-vertical epsilon, matching CameraUtils.h's default), flt_82001770 / flt_82002514 =
// +/-2^-23 (the fpu IsZero pair). All dumped, none guessed.
//
// ⚠️ ONE DOCUMENTED QUIET GATE -- the angular half. Utils::EulerAnglesZXYFromMatrix44Affine
// @0x82222180 is DECLARATION-ONLY in this tree, so lNewAngularVelocity cannot be computed
// honestly and mLocalAngularVelocity is left as Prepare set it (zero). Its ONLY reader is
// TrafficLaneTruck::GetLocalAngularVelocity, which the road-runner behaviour uses for the
// camera BANK -- so the consequence is an unbanked fly-by, not a wrong position. The linear
// half and the transform advance are both real.
// DELETE-WHEN: EulerAnglesZXYFromMatrix44Affine is bodied.
// ----------------------------------------------------------------------------
void TrafficLaneTruck::Update(const BrnDirector::WorldMap& lrWorldMap, f32 lfTimestep,
                              void* /*lpRandom*/)
{
    CGS_ASSERT(mbPrepared, "mbPrepared");

    if (!mLanePosition.mbValid)
        return;

    const f32 lfDistToMove    = mfSpeed * lfTimestep;
    const f32 lfBlendDistance = (lfDistToMove < KF_ZERO) ? -mfBlendDistance : mfBlendDistance;

    rw::math::vpu::Matrix44Affine        lTransformA;
    rw::math::vpu::Matrix44Affine        lTransformB;
    BrnDirector::WorldMap::LanePosition  lLanePositionA = mLanePosition;
    BrnDirector::WorldMap::LanePosition  lLanePositionB = mLanePosition;

    MoveAlongTrafficLane(BrnTraffic::E_DIR_LEFT, lrWorldMap, lfDistToMove,
                         &lLanePositionA, &lTransformA);
    MoveAlongTrafficLane(BrnTraffic::E_DIR_LEFT, lrWorldMap, lfDistToMove + lfBlendDistance,
                         &lLanePositionB, &lTransformB);

    ::VecFloat lUnusedAngle = { 0.0f, 0.0f, 0.0f, 0.0f };
    const rw::math::vpu::Matrix44Affine lNewTransform =
        rw::math::vpu::SLerp(mTransform,
                             Utils::CreateLookAt(lTransformA.wAxis, lTransformB.wAxis),
                             mfTransformBlendAmount,
                             reinterpret_cast<rw::math::vpu::Vector3*>(&lUnusedAngle));

    if (!rw::math::fpu::IsZero(lfTimestep))
    {
        const f32 lfFramesPerSec = 1.0f / lfTimestep;

        mLinearVelocity = rw::math::vpu::Mult(lNewTransform.wAxis - mTransform.wAxis,
                                              lfFramesPerSec);

        // ⚠️ the gate documented above: the relative rotation is built (it is pure named-member
        // matrix algebra the vendor home already provides) but its Euler decomposition is not
        // available, so the lerp into mLocalAngularVelocity cannot run.
        //   lRelative           = Mult(lNewTransform, InverseOfMatrixWithOrthonormal3x3(mTransform));
        //   lNewAngularVelocity = Utils::EulerAnglesZXYFromMatrix44Affine(lRelative, 0, 0.01f)
        //                             * lfFramesPerSec;
        //   mLocalAngularVelocity = Lerp(mLocalAngularVelocity, lNewAngularVelocity,
        //                                mfTransformBlendAmount * 0.5f);
    }

    CGS_ASSERT(rw::math::vpu::IsValid(mLocalAngularVelocity), "IsValid(mLocalAngularVelocity)");

    mTransform    = lNewTransform;
    mLanePosition = lLanePositionA;
}

// ----------------------------------------------------------------------------
// TrafficLaneTruck::GetTransform @0x821F53D8
// Asserts the truck is prepared, then copies the 64-byte transform (four 16-byte aligned rows,
// lvx128/stvx128 at base this+0x20, stride 16) into the by-value return slot.
// ----------------------------------------------------------------------------
rw::math::vpu::Matrix44Affine TrafficLaneTruck::GetTransform() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    return mTransform;                                 // 4x lvx128 (this+0x20) -> 4x stvx128 (sret)
}

// ----------------------------------------------------------------------------
// TrafficLaneTruck::GetLocalAngularVelocity @0x821F5470
// ----------------------------------------------------------------------------
rw::math::vpu::Vector3 TrafficLaneTruck::GetLocalAngularVelocity() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    return mLocalAngularVelocity;                      // lvx128 (this+0x60) -> stvx128 (sret)
}

// ----------------------------------------------------------------------------
// TrafficLaneTruck::GetLinearVelocity @0x821F54E0
// ----------------------------------------------------------------------------
rw::math::vpu::Vector3 TrafficLaneTruck::GetLinearVelocity() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    return mLinearVelocity;                            // lvx128 (this+0x70) -> stvx128 (sret)
}

// ============================================================================
// BehaviourRoadRunner::Construct @0x8222BCE0  -- vtable slot 0, dispatched by
// BehaviourHelper::Prepare the instant the pool hands out a slot.
//
// Every store below is in the asm; the offset each one names is resolved by the layout in the
// header. The first seven (`*(this+4)`, `*(this+8..12)`, `*(this+16)`) are the base's own six
// fields -- i.e. the inlined Behaviour::Construct.
// ============================================================================
void BehaviourRoadRunner::Construct()
{
    Behaviour::Construct();                        // *(this+4)=0, +8..+12=0, +16=0

    mpParameters = 0;                              // *(this+20) = 0   (the LAST store in the asm)

    mTrafficLaneTruck.mLanePosition.mbValid = false;   // *(this+62)  == truck +0x1E
    mTrafficLaneTruck.mfSpeed               = 0.0f;    // *(this+160) == truck +0x80
    mTrafficLaneTruck.mbPrepared            = false;   // *(this+172) == truck +0x8C

    // The two near-clip diagonal post boxes start idle (`*(this+176)` / `*(this+256)` -- the
    // post box's own head word). Reached through the named sub-objects; the interiors are
    // un-homed (see the header FLAG), so the console's single head store is expressed as a
    // whole-object reset.
    mTopLeftBottomRight = LineTestNearestPostBox();     // *(this+176) = 0
    mTopRightBottomLeft = LineTestNearestPostBox();     // *(this+256) = 0
    mLineTestFineA      = LineTestFinePostBox();        // *(this+336) = 0
    mLineTestFineB      = LineTestFinePostBox();        // *(this+344) = 0

    // The shake transform the console builds with four stvx128s at +0x200/+0x210/+0x220/+0x230
    // (r10 = this+512, offsets 0/16/32/48) from two register constants.
    mLastShakeTransform = rw::math::vpu::Matrix44Affine();

    // ⚠️ QUIET GATE: the shake seeds. The asm's `*(this+576..588) = 0.0` (mShake) and
    //   `*(this+592..604) = {0.0, 0.0, 1.0, 0.25}` (mShakeParams, written twice -- 0.06/1.15/
    //   0.11 first, then overwritten) land inside the two sub-objects this header models
    //   opaquely because Utils::CameraShake has no home outside BehaviourRig.h (see the header
    //   FLAG). Writing them would mean fabricating that type's field order.
    //   CONSEQUENCE: the fly-by's camera shake starts zeroed rather than at the authored
    //   amplitude -- cosmetic on a camera that cannot move yet anyway.
    //   DELETE-WHEN: Utils::CameraShake gets its own header.

    mfDirection             = 1.0f;                // *(this+668)
    mbIsColliding           = false;               // *(this+692)
    mbWasCollidingLastFrame = false;               // *(this+693)
    mbFixationsAllowed      = false;               // *(this+700)
    mbOccluded              = false;               // *(this+701)
}

// ============================================================================
// BehaviourRoadRunner::Prepare @0x8220F748 -- vtable slot 1, dispatched by
// BehaviourManager::PrepareBehaviours once per allocation.
//
// ⭐ IT TOUCHES NOTHING OUTSIDE ITSELF AND ALWAYS RETURNS TRUE. That matters: the road-runner's
// Prepare cannot fail, so BehaviourManager::PrepareBehaviours always clears the "needs
// preparing" bit on the very next pass -- which is precisely what lets
// ArbStateAttractMode::Prepare stop returning false and the attract state leave
// E_STATE_PREPARING. The world-data dependency lives in Update, not here.
//
// Every store is in the asm; the offsets resolve through the header's layout.
// ============================================================================
bool BehaviourRoadRunner::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    SetNotPrepared();                              // *(this+8) = 0 -- Update re-seeds the truck

    // ⚠️ QUIET GATE (one call, not a branch): the console opens with
    //     Utils::TransitionSmoother::Set(&mHeight, 4.0f, <3 more floats>)
    //   Only the 4.0f survives the decompile as a named constant; the other three arguments
    //   (the ideal lerp amount, its own lerp amount and the similarity tolerance scale) are
    //   register garbage in the pseudocode and are NOT recoverable from this site alone.
    //   Guessing them would silently set the camera-height smoothing rate. The one store the
    //   asm DOES pin at that address (`*(this+640) = 4.0` == mHeight.mfTarget) is performed
    //   through the smoother's own named setter, so the height TARGET is right and only the
    //   smoothing RATE is left at whatever Construct left it.
    //   CONSEQUENCE: the fly-by camera's height chases its 4.0 target with an unseeded lerp
    //   amount. Cosmetic until Update's prepared leg runs.
    //   DELETE-WHEN: TransitionSmoother::Set's three lerp arguments are read off a second call
    //   site (the other callers of @0x821F22A0).
    mHeight.SetTarget(4.0f);                       // *(this+640) = 4.0

    mfFixationAmount              = 0.0f;          // *(this+608)
    mfFixationBlendTimeReciprocal = 0.125f;        // *(this+612)
    mfFixationStartDistance       = 100.0f;        // *(this+616)
    mfFixationEndDistance         = 120.0f;        // *(this+620)

    mfCurrentModeTime             = 0.0f;          // *(this+628)
    mfCurrentModeDuration         = 30.0f;         // *(this+632)

    mfDesiredSpeed                = mfDirection * 3.0f;   // *(this+660) = *(this+668) * 3.0
    mfSpeedBlendAmount            = 0.0099999998f; // *(this+664)

    mfDesiredBankingScale         = 0.0f;          // *(this+672)
    mfBankingScale                = 0.15000001f;   // *(this+676)
    mfBankingScaleBlendAmount     = 0.1f;          // *(this+680)

    mfTimeSinceLastCollisionStarted = 0.0f;        // *(this+684)
    mfTimeSinceLastCollision        = 0.0f;        // *(this+688)

    meMode              = E_MODE_LOW_SLOW;         // *(this+696) = 0
    mbHasFixation       = false;                   // *(this+702)
    mbStartingFixation  = false;                   // *(this+703)
    mbFixationIsValid   = false;                   // *(this+704)

    return true;                                   // li r3, 1 (unconditional)
}

// ============================================================================
// BehaviourRoadRunner::Reverse (BrnBehaviourRoadRunner.h:331)
//
// Turn the fly-by round. X360: inlined at BrnArbStateCrashNav::Update's ACTIVE_TURNABOUT case
// (asm @0x8226DF7C), where three `fmuls` against flt_820037C8 (== -1.0) negate, IN THIS ORDER,
// the words at +0x29C, +0x294 and +0xA0 -- which this layout resolves to mfDirection, the
// desired speed, and the truck's live speed. The crash-nav state calls it right after clearing
// the prepared gate, so Update re-seeds the lane walking the other way.
// ============================================================================
void BehaviourRoadRunner::Reverse()
{
    mfDirection    = mfDirection    * -1.0f;                           // +0x29C
    mfDesiredSpeed = mfDesiredSpeed * -1.0f;                           // +0x294
    mTrafficLaneTruck.SetSpeed(mTrafficLaneTruck.GetSpeed() * -1.0f);  // +0x0A0 (truck +0x80)
}

// ============================================================================
// BehaviourRoadRunner::Update @0x82247E98 -- vtable slot 2, the per-frame camera producer.
//
// ⚠️ THE PREPARED LEG IS A DOCUMENTED QUIET GATE. The un-prepared leg below is REAL and is,
// today, the branch the console itself would take.
//
// Console shape:
//     if (mbHasFailed) return true;                                     // early-out, frame 2+
//     if (!mbIsPrepared)
//     {
//         if (!mTrafficLaneTruck.Prepare(*lrInfo.mpWorldMap, lrInfo.<+0x280>))
//         { Behaviour::Fail(lrCamera, 6); return true; }                // <-- TODAY'S BRANCH
//         <first-frame mode seed>; mbIsPrepared = true;
//     }
//     <~280 lines: the two diagonal near-clip line tests, the collision timers, the height
//      smoother, TrafficLaneTruck::Update, camera = truck transform + up*height + right*2.25,
//      SetFOV(95), a VMX sin/cos banking-roll pipeline, the fixation SLerp, the fixation
//      acquisition + occlusion probes, and the camera-shake concat>
//     return true;                                                       // always
//
// WHY THE PREPARED LEG IS GATED, precisely:
//   1. ⭐ THE DATA IS NOT THERE. TrafficLaneTruck::Prepare @0x82247A08 calls
//      WorldMap::GetLanePositionNearestPoint @0x8221CE98, which reads the world's TRAFFIC DATA
//      (BrnTraffic::TrafficData -> Pvs::GetHullIndexForPoint -> hull rungs) and leaves
//      mLanePosition.mbValid == 0 when there is none. WorldMap::LoadData @0x8225F5A0 is itself
//      a documented gate (the GameDataEvent request RECORD SHAPE is unproven -- see
//      Utils/BrnDirectorWorldMap.cpp), so GetTrafficData() returns null and the truck's Prepare
//      fails. Every lane-walk entry point additionally asserts
//      `worldMap->meLoadingState == E_LOADING_STATE_LOADED`, which that same gate never sets.
//      ⇒ THE CONSOLE WOULD TAKE THE FAIL BRANCH TOO. The gated leg is unreachable today even in
//        principle, so gating it costs nothing.
//   2. The banking block is a genuine VMX lane pipeline (~40 vmaddfp against the coefficient
//      tables at unk_82000BD0..C20 computing sin and cos in separate lanes, then a vperm128 /
//      vrlimi128 assembly of a roll matrix), and so are the normalise-with-Newton-Raphson
//      blocks feeding the two line tests. The project rules forbid paraphrasing those scalar-wise.
//   3. It needs LineTestNearestPostBox / LineTestFinePostBox interiors, Utils::CameraShake,
//      Utils::CreateLookAt / SineLerp / CalcNearClipCorners, SceneQueryInterface::LineTestNearest
//      / LineTestFine, and WorldMap::GetInterestingPointNear -- none of which is homed.
//
// CONSEQUENCE, stated plainly: the road-runner behaviour allocates, constructs and prepares,
// the attract state advances to E_STATE_ACTIVE, and the behaviour then FAILS on its first
// Update with ValidityAccount reason 6 -- exactly as retail would with no traffic data loaded.
// The camera it produces stays the one BehaviourHelper::Prepare constructed.
// ⚠️ CONDUCTOR: that means the published attract camera is a DEFAULT-CONSTRUCTED camera at the
// origin. Do NOT call Arbitrator::SetDoAttractMode(true) in a build where the world streams off
// the director camera until the traffic data loads -- a frozen eye outside the city footprint
// empties the PVS set (the streaming wave's documented regression).
//
// DELETE-WHEN: WorldMap::LoadData's GameDataEvent record shape is settled AND the three items
// above are homed. Item 1 is the real blocker and it is the LAST MILE to a moving fly-by.
// ============================================================================
bool BehaviourRoadRunner::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    if (HasFailed())
    {
        return true;                                   // lbz 9(this); bne -> li r3,1
    }

    if (!IsPrepared())
    {
        // The console hands the truck the world map and a Vector3 taken from the shared info at
        // +0x280 -- inside mPlayerInfo, i.e. the PLAYER CAR'S POSITION: the fly-by seats itself
        // on the lane nearest the subject it is about to fly over.
        //
        // ⚠️ FLAG PC seed point: mPlayerInfo's interior is not mapped (Behaviour.h models it as
        // a named opaque sub-object because embedding VehicleInfo drags in a pre-existing
        // SuspensionSpring ODR fork), so the seed point cannot be sourced from it yet. The
        // world map's own lane search is fed the ORIGIN instead, which
        // GetLanePositionNearestPoint answers with the lane nearest the city origin -- a real
        // lane on a real road, just not the one under the player.
        // DELETE-WHEN: mPlayerInfo becomes a real VehicleInfo (the SuspensionSpring fork is
        // reconciled), then pass lrInfo.mPlayerInfo's position.
        const BrnDirector::WorldMap* lpWorldMap = lrInfo.GetWorldMap();
        const rw::math::vpu::Vector3 lSeedPoint = { 0.0f, 0.0f, 0.0f, 0.0f };

        if (lpWorldMap == 0 || !mTrafficLaneTruck.Prepare(*lpWorldMap, lSeedPoint))
        {
            Fail(lrCamera, 6);                         // bl Behaviour::Fail(this, camera, 6)
            return true;
        }

        // The first-frame mode seed (inlined SetMode(E_MODE_LOW_SLOW) at 0x82248160..). Every
        // value here is a .rdata literal, NOT a read through mpParameters -- Update never
        // dereferences that pointer anywhere in its 4 KB body.
        meMode                = E_MODE_LOW_SLOW;              // +0x2B8 = 0
        mTrafficLaneTruck.SetSpeed(0.0f);                     // +0x0A0 (truck +0x80)
        mfCurrentModeTime     = 0.0f;                         // +0x274
        mbHasFixation         = false;                        // +0x2BE
        mfCurrentModeDuration = 30.0f;                        // +0x278  flt_82004F5C
        SetPrepared();                                        // stb 1, 8(this)
        mHeight.SetTarget(4.0f);                              // +0x27C  flt_82004EF4
        mfDesiredSpeed        = mfDirection * 3.0f;            // +0x294  flt_82004270
        mfDesiredBankingScale = 0.15000001f;                   // +0x2A0  flt_82004E58
    }

    const f32 lfTimestep = lrInfo.GetTimestep(GetTimestepType());

    // ---- the collision-timer block (asm 0x82247FC4..0x8224805C) ----------------------------
    // ⚠️ QUIET GATE: `lbHit` comes from last frame's two near-clip diagonal line tests, and
    // those live in the two LineTestNearestPostBox members whose INTERIORS are not homed (the
    // .cpp is forbidden to reach inside one). With no probes issued, the console's own reading
    // of an un-posted box is "no hit", so the timers below simply never trip and the fly-by
    // never enters its collided speed. That is the conservative direction: the camera keeps its
    // desired speed instead of dropping to the collision speed.
    // DELETE-WHEN: Utils/BrnPostBox.h grows the two post-box aliases and the two
    // SceneQueryInterface::LineTestNearest probes at the tail of this body can be issued.
    mbWasCollidingLastFrame = mbIsColliding;

    // 0x82248060: mfBankingScale += (mfDesiredBankingScale - mfBankingScale) * mfBankingScaleBlendAmount
    mfBankingScale += (mfDesiredBankingScale - mfBankingScale) * mfBankingScaleBlendAmount;

    // ⚠️ QUIET GATE (asm 0x82248098..0x82248108): the inlined
    // Utils::TransitionSmoother::Update over mHeight -- a self-smoothed lerp whose live blend
    // weight chases mfIdealLerpAmount through a `1 - 1/(|cur-tgt|*scale)` term. Its declared
    // sibling TransitionSmoother::Update is DECLARATION-ONLY in CameraUtils.h, and the class's
    // private members are not reachable from here. mHeight therefore holds the value
    // Prepare's TransitionSmoother::Set seeded it with (4.0), which is also the target this
    // frame sets -- so the height is right, it just does not ease.
    // DELETE-WHEN: TransitionSmoother::Update is bodied.
    const f32 lfHeight = 4.0f;

    // 0x8224810C: the truck's speed for this frame. The collided arm needs the gated line-test
    // block above, so the desired arm is the one the console takes here too.
    mTrafficLaneTruck.SetSpeed(mfDesiredSpeed);

    // ⭐⭐ THE ADVANCE. 0x82248130: TrafficLaneTruck::Update(worldMap, timestep, random).
    mTrafficLaneTruck.Update(*lrInfo.GetWorldMap(), lfTimestep, lrInfo.GetRandom());

    // ---- the camera placement (asm 0x82248178..0x82248290) ----------------------------------
    //   lrCamera.mTransform = mTrafficLaneTruck.GetTransform();          4 lvx/stvx rows
    //   pos += up * mHeight.Get() + right * 2.25                          two vmaddfp
    //   lrCamera.SetFOV(95.0f)                                            flt_8200A030
    // (flt_82009A74 == 2.25 -- the lateral offset that puts the camera beside the lane rather
    //  than on it.)
    {
        rw::math::vpu::Matrix44Affine lCameraTransform = mTrafficLaneTruck.GetTransform();
        lCameraTransform.wAxis = rw::math::vpu::MultAdd(
            lCameraTransform.yAxis, rw::math::vpu::Vector3{ lfHeight, lfHeight, lfHeight, lfHeight },
            lCameraTransform.wAxis);
        lCameraTransform.wAxis = rw::math::vpu::MultAdd(
            lCameraTransform.xAxis, rw::math::vpu::Vector3{ 2.25f, 2.25f, 2.25f, 2.25f },
            lCameraTransform.wAxis);
        lrCamera.SetTransform(lCameraTransform);
    }
    lrCamera.SetFOV(95.0f);

    // [diagnostic] where the fly-by camera actually IS, once a second. This is the measurement
    // the whole campaign is for -- it must CHANGE frame to frame.
    {
        static s32 siFlybyFrame = 0;
        if ((siFlybyFrame % 60) == 0 && (CgsDev::Message::gxMessageFilterFlags & 1)
            && CgsDev::Log::gpDebugPrint != 0)
        {
            const rw::math::vpu::Matrix44Affine& lrX = lrCamera.GetTransform();
            *CgsDev::Log::gpDebugPrint
                << "[flyby] f" << siFlybyFrame << " dt " << lfTimestep
                << " speed " << mTrafficLaneTruck.GetSpeed()
                << " eye (" << lrX.wAxis.x << ", " << lrX.wAxis.y << ", " << lrX.wAxis.z << ")"
                << " at (" << lrX.zAxis.x << ", " << lrX.zAxis.y << ", " << lrX.zAxis.z << ")\n";
        }
        ++siFlybyFrame;
    }

    // 0x8224844C: mfCurrentModeTime += <the raw frame dt at lrInfo+0x580>; on expiry the mode
    // is re-seeded with the same five constants the first-frame block above wrote.
    mfCurrentModeTime += lfTimestep;
    if (mfCurrentModeTime > mfCurrentModeDuration)
    {
        meMode                = E_MODE_LOW_SLOW;
        mfCurrentModeTime     = 0.0f;
        mfCurrentModeDuration = 30.0f;
        mHeight.SetTarget(4.0f);
        mfDesiredSpeed        = mfDirection * 3.0f;
        mfDesiredBankingScale = 0.15000001f;
    }

    // ⚠️ THE REMAINING GATES, each with what it costs:
    //   * the BANKING roll (0x82248294..0x82248514) -- a genuine VMX sin/cos minimax lane
    //     pipeline that rolls the finished transform about its own forward by
    //     -angularVelocity.y * mfBankingScale. It needs mLocalAngularVelocity, which
    //     TrafficLaneTruck::Update cannot fill yet (EulerAnglesZXYFromMatrix44Affine is
    //     declaration-only), so the input would be zero and the roll the identity anyway.
    //   * the FIXATION blend + acquisition -- needs Utils::SineLerp and
    //     WorldMap::GetInterestingPointNear over mpTriggerData, which the director never loads
    //     (TriggerQueryManager::Prepare @0x82398218 is a GameState-module TU).
    //   * the two near-clip LineTestNearest probes -- need Utils::CalcNearClipCorners (itself
    //     declaration-only) and the post-box interiors.
    //   * the CameraShake concat -- Utils::CameraShake has no home outside BehaviourRig.h.
    // None of them moves the camera; all four only refine an already-placed one.
    return true;                                       // every console exit is `li r3, 1`
}// ============================================================================
// BehaviourRoadRunner::PostCollisionUpdate @0x8220F850 -- vtable slot 3. Drains the two fine
// line-test post boxes, transforms each hit into the fixation's unit box through
// mWorldToFixation and raises mbOccluded, then ages mfFixationOccludedTime and drops
// mbFixationIsValid once the fixation has been occluded for too long.
//
// ⚠️ QUIET GATE, same three reasons as Update (post-box interiors + a VMX abs/compare block +
// no fixation can exist while Update fails). It only ever NARROWS the fixation state, so a
// no-op leaves the behaviour consistent. The one unconditional store at the tail of the console
// body -- clearing mbStartingFixation -- is reproduced.
// DELETE-WHEN: with Update's prepared leg.
// ============================================================================
bool BehaviourRoadRunner::PostCollisionUpdate(Camera& /*lrCamera*/,
                                              const BehaviourSharedInfo& /*lrInfo*/)
{
    mbStartingFixation = false;
    return true;
}

// ============================================================================
// GetName @0x821FB130 -- a single `lis/addi` of the literal, then blr.
// ============================================================================
const char* BehaviourRoadRunner::GetName() const
{
    return "BehaviourRoadRunner";
}

// ============================================================================
// GetParameters / SetParameters (BrnBehaviourRoadRunner.cpp:1172 / :1185 + the typed :312
// setter). The typed setter is the one the arbitrator states call; the virtual pair is how the
// generic (attribute-driven) NewBehaviour overload reaches it.
// ============================================================================
const Behaviour::Parameters* BehaviourRoadRunner::GetParameters() const
{
    return mpParameters;
}

void BehaviourRoadRunner::SetParameters(const Behaviour::Parameters* lpParameters)
{
    SetParameters(static_cast<const Parameters*>(lpParameters));
}

void BehaviourRoadRunner::SetParameters(const Parameters* lpParameters)
{
    mpParameters = lpParameters;
    SetNotPrepared();   // every behaviour's typed SetParameters drops the prepared gate
}

// ============================================================================
// SetupTweaker (BrnBehaviourRoadRunner.cpp:1200) -- hand this behaviour's editable parameters
// to the dev-tools tweaker.
// ⚠️ QUIET GATE: Utils::Tweaker's authoring surface (the AxisMapping tables) is homed but this
// behaviour's per-parameter registration call list is not recovered, and nothing on the live
// path attaches a tweaker (mbTweakerAttached is only ever raised by the manager's AttachTweaker,
// which no committed caller reaches). Marking the base flag is the one observable effect.
// DELETE-WHEN: the tweaker registration call list is read off the console body.
// ============================================================================
void BehaviourRoadRunner::SetupTweaker(Utils::Tweaker& /*lrTweaker*/)
{
    SetTweakerAttached(true);
}

// ============================================================================
// Parameters::Construct (BrnBehaviourRoadRunner.h:300) -- the base parameter head plus this
// behaviour's own type tag.
// FLAG: the road-runner behaviour-TYPE tag value is NOT attested (the block's serialise walk is
// attested empty and Update never dereferences mpParameters, so no site compares the tag). The
// base head is constructed and the tag deliberately left at the base default rather than
// guessed.
// DELETE-WHEN: a SetParameters assert quoting the tag is found for this behaviour.
// ============================================================================
void BehaviourRoadRunner::Parameters::Construct()
{
    Behaviour::Parameters::Construct();
}

} // namespace Camera
} // namespace BrnDirector
