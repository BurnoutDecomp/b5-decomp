// Embed/ODR + template-instantiation gate for the vpu Vector3 / Vector4 / Matrix44 inline
// bodies (vector3_type_inline.h, vector4_type_inline.h, matrix44_operation_platform_inline.h).
// Self-contained from a foreign TU: includes only the public headers and forces every
// templated body to instantiate so the compile gate actually covers the bodies (a header
// alone is a vacuous `cl /c` pass).
//
// The fpu::Vector{3,4}Template-taking ctors are intentionally NOT instantiated here: their
// argument type is declared in a different (fpu) TU and has no definition in this cluster.
// Every register/lane/swizzle body IS exercised.

#include "SDKs/EATech/include/rw/math/vpu/vector3.h"
#include "SDKs/EATech/include/rw/math/vpu/vector4.h"
#include "SDKs/EATech/include/rw/math/vpu/matrix44.h"

using namespace rw::math::vpu;

float Vector3_EmbedCheckEntry(VectorIntrinsic& reg, const float xyz[3])
{
    VecFloat a(xyz[0]);
    VecFloatRef<VectorAxisX> rx(reg);
    VecFloatRef<VectorAxisY> ry(reg);
    VecFloatRef<VectorAxisZ> rz(reg);
    VecFloatRefIndex ri(reg, VectorAxisZ);

    Vector3 v0;
    Vector3 v1(1.0f, 2.0f, 3.0f);
    Vector3 v2(a, a, a);                       // VecFloat ctor
    Vector3 v3(ri, ri, ri);                    // VecFloatRefIndex ctor
    Vector3 v4(rx, ry, rz);                    // VecFloatRef<...> ctor (template)
    Vector3 v5(xyz);                           // float[3]
    Vector3 v6(reg);                           // VectorIntrinsicInParam
    Vector3 v7(v1);                            // copy

    v0 = v1;
    v0 = reg;
    v0.Set(4.0f, 5.0f, 6.0f);
    v0.Set(a, a, a);
    v0.Set(ri, ri, ri);
    v0.Set(rx, ry, rz);                        // template Set
    v0.SetComponent(0, 7.0f);
    v0.SetComponent(1, a);
    v0.SetComponent(2, ri);
    v0.SetComponent(0, rx);                    // template SetComponent
    v0.SetX(1.0f); v0.SetX(a); v0.SetX(ri); v0.SetX(rx);
    v0.SetY(1.0f); v0.SetY(a); v0.SetY(ri); v0.SetY(ry);
    v0.SetZ(1.0f); v0.SetZ(a); v0.SetZ(ri); v0.SetZ(rz);
    v0.SetZero();
    v0.SetVector(reg);

    VectorIntrinsic& gv = v0.GetVector();
    const VectorIntrinsic& cgv = v7.GetVector();
    (void)cgv;
    VectorIntrinsic& opv = v0;                 // operator VectorIntrinsic&
    (void)opv; (void)gv;

    float acc = 0.0f;
    acc += v0.X().GetFloat() + v0.Y().GetFloat() + v0.Z().GetFloat();
    acc += v0.GetX().GetFloat() + v0.GetY().GetFloat() + v0.GetZ().GetFloat();
    acc += v0.GetComponent(0).GetFloat() + v0[1].GetFloat();
    acc += GetFloat(v2.X()) + GetFloat(v3.Y()) + GetFloat(v4.Z()) + GetFloat(v5.X());
    acc += GetFloat(v6.X());
    return acc;
}

float Vector4_EmbedCheckEntry(VectorIntrinsic& reg, const float xyzw[4])
{
    VecFloat a(xyzw[0]);
    VecFloatRef<VectorAxisX> rx(reg);
    VecFloatRef<VectorAxisY> ry(reg);
    VecFloatRef<VectorAxisZ> rz(reg);
    VecFloatRef<VectorAxisW> rw(reg);
    VecFloatRefIndex ri(reg, VectorAxisW);
    Vector3 v3(1.0f, 2.0f, 3.0f);

    Vector4 w0;
    Vector4 w1(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4 w2(a, a, a, a);                    // VecFloat ctor
    Vector4 w3(ri, ri, ri, ri);                // VecFloatRefIndex ctor
    Vector4 w4(rx, ry, rz, rw);                // VecFloatRef<...> ctor (template)
    Vector4 w5(v3, rw);                        // (Vector3, VecFloatRefW)
    Vector4 w6(v3, ri);                        // (Vector3, VecFloatRefIndex)
    Vector4 w7(v3, a);                         // (Vector3, VecFloat)
    Vector4 w8(v3, 9.0f);                      // (Vector3, float)
    Vector4 w9(xyzw);                          // float[4]
    Vector4 wA(reg);                           // VectorIntrinsicInParam
    Vector4 wB(w1);                            // copy

    w0 = w1;
    w0 = reg;
    w0.Set(4.0f, 5.0f, 6.0f, 7.0f);
    w0.Set(a, a, a, a);
    w0.Set(ri, ri, ri, ri);
    w0.Set(rx, ry, rz, rw);                    // template Set
    w0.SetComponent(0, 7.0f);
    w0.SetComponent(1, a);
    w0.SetComponent(2, ri);
    w0.SetComponent(3, rw);                    // template SetComponent
    w0.SetX(1.0f); w0.SetX(a); w0.SetX(ri); w0.SetX(rx);
    w0.SetY(1.0f); w0.SetY(a); w0.SetY(ri); w0.SetY(ry);
    w0.SetZ(1.0f); w0.SetZ(a); w0.SetZ(ri); w0.SetZ(rz);
    w0.SetW(1.0f); w0.SetW(a); w0.SetW(ri); w0.SetW(rw);
    w0.SetZero();
    w0.SetVector(reg);

    const VectorIntrinsic& cgv = wB.GetVector();
    (void)cgv;
    VectorIntrinsic& opv = w0;                 // operator VectorIntrinsic&
    (void)opv;

    float acc = 0.0f;
    acc += w0.X().GetFloat() + w0.Y().GetFloat() + w0.Z().GetFloat() + w0.W().GetFloat();
    acc += w0.GetX().GetFloat() + w0.GetY().GetFloat() + w0.GetZ().GetFloat() + w0.GetW().GetFloat();
    acc += w0.GetComponent(0).GetFloat() + w0[1].GetFloat();
    acc += GetFloat(w2.X()) + GetFloat(w3.Y()) + GetFloat(w4.Z()) + GetFloat(w5.W());
    acc += GetFloat(w6.W()) + GetFloat(w7.W()) + GetFloat(w8.W()) + GetFloat(w9.X()) + GetFloat(wA.X());
    return acc;
}

float Matrix44_EmbedCheckEntry(Vector4& r0, Vector4& r1, Vector4& r2, Vector4& r3, Vector4& vv)
{
    Matrix44 m(r0, r1, r2, r3);
    Matrix44 b(r3, r2, r1, r0);
    Matrix44Affine ma(r0, r1, r2, r3);

    Vector4 mv = Mult(vv, m);                  // Mult(Vector4, Matrix44)   -- bodied
    Matrix44 mm = Mult(m, b);                  // Mult(Matrix44, Matrix44)  -- bodied
    Matrix44 mam = Mult(ma, b);                // Mult(Matrix44Affine, M44) -- bodied
    Matrix44 mt = Transpose(m);                // Transpose(Matrix44)       -- bodied

    return GetFloat(mv.X()) + GetFloat(mm.xAxis.Y()) + GetFloat(mam.wAxis.Z()) + GetFloat(mt.yAxis.X());
}
