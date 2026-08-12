// CgsGraphics::Camera -- the graphics-side camera (view / projection / view-projection transforms
// plus cached projection scalars), reconstructed from the X360 ARTIST asm.
//
// Source-of-truth: X360 ASM is the spine; the vector/matrix members are the repo-canonical
// rw::math::vpu Vector4 / Matrix44 (BrnCommonTypes.h). See CgsCamera.h for the layout authority.

#include "GameShared/GameClasses/Graphics/CgsCamera.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (GetViewProjectionMatrixModified devs)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // CgsDev::Log::gpDebugPrint (the
                                                            // off-axis projection log-once gate)

#include <cmath>    // tanf/atanf (SetFovHorizontal -- the XMVectorTan/ATan lowering), sqrt (LookAt / frustum normalizes)
#include <cstring>  // memcpy (Clone)

// ----------------------------------------------------------------------------
// Camera defaults (DWARF ::-scope globals, CgsCamera.h:32-35). The near/far
// magnitudes are RECOVERED from the X360 Construct()/Release() body @0x827F94E8
// (lfs immediates 0.1 / 1000.0). The fov/aspect pair lives in the un-dumped
// .data block (flt_82F30FD4 / flt_82F30FD8; IDA exports carry no data) -- FLAG:
// carried as honest zeros per the project convention. NOTE: with a zero default
// fov, SetFovHorizontal's 1/tan(0) is inf -- the FLAG values must be recovered
// before the camera path is driven with defaults.
// ----------------------------------------------------------------------------
const f32 KF_DEFAULT_FOVHORIZONTAL = 0.0f;   // flt_82F30FD4 -- FLAG: value not recovered
const f32 KF_DEFAULT_ASPECTRATIO   = 0.0f;   // flt_82F30FD8 -- FLAG: value not recovered
const f32 KF_DEFAULT_NEARCLIPPLANE = 0.1f;   // @0x827F94E8 immediate
const f32 KF_DEFAULT_FARCLIPPLANE  = 1000.0f; // @0x827F94E8 immediate

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
        struct Vec3 { f32 x, y, z; };

        inline Vec3 Cross3(const Vec3& lrA, const Vec3& lrB)
        {
            // the vpermwi128-0x63 cross shape: perm(a*perm(b) - perm(a)*b)
            Vec3 lvOut;
            lvOut.x = lrA.y * lrB.z - lrA.z * lrB.y;
            lvOut.y = lrA.z * lrB.x - lrA.x * lrB.z;
            lvOut.z = lrA.x * lrB.y - lrA.y * lrB.x;
            return lvOut;
        }

        inline f32 Dot3(const Vec3& lrA, const Vec3& lrB)
        {
            return lrA.x * lrB.x + lrA.y * lrB.y + lrA.z * lrB.z;   // vmsum3fp128
        }

        inline Vec3 Sub3(const Vec3& lrA, const Vec3& lrB)
        {
            Vec3 lvOut; lvOut.x = lrA.x - lrB.x; lvOut.y = lrA.y - lrB.y; lvOut.z = lrA.z - lrB.z;
            return lvOut;
        }

        inline Vec3 Normalize3(const Vec3& lrA)
        {
            // vmsum3fp128 + vrsqrtefp + 2x Newton-Raphson -> exact
            const f32 lfInvLen = 1.0f / std::sqrt(Dot3(lrA, lrA));
            Vec3 lvOut; lvOut.x = lrA.x * lfInvLen; lvOut.y = lrA.y * lfInvLen; lvOut.z = lrA.z * lfInvLen;
            return lvOut;
        }

        inline Vec3 RowXYZ(const Vector4& lrRow)
        {
            Vec3 lvOut; lvOut.x = lrRow.x; lvOut.y = lrRow.y; lvOut.z = lrRow.z;
            return lvOut;
        }

        // The shared inlined head of both frustum writers (see the banner):
        // world right/up/dir basis + eye from the view matrix inverse.
        struct CameraWorldBasis { Vec3 mRight, mUp, mDir, mEye; };

        CameraWorldBasis ComputeWorldBasisAndEye(const Matrix44& lrView)
        {
            const Vec3 lvRow0 = RowXYZ(lrView.xAxis);
            const Vec3 lvRow1 = RowXYZ(lrView.yAxis);
            const Vec3 lvRow2 = RowXYZ(lrView.zAxis);
            const Vec3 lvRowW = RowXYZ(lrView.wAxis);

            const Vec3 lvC12 = Cross3(lvRow1, lvRow2);
            const Vec3 lvC20 = Cross3(lvRow2, lvRow0);
            const Vec3 lvC01 = Cross3(lvRow0, lvRow1);
            const f32  lfInvDet = 1.0f / Dot3(lvRow0, lvC12);   // vrefp + 2x NR

            CameraWorldBasis lBasis;
            // inverse rows == transposed crosses * 1/det (the vmrghw/vmrglw block)
            lBasis.mRight.x = lvC12.x * lfInvDet; lBasis.mRight.y = lvC20.x * lfInvDet; lBasis.mRight.z = lvC01.x * lfInvDet;
            lBasis.mUp.x    = lvC12.y * lfInvDet; lBasis.mUp.y    = lvC20.y * lfInvDet; lBasis.mUp.z    = lvC01.y * lfInvDet;
            lBasis.mDir.x   = lvC12.z * lfInvDet; lBasis.mDir.y   = lvC20.z * lfInvDet; lBasis.mDir.z   = lvC01.z * lfInvDet;

            // eye = -(w-row through the inverse) -- the vmaddfp accumulate + vxor sign flip
            lBasis.mEye.x = -(lvRowW.x * lBasis.mRight.x + lvRowW.y * lBasis.mUp.x + lvRowW.z * lBasis.mDir.x);
            lBasis.mEye.y = -(lvRowW.x * lBasis.mRight.y + lvRowW.y * lBasis.mUp.y + lvRowW.z * lBasis.mDir.y);
            lBasis.mEye.z = -(lvRowW.x * lBasis.mRight.z + lvRowW.y * lBasis.mUp.z + lvRowW.z * lBasis.mDir.z);
            return lBasis;
        }

        inline void StorePlane(Vector4& lrOut, const Vec3& lrN, f32 lfD)
        {
            lrOut.x = lrN.x; lrOut.y = lrN.y; lrOut.z = lrN.z; lrOut.w = lfD;
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
        const Vec3& lrRight = lBasis.mRight;
        const Vec3& lrUp    = lBasis.mUp;
        const Vec3& lrDir   = lBasis.mDir;

        const f32 lfSx   = 1.0f / mProjection.xAxis.x;   // fdivs 1.0 / [0][0]
        const f32 lfSy   = 1.0f / mProjection.yAxis.y;   // fdivs 1.0 / [1][1]
        const f32 lfNear = maProjectionScalars[7];       // lfs 0x15C
        const f32 lfFar  = maProjectionScalars[8];       // lfs 0x160
        const f32 lfSpan = lfFar - lfNear;

        // near corners: (+r+u), (-r+u), (-r-u), (+r-u); far = +span*dir.
        Vec3 laNear[4], laFar[4];
        laNear[0].x = lBasis.mEye.x + lrRight.x * lfSx + lrUp.x * lfSy + lrDir.x * lfNear;
        laNear[0].y = lBasis.mEye.y + lrRight.y * lfSx + lrUp.y * lfSy + lrDir.y * lfNear;
        laNear[0].z = lBasis.mEye.z + lrRight.z * lfSx + lrUp.z * lfSy + lrDir.z * lfNear;
        laNear[1] = Sub3(laNear[0], Vec3{ lrRight.x * (2.0f * lfSx), lrRight.y * (2.0f * lfSx), lrRight.z * (2.0f * lfSx) });
        laNear[2] = Sub3(laNear[1], Vec3{ lrUp.x * (2.0f * lfSy), lrUp.y * (2.0f * lfSy), lrUp.z * (2.0f * lfSy) });
        laNear[3].x = laNear[2].x + lrRight.x * (2.0f * lfSx);
        laNear[3].y = laNear[2].y + lrRight.y * (2.0f * lfSx);
        laNear[3].z = laNear[2].z + lrRight.z * (2.0f * lfSx);
        for (int liCorner = 0; liCorner < 4; ++liCorner)   // the 4x far loop
        {
            laFar[liCorner].x = laNear[liCorner].x + lrDir.x * lfSpan;
            laFar[liCorner].y = laNear[liCorner].y + lrDir.y * lfSpan;
            laFar[liCorner].z = laNear[liCorner].z + lrDir.z * lfSpan;
        }

        StorePlane(lrOut.maPlanes[0], lrDir, Dot3(lrDir, laNear[0]));                       // near
        const Vec3 lvNegDir{ -lrDir.x, -lrDir.y, -lrDir.z };                                // vxor
        StorePlane(lrOut.maPlanes[1], lvNegDir, -Dot3(lrDir, laFar[0]));                    // far
        const Vec3 lvLeft   = Normalize3(Cross3(Sub3(laNear[1], laFar[1]), Sub3(laFar[2], laFar[1])));
        StorePlane(lrOut.maPlanes[2], lvLeft, Dot3(lvLeft, laNear[1]));                     // left
        const Vec3 lvRightN = Normalize3(Cross3(Sub3(laNear[3], laFar[3]), Sub3(laFar[0], laFar[3])));
        StorePlane(lrOut.maPlanes[3], lvRightN, Dot3(lvRightN, laNear[3]));                 // right
        const Vec3 lvTop    = Normalize3(Cross3(Sub3(laNear[0], laFar[0]), Sub3(laFar[1], laFar[0])));
        StorePlane(lrOut.maPlanes[4], lvTop, Dot3(lvTop, laNear[0]));                       // top
        const Vec3 lvBottom = Normalize3(Cross3(Sub3(laNear[2], laFar[2]), Sub3(laFar[3], laFar[2])));
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
        const Vec3& lrRight = lBasis.mRight;
        const Vec3& lrUp    = lBasis.mUp;
        const Vec3& lrDir   = lBasis.mDir;

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
        Vec3 laNear[4], laFar[4];
        for (int liCorner = 0; liCorner < 4; ++liCorner)
        {
            const f32 lfS = KAF_SIGNS[liCorner][0] * lfTanH;
            const f32 lfT = KAF_SIGNS[liCorner][1] * lfTanV;
            const Vec3 lvRay{ lrDir.x + lfS * lrRight.x + lfT * lrUp.x,
                              lrDir.y + lfS * lrRight.y + lfT * lrUp.y,
                              lrDir.z + lfS * lrRight.z + lfT * lrUp.z };
            laNear[liCorner].x = lBasis.mEye.x + lfNear * lvRay.x;
            laNear[liCorner].y = lBasis.mEye.y + lfNear * lvRay.y;
            laNear[liCorner].z = lBasis.mEye.z + lfNear * lvRay.z;
            laFar[liCorner].x  = lBasis.mEye.x + lfFar * lvRay.x;
            laFar[liCorner].y  = lBasis.mEye.y + lfFar * lvRay.y;
            laFar[liCorner].z  = lBasis.mEye.z + lfFar * lvRay.z;
        }

        StorePlane(lrOut.maPlanes[0], lrDir, Dot3(lrDir, laNear[0]));                       // near
        const Vec3 lvNegDir{ -lrDir.x, -lrDir.y, -lrDir.z };
        StorePlane(lrOut.maPlanes[1], lvNegDir, -Dot3(lrDir, laFar[0]));                    // far
        const Vec3 lvLeft   = Normalize3(Cross3(Sub3(laNear[1], laFar[1]), Sub3(laFar[2], laFar[1])));
        StorePlane(lrOut.maPlanes[2], lvLeft, Dot3(lvLeft, laNear[1]));                     // left
        const Vec3 lvRightN = Normalize3(Cross3(Sub3(laNear[3], laFar[3]), Sub3(laFar[0], laFar[3])));
        StorePlane(lrOut.maPlanes[3], lvRightN, Dot3(lvRightN, laNear[3]));                 // right
        const Vec3 lvTop    = Normalize3(Cross3(Sub3(laNear[0], laFar[0]), Sub3(laFar[1], laFar[0])));
        StorePlane(lrOut.maPlanes[4], lvTop, Dot3(lvTop, laNear[0]));                       // top
        const Vec3 lvBottom = Normalize3(Cross3(Sub3(laNear[2], laFar[2]), Sub3(laFar[3], laFar[2])));
        StorePlane(lrOut.maPlanes[5], lvBottom, Dot3(lvBottom, laNear[2]));                 // bottom

        if (lbNegateNearFar)   // the 6-plane negate loop @0x827F1104
        {
            for (int liPlane = 0; liPlane < 6; ++liPlane)
            {
                Vector4& lrPlane = lrOut.maPlanes[liPlane];
                lrPlane.x = -lrPlane.x; lrPlane.y = -lrPlane.y;
                lrPlane.z = -lrPlane.z; lrPlane.w = -lrPlane.w;
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
        if (mProjection.wAxis.w == 1.0f)
        {
            GetFrustumParallel(lrOut);
        }
        else
        {
            GetFrustumPerspective(lrOut, false);
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
        if (mProjection.wAxis.w == 1.0f)
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

        mView.xAxis = Vector4{ 1.0f, 0.0f, 0.0f, 0.0f };
        mView.yAxis = Vector4{ 0.0f, 1.0f, 0.0f, 0.0f };
        mView.zAxis = Vector4{ 0.0f, 0.0f, 1.0f, 0.0f };
        mView.wAxis = Vector4{ 0.0f, 0.0f, 0.0f, 0.0f };

        SetFovHorizontal(lfFovHorizontal);
    }

    // ------------------------------------------------------------------------
    // CgsGraphics::Camera::Construct() / Release() @0x827F94E8
    //
    // The no-arg reset pair (DWARF CgsCamera.h:59 / :78). On the X360 both fold
    // (ICF) to the single body that forwards the camera defaults into the 4-arg
    // Construct: Construct(KF_DEFAULT_FOVHORIZONTAL, KF_DEFAULT_ASPECTRATIO,
    // 0.1f, 1000.0f). (The call sites pair it with the empty ICF-folded teardown
    // stub -- see CgsCollisionGenerator.cpp's BaseCollisionGenerator::Destruct
    // @0x8284CB38, a single blr -- so the pair's net effect is exactly this reset.)
    // ------------------------------------------------------------------------
    void Camera::Construct()
    {
        Construct(KF_DEFAULT_FOVHORIZONTAL, KF_DEFAULT_ASPECTRATIO,
                  KF_DEFAULT_NEARCLIPPLANE, KF_DEFAULT_FARCLIPPLANE);
    }

    void Camera::Release()
    {
        Construct(KF_DEFAULT_FOVHORIZONTAL, KF_DEFAULT_ASPECTRATIO,
                  KF_DEFAULT_NEARCLIPPLANE, KF_DEFAULT_FARCLIPPLANE);
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
    // ------------------------------------------------------------------------
    void Camera::UpdatePerspectiveProjectionMatrix()
    {
        const f32 lfNear = maProjectionScalars[7];   // a1[87]
        const f32 lfFar  = maProjectionScalars[8];   // a1[88]
        const f32 lfNearTimesFar = lfNear * lfFar;   // v10

        mProjection.xAxis = Vector4{ maProjectionScalars[1], 0.0f, 0.0f, 0.0f };  // v15
        mProjection.yAxis = Vector4{ 0.0f, maProjectionScalars[4], 0.0f, 0.0f };  // v14
        mProjection.zAxis = Vector4{ 0.0f, 0.0f,
                                     (lfNear + lfFar) / (lfFar - lfNear), 1.0f }; // v17
        mProjection.wAxis = Vector4{ 0.0f, 0.0f,
                                     (lfNearTimesFar * -2.0f) / (lfFar - lfNear), 0.0f }; // v16

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

        mProjection.xAxis = Vector4{ lfInvScale, 0.0f, 0.0f, 0.0f };
        mProjection.yAxis = Vector4{ 0.0f, lfInvScale, 0.0f, 0.0f };
        mProjection.zAxis = Vector4{ 0.0f, 0.0f, 1.0f / (lfFar - lfNear), 0.0f };
        mProjection.wAxis = Vector4{ 0.0f, 0.0f, lfNear / (lfNear - lfFar), 1.0f };

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

        // The four RwMath::IsSimilar(m_projectionMatrix.GetElem(..), 0.0f) devs
        // (CgsCamera.cpp:0xB6..0xB9) -- DEGRADED TO A LOG-ONCE GATE 2026-08-12 (conductor).
        //
        // These are faithful console dev tripwires, and with the real epsilon now recovered
        // (above) they are also correctly TUNED -- but they are still firing, which means the
        // off-axis terms are NOT sub-epsilon residue: some caller genuinely hands this
        // function a projection with real z/w terms in its x/y rows. That is a REAL BUG and
        // it is NOT in this function; this function is only where it is detected.
        //
        // MEASURED: 432 halts in one ~2.5-minute run, four per call, every frame, from the
        // shadow-cascade leg (BrnWorldModule.cpp:5550). As hard asserts they made the build
        // unplayable on their own. Agent B9 pinned the frontier: cascade 0's projection is
        // CLEAN, so the offender is cascade 1/2 or a non-cascade shadow map, and since every
        // writer of Camera::mProjection stores literal 0.0f into those slots, the non-zero can
        // only enter via the TSM / bounding-box post-multiply at BrnShadowMap.cpp:1241/1249.
        //
        // So: log the OFFENDING VALUES once (which is strictly more useful than halting -- a
        // halt tells you nothing about magnitude) and keep running. ⛔ RESTORE THE HARD
        // ASSERTS once that post-multiply is fixed; they are correct as written.
        if ( !RWMATH_IS_SIMILAR(mProjection.xAxis.z, 0.0f) ||
             !RWMATH_IS_SIMILAR(mProjection.yAxis.z, 0.0f) ||
             !RWMATH_IS_SIMILAR(mProjection.xAxis.w, 0.0f) )
        {
            static bool s_bLoggedOffAxis = false;
            if ( !s_bLoggedOffAxis && CgsDev::Log::gpDebugPrint != 0 )
            {
                s_bLoggedOffAxis = true;
                *CgsDev::Log::gpDebugPrint
                    << "[camera] projection has off-axis z/w terms in its x/y rows -- "
                       "[0][2]=" << mProjection.xAxis.z
                    << " [1][2]=" << mProjection.yAxis.z
                    << " [0][3]=" << mProjection.xAxis.w
                    << " (epsilon 1.52587891e-05). Suspect the shadow TSM/bounding-box "
                       "post-multiply, BrnShadowMap.cpp:1241/1249. [FLAG PC boot gate]\n";
            }
        }

        #undef RWMATH_IS_SIMILAR

        Matrix44 lResult;
        // row0/row1 = VP columns 0/1 (the vmrghw ladder)
        lResult.xAxis = Vector4{ mViewProjection.xAxis.x, mViewProjection.yAxis.x,
                                 mViewProjection.zAxis.x, mViewProjection.wAxis.x };
        lResult.yAxis = Vector4{ mViewProjection.xAxis.y, mViewProjection.yAxis.y,
                                 mViewProjection.zAxis.y, mViewProjection.wAxis.y };
        // row2 = mView column 2 (the vmrglw pair over the view rows)
        lResult.zAxis = Vector4{ mView.xAxis.z, mView.yAxis.z, mView.zAxis.z, mView.wAxis.z };
        // row3 = the projection z/w coefficient quartet (vperm/vsldoi combine)
        lResult.wAxis = Vector4{ mProjection.zAxis.z, mProjection.wAxis.z,
                                 mProjection.zAxis.w, mProjection.wAxis.w };
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

        mProjection.xAxis = Vector4{ maProjectionScalars[1], 0.0f, 0.0f, 0.0f };
        mProjection.yAxis = Vector4{ 0.0f, maProjectionScalars[4], 0.0f, 0.0f };
        mProjection.zAxis = Vector4{ 0.0f, 0.0f,
                                     (lfNear + lfFar) / (lfNear - lfFar), -1.0f };
        mProjection.wAxis = Vector4{ 0.0f, 0.0f,
                                     (lfNearTimesFar * 2.0f) / (lfNear - lfFar), 0.0f };

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
        // dir = normalize(target - eye)   (v39/v38/v40 -> v42/v44/v43)
        const f64 lfDx = static_cast<f64>(lTargetPosition.x) - lEyePosition.x;
        const f64 lfDy = static_cast<f64>(lTargetPosition.y) - lEyePosition.y;
        const f64 lfDz = static_cast<f64>(lTargetPosition.z) - lEyePosition.z;
        const f64 lfInvLen = 1.0 / std::sqrt(lfDx * lfDx + lfDz * lfDz + lfDy * lfDy);   // v41
        const f64 lfDirX = lfDx * lfInvLen;   // v42
        const f64 lfDirZ = lfDz * lfInvLen;   // v43
        const f64 lfDirY = lfDy * lfInvLen;   // v44

        // right-ish basis = normalize(cross(up, dir)) -- lane order as computed:
        //   v45 = dir.z*up.x - up.z*dir.x
        //   v46 = up.z*dir.y - up.y*dir.z
        //   v47 = up.y*dir.x - dir.y*up.x
        const f64 lfCx = lfDirZ * lUpVector.x - static_cast<f64>(lUpVector.z) * lfDirX;  // v45
        const f64 lfCy = static_cast<f64>(lUpVector.z) * lfDirY - static_cast<f64>(lUpVector.y) * lfDirZ; // v46
        const f64 lfCz = static_cast<f64>(lUpVector.y) * lfDirX - lfDirY * lUpVector.x;  // v47
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
        maClipState[12] = -(lfB0x * lEyePosition.x + lfB0z * lEyePosition.z + lfB0y * lEyePosition.y); // +0x120
        maClipState[13] = -(lfB1x * lEyePosition.x + lfB1z * lEyePosition.z + lfB1y * lEyePosition.y); // +0x128
        maClipState[14] = -(lfDirX * lEyePosition.x + lfDirZ * lEyePosition.z + lfDirY * lEyePosition.y); // +0x130
        maClipState[15] = 1.0;                                                          // +0x138 (0x3FF0...)

        // Narrow into the float view rows (w lanes zero), then rebuild the VP.
        mView.xAxis = Vector4{ static_cast<f32>(maClipState[0]),  static_cast<f32>(maClipState[1]),
                               static_cast<f32>(maClipState[2]),  0.0f };
        mView.yAxis = Vector4{ static_cast<f32>(maClipState[4]),  static_cast<f32>(maClipState[5]),
                               static_cast<f32>(maClipState[6]),  0.0f };
        mView.zAxis = Vector4{ static_cast<f32>(maClipState[8]),  static_cast<f32>(maClipState[9]),
                               static_cast<f32>(maClipState[10]), 0.0f };
        mView.wAxis = Vector4{ static_cast<f32>(maClipState[12]), static_cast<f32>(maClipState[13]),
                               static_cast<f32>(maClipState[14]), 0.0f };

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
    // CgsGraphics::Camera::SetFarClip -- inlined on console (no standalone X360
    // body): the m_farClipPlane store the callers perform (attested by
    // BrnWorld::ShadowMap::ComputeTSMMatrix's far clamp at camera+0x160 and the
    // WorldModule env-map face flow, which follows it with the projection
    // rebuild calls). DWARF name: SetFarClipPlane (CgsCamera.h:103).
    // ------------------------------------------------------------------------
    void Camera::SetFarClip(f32 lfFarClip)
    {
        maProjectionScalars[8] = lfFarClip;   // m_farClipPlane
    }

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
        const Vector4& lrW = mView.wAxis;
        Vector3 lvPosition;
        lvPosition.x = -(lrW.x * mView.xAxis.x + lrW.y * mView.xAxis.y + lrW.z * mView.xAxis.z);
        lvPosition.y = -(lrW.x * mView.yAxis.x + lrW.y * mView.yAxis.y + lrW.z * mView.yAxis.z);
        lvPosition.z = -(lrW.x * mView.zAxis.x + lrW.y * mView.zAxis.y + lrW.z * mView.zAxis.z);
        lvPosition.w = 0.0f;
        return lvPosition;
    }

    Vector3 Camera::GetDirection() const
    {
        Vector3 lvDirection;
        lvDirection.x = mView.xAxis.z;   // view column 2 == the LookAt dir
        lvDirection.y = mView.yAxis.z;
        lvDirection.z = mView.zAxis.z;
        lvDirection.w = 0.0f;
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
    // @0x827E70CC..70D8 store to +0x80..+0xB0. blr with r3 untouched -> void. Scalar lowering
    // matches the committed SDK reference (SDKs/EATech/include/rw/math/vpu/detail/
    // matrix44_operation_platform_inline.h, Mult(Matrix44Affine, Matrix44)) lane for lane.
    void Camera::UpdateViewProjectionMatrix()
    {
        const Vector4* lapViewRows[4] =
        {
            &mView.xAxis, &mView.yAxis, &mView.zAxis, &mView.wAxis
        };
        Vector4* lapOutRows[4] =
        {
            &mViewProjection.xAxis, &mViewProjection.yAxis,
            &mViewProjection.zAxis, &mViewProjection.wAxis
        };

        for (int liRow = 0; liRow < 4; ++liRow)
        {
            // vspltw broadcasts of the view row's X/Y/Z lanes.
            const f32 lfX = lapViewRows[liRow]->x;
            const f32 lfY = lapViewRows[liRow]->y;
            const f32 lfZ = lapViewRows[liRow]->z;

            const f32* lpfProj0 = &mProjection.xAxis.x;
            const f32* lpfProj1 = &mProjection.yAxis.x;
            const f32* lpfProj2 = &mProjection.zAxis.x;
            const f32* lpfProj3 = &mProjection.wAxis.x;
            f32*       lpfOut   = &lapOutRows[liRow]->x;

            for (int liLane = 0; liLane < 4; ++liLane)
            {
                // rows 0..2: vmulfp128 seed; row 3: vmaddfp seeded with proj.wAxis.
                f32 lfAcc = (liRow == 3) ? lpfProj3[liLane] : 0.0f;
                lfAcc += lfX * lpfProj0[liLane];
                lfAcc += lfY * lpfProj1[liLane];
                lfAcc += lfZ * lpfProj2[liLane];
                lpfOut[liLane] = lfAcc;
            }
        }
    }
}
