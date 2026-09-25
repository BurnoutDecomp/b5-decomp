// ============================================================================
// GameSource/Director/Arbitrator/States/BrnSimpleIceTakedownPlayer.cpp
//
// BrnDirector::SimpleIceTakedownPlayer::SetIceAnim @ 0x821F58C8 -- bind the ICE anim
// that drives this takedown camera. In a debug build it first validates the passed
// object's AttribSys class-key, then stores the pointer at the player's iceanim slot
// (v3[6] = a2, i.e. +0x18). The baked assert string/file/line are
// BrnArbStateTakedown.h:236 (the method body is inlined into that header in the X360
// image); CGS_ASSERT injects our own __FILE__/__LINE__.
//
// ⭐ CORRECTED 2026-09-25 (FX-DIRECTOR2) -- THE CLASS-KEY TEST IS ONE 64-BIT COMPARE.
// The asm (0x821F58E4..0x821F5908):
//     cmplwi r31, 0 ; beq <assert>                      -- a null shot asserts
//     ld     r11, 0(r31)                                -- the RefSpec's whole u64 class key
//     lis/ori r10 = 0xA997C1EE ; lis/ori r9 = 0x4644E379 ; insrdi r10, r9, 32,0
//     cmpld  r11, r10 ; beq <store>                     -- == 0x4644E379A997C1EE, Attrib::Gen::iceanim::ClassKey()
// then `stw r31, 0x18(r30)` on every path. The old body read Hex-Rays' `a2[1] != (*a2 | 0xC1EE)` as two
// dwords, "verification word == type word | 0xC1EE". On a little-endian host that is never true of the
// real iceanim key: its low half 0xA997C1EE sits at +0, its high half 0x4644E379 at +4. So EVERY
// shutdown takedown asserted (fxdirector2_iceanim_bystander/20260925_220559), and a PC assert pauses the
// game. The two-dword test also passed a wrong key whose halves are equal. Written as the other class-key
// sites are (BehaviourIceAnim::SetParameters, ShotSelector, CameraShakeICEController).
// ============================================================================

#include "GameSource/Director/Arbitrator/States/BrnSimpleIceTakedownPlayer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnDirector
{
    void SimpleIceTakedownPlayer::SetIceAnim(Camera::Camera::ShotReference* lpIceAnim)
    {
        // The && short-circuit matches the asm: the class key is only read when the shot is non-null.
        CGS_ASSERT(lpIceAnim != nullptr &&
                   static_cast<s64>(lpIceAnim->GetClassKey()) == Attrib::Gen::iceanim::ClassKey(),
                   "lpIceAnim && lpIceAnim->GetClassKey()==Attrib::Gen::iceanim::ClassKey()");

        mpIceAnim = lpIceAnim;
    }
}
