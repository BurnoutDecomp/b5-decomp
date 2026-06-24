// ============================================================================
// GameSource/Director/Camera/BrnCameraState.cpp
//
// Compilation home for the BrnDirector::Camera::CameraState dirty-flag double buffer:
//   - CameraState::SetFlag    @0x82204368  (set/clear a flag in the current bit set)
//   - CameraState::ClearFlag  @0x822044B0  (clear a flag in the current bit set)
//   - CameraState::HasChanged @0x8227D380  (current vs previous bit, per flag)
//
// Used by the camera behaviours to flag which camera outputs changed this frame
// (BehaviourGameplayExternal::Update sets/clears; EffectsModule::Update queries HasChanged).
// ============================================================================

#include "GameSource/Director/Camera/BrnCameraState.h"
#include <cstddef>   // offsetof

namespace BrnDirector
{
namespace Camera
{

// The X360 places the two 64-bit bit fields at +0x08 (current) and +0x10 (previous):
// SetFlag/ClearFlag use `addi r,this,8` then a qword ldx/stdx; HasChanged reads the
// adjacent qword at +0x10 as the previous-frame comparand.
static_assert(offsetof(CameraState, mCurrentFlags)  == 0x08, "current flag set @ +0x08");
static_assert(offsetof(CameraState, mPreviousFlags) == 0x10, "previous flag set @ +0x10");

// ----------------------------------------------------------------------------
// BrnDirector::Camera::CameraState::SetFlag @0x82204368
//
// Two arms guarded by lbValue (r5):
//   lbValue != 0 -> OR-set bit luIndex in the current set (bound assert at CgsBitArray.h:222).
//   lbValue == 0 -> ANDC-clear bit luIndex in the current set (bound assert at :241).
// The current set base is `this+0x08`; index math is bit luIndex (luIndex<30 -> field 0):
//   set:   ldx q,(base); or  q, q, (1<<(luIndex&63)); stdx q,(base)
//   clear: ldx q,(base); andc q, q, (1<<(luIndex&63)); stdx q,(base)
// ----------------------------------------------------------------------------
void CameraState::SetFlag(u32 luIndex, bool lbValue)
{
    if (lbValue)
    {
        CGS_ASSERT(luIndex < KU_NUM_FLAGS, "luIndex < NUMBITS");
        mCurrentFlags.SetBit(luIndex);                 // ldx/or/stdx @ this+8
    }
    else
    {
        CGS_ASSERT(luIndex < KU_NUM_FLAGS, "luIndex < NUMBITS");
        mCurrentFlags.UnSetBit(luIndex);               // ldx/andc/stdx @ this+8
    }
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::CameraState::ClearFlag @0x822044B0
//
// Bound assert (CgsBitArray.h:241), then ANDC-clear bit luIndex in the current set:
//   ldx q,(this+8); andc q, q, (1<<(luIndex&63)); stdx q,(this+8)
// ----------------------------------------------------------------------------
void CameraState::ClearFlag(u32 luIndex)
{
    CGS_ASSERT(luIndex < KU_NUM_FLAGS, "luIndex < NUMBITS");
    mCurrentFlags.UnSetBit(luIndex);                   // ldx/andc/stdx @ this+8
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::CameraState::HasChanged @0x8227D380
//
// Reads bit luIndex from the current set (qword at this+0x08) and from the previous set
// (qword at this+0x10), then returns whether the two differ:
//   bCurrent  = (current[field]  & (1<<(luIndex&63))) != 0
//   bPrevious = (previous[field] & (1<<(luIndex&63))) != 0
//   return bCurrent != bPrevious
// The asm guards each read with the index-bound assert (CgsBitArray.h:203). The trailing
// cntlzw/extrwi/xori idiom is the compiler lowering of the boolean `!=` comparison.
// ----------------------------------------------------------------------------
bool CameraState::HasChanged(u32 luIndex) const
{
    CGS_ASSERT(luIndex < KU_NUM_FLAGS, "luIndex < NUMBITS");
    const bool lbCurrent = mCurrentFlags.IsBitSet(luIndex);      // first qword read @ this+8

    CGS_ASSERT(luIndex < KU_NUM_FLAGS, "luIndex < NUMBITS");
    const bool lbPrevious = mPreviousFlags.IsBitSet(luIndex);    // second qword read @ this+16

    return lbPrevious != lbCurrent;
}

} // namespace Camera
} // namespace BrnDirector
