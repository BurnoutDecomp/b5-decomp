// CgsGraphics::Camera -- the graphics-side camera (view / projection / view-projection transforms
// plus cached projection scalars), reconstructed from the X360 ARTIST asm.
//
// Source-of-truth: X360 ASM is the spine; the vector/matrix members are the repo-canonical
// rw::math::vpu Vector4 / Matrix44 (BrnCommonTypes.h). See CgsCamera.h for the layout authority.

#include "GameShared/GameClasses/Graphics/CgsCamera.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (GetViewProjectionMatrixModified devs)
#include "rw/math/vpu/matrix44_operation.h"            // native affine view-projection multiply

#include <cmath>    // tanf/atanf (SetFovHorizontal -- the XMVectorTan/ATan lowering), sqrt (LookAt / frustum normalizes)
#include <cstring>  // memcpy (Clone)

// ----------------------------------------------------------------------------
// Camera defaults (DWARF ::-scope globals, CgsCamera.h:32-35).
//
// RECOVERED 2026-08-12 (shadow wave). The old note claimed the fov/aspect pair
// "lives in the un-dumped .data block (IDA exports carry no data)" and carried both
// as honest zeros. The EXPORTS carry no data; the DATABASE does -- dumped from
// BURNOUT_X360_ARTIST.XEX.i64 (this is the fifth "unrecoverable data" claim to fall
// this way, so treat any such note as untested):
//
//   flt_82F30FD4 = 3F C9 0F DB = 1.5707964f  (pi/2, i.e. 90 degrees horizontal)
//   flt_82F30FD8 = 3F E3 8E 39 = 1.7777778f  (16/9)
//
// Slot binding is PROVEN, not inferred, from the reset thunk @0x827F94E8, which
// tail-calls Camera::Construct with the PPC float-arg registers loaded explicitly:
//   lis r11,0x82F3 ; lfs f1,0xFD4(r11)   -> arg1 = fovHorizontal
//                    lfs f2,0xFD8(r11)   -> arg2 = aspectRatio
//   lis r11,0x820D ; addi r11,r11,0x1704 ; lfs f3,0(r11) ; lfs f4,4(r11)
//                                        -> arg3/arg4 = near 0.1 / far 1000.0
// (0x82F30FD0 holds a second 16/9 -- a display-aspect block, not a stray. The
// near/far are rodata lfs loads, not the "immediates" the old comment called them;
// their values happened to be right.)
//
// THE ZERO ASPECT WAS LOAD-BEARING, and it cost this campaign a full wave.
// BrnDirector::Camera::Camera::CopyToCgsCamera (Camera.cpp:509) opens with
// Construct() -> Construct(KF_DEFAULT_FOVHORIZONTAL, KF_DEFAULT_ASPECTRATIO, ...)
// -- it said Release() until 2026-08-17, see the Construct()/Release() banner below --
// and nothing else ever writes m_aspectRatio on that camera. SetFovHorizontal then
// computes 2*atanf((1.0f/0.0f) * tanHalfH) = 2*atanf(inf) = pi EXACTLY, so
// tanHalfV = tanf(float(pi)*0.5f) = -2.2877e7 and every frame camera became a
// 180-degree-tall fan. The shadow cascade fit was then perfectly correct code
// fitting a minimum-area box around a genuinely astronomical point cloud --
// fitHalfW came out 1e8..1e9 metres, every caster triangle was sub-pixel, and
// ~2200 shadow draws per frame rasterised ZERO fragments. Nothing was ever
// non-finite, which is precisely why it survived so long.
// Diagnostic signature, if this ever regresses: fitHalfW / slabFar is a CONSTANT
// across all cascades, and that constant is tanf(float(pi/2)) = 2.2877e7.
// ----------------------------------------------------------------------------
const f32 KF_DEFAULT_FOVHORIZONTAL = 1.5707964f;  // flt_82F30FD4 (.i64-recovered, pi/2)
const f32 KF_DEFAULT_ASPECTRATIO   = 1.7777778f;  // flt_82F30FD8 (.i64-recovered, 16/9)
const f32 KF_DEFAULT_NEARCLIPPLANE = 0.1f;        // flt_820D1704 (.i64-recovered)
const f32 KF_DEFAULT_FARCLIPPLANE  = 1000.0f;     // flt_820D1708 (.i64-recovered)

namespace CgsGraphics
{
    // CgsGraphics::Camera::Camera @0x823C51D0
    //
    // The X360 body block-copies the whole object: three Matrix44 via twelve lvx128/stvx128
    // 16-byte moves (+0x00..0xBF), sixteen f64 via lfd/stfd (+0xC0..0x13F), then nine f32 via
    // lfs/stfs (+0x140..0x160). De-optimised to field-for-field copies.
    Camera::Camera(const Camera& rhs)
        : mView(rhs.mView)
        , mProjection(rhs.mProjection)
        , mViewProjection(rhs.mViewProjection)
    {
        for (int i = 0; i < 16; ++i)
        {
            maClipState[i] = rhs.maClipState[i];
        }
        for (int i = 0; i < 9; ++i)
        {
            maProjectionScalars[i] = rhs.maProjectionScalars[i];
        }
    }

    // CgsGraphics::Camera::operator= @0x82218ED0
    //
    // Identical copy extent to the copy ctor: three transforms, the clip-state doubles, the
    // nine projection scalars.
    Camera& Camera::operator=(const Camera& rhs)
    {
        mView           = rhs.mView;
        mProjection     = rhs.mProjection;
        mViewProjection = rhs.mViewProjection;
        for (int i = 0; i < 16; ++i)
        {
            maClipState[i] = rhs.maClipState[i];
        }
        for (int i = 0; i < 9; ++i)
        {
            maProjectionScalars[i] = rhs.maProjectionScalars[i];
        }
        return *this;
    }

    // ========================================================================
    // The frustum-writer family (camera-frustum wave 2026-07-27).
    //
    // Every body below was decoded from the raw X360 instruction stream and
    // VERIFIED numerically: a per-instruction VMX emulator (session scratchpad
    // ppcnum.py) executed the dumps @0x827F11A8 / @0x827F0AD8 / @0x82277298 /
    // @0x827F9778 / @0x827F97B8 / @0x827EC858 / @0x827E72E0 over randomised
    // cameras (orthonormal AND general-invertible view matrices, both bool
    // values), and the C++ formulations below (mirrored in mirror.py) match
    // every output lane to within float32 rounding of the emulated stream.
    //
    // Shared head (inlined in both writers): the camera's WORLD basis + eye
    // are recovered from the mView 3x3 by the adjugate/determinant inverse --
    //   cross-pair build:      vpermwi128 0x63 == the (y,z,x,w) word rotate,
    //                          crossAB = perm(a*perm(b) - perm(a)*b)
    //   det  = dot3(row0, cross(row1,row2))          (vmsum3fp128)
    //   1/det = vrefp + 2x Newton-Raphson            (de-optimised exact)
    //   basis rows (right/up/dir) = the transposed crosses * 1/det
    //     (the vmrghw/vmrglw interleave block is exactly that transpose)
    //   eye  = -(w-row * inverse)                    (vmaddfp chain + vxor)
    // For the orthonormal cameras the game builds (LookAt / CopyToCgsCamera)
    // this equals the transpose, but the CODE computes the true inverse and is
    // reproduced as such.
    // ========================================================================

    namespace
    {
        inline Vector3 Cross3(const Vector3& lrA, const Vector3& lrB)
        {
            return rw::math::vpu::Cross(lrA, lrB);
        }

        inline f32 Dot3(const Vector3& lrA, const Vector3& lrB)
        {
            return static_cast<f32>(rw::math::vpu::Dot(lrA, lrB));
        }

        inline Vector3 Sub3(const Vector3& lrA, const Vector3& lrB)
        {
            return rw::math::vpu::Subtract(lrA, lrB);
        }

        inline Vector3 Normalize3(const Vector3& lrA)
        {
            return rw::math::vpu::Normalize(lrA);
        }

        inline Vector3 RowXYZ(const Vector4& lrRow)
        {
            Vector3 lvOut(0.0f, 0.0f, 0.0f);
            lvOut.Set(lrRow.GetX(), lrRow.GetY(), lrRow.GetZ());
            return lvOut;
        }

        // The shared inlined head of both frustum writers (see the banner):
        // world right/up/dir basis + eye from the view matrix inverse.
        struct CameraWorldBasis { Vector3 mRight, mUp, mDir, mEye; };

        CameraWorldBasis ComputeWorldBasisAndEye(const Matrix44& lrView)
        {
            const Vector3 lvRow0 = RowXYZ(lrView.xAxis);
            const Vector3 lvRow1 = RowXYZ(lrView.yAxis);
            const Vector3 lvRow2 = RowXYZ(lrView.zAxis);
            const Vector3 lvRowW = RowXYZ(lrView.wAxis);

            const Vector3 lvC12 = Cross3(lvRow1, lvRow2);
            const Vector3 lvC20 = Cross3(lvRow2, lvRow0);
            const Vector3 lvC01 = Cross3(lvRow0, lvRow1);
            const Vector3 lvOne(1.0f, 1.0f, 1.0f);
            const Vector3 lvInvDet = rw::math::vpu::Divide(
                lvOne, rw::math::vpu::Dot(lvRow0, lvC12));  // vrefp + two fused refinements
            const Vector3 lvC12Scaled = rw::math::vpu::Mult(lvC12, lvInvDet);
            const Vector3 lvC20Scaled = rw::math::vpu::Mult(lvC20, lvInvDet);
            const Vector3 lvC01Scaled = rw::math::vpu::Mult(lvC01, lvInvDet);

            CameraWorldBasis lBasis;
            // inverse rows == transposed crosses * 1/det (the vmrghw/vmrglw block)
            lBasis.mRight.SetX(lvC12Scaled.GetX());
            lBasis.mRight.SetY(lvC20Scaled.GetX());
            lBasis.mRight.SetZ(lvC01Scaled.GetX());
            lBasis.mUp.SetX(lvC12Scaled.GetY());
            lBasis.mUp.SetY(lvC20Scaled.GetY());
            lBasis.mUp.SetZ(lvC01Scaled.GetY());
            lBasis.mDir.SetX(lvC12Scaled.GetZ());
            lBasis.mDir.SetY(lvC20Scaled.GetZ());
            lBasis.mDir.SetZ(lvC01Scaled.GetZ());

            // eye = -(w-row through the inverse) -- the vmaddfp accumulate + vxor sign flip
            const rw::math::vpu::VectorIntrinsic lvNegW = rw::math::vpu::Negate(lvRowW).GetVector();
            rw::math::vpu::VectorIntrinsic lvEye = __vmulfp(
                lBasis.mRight.GetVector(), __vspltw(lvNegW, 0));
            lvEye = __vmaddfp(lBasis.mUp.GetVector(), __vspltw(lvNegW, 1), lvEye);
            lvEye = __vmaddfp(lBasis.mDir.GetVector(), __vspltw(lvNegW, 2), lvEye);
            lBasis.mEye.SetVector(lvEye);
            return lBasis;
        }

        inline void StorePlane(Vector4& lrOut, const Vector3& lrN, f32 lfD)
        {
            lrOut.SetX(lrN.GetX());
            lrOut.SetY(lrN.GetY());
            lrOut.SetZ(lrN.GetZ());
            lrOut.SetW(lfD);
        }
    }

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::GetFrustumParallel @0x827F11A8 (DWARF CgsCamera.h:170)
    //
    // World-space planes of the orthogonal view volume. The X360:
    //   1. inverse-basis head (see the banner above);
    //   2. half extents sx = 1/mProjection[0][0], sy = 1/mProjection[1][1]
    //      (fdivs of flt_82001C98 == 1.0), near/far from scalars [7]/[8];
    //   3. near-face corners walked (+r+u) -> (-r+u) -> (-r-u) -> (+r-u)
    //      (corner0 = eye + sx*right + sy*up + near*dir, then the two
    //      2*sx*right / 2*sy*up edge subtractions), far corners = the
    //      4-iteration loop adding (far-near)*dir (@0x827F1434..0x827F146C);
    //   4. planes (write order == CameraRwFrustum order):
    //        near  [ dir, dot3(dir, c0n)]     (dir NOT normalised)
    //        far   [-dir, -dot3(dir, c0f)]    (vxor sign flips)
    //        left  n=norm(cross(c1n-c1f, c2f-c1f)), d=dot3(n, c1n)
    //        right n=norm(cross(c3n-c3f, c0f-c3f)), d=dot3(n, c3n)
    //        top   n=norm(cross(c0n-c0f, c1f-c0f)), d=dot3(n, c0n)
    //        bottom n=norm(cross(c2n-c2f, c3f-c2f)), d=dot3(n, c2n)
    // ------------------------------------------------------------------------
    void Camera::GetFrustumParallel(CameraRwFrustum& lrOut) const
    {
        const CameraWorldBasis lBasis = ComputeWorldBasisAndEye(mView);
        const Vector3& lrRight = lBasis.mRight;
        const Vector3& lrUp    = lBasis.mUp;
        const Vector3& lrDir   = lBasis.mDir;

        const f32 lfSx   = 1.0f / static_cast<f32>(mProjection.xAxis.GetX());   // fdivs 1.0 / [0][0]
        const f32 lfSy   = 1.0f / static_cast<f32>(mProjection.yAxis.GetY());   // fdivs 1.0 / [1][1]
        const f32 lfNear = maProjectionScalars[7];       // lfs 0x15C
        const f32 lfFar  = maProjectionScalars[8];       // lfs 0x160
        const f32 lfSpan = lfFar - lfNear;

        // near corners: (+r+u), (-r+u), (-r-u), (+r-u); far = +span*dir.
        Vector3 laNear[4], laFar[4];
        const Vector3 lvSx(lfSx, lfSx, lfSx);
        const Vector3 lvSy(lfSy, lfSy, lfSy);
        const Vector3 lvNear(lfNear, lfNear, lfNear);
        const Vector3 lvSpan(lfSpan, lfSpan, lfSpan);
        const f32 lfTwoSx = 2.0f * lfSx;
        const f32 lfTwoSy = 2.0f * lfSy;
        laNear[0] = rw::math::vpu::MultAdd(
            lrDir, lvNear,
            rw::math::vpu::MultAdd(lrUp, lvSy,
                rw::math::vpu::MultAdd(lrRight, lvSx, lBasis.mEye)));
        laNear[1] = rw::math::vpu::Subtract(
            laNear[0], rw::math::vpu::Mult(lrRight, lfTwoSx));
        laNear[2] = rw::math::vpu::Subtract(
            laNear[1], rw::math::vpu::Mult(lrUp, lfTwoSy));
        laNear[3] = rw::math::vpu::Add(
            laNear[2], rw::math::vpu::Mult(lrRight, lfTwoSx));
        for (int liCorner = 0; liCorner < 4; ++liCorner)   // the 4x far loop
        {
            laFar[liCorner] = rw::math::vpu::MultAdd(lrDir, lvSpan, laNear[liCorner]);
        }

        StorePlane(lrOut.maPlanes[0], lrDir, Dot3(lrDir, laNear[0]));                       // near
        const Vector3 lvNegDir = rw::math::vpu::Negate(lrDir);                              // vxor
        StorePlane(lrOut.maPlanes[1], lvNegDir, -Dot3(lrDir, laFar[0]));                    // far
        const Vector3 lvLeft   = Normalize3(Cross3(Sub3(laNear[1], laFar[1]), Sub3(laFar[2], laFar[1])));
        StorePlane(lrOut.maPlanes[2], lvLeft, Dot3(lvLeft, laNear[1]));                     // left
        const Vector3 lvRightN = Normalize3(Cross3(Sub3(laNear[3], laFar[3]), Sub3(laFar[0], laFar[3])));
        StorePlane(lrOut.maPlanes[3], lvRightN, Dot3(lvRightN, laNear[3]));                 // right
        const Vector3 lvTop    = Normalize3(Cross3(Sub3(laNear[0], laFar[0]), Sub3(laFar[1], laFar[0])));
        StorePlane(lrOut.maPlanes[4], lvTop, Dot3(lvTop, laNear[0]));                       // top
        const Vector3 lvBottom = Normalize3(Cross3(Sub3(laNear[2], laFar[2]), Sub3(laFar[3], laFar[2])));
        StorePlane(lrOut.maPlanes[5], lvBottom, Dot3(lvBottom, laNear[2]));                 // bottom
    }

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::GetFrustumPerspective @0x827F0AD8 (DWARF CgsCamera.h:166)
    //
    // Same construction as the parallel writer with the corner spreads coming
    // from the cached tan-half-fov scalars ([2]/[5], lfs 0x148/0x154): the four
    // corner RAYS are dir +/- tanH*right +/- tanV*up (sign order (+,+), (-,+),
    // (-,-), (+,-)), corners = eye + near*ray / eye + far*ray (the 4x loop
    // @0x827F0D00..0x827F0D4C). When lbNegateNearFar is set the X360 fnegs both
    // clip distances on entry (@0x827EC0C..) and fully negates ALL SIX result
    // planes on exit (the 6-iteration vxor/vrlimi loop @0x827F1104..0x827F1194).
    // ------------------------------------------------------------------------
    void Camera::GetFrustumPerspective(CameraRwFrustum& lrOut, bool lbNegateNearFar) const
    {
        const CameraWorldBasis lBasis = ComputeWorldBasisAndEye(mView);
        const Vector3& lrRight = lBasis.mRight;
        const Vector3& lrUp    = lBasis.mUp;
        const Vector3& lrDir   = lBasis.mDir;

        const f32 lfTanH = maProjectionScalars[2];   // lfs 0x148 (m_tanHalfFovHorizontal)
        const f32 lfTanV = maProjectionScalars[5];   // lfs 0x154 (m_tanHalfFovVertical)
        f32 lfNear = maProjectionScalars[7];
        f32 lfFar  = maProjectionScalars[8];
        if (lbNegateNearFar)                         // the two fnegs @0x827F0C0C
        {
            lfNear = -lfNear;
            lfFar  = -lfFar;
        }

        static const f32 KAF_SIGNS[4][2] = { { 1.0f, 1.0f }, { -1.0f, 1.0f }, { -1.0f, -1.0f }, { 1.0f, -1.0f } };
        Vector3 laNear[4], laFar[4];
        for (int liCorner = 0; liCorner < 4; ++liCorner)
        {
            const f32 lfS = KAF_SIGNS[liCorner][0] * lfTanH;
            const f32 lfT = KAF_SIGNS[liCorner][1] * lfTanV;
            const Vector3 lvRay = rw::math::vpu::MultAdd(
                lrUp, Vector3(lfT, lfT, lfT),
                rw::math::vpu::MultAdd(lrRight, Vector3(lfS, lfS, lfS), lrDir));
            laNear[liCorner] = rw::math::vpu::MultAdd(
                lvRay, Vector3(lfNear, lfNear, lfNear), lBasis.mEye);
            laFar[liCorner] = rw::math::vpu::MultAdd(
                lvRay, Vector3(lfFar, lfFar, lfFar), lBasis.mEye);
        }

        StorePlane(lrOut.maPlanes[0], lrDir, Dot3(lrDir, laNear[0]));                       // near
        const Vector3 lvNegDir = rw::math::vpu::Negate(lrDir);
        StorePlane(lrOut.maPlanes[1], lvNegDir, -Dot3(lrDir, laFar[0]));                    // far
        const Vector3 lvLeft   = Normalize3(Cross3(Sub3(laNear[1], laFar[1]), Sub3(laFar[2], laFar[1])));
        StorePlane(lrOut.maPlanes[2], lvLeft, Dot3(lvLeft, laNear[1]));                     // left
        const Vector3 lvRightN = Normalize3(Cross3(Sub3(laNear[3], laFar[3]), Sub3(laFar[0], laFar[3])));
        StorePlane(lrOut.maPlanes[3], lvRightN, Dot3(lvRightN, laNear[3]));                 // right
        const Vector3 lvTop    = Normalize3(Cross3(Sub3(laNear[0], laFar[0]), Sub3(laFar[1], laFar[0])));
        StorePlane(lrOut.maPlanes[4], lvTop, Dot3(lvTop, laNear[0]));                       // top
        const Vector3 lvBottom = Normalize3(Cross3(Sub3(laNear[2], laFar[2]), Sub3(laFar[3], laFar[2])));
        StorePlane(lrOut.maPlanes[5], lvBottom, Dot3(lvBottom, laNear[2]));                 // bottom

        if (lbNegateNearFar)   // the 6-plane negate loop @0x827F1104
        {
            for (int liPlane = 0; liPlane < 6; ++liPlane)
            {
                Vector4& lrPlane = lrOut.maPlanes[liPlane];
                lrPlane = rw::math::vpu::Negate(lrPlane);
            }
        }
    }

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::GetFrustum @0x82277298 (DWARF CgsCamera.h:158)
    //
    // Dispatch on the projection type: the X360 selects word lane THREE of
    // mProjection.wAxis (lvsl 0xC + vperm) and vcmpeqfp.-compares it against
    // 1.0 (flt_82001C98) -- the affine 1.0 the orthogonal projection stores at
    // [3][3]; equal -> parallel, else perspective with the negate flag FALSE
    // (li r5, 0 @0x82277314).
    // ------------------------------------------------------------------------
    void Camera::GetFrustum(CameraRwFrustum& lrOut) const
    {
        if (static_cast<f32>(mProjection.wAxis.GetW()) == 1.0f)
        {
            GetFrustumParallel(lrOut);
        }
        else
        {
            GetFrustumPerspective(lrOut, false);
        }
    }

    // ========================================================================
    // CgsGraphics::Camera::GetFrustumPerspectiveVertices @0x827E70E0 (DWARF CgsCamera.h:174, .cpp:456)
    // ADDITIVE (L3 RACEINTRO, 2026-09-27). A leaf, 0x827E70E0..0x827E72DC, read with tools/re/vmx128.py. Its one
    // caller is TrafficEntityModule::RenderTrafficLightCoronas @0x8271EC80, which bounds these eight corners to walk
    // the Pvs cells whose traffic lights it draws.
    //
    // frustumVerts[0..3] are the near-plane corners and [4..7] the far-plane ones, each set walked (+x +y), (-x +y),
    // (-x -y), (+x -y) in the camera's own axes. The DWARF locals name the steps: `Matrix44Affine transform` (the
    // inverse of mView, inlined as the adjugate), lrNear / lrFar (scalars [7] / [8], lfs 0x15C / 0x160),
    // lrTanHalfFovX / lrTanHalfFovY (scalars [2] / [5], lfs 0x148 / 0x154), lRightVec, lUpVec, tmpVec3, index.
    //   the three crosses, each as the console forms it: c = a * b.yzxw (vmulfp128), c = -(a.yzxw * b - c)
    //     (vnmsubfp, ONE rounding), then c.yzxw (vpermwi128 0x63) -- Z x X at 0x827E7124 / 0x827E7178 / 0x827E7198,
    //     Y x Z at 0x827E7130 / 0x827E7184 / 0x827E719C, X x Y at 0x827E7138 / 0x827E718C / 0x827E71A0
    //   det = X . (Y x Z), vmsum3fp128 at 0x827E71AC                                          ROUNDING_RULE 1
    //   1 / det: the vrefp estimate (0x827E71C0), refined twice by e = -(r * det - 1) (vnmsubfp), r = r * e + r
    //     (vmaddfp): 0x827E71C4 / 0x827E71C8, then 0x827E71D0 / 0x827E71F4                       rules 5 and 3
    //   the transform rows are the crosses transposed (vmrghw / vmrglw, 0x827E71A4..0x827E71E0), each * 1/det
    //     (vmulfp128 0x827E71FC / 0x827E7200 / 0x827E7204). The merge puts (Z x X).x / .y / .z in their w lanes.
    //   transform.Pos = -W.x * row0 (vmulfp128 0x827E720C), then + -W.y * row1 and + -W.z * row2 (vmaddfp
    //     0x827E7214 / 0x827E7218). -W is the sign-bit vxor at 0x827E7148.
    //   lRightVec = row0 * lrTanHalfFovX (0x827E7208) ; lUpVec = row1 * lrTanHalfFovY (0x827E7210)
    //   the corner directions: d0 = (lRightVec + lUpVec) + row2 (vaddfp 0x827E721C / 0x827E724C);
    //     d1 = d0 - lRightVec * 2 (vmulfp128 0x827E7270 by flt_82001D9C == 2.0f, vsubfp 0x827E7274);
    //     d2 = d1 - lUpVec * 2 (0x827E7258 / 0x827E7278) ; d3 = d2 + lRightVec * 2 (vaddfp 0x827E7280)
    //   for index 0..3: frustumVerts[index] = d * lrNear + Pos and frustumVerts[index + 4] = d * lrFar + Pos, each
    //     a vmaddfp (ONE rounding): 0x827E72C0 / 0x827E72D0
    // Every lane is computed, the w lanes too, as the console stores all four.
    // The reconstruction below uses the native VPU reciprocal and four-lane operations, including the same two
    // fused reciprocal refinements and all four result lanes as the X360 path.
    // ========================================================================
    namespace
    {
        inline Vector4 FrustumVertexCross(const Vector4& lrA, const Vector4& lrB)
        {
            const rw::math::vpu::VectorIntrinsic lvAyzx = __vpermwi(lrA.GetVector(), 0x63);
            const rw::math::vpu::VectorIntrinsic lvByzx = __vpermwi(lrB.GetVector(), 0x63);
            const rw::math::vpu::VectorIntrinsic lvProduct = __vmulfp(lrA.GetVector(), lvByzx);
            return Vector4(__vpermwi(__vnmsubfp(lvAyzx, lrB.GetVector(), lvProduct), 0x63));
        }
    }

    void Camera::GetFrustumPerspectiveVertices(Vector3* frustumVerts) const
    {
        const f32 KF_TWO = 2.0f;   // flt_82001D9C (0x40000000)
        const VecFloat lvTwo(KF_TWO);

        const Vector4& lX = mView.xAxis;   // lvx128 +0x00
        const Vector4& lY = mView.yAxis;   // lvx128 +0x10
        const Vector4& lZ = mView.zAxis;   // lvx128 +0x20
        const Vector4& lW = mView.wAxis;   // lvx128 +0x30

        const Vector4 lZCrossX = FrustumVertexCross(lZ, lX);
        const Vector4 lYCrossZ = FrustumVertexCross(lY, lZ);
        const Vector4 lXCrossY = FrustumVertexCross(lX, lY);

        const Vector3 lvDeterminant(lX.GetVector());
        const Vector3 lvCrossForDot(lYCrossZ.GetVector());
        const Vector3 lvInverseDeterminant = rw::math::vpu::Divide(
            Vector3(1.0f, 1.0f, 1.0f), rw::math::vpu::Dot(lvDeterminant, lvCrossForDot));
        const rw::math::vpu::VectorIntrinsic lvReciprocal =
            __vspltw(lvInverseDeterminant.GetVector(), 0);

        // transform: rows (Y x Z, Z x X, X x Y, Z x X) transposed, each * 1/det; Pos = -W through them.
        const Vector4 lRow0(lYCrossZ.GetX(), lZCrossX.GetX(), lXCrossY.GetX(), lZCrossX.GetX());
        const Vector4 lRow1(lYCrossZ.GetY(), lZCrossX.GetY(), lXCrossY.GetY(), lZCrossX.GetY());
        const Vector4 lRow2(lYCrossZ.GetZ(), lZCrossX.GetZ(), lXCrossY.GetZ(), lZCrossX.GetZ());
        const Vector4 lTransform0(__vmulfp(lRow0.GetVector(), lvReciprocal));
        const Vector4 lTransform1(__vmulfp(lRow1.GetVector(), lvReciprocal));
        const Vector4 lTransform2(__vmulfp(lRow2.GetVector(), lvReciprocal));

        const Vector4 lNegW = rw::math::vpu::Negate(lW);
        rw::math::vpu::VectorIntrinsic lvPos = __vmulfp(
            lTransform0.GetVector(), __vspltw(lNegW.GetVector(), 0));
        lvPos = __vmaddfp(lTransform1.GetVector(), __vspltw(lNegW.GetVector(), 1), lvPos);
        lvPos = __vmaddfp(lTransform2.GetVector(), __vspltw(lNegW.GetVector(), 2), lvPos);
        const Vector4 lPos(lvPos);

        const f32 lrNear        = maProjectionScalars[7];   // m_nearClipPlane
        const f32 lrFar         = maProjectionScalars[8];   // m_farClipPlane
        const f32 lrTanHalfFovX = maProjectionScalars[2];   // m_tanHalfFovHorizontal
        const f32 lrTanHalfFovY = maProjectionScalars[5];   // m_tanHalfFovVertical

        const Vector4 lRightVec(__vmulfp(lTransform0.GetVector(), VecFloat(lrTanHalfFovX).GetVector()));
        const Vector4 lUpVec(__vmulfp(lTransform1.GetVector(), VecFloat(lrTanHalfFovY).GetVector()));
        const Vector4 lRight2(__vmulfp(lRightVec.GetVector(), lvTwo.GetVector()));
        const Vector4 lUp2(__vmulfp(lUpVec.GetVector(), lvTwo.GetVector()));
        Vector4 laDirections[4];
        laDirections[0] = Vector4(__vaddfp(__vaddfp(lRightVec.GetVector(), lUpVec.GetVector()),
                                            lTransform2.GetVector()));
        laDirections[1] = Vector4(__vsubfp(laDirections[0].GetVector(), lRight2.GetVector()));
        laDirections[2] = Vector4(__vsubfp(laDirections[1].GetVector(), lUp2.GetVector()));
        laDirections[3] = Vector4(__vaddfp(laDirections[2].GetVector(), lRight2.GetVector()));

        const rw::math::vpu::VectorIntrinsic lvNear = VecFloat(lrNear).GetVector();
        const rw::math::vpu::VectorIntrinsic lvFar = VecFloat(lrFar).GetVector();

        for (u32 index = 0; index < 4; ++index)
        {
            Vector3& lrNearVertex = frustumVerts[index];
            Vector3& lrFarVertex  = frustumVerts[index + 4];
            lrNearVertex.SetVector(__vmaddfp(laDirections[index].GetVector(), lvNear, lPos.GetVector()));
            lrFarVertex.SetVector(__vmaddfp(laDirections[index].GetVector(), lvFar, lPos.GetVector()));
        }
    }

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::GetCgsFrustum @0x827F9778 / GetCgsFrustumParallel
    // @0x827F97B8 -- thin wrappers: write the RW frustum into a stack snapshot,
    // convert with CgsGeometric::Frustum::SetFromRwFrustum.
    // ------------------------------------------------------------------------
    void Camera::GetCgsFrustum(CgsGeometric::Frustum& lrOut) const
    {
        CameraRwFrustum lRwFrustum;
        GetFrustum(lRwFrustum);
        lrOut.SetFromRwFrustum(lRwFrustum);
    }

    void Camera::GetCgsFrustumParallel(CgsGeometric::Frustum& lrOut) const
    {
        CameraRwFrustum lRwFrustum;
        GetFrustumParallel(lRwFrustum);
        lrOut.SetFromRwFrustum(lRwFrustum);
    }

    // ------------------------------------------------------------------------
    // PC-ADDITIVE no-arg bridges (see the header FLAG): run the REAL writer
    // into a function-local static (POD, zero-init) so the committed
    // BrnWorldModule call shape keeps X360 semantics.
    // ------------------------------------------------------------------------
    const CgsGeometric::Frustum& Camera::GetFrustumParallel() const
    {
        static CgsGeometric::Frustum sFrustum;
        CameraRwFrustum lRwFrustum;
        GetFrustumParallel(lRwFrustum);
        sFrustum.SetFromRwFrustum(lRwFrustum);
        return sFrustum;
    }

    const CgsGeometric::Frustum& Camera::GetFrustumPerspective() const
    {
        static CgsGeometric::Frustum sFrustum;
        CameraRwFrustum lRwFrustum;
        GetFrustumPerspective(lRwFrustum, false);
        sFrustum.SetFromRwFrustum(lRwFrustum);
        return sFrustum;
    }

    const CgsGeometric::Frustum& Camera::GetFrustum()
    {
        // the @0x82277298 dispatch through the bridge pair
        if (static_cast<f32>(mProjection.wAxis.GetW()) == 1.0f)
        {
            return GetFrustumParallel();
        }
        return GetFrustumPerspective();
    }

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::SetFovHorizontal @0x821F13B0 (destub wave 2026-07-26)
    //
    // Recompute the cached projection scalars from a horizontal fov (radians):
    //   m_fovHorizontal              = fov
    //   m_tanHalfFovHorizontal       = tan(fov * 0.5)            (XMVectorTan)
    //   m_oneOverTanHalfFovHorizontal= 1 / tanHalfH              (vrefp + NR)
    //   m_fovVertical                = 2 * atan(tanHalfH / m_aspectRatio)  (XMVectorATan)
    //   m_tanHalfFovVertical         = tan(m_fovVertical * 0.5)  (XMVectorTan)
    //   m_oneOverTanHalfFovVertical  = 1 / tanHalfV
    // then rebuild the projection (tail-call UpdatePerspectiveProjectionMatrix).
    // Every store is pinned by the DWARF scalar names + the a1[20]/a1[21] lane
    // indices in the asm. FLAG (transcendental lowering): the X360 computes
    // tan/atan with the XMVectorTan/XMVectorATan minimax polynomials whose
    // coefficient rodata is not dumped; lowered to tanf/atanf per the committed
    // ICEMath.cpp precedent ("poly table un-recovered; libm is the equivalent").
    // ------------------------------------------------------------------------
    void Camera::SetFovHorizontal(f32 fovHorizontal)
    {
        maProjectionScalars[0] = fovHorizontal;                       // m_fovHorizontal

        const f32 lfTanHalfH = tanf(fovHorizontal * 0.5f);            // flt_82001DA0 == 0.5
        maProjectionScalars[2] = lfTanHalfH;                          // m_tanHalfFovHorizontal
        maProjectionScalars[1] = 1.0f / lfTanHalfH;                   // m_oneOverTanHalfFovHorizontal

        // fovV = 2 * atan(tanHalfH / aspect) -- the asm forms (1/aspect) * tanHalfH
        // (vrefp reciprocal), atan, then the 2.0 (flt_82001D9C) broadcast multiply.
        const f32 lfFovVertical = 2.0f * atanf((1.0f / maProjectionScalars[6]) * lfTanHalfH);
        maProjectionScalars[3] = lfFovVertical;                       // m_fovVertical

        const f32 lfTanHalfV = tanf(lfFovVertical * 0.5f);
        maProjectionScalars[5] = lfTanHalfV;                          // m_tanHalfFovVertical
        maProjectionScalars[4] = 1.0f / lfTanHalfV;                   // m_oneOverTanHalfFovVertical

        UpdatePerspectiveProjectionMatrix();
    }

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::Construct(f32,f32,f32,f32) @0x827F0A08
    //
    // The parameterised constructor (DWARF CgsCamera.h:66): store aspect/near/far
    // into the scalar block (a1[21].f32[2..3] / a1[22].f32[0] == scalars [6]/[7]/[8]),
    // reset mView to the identity basis with a ZERO w row (the four stvx128 of
    // {1,0,0,0}/{0,1,0,0}/{0,0,1,0}/{0,0,0,0}), then tail-call SetFovHorizontal
    // (which rebuilds mProjection + mViewProjection).
    // ------------------------------------------------------------------------
    void Camera::Construct(f32 lfFovHorizontal, f32 lfAspectRatio, f32 lfNearClipPlane, f32 lfFarClipPlane)
    {
        maProjectionScalars[6] = lfAspectRatio;    // m_aspectRatio
        maProjectionScalars[7] = lfNearClipPlane;  // m_nearClipPlane
        maProjectionScalars[8] = lfFarClipPlane;   // m_farClipPlane

        {
            const Vector4 lAggregateValue ( 1.0f, 0.0f, 0.0f, 0.0f );
            mView.xAxis = lAggregateValue;
        }
        {
            const Vector4 lAggregateValue ( 0.0f, 1.0f, 0.0f, 0.0f );
            mView.yAxis = lAggregateValue;
        }
        {
            const Vector4 lAggregateValue ( 0.0f, 0.0f, 1.0f, 0.0f );
            mView.zAxis = lAggregateValue;
        }
        {
            const Vector4 lAggregateValue ( 0.0f, 0.0f, 0.0f, 0.0f );
            mView.wAxis = lAggregateValue;
        }

        SetFovHorizontal(lfFovHorizontal);
    }

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::Construct() @0x827F94E8 (DWARF CgsCamera.h:59)
    //
    // The no-arg reset: forward the camera defaults into the 4-arg Construct.
    // The X360 body is a pure tail-forward with the PPC float-arg registers loaded
    // explicitly (lfs f1 = flt_82F30FD4, f2 = flt_82F30FD8, f3/f4 = flt_820D1704/08
    // -> `b CgsGraphics__Camera__Construct`).
    // ------------------------------------------------------------------------
    void Camera::Construct()
    {
        Construct(KF_DEFAULT_FOVHORIZONTAL, KF_DEFAULT_ASPECTRATIO,
                  KF_DEFAULT_NEARCLIPPLANE, KF_DEFAULT_FARCLIPPLANE);
    }

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::Release() @0x8284CB38 (DWARF CgsCamera.h:78) -- EMPTY.
    //
    // ⚠ CORRECTED 2026-08-17 (reflections step 2, envproducer finding F1). This body
    // used to be a second copy of Construct()'s defaults reset, on the claim that the
    // no-arg Construct()/Release() pair ICF-folded to the one body @0x827F94E8. The
    // image says otherwise, and BrnGraphics::EnvironmentMap settles it because it
    // calls BOTH over the SAME six CgsGraphics::Camera objects at the same 0x170
    // stride:
    //     EnvironmentMap::Construct @0x827B40D0  loop body: `bl sub_827F94E8`
    //         (0x827F94E8 = the KF_DEFAULT_* forward above)
    //     EnvironmentMap::Release   @0x827B4218  loop body: `bl BaseCollision-
    //         Generator__Destruct` == 0x8284CB38
    //     EnvironmentMap::Prepare   @0x827B4188  first call of each iteration: the
    //         same 0x8284CB38
    // and 0x8284CB38.json is, in full:
    //     0x8284CB38  blr
    //     0x8284CB3C  .long 0
    // i.e. the identical-code-folding sink every empty body in the image collapsed
    // into (193 xrefs), which is why the export labels it with an unrelated class's
    // name. So Release() is the empty teardown and Construct() is the reset.
    //
    // WHAT THIS CHANGES AT RUNTIME. Only through call sites, and all of them were
    // moved in the same change:
    //   * BrnGraphics::EnvironmentMap::Release / ::Destruct (BrnEnvironmentMap.cpp)
    //     no longer reset the six face cameras to KF_DEFAULT_*; they now do nothing,
    //     which is the console. Inert either way -- WorldModule::Prepare re-runs
    //     EnvironmentMap::Prepare, which re-stamps all four scalars.
    //   * EnvironmentMap::Prepare's leading per-face Release() likewise becomes the
    //     no-op it is on the console. Net-neutral: SetAspectRatio re-reads
    //     m_fovHorizontal, and the trailing SetFovHorizontal(KF_ENVMAP_FOV_HORIZONTAL)
    //     overwrites it anyway.
    //   * the three WorldModule cameras (gFrustumQueryCamera, gDispatchCamera,
    //     sBringUpCamera) and BrnDirector::Camera::Camera::CopyToCgsCamera now call
    //     Construct(). Those four sites are the ones the X360 shows calling
    //     sub_827F94E8 -- 0x827F94E8's xrefs_to list is exactly {CopyToCgsCamera
    //     @0x8220AC48, MainDirector::Construct @0x8225B448, EnvironmentMap::Construct
    //     @0x827B40D0, WorldModule::GenerateDispatchLists @0x827D1CE8,
    //     WorldModule::GenerateFrustumQueries @0x827DADF8, three CgsGui Constructs}.
    //     ⚠ CopyToCgsCamera's is LOAD-BEARING: it is the ONLY writer of m_aspectRatio
    //     on the frame camera and on every shadow cascade camera, and BrnShadowMap's
    //     lFrameCamera is a fresh stack object (the default ctor is `Camera() {}`).
    //     Leaving that one as an empty Release() would hand SetFovHorizontal an
    //     uninitialised aspect -- the exact 180-degree-tall-camera failure the
    //     2026-08-12 shadow wave spent a day on. It is fixed in Camera.cpp in the
    //     same commit; this file must not land without it.
    // ------------------------------------------------------------------------
    void Camera::Release()
    {
    }

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::UpdatePerspectiveProjectionMatrix @0x827EC778
    //
    // Rebuild mProjection from the cached scalars (n = m_nearClipPlane [87],
    // f = m_farClipPlane [88], the two one-over-tan terms [81]/[84]):
    //   xAxis = { 1/tanHalfH, 0, 0, 0 }
    //   yAxis = { 0, 1/tanHalfV, 0, 0 }
    //   zAxis = { 0, 0, (n+f)/(f-n),  1 }
    //   wAxis = { 0, 0, (n*f*-2)/(f-n), 0 }
    // then rebuild mViewProjection (tail-call UpdateViewProjectionMatrix).
    // Expression shapes transcribed operand-for-operand from the asm.
    //
    void Camera::UpdatePerspectiveProjectionMatrix()
    {
        const f32 lfNear = maProjectionScalars[7];   // a1[87]
        const f32 lfFar  = maProjectionScalars[8];   // a1[88]
        const f32 lfNearTimesFar = lfNear * lfFar;   // v10

        {
            const Vector4 lAggregateValue ( maProjectionScalars[1], 0.0f, 0.0f, 0.0f );
            mProjection.xAxis = lAggregateValue;
        }  // v15
        {
            const Vector4 lAggregateValue ( 0.0f, maProjectionScalars[4], 0.0f, 0.0f );
            mProjection.yAxis = lAggregateValue;
        }  // v14
        {
            const Vector4 lAggregateValue ( 0.0f, 0.0f,
                                     (lfNear + lfFar) / (lfFar - lfNear), 1.0f );
            mProjection.zAxis = lAggregateValue;
        } // v17
        {
            const Vector4 lAggregateValue ( 0.0f, 0.0f,
                                     (-2.0f * lfNearTimesFar) / (lfFar - lfNear), 0.0f );
            mProjection.wAxis = lAggregateValue;
        } // v16

        UpdateViewProjectionMatrix();
    }

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::UpdateOrthogonalProjectionMatrix @0x827E72E0
    // (DWARF CgsCamera.h:184; camera-frustum wave 2026-07-27)
    //
    // Rebuild mProjection as the orthogonal projection for half-extent scale
    // lfOrthoScale (n/f = scalars [7]/[8], lfs 0x15C/0x160):
    //   xAxis = { 1/s, 0, 0, 0 }            (fdivs flt_82001C98 / f1)
    //   yAxis = { 0, 1/s, 0, 0 }
    //   zAxis = { 0, 0, 1/(f-n), 0 }        (fdivs of the fsubs pair)
    //   wAxis = { 0, 0, n/(n-f), 1 }        (the [3][3] affine 1.0 is what
    //                                        GetFrustum dispatches on)
    // then rebuild mViewProjection (bl UpdateViewProjectionMatrix). Store
    // stream transcribed operand-for-operand from the asm.
    // ------------------------------------------------------------------------
    void Camera::UpdateOrthogonalProjectionMatrix(f32 lfOrthoScale)
    {
        const f32 lfNear = maProjectionScalars[7];   // f13
        const f32 lfFar  = maProjectionScalars[8];   // f10
        const f32 lfInvScale = 1.0f / lfOrthoScale;  // f11

        {
            const Vector4 lAggregateValue ( lfInvScale, 0.0f, 0.0f, 0.0f );
            mProjection.xAxis = lAggregateValue;
        }
        {
            const Vector4 lAggregateValue ( 0.0f, lfInvScale, 0.0f, 0.0f );
            mProjection.yAxis = lAggregateValue;
        }
        {
            const Vector4 lAggregateValue ( 0.0f, 0.0f, 1.0f / (lfFar - lfNear), 0.0f );
            mProjection.zAxis = lAggregateValue;
        }
        {
            const Vector4 lAggregateValue ( 0.0f, 0.0f, lfNear / (lfNear - lfFar), 1.0f );
            mProjection.wAxis = lAggregateValue;
        }

        UpdateViewProjectionMatrix();
    }

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::GetViewProjectionMatrixModified @0x827EC858
    // (DWARF CgsCamera.h:136; camera-frustum wave 2026-07-27 -- verified
    // lane-for-lane against the numeric emulation, see the family banner)
    //
    // The shader-constant form of the view-projection (SetShaderConstantData
    // slot 34 in the WorldModule feeds): a TRANSPOSED clip x/y with a
    // LINEARISED depth pair --
    //   row0 = mViewProjection column 0     (clip.x = dot(row0, pos))
    //   row1 = mViewProjection column 1     (clip.y = dot(row1, pos))
    //   row2 = mView column 2               (LINEAR view-space z)
    //   row3 = { proj[2][2], proj[3][2], proj[2][3], proj[3][3] }
    //          (the projection z/w coefficient quartet: the consumer rebuilds
    //           clip.z = viewZ*row3.x + row3.y, clip.w = viewZ*row3.z + row3.w)
    //
    // The X360 assembles rows 0/1/2 with the vmrghw/vmrglw transpose ladder and
    // row3 with two vperm+vsldoi combines whose lane controls live in the
    // UN-DUMPED rodata pair unk_82CDA3C0 / unk_82CDA400. DERIVATION (no silent
    // guess): both vperm sources are single-element SPLATS (vperm(splat(p22),
    // splat(p32), ctlA) / vperm(splat(p23), splat(p33), ctlB)) and the vsldoi
    // #8 keeps ctlA's lanes 2,3 and ctlB's lanes 0,1 -- so the only observable
    // freedom is "lane from source A vs source B", and the surviving lane
    // pattern is forced to [A,B] / [A,B] by what the consumer must read back
    // (the z/w reconstruction above): row3 == {p22, p32, p23, p33} exactly.
    // The initial whole-mViewProjection copy into the return slot is fully
    // overwritten (dead stores @0x827ECBD0..0x827ECBF0) and is not reproduced.
    //
    // The four leading dev asserts check the projection has no off-axis z/w
    // terms in its x/y rows: RwMath::IsSimilar(GetElem, 0) on [0][2], [1][2],
    // [0][3] and (the fourth block re-checks [1][2] with the same assert
    // string aRwmathIssimila_5 -- a duplicated source line, kept faithfully).
    // ⭐ EPSILON RECOVERED 2026-08-12 (conductor, targeted IDA export on a DB copy).
    // The old FLAG here said unk_820D16CC was un-dumped rodata, so the four asserts were
    // carried as EXACT `== 0.0f` compares -- "the conservative form". That was the wrong
    // direction of conservative: exact-zero is STRICTER than the console, so any projection
    // carrying a legitimate sub-epsilon residue trips an assert the real game would pass.
    // MEASURED: it fired 204 times in one ~4-minute run (four per call, every frame, from
    // the shadow-cascade leg) and was the ONLY remaining assert kind in the build.
    //
    // Dumped: 0x820D16CC = 0x37800000 = 1.52587891e-05f, i.e. exactly 2^-16. Its neighbours
    // in that pool confirm it is the rwmath scalar-constant block, not a one-off:
    //   +0x04 0x3C8EFA35 = 0.0174532924 (deg->rad)   +0x08 0x42652EE1 = 57.2957802 (rad->deg)
    //   +0x0C 0x40490FDB = pi           +0x10 = 2pi  +0x14 = pi/2     +0x18 = sqrt(2)
    // And agent B9 independently recovered the COMPARE SHAPE from the asm @0x827EC858:
    // `vandc` (clear the sign bit == fabs) then `vcmpgtfp` against that epsilon, i.e.
    //     IsSimilar(a, b)  ==  fabs(a - b) <= 1.52587891e-05f
    // So both halves are now attested and the real predicate is reproduced below.
    // ------------------------------------------------------------------------
    Matrix44 Camera::GetViewProjectionMatrixModified() const
    {
        // rw::math IsSimilar tolerance -- rodata 0x820D16CC (see the banner above).
        const f32 KF_RWMATH_IS_SIMILAR_EPSILON = 1.52587891e-05f;   // 0x37800000 == 2^-16
        #define RWMATH_IS_SIMILAR(a, b) \
            (((a) - (b) < 0.0f ? -((a) - (b)) : ((a) - (b))) <= KF_RWMATH_IS_SIMILAR_EPSILON)

        // The X360 has four native assertion tripwires here. The fourth deliberately
        // repeats [1][2] with the same source message as the second assertion.
        CGS_ASSERT(RWMATH_IS_SIMILAR(static_cast<f32>(mProjection.xAxis.GetZ()), 0.0f),
                   "RwMath::IsSimilar( m_projectionMatrix.GetElem(0, 2), 0.0f )");
        CGS_ASSERT(RWMATH_IS_SIMILAR(static_cast<f32>(mProjection.yAxis.GetZ()), 0.0f),
                   "RwMath::IsSimilar( m_projectionMatrix.GetElem(1, 2), 0.0f )");
        CGS_ASSERT(RWMATH_IS_SIMILAR(static_cast<f32>(mProjection.xAxis.GetW()), 0.0f),
                   "RwMath::IsSimilar( m_projectionMatrix.GetElem(0, 3), 0.0f )");
        CGS_ASSERT(RWMATH_IS_SIMILAR(static_cast<f32>(mProjection.yAxis.GetZ()), 0.0f),
                   "RwMath::IsSimilar( m_projectionMatrix.GetElem(1, 2), 0.0f )");

        #undef RWMATH_IS_SIMILAR

        Matrix44 lResult;
        // row0/row1 = VP columns 0/1 (the vmrghw ladder)
        {
            const Vector4 lAggregateValue ( mViewProjection.xAxis.GetX(), mViewProjection.yAxis.GetX(),
                                            mViewProjection.zAxis.GetX(), mViewProjection.wAxis.GetX() );
            lResult.xAxis = lAggregateValue;
        }
        {
            const Vector4 lAggregateValue ( mViewProjection.xAxis.GetY(), mViewProjection.yAxis.GetY(),
                                            mViewProjection.zAxis.GetY(), mViewProjection.wAxis.GetY() );
            lResult.yAxis = lAggregateValue;
        }
        // row2 = mView column 2 (the vmrglw pair over the view rows)
        {
            const Vector4 lAggregateValue ( mView.xAxis.GetZ(), mView.yAxis.GetZ(),
                                            mView.zAxis.GetZ(), mView.wAxis.GetZ() );
            lResult.zAxis = lAggregateValue;
        }
        // row3 = the projection z/w coefficient quartet (vperm/vsldoi combine)
        {
            const Vector4 lAggregateValue ( mProjection.zAxis.GetZ(), mProjection.wAxis.GetZ(),
                                            mProjection.zAxis.GetW(), mProjection.wAxis.GetW() );
            lResult.wAxis = lAggregateValue;
        }
        return lResult;
    }

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::SetPerspectiveProjectionMatrixRightHanded @0x827EC698
    //
    // The right-handed variant: identical scalar reads, mirrored z terms
    // ((n+f)/(n-f) with w = -1; 2nf/(n-f)), then the view-projection rebuild.
    // ------------------------------------------------------------------------
    void Camera::SetPerspectiveProjectionMatrixRightHanded()
    {
        const f32 lfNear = maProjectionScalars[7];   // a1[87]
        const f32 lfFar  = maProjectionScalars[8];   // a1[88]
        const f32 lfNearTimesFar = lfNear * lfFar;   // v10

        {
            const Vector4 lAggregateValue ( maProjectionScalars[1], 0.0f, 0.0f, 0.0f );
            mProjection.xAxis = lAggregateValue;
        }
        {
            const Vector4 lAggregateValue ( 0.0f, maProjectionScalars[4], 0.0f, 0.0f );
            mProjection.yAxis = lAggregateValue;
        }
        {
            const Vector4 lAggregateValue ( 0.0f, 0.0f,
                                     (lfNear + lfFar) / (lfNear - lfFar), -1.0f );
            mProjection.zAxis = lAggregateValue;
        }
        {
            const Vector4 lAggregateValue ( 0.0f, 0.0f,
                                     (lfNearTimesFar * 2.0f) / (lfNear - lfFar), 0.0f );
            mProjection.wAxis = lAggregateValue;
        }

        UpdateViewProjectionMatrix();
    }

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::LookAt @0x827F9510
    //
    // Build the view matrix from eye/up/target. The X360 computes the basis in
    // DOUBLE precision into m_viewMatrix_64bit (the maClipState f64 span,
    // row-major [row*4+col]), then narrows the four rows into mView (each float
    // row's w lane zeroed) and rebuilds mViewProjection. The arithmetic below is
    // transcribed operand-for-operand (including the cross-product lane order
    // and the negated translation dot products).
    // ------------------------------------------------------------------------
    void Camera::LookAt(rw::math::vpu::Vector3 lEyePosition,
                        rw::math::vpu::Vector3 lUpVector,
                        rw::math::vpu::Vector3 lTargetPosition)
    {
        const f32 lfEyeX = static_cast<f32>(lEyePosition.GetX());
        const f32 lfEyeY = static_cast<f32>(lEyePosition.GetY());
        const f32 lfEyeZ = static_cast<f32>(lEyePosition.GetZ());
        const f32 lfUpX = static_cast<f32>(lUpVector.GetX());
        const f32 lfUpY = static_cast<f32>(lUpVector.GetY());
        const f32 lfUpZ = static_cast<f32>(lUpVector.GetZ());
        const f32 lfTargetX = static_cast<f32>(lTargetPosition.GetX());
        const f32 lfTargetY = static_cast<f32>(lTargetPosition.GetY());
        const f32 lfTargetZ = static_cast<f32>(lTargetPosition.GetZ());

        // dir = normalize(target - eye)   (v39/v38/v40 -> v42/v44/v43)
        const f64 lfDx = static_cast<f64>(lfTargetX) - lfEyeX;
        const f64 lfDy = static_cast<f64>(lfTargetY) - lfEyeY;
        const f64 lfDz = static_cast<f64>(lfTargetZ) - lfEyeZ;
        const f64 lfInvLen = 1.0 / std::sqrt(lfDx * lfDx + lfDz * lfDz + lfDy * lfDy);   // v41
        const f64 lfDirX = lfDx * lfInvLen;   // v42
        const f64 lfDirZ = lfDz * lfInvLen;   // v43
        const f64 lfDirY = lfDy * lfInvLen;   // v44

        // right-ish basis = normalize(cross(up, dir)) -- lane order as computed:
        //   v45 = dir.z*up.x - up.z*dir.x
        //   v46 = up.z*dir.y - up.y*dir.z
        //   v47 = up.y*dir.x - dir.y*up.x
        const f64 lfCx = lfDirZ * lfUpX - static_cast<f64>(lfUpZ) * lfDirX;  // v45
        const f64 lfCy = static_cast<f64>(lfUpZ) * lfDirY - static_cast<f64>(lfUpY) * lfDirZ; // v46
        const f64 lfCz = static_cast<f64>(lfUpY) * lfDirX - lfDirY * lfUpX;  // v47
        const f64 lfInvLenC = 1.0 / std::sqrt(lfCy * lfCy + lfCz * lfCz + lfCx * lfCx);  // v48
        const f64 lfB0y = lfCx * lfInvLenC;   // v49 -> [1][0]
        const f64 lfB0x = lfInvLenC * lfCy;   // v50 -> [0][0]
        const f64 lfB0z = lfCz * lfInvLenC;   // v51 -> [2][0]

        // second basis column = normalize(cross(dir, b0)) -- lane order as computed:
        //   v52 = b0z*dir.x - dir.z*b0x
        //   v53 = dir.y*b0x - b0y*dir.x
        //   v54 = b0y*dir.z - b0z*dir.y
        const f64 lfC2x = lfB0z * lfDirX - lfDirZ * lfB0x;   // v52
        const f64 lfC2y = lfDirY * lfB0x - lfB0y * lfDirX;   // v53
        const f64 lfC2z = lfB0y * lfDirZ - lfB0z * lfDirY;   // v54
        const f64 lfInvLen2 = 1.0 / std::sqrt(lfC2z * lfC2z + lfC2y * lfC2y + lfC2x * lfC2x);  // v55
        const f64 lfB1y = lfC2x * lfInvLen2;  // v56 -> [1][1]
        const f64 lfB1x = lfInvLen2 * lfC2z;  // v57 -> [0][1]
        const f64 lfB1z = lfC2y * lfInvLen2;  // v58 -> [2][1]

        // m_viewMatrix_64bit (row-major f64[16]; the double store stream):
        maClipState[0]  = lfB0x;                                                        // +0xC0
        maClipState[1]  = lfB1x;                                                        // +0xC8
        maClipState[2]  = lfDirX;                                                       // +0xD0
        maClipState[3]  = 0.0;                                                          // +0xD8
        maClipState[4]  = lfB0y;                                                        // +0xE0
        maClipState[5]  = lfB1y;                                                        // +0xE8
        maClipState[6]  = lfDirY;                                                       // +0xF0
        maClipState[7]  = 0.0;                                                          // +0xF8
        maClipState[8]  = lfB0z;                                                        // +0x100
        maClipState[9]  = lfB1z;                                                        // +0x108
        maClipState[10] = lfDirZ;                                                       // +0x110
        maClipState[11] = 0.0;                                                          // +0x118
        maClipState[12] = -(lfB0x * lfEyeX + lfB0z * lfEyeZ + lfB0y * lfEyeY); // +0x120
        maClipState[13] = -(lfB1x * lfEyeX + lfB1z * lfEyeZ + lfB1y * lfEyeY); // +0x128
        maClipState[14] = -(lfDirX * lfEyeX + lfDirZ * lfEyeZ + lfDirY * lfEyeY); // +0x130
        maClipState[15] = 1.0;                                                          // +0x138 (0x3FF0...)

        // Narrow into the float view rows (w lanes zero), then rebuild the VP.
        {
            const Vector4 lAggregateValue ( static_cast<f32>(maClipState[0]),  static_cast<f32>(maClipState[1]),
                               static_cast<f32>(maClipState[2]),  0.0f );
            mView.xAxis = lAggregateValue;
        }
        {
            const Vector4 lAggregateValue ( static_cast<f32>(maClipState[4]),  static_cast<f32>(maClipState[5]),
                               static_cast<f32>(maClipState[6]),  0.0f );
            mView.yAxis = lAggregateValue;
        }
        {
            const Vector4 lAggregateValue ( static_cast<f32>(maClipState[8]),  static_cast<f32>(maClipState[9]),
                               static_cast<f32>(maClipState[10]), 0.0f );
            mView.zAxis = lAggregateValue;
        }
        {
            const Vector4 lAggregateValue ( static_cast<f32>(maClipState[12]), static_cast<f32>(maClipState[13]),
                               static_cast<f32>(maClipState[14]), 0.0f );
            mView.wAxis = lAggregateValue;
        }

        UpdateViewProjectionMatrix();
    }

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::Clone @0x827E7018
    //
    // memcpy(lpDest, this, 368) -- the whole 0x170 camera blob.
    // ------------------------------------------------------------------------
    void Camera::Clone(Camera* lpDest)
    {
        std::memcpy(lpDest, this, sizeof(Camera));
    }

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::SetFarClip -- RETIRED 2026-08-17 (reflections step 2,
    // envproducer finding F2). It was the m_farClipPlane store WITHOUT the projection
    // rebuild, and all three of its call sites (BrnEnvironmentMap.cpp:196 and
    // BrnWorldModule.cpp:4128/:6652) wrote the store and then called
    // UpdatePerspectiveProjectionMatrix on the next line -- i.e. every one of them was
    // spelling out the console's inlined Camera::SetFarClipPlane, which is now a real
    // member (CgsCamera.h, DWARF CgsCamera.h:103). Those three sites call it by name.
    // ------------------------------------------------------------------------

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::GetPosition / GetDirection -- PC-additive accessors
    // (FLAG: the DWARF Camera has neither member; the X360 WorldModule call
    // sites read the DIRECTOR camera transform rows +0x30/+0x20 instead). The
    // values are recovered from the orthonormal view matrix this camera was
    // built from (LookAt/CopyToCgsCamera): the basis columns are the view rows'
    // lanes, dir = column 2, and eye = -(w.x*b0 + w.y*b1 + w.z*b2).
    // ------------------------------------------------------------------------
    Vector3 Camera::GetPosition() const
    {
        const Vector3 lvW(mView.wAxis.GetVector());
        const Vector3 lvRow0(mView.xAxis.GetVector());
        const Vector3 lvRow1(mView.yAxis.GetVector());
        const Vector3 lvRow2(mView.zAxis.GetVector());
        Vector3 lvPosition = rw::math::vpu::Mult(lvRow0, static_cast<f32>(lvW.GetX()));
        lvPosition = rw::math::vpu::MultAdd(
            lvRow1, Vector3(static_cast<f32>(lvW.GetY()), static_cast<f32>(lvW.GetY()),
                            static_cast<f32>(lvW.GetY())), lvPosition);
        lvPosition = rw::math::vpu::MultAdd(
            lvRow2, Vector3(static_cast<f32>(lvW.GetZ()), static_cast<f32>(lvW.GetZ()),
                            static_cast<f32>(lvW.GetZ())), lvPosition);
        return rw::math::vpu::Negate(lvPosition);
    }

    Vector3 Camera::GetDirection() const
    {
        Vector3 lvDirection(0.0f, 0.0f, 0.0f);
        lvDirection.SetX(mView.xAxis.GetZ());   // view column 2 == the LookAt dir
        lvDirection.SetY(mView.yAxis.GetZ());
        lvDirection.SetZ(mView.zAxis.GetZ());
        return lvDirection;
    }

    // CgsGraphics::Camera::UpdateViewProjectionMatrix @0x827E7030 (wave 2)
    //
    // The X360 body (41 instructions, straight-line, no branches) is EXACTLY one inlined
    // rw::math::vpu::Mult(Matrix44Affine, Matrix44) -- the unity ledger attributes the outlined
    // instance to the vpu inline header. Loads the four mView rows (+0x00..0x3F) and the four
    // mProjection rows (+0x40..0x7F), combines them with the AFFINE row recipe and stores the
    // four result rows to mViewProjection (+0x80..0xBF):
    //
    //   out_i = vspltw(view_i,X)*proj0 + vspltw(view_i,Y)*proj1 + vspltw(view_i,Z)*proj2
    //   out_3 += proj3                       (w row seeds its accumulator with proj.wAxis)
    //
    // i.e. mViewProjection = Mult((Matrix44Affine)mView, mProjection): the view matrix's fourth
    // column is treated as the implicit (0,0,0,1), so view row w-lanes never contribute.
    // Instruction accounting (vmaddfp vD,vA,vB,vC == vD = vA*vC + vB, validated against the
    // affine-Mult reference): vmulfp128 seeds rows 0..2 @0x827E7070/7C/88; vmaddfp seeds row 3
    // with proj3 @0x827E7090; the y*proj1 madds @0x827E70AC..70B8; the z*proj2 madds
    // @0x827E70CC..70D8 store to +0x80..+0xB0. blr with r3 untouched -> void. The native
    // implementation below calls that same affine multiply directly.
    void Camera::UpdateViewProjectionMatrix()
    {
        const Matrix44Affine lView(
            Vector3(mView.xAxis.GetVector()), Vector3(mView.yAxis.GetVector()),
            Vector3(mView.zAxis.GetVector()), Vector3(mView.wAxis.GetVector()));
        mViewProjection = rw::math::vpu::Mult(lView, mProjection);
    }
}
