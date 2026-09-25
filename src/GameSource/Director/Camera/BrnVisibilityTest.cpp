// ============================================================================
// GameSource/Director/Camera/BrnVisibilityTest.cpp
//
// Compilation home for BrnDirector::Camera::VisibilityTest (DWARF BrnCollisionPolicy.h:175):
//   - VisibilityTest::GetOffscreenTime          @0x821F3718
//   - VisibilityTest::IsOnScreen                @0x821F3770
//   - VisibilityTest::GenerateSceneQueries      @0x822400B0   (FX-DIRECTOR2 2026-09-25)
//   - VisibilityTest::ProcessSceneQueryResults  @0x8220E290   (FX-DIRECTOR2 2026-09-25)
//
// Driven by BrnDirector::Camera::VisibilityCollisionPolicy::GenerateSceneQueries /
// ::ProcessSceneQueryResults. Mounted 2026-09-25 (it held two bodies nothing linked).
// ============================================================================

#include "GameSource/Director/Camera/BrnCollisionPolicy.h"
#include "GameSource/Director/Camera/Camera.h"                   // Camera (the transform, mfFOV)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"        // Utils::GetZoomFromFOVDegs
#include "GameSource/Director/Utils/BrnSceneQueryInterface.h"   // BrnDirector::SceneQueryInterface::LineTestNearest

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// BrnDirector::Camera::VisibilityTest::GetOffscreenTime @0x821F3718
//   lbz   r11, 0xB0(this)      ; mbTestLookingAt
//   cmplwi r11, 0              ; assert it is set (the looking-at test was enabled)
//   bne   skip                 ; (asm asserts when CLEAR, i.e. !mbTestLookingAt)
//   ... Begin/Fire/End assert (BrnCollisionPolicy.h:248) ...
//   lfs   f1, 0xA4(this)       ; return mfOffscreenTime  (f32 load -> returns f32, not double)
// ----------------------------------------------------------------------------
f32 VisibilityTest::GetOffscreenTime() const
{
    CGS_ASSERT(mbTestLookingAt, "mbTestLookingAt");   // lbz 0xB0; assert set
    return mfOffscreenTime;                           // lfs f1, 0xA4(this)
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::VisibilityTest::IsOnScreen @0x821F3770
//   lbz   r11, 0xB0(this)      ; mbTestLookingAt
//   cmplwi r11, 0              ; assert it is set (the looking-at test was enabled)
//   bne   skip                 ; (asm asserts when CLEAR, i.e. !mbTestLookingAt)
//   ... Begin/Fire/End assert (BrnCollisionPolicy.h:269) ...
//   lbz   r3, 0xB2(this)       ; return mbIsOnScreen  (byte load)
// ----------------------------------------------------------------------------
bool VisibilityTest::IsOnScreen() const
{
    CGS_ASSERT(mbTestLookingAt, "mbTestLookingAt");   // lbz 0xB0; assert set
    return mbIsOnScreen;                              // lbz r3, 0xB2(this)
}

// ----------------------------------------------------------------------------
// VisibilityTest::GenerateSceneQueries @0x822400B0 (DWARF BrnCollisionPolicy.h:237)
//
//   lfMaxDistanceSq = KF (25.0, flt_82CDAD54) squared                      (0x822400D8/DC)
//   if (mbTestLookingAt && lbDoTest)                                       (0x822400E0..F4)
//       if (!IsLookingAtTarget(camera, target, bounds))   -> off screen    (0x82240104)
//       else if (|camera.w - target.w|^2 > lfMaxDistanceSq * zoom)          (vmsum3fp128 = xyz dot;
//                                               -> off screen             vcmpgtfp.; zoom =
//                                                                          GetZoomFromFOVDegs(fov))
//       else { mfOffscreenTime = 0; mbIsOnScreen = true; }                 (0x82240188/8C)
//       (off screen: mbIsOnScreen = false; the timer is left to run)       (0x82240194)
//   if (lbDoTest)                                                          (0x82240198..0x822401F4)
//       the two nearest line tests, camera -> target into mLineTestA and target -> camera into
//       mLineTestB: flags 30 (0x1E), volume flags 0xFF, the TARGET entity excluded with all its
//       parts (mode 1, E_EXCLUDE_ALL_CHILD_PARTS).
// The timestep and the Random are part of the DWARF signature and unused by the ARTIST body
// (mfTimeSinceLastTest / mfMaxTimeBetweenTests are never read here).
// ----------------------------------------------------------------------------
void VisibilityTest::GenerateSceneQueries(const Camera& lrCamera, f32 lfTimestep, CgsNumeric::Random& lrRandom,
                                          const SceneQueryInterface* lpRequestInterface,
                                          const rw::math::vpu::Matrix44Affine& lrTargetTransform,
                                          const AABBox& lrTargetAABB, bool lbDoTest,
                                          CgsSceneManager::EntityId lTargetEntityId)
{
    (void)lfTimestep;
    (void)lrRandom;

    const f32 KF_MAX_ON_SCREEN_DISTANCE = 25.0f;                                     // flt_82CDAD54 == 0x41C80000
    const f32 lfMaxDistanceSq = KF_MAX_ON_SCREEN_DISTANCE * KF_MAX_ON_SCREEN_DISTANCE;

    const rw::math::vpu::Vector3& lrCameraPosition = lrCamera.mTransform.wAxis;
    const rw::math::vpu::Vector3& lrTargetPosition = lrTargetTransform.wAxis;

    if (mbTestLookingAt && lbDoTest)
    {
        bool lbOnScreen = false;
        if (IsLookingAtTarget(lrCamera, lrTargetTransform, lrTargetAABB))
        {
            const f32 lfZoom = Utils::GetZoomFromFOVDegs(lrCamera.mfFOV);
            const f32 lfDX = lrCameraPosition.x - lrTargetPosition.x;
            const f32 lfDY = lrCameraPosition.y - lrTargetPosition.y;
            const f32 lfDZ = lrCameraPosition.z - lrTargetPosition.z;
            const f32 lfDistanceSq = lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ;
            lbOnScreen = !(lfDistanceSq > lfMaxDistanceSq * lfZoom);
        }

        if (lbOnScreen)
        {
            mfOffscreenTime = 0.0f;                                                   // flt_82001CC0
            mbIsOnScreen    = true;
        }
        else
        {
            mbIsOnScreen = false;
        }
    }

    if (lbDoTest)
    {
        lpRequestInterface->LineTestNearest(mLineTestA, 0x1Eu, 0xFFu, lrCameraPosition, lrTargetPosition,
                                            lTargetEntityId,
                                            CgsSceneManager::SceneManagerIO::E_EXCLUDE_ALL_CHILD_PARTS);
        lpRequestInterface->LineTestNearest(mLineTestB, 0x1Eu, 0xFFu, lrTargetPosition, lrCameraPosition,
                                            lTargetEntityId,
                                            CgsSceneManager::SceneManagerIO::E_EXCLUDE_ALL_CHILD_PARTS);
    }
}

// ----------------------------------------------------------------------------
// VisibilityTest::ProcessSceneQueryResults @0x8220E290 (DWARF BrnCollisionPolicy.h:241)
//
//   if either box has its answer (A GOT || B GOT)                          (0x8220E2AC..0x8220E2C4)
//       hit = (A GOT && A.mbIntersection) || (B GOT && B.GetPackage().mbIntersection)
//            (A's GetPackage is folded into the state test; B's keeps its BrnPostBox.h:110 assert)
//       both boxes emptied (`stw 0` to +0x00 and +0x50)
//       hit ? mbOccluded = true : { mbOccluded = false; mfOccludedTime = 0 }
//   if (mbOccluded)     mfOccludedTime  += dt                              (0x8220E364..74)
//   if (!mbIsOnScreen)  mfOffscreenTime += dt                              (0x8220E37C..90)
// ----------------------------------------------------------------------------
void VisibilityTest::ProcessSceneQueryResults(f32 lfTimestep)
{
    if (mLineTestA.HasPackage() || mLineTestB.HasPackage())
    {
        const bool lbHitA = mLineTestA.HasPackage() && mLineTestA.GetPackage().mbIntersection;
        const bool lbHitB = mLineTestB.HasPackage() && mLineTestB.GetPackage().mbIntersection;

        mLineTestA.Clear();
        mLineTestB.Clear();

        if (lbHitA || lbHitB)
        {
            mbOccluded = true;
        }
        else
        {
            mbOccluded     = false;
            mfOccludedTime = 0.0f;                                                    // flt_82001CC0
        }
    }

    if (mbOccluded)
        mfOccludedTime += lfTimestep;

    if (!mbIsOnScreen)
        mfOffscreenTime += lfTimestep;
}

} // namespace Camera
} // namespace BrnDirector
