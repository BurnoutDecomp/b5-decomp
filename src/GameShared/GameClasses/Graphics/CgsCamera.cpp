// CgsGraphics::Camera -- the graphics-side camera (view / projection / view-projection transforms
// plus cached projection scalars), reconstructed from the X360 ARTIST asm.
//
// Source-of-truth: X360 ASM is the spine; the vector/matrix members are the repo-canonical
// rw::math::vpu Vector4 / Matrix44 (BrnCommonTypes.h). See CgsCamera.h for the layout authority.

#include "GameShared/GameClasses/Graphics/CgsCamera.h"

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

    // CgsGraphics::Camera::SetFovHorizontal @0x821F13B0 -- DECLARATION-ONLY + FLAGGED.
    //
    // The X360 body is a multi-stage transcendental pipeline: it forms (fovHorizontal * 0.5)
    // (flt_82001DA0 == 0.5), runs XMVectorTan / XMVectorATan (both still external [todo] in this
    // cluster) interleaved with reciprocals (flt_82001C98 == 1.0 numerator) and the constant
    // 2.0 (flt_82001D9C), stores the resulting per-axis tangent / reciprocal-tangent terms into the
    // 0x140 projection-scalar block, then tail-calls the sibling
    // CgsGraphics::Camera::UpdatePerspectiveProjectionMatrix (a separate, not-yet-landed TU).
    // Bodying it would require fabricating the tan/atan VMX sequence and the projection-matrix
    // build -- left unbodied per the no-fabrication / no-paraphrase-of-VMX rule.
    //
    // void Camera::SetFovHorizontal(f32 fovHorizontal) { /* XMVectorTan/ATan + UpdatePerspectiveProjectionMatrix */ }

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
