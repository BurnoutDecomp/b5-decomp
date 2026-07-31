// Attrib::Gen::iceanim -- generated AttribSys class, out-of-line function bodies.
//
// See iceanim.h for the class shape. This file follows the convention songlist.cpp
// established in this directory: a per-generated-class .cpp for the bodies that are not
// inline in the header. GetAnimGuid is not itself an X360 out-of-line symbol (the console
// inlines the generated accessor); it is given a home here because it had none -- it was a
// declaration with committed callers and no definition anywhere in the tree.

#include "GameSource/AttribSys/Generated/classes/iceanim.h"

namespace Attrib
{
namespace Gen
{

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
