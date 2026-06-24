// ============================================================================
// GameSource/Director/Shots/ShotControllers/BrnKeyAnimController.cpp
//
// Compilation home for the BrnDirector::KeyAnimController accessor slice this TU owns:
//   - KeyAnimController::GetEyeSpace  @0x821F4138
//   - KeyAnimController::GetLookSpace @0x821F4190
//   - KeyAnimController::GetLookPos   @0x821F41E8
//   - KeyAnimController::HasFinished  @0x821F4258
//
// Sampled each frame by BehaviourIceAnim::Update / HasFinishedOrFailed. Each accessor asserts the
// controller has been prepared before sampling. The full controller (Prepare/Update/the rig) lands
// with its own TU; this .cpp pins the asm-attested member offsets and homes the four bodies.
// ============================================================================

#include "GameSource/Director/Shots/ShotControllers/BrnKeyAnimController.h"
#include <cstddef>   // offsetof

namespace BrnDirector
{

// Pin the asm-attested size-stable member offsets the four accessors touch (the console's 4-byte
// pointer slots are pinned; the type-correct values live in the tail shadows, reached by name).
static_assert(offsetof(KeyAnimController, mLookPos)        == 0x010, "look position @ +0x10");
static_assert(offsetof(KeyAnimController, muClipSlot)      == 0x024, "clip ptr slot @ +0x24");
static_assert(offsetof(KeyAnimController, muEyeSpaceSlot)  == 0x758, "eye space slot @ +0x758");
static_assert(offsetof(KeyAnimController, muLookSpaceSlot) == 0x75C, "look space slot @ +0x75C");
static_assert(offsetof(KeyAnimController, mfTime)          == 0x760, "clip time @ +0x760");
static_assert(offsetof(KeyAnimController, mbPrepared)      == 0x765, "mbPrepared @ +0x765");
static_assert(offsetof(KeyAnimController, mbReverse)       == 0x767, "mbReverse @ +0x767");
static_assert(offsetof(KeyAnimClip, mfDuration)            == 0x2C,  "clip duration @ +0x2C");

// ----------------------------------------------------------------------------
// BrnDirector::KeyAnimController::GetEyeSpace @0x821F4138
//   assert mbPrepared (BrnKeyAnimController.h:107), then  lwz r3, 0x758(this); blr
// ----------------------------------------------------------------------------
Space* KeyAnimController::GetEyeSpace() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    return mpEyeSpace;                                 // lwz r3, 0x758(this)
}

// ----------------------------------------------------------------------------
// BrnDirector::KeyAnimController::GetLookSpace @0x821F4190
//   assert mbPrepared (:110), then  lwz r3, 0x75C(this); blr
// ----------------------------------------------------------------------------
Space* KeyAnimController::GetLookSpace() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    return mpLookSpace;                                // lwz r3, 0x75C(this)
}

// ----------------------------------------------------------------------------
// BrnDirector::KeyAnimController::GetLookPos @0x821F41E8
//   assert mbPrepared (:113), then copy the 16-byte aligned look position from this+0x10 (lvx128,
//   this in r4) into the by-value return slot (stvx128, sret in r3). A plain aligned vector copy.
// ----------------------------------------------------------------------------
rw::math::vpu::Vector3 KeyAnimController::GetLookPos() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    return mLookPos;                                   // lvx128 (this+0x10) -> stvx128 (sret)
}

// ----------------------------------------------------------------------------
// BrnDirector::KeyAnimController::HasFinished @0x821F4258
//   assert mbPrepared (:157).
//   if (mbReverse @+0x767)  : finished when mfTime <= 0.0   (lfs 0.0; fcmpu; ble -> 1 else 0)
//   else (forward)          : duration = mpClip ? mpClip->mfDuration : 0.0 (lwz 0x24; lfs 0x2C)
//                             finished when mfTime >= duration  (fcmpu; bge -> 1 else 0)
//   The fcmpu/ble/bge lowering matches C's `<=` / `>=` (unordered/NaN -> false -> not finished).
// ----------------------------------------------------------------------------
bool KeyAnimController::HasFinished() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");

    if (mbReverse)                                     // lbz 0x767; bne
    {
        return mfTime <= 0.0f;                          // lfs 0.0; fcmpu mfTime,0.0; ble
    }

    const f32 lfDuration = mpClip ? mpClip->mfDuration : 0.0f;   // lwz 0x24; lfs 0x2C / 0.0
    return mfTime >= lfDuration;                        // fcmpu mfTime,duration; bge
}

} // namespace BrnDirector
