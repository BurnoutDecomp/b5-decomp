// ============================================================================
// BrnWorld::BoostBurnout3 -- wave P partfile 04 (three stunt/crash-play hooks).
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.cpp
//
//   0x822A6620  BoostBurnout3::OnPropHit            (vtable slot 16)
//   0x822A6830  BoostBurnout3::OnStuntCompletion    (vtable slot 10)
//   0x822A6AA0  BoostBurnout3::OnStartCrashPlay     (vtable slot 13)
//
// SOURCES (authority order): X360 ARTIST asm at the three addresses above;
// DecFIGS DWARF BrnBoostBurnout3.h (declaration shape + member/constant names);
// Feb-2007 BrnBoostBurnout3.{h,cpp} for idiom only -- see the divergence notes.
//
// VTABLE DECODE (X360 slot offsets, 4-byte slots):
//   +0xC4 == slot 49 == BoostStrategy::AddBoost(f32)          (protected, base)
//   +0xC8 == slot 50 == BoostBurnout3::UpdateMaxBoost(bool)   (B3's OWN new
//            virtual, DWARF B3 h:118 inline `{}`, which HIDES the base's
//            NON-virtual UpdateMaxBoost -- so an unqualified call from a B3
//            body compiles to an indirect call through +0xC8, exactly as the
//            asm shows). BoostBurnout5, which does not redeclare it, instead
//            branches straight to BrnWorld__BoostStrategy__UpdateMaxBoost
//            (@0x822D534C) -- the control confirming the +0xC8 decode is B3's
//            own extension slot and not a base slot.
//
// MEMBER DECODE:
//   base +0x80 mfStuntJumpEarning   +0x84 mfStuntSmashEarning
//   base +0x88 mfStuntBillBoardEarning     base +0x100 mfMinBoostAllowedAmount
//   B3   +0x134 miBoostLevel        B3   +0x138 miOldBoostLevel
//   (B3's miBoostLevel@+0x134 SHADOWS the base's miBoostLevel@+0x104; the base
//   member is the one SetBoostSegments @0x822C0F30 writes. Unqualified uses in
//   a B3 body bind to B3's, which is what the asm touches.)
//
// FEB-2007 DIVERGENCES (asm wins -- documented per body below):
//   * Feb-2007 has NO OnPropHit at all.
//   * Feb-2007 OnStuntCompletion takes no argument and is just `OnTakedown();`.
//     Retail takes a BrnGameState::StuntElementType and branches on it.
//   * Feb-2007 OnStartCrashPlay is `miBoostLevel = KI_BOOST_LEVELS;
//     UpdateMaxBoost();` -- retail additionally saves miOldBoostLevel first and
//     stores mfMinBoostAllowedAmount afterwards, and UpdateMaxBoost takes bool.
//   * Feb-2007 KI_BOOST_LEVELS == 5; DWARF and the retail asm both say 3
//     (0x822A6AB4 `li r10, 3` -> miBoostLevel; 0x822A6858 `cmpwi r11, 3` cap).
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.h"

#include "GameSource/GameState/BrnGameStateTypes.h"   // BrnGameState::StuntElementType

namespace BrnWorld
{

// ----------------------------------------------------------------------------
// X360 0x822A6620 -- vtable slot 16. A bare tail-call, no stack frame:
//     lwz r11, 0(r3)      ; vptr
//     lfs f1, 0x84(r3)    ; mfStuntSmashEarning
//     lwz r11, 0xC4(r11)  ; AddBoost
//     mtctr r11 / bctr
// Retail-only: the Feb-2007 BoostBurnout3 has no prop-hit hook, and retail's
// OnStuntCompletion deliberately does NOT handle E_STUNT_ELEMENT_TYPE_SMASH --
// the smash earning is paid here instead.
// ----------------------------------------------------------------------------
void
BoostBurnout3::OnPropHit()
{
    AddBoost( mfStuntSmashEarning );
}


// ----------------------------------------------------------------------------
// X360 0x822A6830 -- vtable slot 10. Compare chain (not a jump table):
//     cmpwi r4, 0 ; beq  -> loc_822A68AC : AddBoost(0x80) then return
//     cmpwi r4, 2 ; bne  -> loc_822A68C4 : return, no side effect
//     ; leElementType == 2:
//     lwz  r11, 0x134(r31) ; cmpwi r11, 3 ; bge skip
//     addi r11, r11, 1 ; stw r11, 0x134(r31)
//     lfs  f1, 0x88(r31) ; call vtable+0xC4 (AddBoost)
//     li   r4, 0         ; call vtable+0xC8 (UpdateMaxBoost(false))
// Note the call ORDER: AddBoost BEFORE UpdateMaxBoost here, whereas
// BoostBurnout3::OnTakedown @0x822A6638 calls UpdateMaxBoost (+0xC8) first and
// AddBoost (+0xC4) second. Kept as the asm has it.
// E_STUNT_ELEMENT_TYPE_SMASH (1) falls through with no side effect.
// ----------------------------------------------------------------------------
void
BoostBurnout3::OnStuntCompletion(
    BrnGameState::StuntElementType leElementType )
{
    switch( leElementType )
    {
    case BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP:
        AddBoost( mfStuntJumpEarning );
        break;

    case BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD:
        if( miBoostLevel < KI_BOOST_LEVELS )
        {
            ++miBoostLevel;
        }

        AddBoost( mfStuntBillBoardEarning );

        UpdateMaxBoost( false );
        break;

    default:
        // E_STUNT_ELEMENT_TYPE_SMASH is paid by OnPropHit, not here.
        break;
    }
}


// ----------------------------------------------------------------------------
// X360 0x822A6AA0 -- vtable slot 13.
//     lwz  r11, 0x134(r31)   ; miBoostLevel
//     li   r10, 3            ; KI_BOOST_LEVELS
//     stw  r10, 0x134(r31)   ; miBoostLevel    = KI_BOOST_LEVELS
//     stw  r11, 0x138(r31)   ; miOldBoostLevel = <previous level>
//     li   r4, 0 ; call vtable+0xC8            ; UpdateMaxBoost(false)
//     lfs  f0, flt_82014460 ; stfs f0, 0x100(r31)
// flt_82014460 == 0x34000000 == 1.1920929e-07 == FLT_EPSILON; the same .rdata
// slot is the near-zero spin-angle threshold in BoostStrategy::Update
// @0x822F82CC and the same value BoostBurnout5::OnStartCrashPlay @0x822D5344
// stores into mfMinBoostAllowedAmount. Effect: crash play forces the bar to the
// top segment while leaving essentially no floor on the allowed boost amount.
// ----------------------------------------------------------------------------
void
BoostBurnout3::OnStartCrashPlay()
{
    miOldBoostLevel = miBoostLevel;
    miBoostLevel    = KI_BOOST_LEVELS;

    UpdateMaxBoost( false );

    mfMinBoostAllowedAmount = 1.1920929e-07f;   // flt_82014460 == FLT_EPSILON
}

} // namespace BrnWorld
