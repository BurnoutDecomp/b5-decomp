// Attrib::Gen::iceanim -- generated AttribSys class, out-of-line function bodies.
//
//   Attrib::Gen::iceanim::iceanim(const RefSpec&, void*)  @ 0x82206908
//
// See iceanim.h for the class shape. This file follows the convention songlist.cpp
// established in this directory: a per-generated-class .cpp for the bodies that are not
// inline in the header. GetAnimGuid is not itself an X360 out-of-line symbol (the console
// inlines the generated accessor); it is given a home here because it had none -- it was a
// declaration with committed callers and no definition anywhere in the tree. The RefSpec
// constructor, by contrast, IS a real X360 symbol -- the only iceanim ctor in the image.

#include "GameSource/AttribSys/Generated/classes/iceanim.h"

namespace Attrib
{
namespace Gen
{

// ============================================================================
// Attrib::Gen::iceanim::iceanim(const Attrib::RefSpec&, void*)  @ 0x82206908
// ============================================================================
// The generated "construct over a reference spec" ctor. Chain the Attrib::Instance RefSpec
// ctor (@0x8280A248 -- resolve the ref into a collection, cache its layout block, take a
// reference), assert the resolved collection really is an `iceanim` collection, then give
// the instance a 0x10-byte default data area if the resolve produced no layout block.
//
// X360 body:
//     bl  sub_8280A248                    ; Instance(refspec, owner)
//     bl  Attrib__Instance__GetClass      ; cmpld against 0x4644E379A997C1EE -> skip
//     bl  Attrib__Instance__GetClass      ; == 0 (class unset) -> skip
//     bl  Attrib__Instance__GetCollection ; else fire the class-check diagnostic
//     bl  Attrib__AssertOnClassCheck(GetClass(), ClassKey(), GetCollection())
//     lwz r11,4(this) ; if (!mpAttributeData) mpAttributeData = DefaultDataArea(0x10)
//
// That is the same shape as the Collection* sibling inlined in iceanim.h, and it is spelled
// the same way here (same KI_ICEANIM_CLASS constant, same guard order) so the two agree.
//
// ⚠️ ONE KNOWN NARROWING, inherited and NOT introduced here: the console compares the FULL
// 64-bit class key (`Attrib::Instance::GetClass` @0x82802F18 ends in `ld r3,0(r11)`, a
// doubleword load of Class +0x00), whereas this tree's Instance::GetClass returns `int` and
// every generated class in this directory therefore checks only the key's low word
// (KI_ICEANIM_CLASS == 0xA997C1EE == the low half of ClassKey()). The narrowing affects the
// diagnostic assert only -- never the construction or the layout pointer. Widening
// Instance::GetClass to u64 is a whole-directory change (~40 generated headers) and belongs
// to its own wave.
iceanim::iceanim(const Attrib::RefSpec& lrRefSpec, void* lpOwner)
    : Instance(lrRefSpec, lpOwner)
{
    static const int KI_ICEANIM_CLASS = -1449672210; // Attrib::ClassName::iceanim
    if (GetClass() != KI_ICEANIM_CLASS && GetClass() != 0)
        AssertOnClassCheck(GetClass(), KI_ICEANIM_CLASS, GetCollection());
    if (!mpAttributeData)
        mpAttributeData = DefaultDataArea(0x10u);
}

// ============================================================================
// Attrib::Gen::iceanim::GetAnimGuid
// ============================================================================
// The ICE take guid the shot names, read out of this instance's resolved layout block at
// +0xC -- the same block SuitableFor() (+0x00) and ShotProperties() (+0x04) read.
//
// Recovered from the consumer, BrnDirector::Camera::BehaviourIceAnim::SetParameters
// @0x8220F5C0, which is the only site that reaches it:
//     Attrib::Gen::iceanim::iceanim(v6, a2, 0);   // stack instance over the shot RefSpec
//     v4 = *(v7 + 12);                            // v7 == v6[+4] == mpAttributeData
//     *(a1 + 3620) = a2;                          // mpSourceShot  = lpParameters
//     *(a1 + 3616) = v4;                          // miAnimGuid    = the guid
//     Attrib::Instance::~Instance(v6);
// The base is mpAttributeData, NOT `this` -- see the FLAG on the declaration for the
// call-site consequence of that in the (unmounted) behaviour TU.
s32 iceanim::GetAnimGuid() const
{
    return reinterpret_cast<const s32*>(GetLayoutPointer())[3]; // layout +0x0C
}

}
}
