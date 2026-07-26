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
    // FLAG (opaque placeholder): the RW-side frustum snapshot
    // Camera::GetFrustum(CameraRwFrustum&) writes and CgsGeometric::Frustum::
    // SetFromRwFrustum consumes (both pending externals; the only attested
    // producer/consumer pair is BrnWorld::ShadowMap::ComputeTSMMatrix
    // @ 0x827BFF58). 128 bytes of 16-aligned storage is provenance-honest
    // (the X360 stack slot region), not a decoded layout.
    struct CameraRwFrustum
    {
        Vector4 maData[8];
    };

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
        // scalars from a horizontal field-of-view (RADIANS) and rebuild the perspective
        // projection matrix. BODIED in CgsCamera.cpp (destub wave 2026-07-26): the DWARF scalar
        // names (m_fovHorizontal / m_(oneOver)TanHalfFov* / m_fovVertical / m_aspectRatio) pin
        // every store; the XMVectorTan/XMVectorATan polynomial pair is lowered to tanf/atanf
        // per the ICEMath.cpp precedent (FLAG there: poly table un-recovered, libm equivalent).
        //
        // ---- Construct family (X360): the DWARF (CgsCamera.h:59/:66/:78) declares BOTH the
        //      no-arg Construct() and Construct(f32,f32,f32,f32), plus Release(). The 4-arg
        //      body is @0x827F0A08 (aspect/near/far stores + identity view + SetFovHorizontal
        //      tail call); the no-arg Construct()/Release() pair ICF-fold to one X360 body
        //      @0x827F94E8 that forwards the KF_DEFAULT_* camera defaults. All bodied in
        //      CgsCamera.cpp. ----
        void Construct();
        // ADDITIVE overload (DWARF CgsCamera.h:66, X360 @0x827F0A08).
        void Construct(f32 lfFovHorizontal, f32 lfAspectRatio, f32 lfNearClipPlane, f32 lfFarClipPlane);
        // ADDITIVE (WorldModule::Prepare @0x827D53B0 stage 1 clears the cached
        // camera input). Declaration-only; body with the Camera TU. FLAG: no DWARF
        // Clear() member and no identified X360 body -- referencing site not yet
        // reconciled (see WorldLinkStubs.cpp).
        void Clear();
        void Release();
        void LookAt(rw::math::vpu::Vector3 lEyePosition,
                    rw::math::vpu::Vector3 lUpVector,
                    rw::math::vpu::Vector3 lTargetPosition);
        void SetPerspectiveProjectionMatrixRightHanded();

        void SetFovHorizontal(f32 fovHorizontal);

        // CgsGraphics::Camera::UpdateViewProjectionMatrix @0x827E7030 (wave 2) --
        // rebuild mViewProjection = mView (affine) * mProjection. Body in
        // CgsCamera.cpp. Attested callers: BrnWorld::ShadowMap::
        // CalculateShadowMapCameras, Camera::UpdateOrthogonalProjectionMatrix,
        // Camera::SetPerspectiveProjectionMatrixRightHanded,
        // Camera::UpdatePerspectiveProjectionMatrix, Camera::LookAt.
        void UpdateViewProjectionMatrix();

        // --- PENDING DECLARATIONS (cross-TU; bodies live in their own not-yet-
        //     landed TUs; call shapes attested at the BrnWorld::ShadowMap::
        //     ComputeTSMMatrix @ 0x827BFF58 call sites) ------------------------

        // @call 0x827BFFD4: r3 = source camera (this), r4 = destination.
        void Clone(Camera* lpDest);
        // @calls 0x827BFFF0 / 0x827C000C: rebuild mProjection/mViewProjection
        // from the cached projection scalars.
        void UpdatePerspectiveProjectionMatrix();
        // @call 0x827C004C: overload writing an RW-side frustum snapshot to
        // lrOut (see CameraRwFrustum above).
        void GetFrustum(CameraRwFrustum& lrOut);
        // @call 0x827C1060: fill lpOut from this camera's parallel projection.
        void GetCgsFrustumParallel(CgsGeometric::Frustum* lpOut);

    public:
        // PROMOTED 2026-07-24: WorldModule::GenerateFrustumQueries @0x827DADF8 reads the
        // cached perspective frustum from CONST cameras (the six env-map face cameras and
        // the shadow cascades), so this accessor is public + const, matching its real use.
        // (GetFrustum still dispatches to these two; defined in their own homes.)
        const CgsGeometric::Frustum& GetFrustumParallel() const;
        const CgsGeometric::Frustum& GetFrustumPerspective() const;

        // The cached view-projection the frustum-test events carry.
        const Matrix44& GetViewProjectionMatrix() const { return mViewProjection; }

        // ---- ADDITIVE (WorldModule::GenerateDispatchLists @0x827D1CE8) ----
        // Bodied in CgsCamera.cpp. FLAG: the DWARF Camera has NO GetPosition/
        // GetDirection members -- the X360 call sites read the DIRECTOR camera's
        // transform rows (+0x30/+0x20) directly; these PC-additive accessors return
        // the same values derived from the orthonormal view matrix this camera was
        // built from (LookAt/CopyToCgsCamera), so the committed WorldModule call
        // shape keeps X360 semantics.
        Vector3 GetPosition() const;
        Vector3 GetDirection() const;
        // The renderer's half-texel/viewport-adjusted view-projection (X360
        // GetViewProjectionMatrixModified, returned by value).
        Matrix44 GetViewProjectionMatrixModified() const;
        void SetFarClip( f32 lfFarClip );

    public:
        // +0x00..0x3F -- view transform (4 Vector4 rows).
        Matrix44 mView;
        // +0x40..0x7F -- projection transform. GetFrustum reads mProjection.wAxis lane0 (+0x70) as
        // the parallel-vs-perspective discriminator (== 1.0 -> parallel).
        Matrix44 mProjection;
        // +0x80..0xBF -- combined view*projection transform.
        Matrix44 mViewProjection;
        // +0xC0..0x13F -- DWARF (CgsCamera.h:206): `Matrix44_64 m_viewMatrix_64bit`, the
        // DOUBLE-precision view matrix (row-major 4x4 of f64). LookAt @0x827F9510 builds the
        // view in double precision here, then narrows it into mView. Kept as the flat f64[16]
        // span (row-major: element [row*4 + col]) to preserve the committed layout pins.
        f64      maClipState[16];
        // +0x140..0x163 -- cached projection scalars. DWARF names (CgsCamera.h:209-219):
        //   [0] m_fovHorizontal              [1] m_oneOverTanHalfFovHorizontal
        //   [2] m_tanHalfFovHorizontal       [3] m_fovVertical
        //   [4] m_oneOverTanHalfFovVertical  [5] m_tanHalfFovVertical
        //   [6] m_aspectRatio                [7] m_nearClipPlane
        //   [8] m_farClipPlane
        // (kept as the committed flat span -- the ShadowMap TU already pins [7]/[8] by index).
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
