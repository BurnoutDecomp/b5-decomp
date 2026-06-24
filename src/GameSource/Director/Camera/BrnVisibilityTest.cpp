// ============================================================================
// GameSource/Director/Camera/BrnVisibilityTest.cpp
//
// Compilation home for the BrnDirector::Camera::VisibilityTest slices this TU owns:
//   - VisibilityTest::GetOffscreenTime @0x821F3718
//   - VisibilityTest::IsOnScreen       @0x821F3770
//
// Both are read by BrnDirector::Camera::VisibilityCollisionPolicy::ProcessSceneQueryResults
// after a scene query resolves the subject's on-screen state.
// ============================================================================

#include "GameSource/Director/Camera/BrnCollisionPolicy.h"

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
//   lbz   r3, 0xB2(this)       ; return mbOnScreen  (byte load)
// ----------------------------------------------------------------------------
bool VisibilityTest::IsOnScreen() const
{
    CGS_ASSERT(mbTestLookingAt, "mbTestLookingAt");   // lbz 0xB0; assert set
    return mbOnScreen != 0;                           // lbz r3, 0xB2(this)
}

} // namespace Camera
} // namespace BrnDirector
