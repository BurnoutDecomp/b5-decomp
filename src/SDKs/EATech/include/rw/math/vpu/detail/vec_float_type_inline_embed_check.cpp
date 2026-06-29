// Tiny embed/ODR + template-instantiation check for the VecFloat lane wrappers
// (vec_float_type_inline.h). Self-contained from a foreign TU: includes only the public
// header and forces every templated body to instantiate so the compile gate covers them.

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"

using namespace rw::math::vpu;

float VecFloat_EmbedCheckEntry(VectorIntrinsic& reg, float s)
{
    VecFloat a(s);
    VecFloat b(reg, VectorAxisY);
    a = b;
    a = s;
    a.SetFloat(s);

    // VecFloatRef<INDEX> across all four lanes + every compound op flavour.
    VecFloatRef<VectorAxisX> rx(reg);
    VecFloatRef<VectorAxisY> ry(reg);
    VecFloatRef<VectorAxisZ> rz(reg);
    VecFloatRef<VectorAxisW> rw(reg);

    rx = s;
    rx = a;          // VecFloat::InParam
    rx = ry;         // VecFloatRefY -> VecFloatRef<X>
    rx += a;
    rx += ry;        // VecFloatRef<INDEX_OTHER>
    rx -= s;
    rx *= a;
    rz /= a;
    rw *= s;

    // VecFloatRefIndex (runtime lane).
    VecFloatRefIndex ri(reg, VectorAxisZ);
    ri = s;
    ri = a;
    ri = rx;         // VecFloatRef<INDEX>
    ri += a;
    ri += rx;
    ri -= s;
    ri *= ri;        // VecFloatRefIndex::InParam
    ri /= a;

    VecFloat c(rx);  // VecFloat(VecFloatRef<INDEX>)
    VecFloat d(ri);  // VecFloat(VecFloatRefIndex)

    return GetFloat(c) + GetFloat(d) + GetFloat(ri) + GetFloat(rx) + GetFloat(s)
         + float(a) + float(rx) + float(ri) + a.GetFloat() + rx.GetFloat() + ri.GetFloat();
}
