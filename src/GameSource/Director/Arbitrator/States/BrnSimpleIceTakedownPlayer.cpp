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
// The X360 class-key test is `a2[1] == (*a2 | 0xC1EE)`: the iceanim object's
// verification dword at +4 must equal its type word at +0 OR'd with the iceanim magic
// 0xC1EE (== the low 16 bits of Attrib::Gen::iceanim::ClassKey() 0x...A997C1EE). This
// is the AttribSys class-key check the DWARF renders as
// lpIceAnim->GetClassKey()==Attrib::Gen::iceanim::ClassKey(); reproduced verbatim as
// the two-dword compare so it matches the asm exactly (a full 8-byte compare against
// ClassKey() would NOT -- the runtime only OR-checks the low word).
// ============================================================================

#include "GameSource/Director/Arbitrator/States/BrnSimpleIceTakedownPlayer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnDirector
{
    void SimpleIceTakedownPlayer::SetIceAnim(Attrib::Gen::iceanim* lpIceAnim)
    {
        // X360: if (!a2 || a2[1] != (*a2 | 0xC1EE)) FireAssert(...). The && short-circuit
        // matches the asm (a2[1]/*a2 are only read when a2 is non-null).
        const u32* lpClassKeyWords = reinterpret_cast<const u32*>(lpIceAnim);
        CGS_ASSERT(lpIceAnim != nullptr &&
                   lpClassKeyWords[1] == (lpClassKeyWords[0] | 0xC1EEu),
                   "lpIceAnim && lpIceAnim->GetClassKey()==Attrib::Gen::iceanim::ClassKey()");

        mpIceAnim = lpIceAnim;
    }
}
