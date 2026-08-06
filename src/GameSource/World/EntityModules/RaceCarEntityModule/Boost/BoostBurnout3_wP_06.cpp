// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 06.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
// Bodies in this partfile (reconstructed from BURNOUT_X360_ARTIST.XEX):
//   BoostBurnout3::RemoveAllBoostAndChunks @ 0x822A6B68   (vtable slot 41)
//   BoostBurnout3::SetCarStatBoostLevel    @ 0x822C1BC8   (vtable slot 43)
//
// NOT here -- PARKED, see the wave report:
//   BoostBurnout3::Prepare                 @ 0x822C1680   (vtable slot 0)
//   parked at scratchpad/waveP/parked/BoostBurnout3_wP_6.parked.cpp (workflow
//   repo), COMPLETE and asm-walked. Its retail body loads the whole 34-entry
//   boostparamsasset tuning record (`lwz r11,4(asset)` @0x822C1718 then 34
//   fixed-offset lfs/lwz), and the committed Attrib::Gen::boostparamsasset
//   inherits Attrib::Instance PRIVATELY and re-exports nothing, so there is no
//   public way to reach that attribute data area. Unblocking it is ONE line in
//   GameSource/AttribSys/Generated/classes/boostparamsasset.h:
//       using Instance::GetLayoutPointer;
//   (verbatim precedent: cameraexternalbehaviour.h:34). Same blocker the wP_01
//   agent hit on ApplyUpdate.
//
// Vtable note (shared with every BoostBurnout3 partfile): vtable +0xC8 == slot
// 50 == BoostBurnout3's OWN `virtual void UpdateMaxBoost(bool)` (DWARF
// BrnBoostBurnout3.h:118, real X360 body @0x822C1C80), a NEW virtual that HIDES
// the base's non-virtual BoostStrategy::UpdateMaxBoost -- so an unqualified
// UpdateMaxBoost(...) from inside BoostBurnout3 reproduces the observed dispatch.
//
// Member note: the BoostBurnout3-specific members live at +0x130..+0x13C in the
// DecFIGS DWARF order mfBoostChunkAmount / miBoostLevel / miOldBoostLevel /
// mfTimeBoosting. That order is independently confirmed by the STORE KINDS in
// Prepare @0x822C1680 -- stfs/stw/stw/stfs at 0x130/0x134/0x138/0x13C matches
// float/int32/int32/float 4-for-4. BoostBurnout3::miBoostLevel (+0x134)
// deliberately hides BoostStrategy::miBoostLevel (+0x104).
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.h"

#include "rw/math/fpu/scalar_operation.h"   // rw::math::fpu::Clamp -- the console's
                                            // branchless double-fsel clamp

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// RemoveAllBoostAndChunks @ 0x822A6B68 -- vtable slot 41 (pure in the base).
//
//   0x822A6B68  lis   r11, flt_82001CC0@ha
//   0x822A6B6C  lwz   r10, 0(r3)              ; vptr
//   0x822A6B70  li    r9, 1
//   0x822A6B74  li    r4, 0                   ; UpdateMaxBoost's bool = false
//   0x822A6B78  lfs   f0, flt_82001CC0@l(r11) ; 0.0f
//   0x822A6B7C  lwz   r11, 0xC8(r10)          ; slot 50 == B3::UpdateMaxBoost(bool)
//   0x822A6B80  stfs  f0, 0xA0(r3)            ; mfBoostAmount = 0.0f
//   0x822A6B84  stw   r9, 0x134(r3)           ; miBoostLevel = 1
//   0x822A6B88  mtctr r11
//   0x822A6B8C  bctr                          ; TAIL call
//
// DIVERGENCE from Feb-2007 (BrnBoostBurnout3.cpp:383): the leaked body is
// `mfBoostAmount = 0.0f; miBoostLevel = 0; UpdateMaxBoost();`. Retail's floor is
// ONE chunk, not zero -- `li r9,1` is unambiguous -- which is consistent with the
// retail UpdateMaxBoost @0x822C1C80 treating a recomputed max boost of 0.0 as an
// error case (it prints "STOP\n" and forces mfMaxBoost = 1.0f). Level 0 is no
// longer a valid resting state. And UpdateMaxBoost now takes a bool.
// ---------------------------------------------------------------------------
void BoostBurnout3::RemoveAllBoostAndChunks()
{
    mfBoostAmount = 0.0f;

    miBoostLevel = 1;

    UpdateMaxBoost( false );
}

// ---------------------------------------------------------------------------
// SetCarStatBoostLevel @ 0x822C1BC8 -- vtable slot 43.
//
//   0x822C1BE0  extsw r10, r5              ; liBoostLossLevel -> 64-bit
//   0x822C1BE8  li    r4, 0                ; UpdateMaxBoost's bool = false
//   0x822C1BF4  lwz   r9, 0xC8(vptr)       ; slot 50 == B3::UpdateMaxBoost(bool)
//   0x822C1BFC  lfs   f31, flt_82001CC0    ; 0.0f
//   0x822C1C08  lfd/fcfid/frsp             ; (f32)liBoostLossLevel
//   0x822C1C14  stfs  f0, 0xF0(this)       ; mfCurrentCarBoostLossLevel = raw
//   0x822C1C18  fneg  f13, f0
//   0x822C1C1C  fsel  f0, f13, f31, f0     ; lower edge at 0.0f
//   0x822C1C20  lfs   f13, flt_82014808    ; 100.0f (Hex-Rays resolves the literal
//   0x822C1C24  fsubs f12, f13, f0           here AND in the base body @0x822D52DC)
//   0x822C1C28  fsel  f0, f12, f0, f13     ; upper edge at 100.0f
//   0x822C1C2C  stfs  f0, 0xF0(this)       ; mfCurrentCarBoostLossLevel = clamped
//   0x822C1C34  bctrl                      ; UpdateMaxBoost(false)
//   0x822C1C38  lfs   f0, 0xA4(this)       ; mfMaxBoost, read AFTER the recompute
//   0x822C1C3C  lfs   f13, flt_820147FC    ; 0.5f
//   0x822C1C40  fmuls f13, f0, f13
//   0x822C1C44  lfs   f12, flt_820147E8    ; 0.15f
//   0x822C1C48  fmuls f12, f0, f12
//   0x822C1C4C  stfs  f12, 0x100(this)     ; mfMinBoostAllowedAmount
//   0x822C1C50  fneg  f12, f13
//   0x822C1C54  fsel  f13, f12, f31, f13   ; lower edge at 0.0f
//   0x822C1C58  fsubs f12, f0, f13
//   0x822C1C5C  fsel  f0, f12, f13, f0     ; upper edge at mfMaxBoost
//   0x822C1C60  stfs  f0, 0xA0(this)       ; mfBoostAmount
//
// The two fsel pairs are the console's branchless double-fsel clamp, which is
// exactly what rw::math::fpu::Clamp models (its header says so); the same two
// pairs appear verbatim in BoostStrategy::SetCarStatBoostLevel @0x822D5290 and in
// BoostStrategy::UpdateMaxBoost @0x822C0EF4.
//
// ⚠️ liBoostLevel IS UNUSED in this override. r4 carries it in and is overwritten
// with 0 at 0x822C1BE8 (the UpdateMaxBoost argument) before ever being read; only
// r5 is consumed. The BASE body DOES store its first parameter -- `stw r11,0xE8`
// and `stw r11,0x104` at 0x822D52AC/B0 (miCombinedBoostLevel and
// BoostStrategy::miBoostLevel) -- and this override deliberately drops both, so a
// Burnout-3 car's maximum stays driven by its own chunk count (+0x134). The
// parameter is kept and named because the base fixes the slot signature.
//
// The 0.15f and 0.5f literals are left as literals on purpose (same call as the
// wP_02 agent made for the identical flt_820147E8 store in OnEndCrashPlay): the
// DWARF for this .cpp declares KF_BOOST_LOSS_SCALE (cpp:37) and
// KF_MIN_BOOST_LOSS_SCALE (cpp:38), and nothing in the image says which -- if
// either -- owns these values, so naming them would be a guess. All three
// comparands (0.0f / 0.15f / 0.5f / 100.0f) are shared rodata slots that the base
// body loads at the same addresses.
//
// No Feb-2007 counterpart: SetCarStatBoostLevel does not exist in the leaked
// source at all.
// ---------------------------------------------------------------------------
void BoostBurnout3::SetCarStatBoostLevel( s32 liBoostLevel, s32 liBoostLossLevel )
{
    (void)liBoostLevel;   // read by the BASE override, never by this one

    mfCurrentCarBoostLossLevel = static_cast<f32>( liBoostLossLevel );
    mfCurrentCarBoostLossLevel = rw::math::fpu::Clamp( mfCurrentCarBoostLossLevel, 0.0f, 100.0f );

    UpdateMaxBoost( false );

    mfMinBoostAllowedAmount = mfMaxBoost * 0.15f;                                  // flt_820147E8
    mfBoostAmount = rw::math::fpu::Clamp( mfMaxBoost * 0.5f, 0.0f, mfMaxBoost );   // flt_820147FC
}

} // namespace BrnWorld
