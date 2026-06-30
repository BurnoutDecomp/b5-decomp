#pragma once

// CgsGraphics::Camera -- Burnout's graphics-side camera: it owns the view / projection /
// view-projection transforms plus the cached projection scalars (fov, aspect, near/far,
// per-axis tangent + reciprocal-tangent terms) the renderer needs, and builds a culling
// CgsGeometric::Frustum on demand.
//
// LAYOUT AUTHORITY = X360 ASM (copy-ctor @0x823C51D0 / operator= @0x82218ED0). The two copy
// bodies block-move a fixed extent: a leading 0xC0 (192) bytes via twelve 16-byte lvx128/stvx128
// pairs == three 64-byte Matrix44 rows-of-four, then sixteen 8-byte lfd/stfd doubles
// (0xC0..0x13F == 128 bytes), then nine 4-byte lfs/stfs floats (0x140..0x160). The copied tail
// is +0x160 (==352) + 4 == 356, which rounded up to the type's 16-byte alignment is 368 -- the
// honest sizeof attested by the CgsGui InputBuffer 368-byte embed (see CgsGuiModuleIO.h
// CgsGraphicsCameraStorage). There is NO vptr: the ctor block-copies offset 0 from the source
// rather than installing a vtable address, so this is a plain (non-polymorphic) value type.
//
// MATH HOME: the vector/matrix members are the repo-canonical rw::math::vpu Vector4 / Matrix44
// (via BrnCommonTypes.h) -- the same 16-byte-lane types every sibling (CgsFrustum, the physics/
// world TUs) embeds. GetFrustum's projection-type test reduces to the single-lane (lane0)
// float compare the X360 vcmpeqfp. emits.
//
// FLAG (member naming): the X360 bodies in this TU touch the leading matrices only positionally
// (GetFrustum reads the second matrix's wAxis lane0 as the projection-type discriminator;
// SetFovHorizontal writes the 0x140 float block). The semantic names mView / mProjection /
// mViewProjection and the projection-scalar field names are inferred from the camera role and
// the SetFovHorizontal store pattern; the byte offsets are the ground truth and are pinned by
// _AssertLayout below. The sixteen f64 at 0xC0 are modelled as an opaque maClipState[16] span --
// no X360 body in this key set reads them by field, so they are left un-named (honest layout,
// not fabricated semantics).

#include <cstddef>  // offsetof
#include "types.hpp"
#include "BrnCommonTypes.h"                                          // rw::math::vpu Vector4 / Matrix44
#include "GameShared/GameClasses/Geometric/Primitives/CgsFrustum.h"  // CgsGeometric::Frustum

namespace CgsGraphics
{
    class Camera
    {
    public:
        // Default ctor -- not attested in this key set (no zero-init body in the ledger); the
        // attested constructor is the copy-ctor below. Provided so the type is default-constructible
        // for the embed/instantiation gate.
        Camera() {}

        // CgsGraphics::Camera::Camera @0x823C51D0 -- the copy constructor. Block-copies the three
        // transforms, the clip-state doubles and the projection scalars from rhs.
        Camera(const Camera& rhs);

        // CgsGraphics::Camera::operator= @0x82218ED0 -- field-for-field assignment (same extent as
        // the copy ctor).
        Camera& operator=(const Camera& rhs);

        // CgsGraphics::Camera::GetFrustum @0x82277298 -- dispatch on the projection type. The X360
        // body broadcasts the second transform's wAxis lane0 and compares it against 1.0; equal ->
        // parallel (orthographic) projection, otherwise perspective.
        const CgsGeometric::Frustum& GetFrustum();

        // CgsGraphics::Camera::SetFovHorizontal @0x821F13B0 -- recompute the cached projection
        // scalars from a horizontal field-of-view and rebuild the perspective projection matrix.
        // DECLARATION-ONLY + FLAGGED: the body is a multi-stage transcendental pipeline built on
        // XMVectorTan / XMVectorATan (both still external [todo] in this cluster) feeding the
        // sibling CgsGraphics::Camera::UpdatePerspectiveProjectionMatrix (a separate, not-yet-landed
        // TU). Reconstructing it would require fabricating the tan/atan VMX sequence and the
        // projection-matrix build -- left unbodied per the no-fabrication rule.
        void SetFovHorizontal(f32 fovHorizontal);

    private:
        // Out-of-scope sibling helpers (their own TUs): GetFrustum dispatches to these. Declared so
        // the GetFrustum body type-checks; defined in their own homes.
        const CgsGeometric::Frustum& GetFrustumParallel();
        const CgsGeometric::Frustum& GetFrustumPerspective();

    public:
        // +0x00..0x3F -- view transform (4 Vector4 rows).
        Matrix44 mView;
        // +0x40..0x7F -- projection transform. GetFrustum reads mProjection.wAxis lane0 (+0x70) as
        // the parallel-vs-perspective discriminator (== 1.0 -> parallel).
        Matrix44 mProjection;
        // +0x80..0xBF -- combined view*projection transform.
        Matrix44 mViewProjection;
        // +0xC0..0x13F -- sixteen f64 of clip/derived state (un-named: no field access in this TU).
        f64      maClipState[16];
        // +0x140..0x163 -- cached projection scalars (the 0x140 block SetFovHorizontal writes:
        // fov, per-axis tangent + reciprocal-tangent terms, aspect). Nine floats are copied by the
        // ctor/operator= (through +0x160); the 9th lands the copied tail at +0x160.
        f32      maProjectionScalars[9];

        // Pad to the 368-byte (0x170) attested sizeof / 16-byte alignment (copied tail +0x160 + 4
        // == 356 rounded up to 368). +0x164..0x16F == 12 bytes.
        u8       mPad164[12];
    };

#ifndef NDEBUG
    // Pin the byte offsets the X360 bodies depend on (pointer-invariant on LLP64 -- no pointer
    // members in the copied extent). Never called.
    inline void Camera_AssertLayout()
    {
        static_assert(sizeof(Camera) == 0x170, "CgsGraphics::Camera sizeof must be 368 (X360 embed)");
        static_assert(__alignof(Camera) == 16, "CgsGraphics::Camera must be 16-aligned");
        static_assert(offsetof(Camera, mView)              == 0x00,  "mView @0x00");
        static_assert(offsetof(Camera, mProjection)        == 0x40,  "mProjection @0x40");
        static_assert(offsetof(Camera, mViewProjection)    == 0x80,  "mViewProjection @0x80");
        static_assert(offsetof(Camera, maClipState)        == 0xC0,  "maClipState @0xC0");
        static_assert(offsetof(Camera, maProjectionScalars)== 0x140, "maProjectionScalars @0x140");
    }
#endif
}
