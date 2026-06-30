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
}
