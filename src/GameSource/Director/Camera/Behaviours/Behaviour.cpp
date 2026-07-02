#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"   // BrnDirector::Camera::Behaviour (base slice)

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT (BeginAssert/FireAssert/EndAssert)

// ============================================================================
// GameSource/Director/Camera/Behaviours/Behaviour.cpp
//
// HOME for the two base BrnDirector::Camera::Behaviour methods every concrete camera
// behaviour shares -- the abstract base the BehaviourManager pool holds an interior of:
//
//   - Behaviour::SetCantSwitchFromMeNow @0x82206388  -- mark "the director cannot switch
//       away from me this frame" with a reason id.
//   - Behaviour::Fail                   @0x822063E8  -- give up: record the failure reason
//       and raise the failed flags.
//
// Both methods reach two foreign blocks the asm touches through the per-frame info pointer
// the caller hands in (`lpInfo`):
//
//   * info +0x138 (312): a BrnDirector::Camera::ValidityAccount -- the per-behaviour
//       validity bit-account. SetCantSwitchFromMeNow records its reason through it
//       (X360 sub_82204148); Fail sets a flag in it (ValidityAccount::SetFlag).
//   * info +0x140 (320): a 64-bit camera-request flag word Fail clears bit 1 of (the
//       "follow" request bit), matching the asm `ld/and ~3.../std` on +0x140.
//
// FLAG: the ValidityAccount home and the +0x140 flag-word block are genuinely un-homed
// (SetFlag @0x82204028 now HAS a committed home -- GameSource/Director/Camera/
// BrnCameraValidityAccount.h -- migrate these helpers to it; sub_82204148 remains
// un-homed). They are modelled
// here as named declaration-only helpers in `detail` -- the call sites read faithfully
// against the asm, but the per-TU `cl /c` gate does not link them. Replace with the real
// homes when the Behaviour shared-info / ValidityAccount TU lands; the helper NAMES are
// stable. No offsets are cast here -- the foreign reaches go through the named helpers.
//
// Source-of-truth: ARTIST X360 pseudocode + ASM @0x82206388 / @0x822063E8.
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// Foreign reaches the two bodies make through the info pointer. Each is the qualified
// callee the asm `bl`s, expressed as a named helper whose real home is its owning TU.
// DECLARATION-ONLY (the per-TU gate does not link them -- the ValidityAccount and the
// info request-flag word are un-homed Behaviour interior).
// ----------------------------------------------------------------------------
namespace detail
{
    // ---- BrnDirector::Camera::ValidityAccount:: ---- (the account at info +0x138) ----
    // Fail sets the failure-reason flag in the account (X360 ValidityAccount::SetFlag).
    void ValidityAccount_SetFlag(void* lpValidityAccount, s32 liReason);
    // SetCantSwitchFromMeNow records the "can't switch" reason in the account (X360
    // sub_82204148 -- a sibling ValidityAccount method).
    void ValidityAccount_RecordCantSwitch(void* lpValidityAccount, s32 liReason);

    // ---- the camera-request flag word at info +0x140 ----
    // Fail drops the "follow" request bit (asm: ld; and ~2; std on +0x140).
    void Info_ClearFollowRequest(void* lpInfo);

    // The byte-typed view of the info pointer the two foreign blocks hang off. Provenance
    // only -- the helpers above own the real reaches; this just names the +0x138 base the
    // asm forms with `addi r3, info, 0x138`.
    inline void* InfoValidityAccount(void* lpInfo)
    {
        return static_cast<void*>(static_cast<u8*>(lpInfo) + 0x138);
    }
}
using namespace detail;

// ============================================================================
// BrnDirector::Camera::Behaviour::SetCantSwitchFromMeNow @0x82206388
//
// Record "the director cannot switch away from me this frame" with the given reason id.
// Asserts the behaviour has not already failed, records the reason in the info's validity
// account, then clears the "failed flag C" byte.
//
// asm:  lbz r11,9(this); cmplwi -> beq          : if (mbFailed) fire assert
//       addi r3,info,0x138; bl sub_82204148      : ValidityAccount record (info +0x138)
//       li r11,0; stb r11,0xC(this)              : mbBaseFlagC = false
// ============================================================================
void Behaviour::SetCantSwitchFromMeNow(void* lpInfo, s32 liReason)
{
    CGS_ASSERT(!mbFailed,
               "Setting \"Cant switch from me now\" when behaviour has failed");

    ValidityAccount_RecordCantSwitch(InfoValidityAccount(lpInfo), liReason);

    mbBaseFlagC = false;   // stb 0, 0xC(this)
}

// ============================================================================
// BrnDirector::Camera::Behaviour::Fail @0x822063E8
//
// Give up following: flag the failure reason in the info's validity account, raise this
// behaviour's failed/clear-state flags, and drop the info's "follow" request bit.
//
// asm:  addi r3,info,0x138; bl ValidityAccount::SetFlag  : account SetFlag (info +0x138)
//       li r11,1; stb r11,0xC(this)                      : mbBaseFlagC   = true
//       stb r11,9(this)                                  : mbFailed      = true
//       li r10,0; stb r10,0xB(this)                      : mbMotionBlurGate = false
//       ld r11,0x140(info); and r11,~3...,2; std         : drop info follow bit (+0x140)
// ============================================================================
void Behaviour::Fail(void* lpInfo, s32 liReason)
{
    ValidityAccount_SetFlag(InfoValidityAccount(lpInfo), liReason);

    mbBaseFlagC      = true;    // stb 1, 0xC(this)
    mbFailed         = true;    // stb 1, 9(this)
    mbMotionBlurGate = false;   // stb 0, 0xB(this)

    Info_ClearFollowRequest(lpInfo);   // *(info+0x140) &= ~2
}

} // namespace Camera
} // namespace BrnDirector
