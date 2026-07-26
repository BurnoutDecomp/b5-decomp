// CgsGraphics::Camera -- the graphics-side camera (view / projection / view-projection transforms
// plus cached projection scalars), reconstructed from the X360 ARTIST asm.
//
// Source-of-truth: X360 ASM is the spine; the vector/matrix members are the repo-canonical
// rw::math::vpu Vector4 / Matrix44 (BrnCommonTypes.h). See CgsCamera.h for the layout authority.

#include "GameShared/GameClasses/Graphics/CgsCamera.h"

#include <cmath>    // tanf/atanf (SetFovHorizontal -- the XMVectorTan/ATan lowering), sqrt (LookAt)
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

    // CgsGraphics::Camera::GetFrustum @0x82277298
    //
    // The X360 body splats the second transform's wAxis lane0 (+0x70) and compares it (vcmpeqfp.
    // lane0) against the immediate 1.0 (flt_82001C98). If equal -> parallel (orthographic)
    // projection; otherwise perspective. The single-lane equality de-optimises to a scalar compare
    // on mProjection.wAxis lane0 (.x).
    const CgsGeometric::Frustum& Camera::GetFrustum()
    {
        // immediate flt_82001C98 == 1.0f
        if (mProjection.wAxis.x == 1.0f)
        {
            return GetFrustumParallel();
        }
        else
        {
            return GetFrustumPerspective();
        }
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
