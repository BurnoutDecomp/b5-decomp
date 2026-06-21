// ============================================================================
// GameSource/Director/Arbitrator/States/BrnSimpleIceTakedownPlayer.cpp
//
// BrnDirector::SimpleIceTakedownPlayer::SetIceAnim -- bind the ICE anim that drives
// this takedown camera. In a debug build it first checks the passed object is an
// iceanim by comparing the object's leading 8-byte class-key tag against iceanim's
// ClassKey(); then it stores the pointer at the player's iceanim slot (+0x18).
//
// FLAG: the class-key check reads the passed object's leading 8 bytes as its tag
//       (the inline form of lpIceAnim->GetClassKey()); modelled as a direct read so
//       the check is reproduced without depending on the full Attrib RTTI layout.
// ============================================================================

#include "GameSource/Director/Arbitrator/States/BrnSimpleIceTakedownPlayer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnDirector
{
    void SimpleIceTakedownPlayer::SetIceAnim(Attrib::Gen::iceanim* lpIceAnim)
    {
        CGS_ASSERT(lpIceAnim != nullptr &&
                   *reinterpret_cast<const s64*>(lpIceAnim) == Attrib::Gen::iceanim::ClassKey(),
                   "lpIceAnim && lpIceAnim->GetClassKey()==Attrib::Gen::iceanim::ClassKey()");

        mpIceAnim = lpIceAnim;
    }
}
