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
#include "GameSource/Director/Camera/BrnCameraValidityAccount.h"   // sbFailFlagMaskSet / sFailFlagMask (Clear)
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

// ----------------------------------------------------------------------------
// BrnDirector::Camera::CameraState::Clear @0x82220950 (destub wave 2026-07-26)
//
// Reset the state:
//   std 0 -> current (+0x08) and previous (+0x10) sets;
//   assert the ValidityAccount fail-flag mask was set up
//     ("sbFailFlagMaskSet", BrnCameraValidityAccount.h:193, non-gating);
//   head set (+0x00) &= sFailFlagMask (the qword_82FAA5D0 AND -- expressed
//     bit-for-bit through the container API per the named-member parity rule;
//     the mask only ever carries bits < 32);
//   ori 1 -> raise bit 0 (CONSISTENCY_TEST) on current and previous.
// ----------------------------------------------------------------------------
void CameraState::Clear()
{
    mCurrentFlags.UnSetAll();     // std 0, this+0x08
    mPreviousFlags.UnSetAll();    // std 0, this+0x10

    CGS_ASSERT(sbFailFlagMaskSet, "sbFailFlagMaskSet");   // :193 (non-gating)

    // mHeadFlags &= sFailFlagMask (single-qword AND on the X360).
    for (u32 luFlag = 0; luFlag < KU_NUM_FLAGS; ++luFlag)
    {
        if (!sFailFlagMask.IsBitSet(luFlag))
        {
            mHeadFlags.UnSetBit(luFlag);
        }
    }

    mCurrentFlags.SetBit(0);      // ori q,q,1
    mPreviousFlags.SetBit(0);     // ori q,q,1
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::CameraState::Construct @0x82252348 (DRIVE wave 2026-07-26)
//
// Zero all three flag sets (std 0 -> +0x00/+0x08/+0x10), run the one-time
// ValidityAccount::SetupFailFlagMask, then tail into Clear. [folded static per
// convention] the X360 re-checks `!strcmp(KAAC_FLAG_NAMES[CONSISTENCY_TEST],
// "CONSISTENCY TEST")` here (BrnCameraState.cpp:82) -- the same static name-table
// tautology folded in SetupFailFlagMask.
// ----------------------------------------------------------------------------
void CameraState::Construct()
{
    mCurrentFlags.UnSetAll();     // a1[1] = 0
    mPreviousFlags.UnSetAll();    // a1[2] = 0
    mHeadFlags.UnSetAll();        // *a1  = 0

    ValidityAccount::SetupFailFlagMask();

    Clear();                      // the X360 tail call
}


// ----------------------------------------------------------------------------
// Interpolate @ 0x82220BC0   (170 asm lines, DWARF BrnCameraState.cpp:121)
//
// The flag-set half of a camera blend. CameraInterpolationController::Update @0x822513D8
// calls it once per frame and copies all three qwords back over the live camera's state.
//
// IT IS A MERGE, NOT AN INTERPOLATION -- lfT only chooses between two merge modes:
//   lfT >= 1.0 : the result IS lrTo, wholesale (all three fields).
//   otherwise  : head = lrFrom's; current = lrFrom.current AND lrTo.current; and previous is
//                set to that same merged current, NOT carried from either input's previous.
// The AND is the conservative choice for a blend: a flag only one endpoint asserts is not
// true of the in-between camera.
//
// FIVE BITS ARE OR-ed INSTEAD, and none is arbitrary -- every one is a VALIDITY bit, where
// "either endpoint says so" is the safe direction:
//   bit 0  (mask 0x1)         within ValidityAccount's FAILED range   [0, 14)
//   bit 3  (mask 0x8)         within the FAILED range
//   bit 13 (mask 0x2000)      the last of the FAILED range
//   bit 27 (mask 0x8000000)   == ValidityAccount::E_FIRST_NOCUTFROM_FLAG
//   bit 28 (mask 0x10000000)  within the NOCUTFROM range [27, 31)
// Bits 0 and 13 are merged INSIDE the `lfT < 1` arm (asm 0x82220C48 / 0x82220C88, each with
// a matching else-branch that CLEARS the bit); bits 3, 27 and 28 are OR-ed AFTER the
// if/else, so they apply on the `lfT >= 1` path too (0x82220CC0 / 0x82220CE8 / 0x82220D10,
// no clear branch). That asymmetry is the console's, which is why this is two groups.
//
// THE MASKS MAP STRAIGHT TO BIT INDICES. The console reads them from `*(state + 12)` -- the
// LOW dword of the 8-byte field on big-endian PPC -- and CgsBitArray's own bit math is
// `1 << index` into a u64, so mask 0x8000000 is index 27, not 27+32. That the two highest
// land exactly on E_FIRST_NOCUTFROM_FLAG and its neighbour is the cross-check.
// ----------------------------------------------------------------------------
CameraState CameraState::Interpolate(const CameraState& lrFrom, const CameraState& lrTo, f32 lfT)
{
    CGS_ASSERT(lfT >= 0.0f && lfT <= 1.0f, "lfT >= 0.0f && lfT <= 1.0f");   // .cpp:121

    // The FAILED-range bits merged inside the `lfT < 1` arm (set OR clear).
    static const u32 KAU_MERGED_IN_ARM[2] = { 0u, 13u };
    // The bits OR-ed unconditionally, on both arms.
    static const u32 KAU_MERGED_ALWAYS[3] =
        { 3u, static_cast<u32>(ValidityAccount::E_FIRST_NOCUTFROM_FLAG), 28u };

    CameraState lResult = lrFrom;   // asm 0x82220C00: all three qwords seeded from lrFrom

    if (lfT >= 1.0f)
    {
        lResult = lrTo;
    }
    else
    {
        for (u32 luIndex = 0; luIndex < KU_NUM_FLAGS; ++luIndex)
        {
            if (lrFrom.mCurrentFlags.IsBitSet(luIndex) && lrTo.mCurrentFlags.IsBitSet(luIndex))
            {
                lResult.mCurrentFlags.SetBit(luIndex);
            }
            else
            {
                lResult.mCurrentFlags.UnSetBit(luIndex);
            }
        }

        for (u32 luSlot = 0; luSlot < 2; ++luSlot)
        {
            const u32 luIndex = KAU_MERGED_IN_ARM[luSlot];
            if (lrFrom.mCurrentFlags.IsBitSet(luIndex) || lrTo.mCurrentFlags.IsBitSet(luIndex))
            {
                lResult.mCurrentFlags.SetBit(luIndex);
            }
            else
            {
                lResult.mCurrentFlags.UnSetBit(luIndex);
            }
        }

        // asm `a1[2] = v13` with v13 == the merged current: the PREVIOUS set is overwritten
        // with the merged CURRENT one, not carried from either input.
        lResult.mPreviousFlags = lResult.mCurrentFlags;
    }

    // The three that apply on both arms -- OR only, never cleared.
    for (u32 luSlot = 0; luSlot < 3; ++luSlot)
    {
        const u32 luIndex = KAU_MERGED_ALWAYS[luSlot];
        if (lrFrom.mCurrentFlags.IsBitSet(luIndex) || lrTo.mCurrentFlags.IsBitSet(luIndex))
        {
            lResult.mCurrentFlags.SetBit(luIndex);
        }
    }

    return lResult;
}

} // namespace Camera
} // namespace BrnDirector
