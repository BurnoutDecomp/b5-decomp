#ifndef SDKS_PACKAGES_ICE_ICECAMERASPACEHANDLER_HPP
#define SDKS_PACKAGES_ICE_ICECAMERASPACEHANDLER_HPP

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Matrix44Affine (the 8 take matrices)
#include "ICEDataEnums.hpp"      // ICE::eICESpace
#include "ICEMath.hpp"           // ICE::Vector3, ICE::Matrix4

// ============================================================================
// SDKs/Packages/ICE/ICECameraSpaceHandler.hpp
//
// ICE::CameraSpaceHandler -- the per-frame cache of the eight ICE camera-take
// reference-space transforms (car / car2 / traffic-light / scene / impact /
// heading / loose-heading / heading2), each a 4x4 affine, plus the gameplay
// camera behaviour back-pointer used to derive the gameplay/takedown spaces.
// TransformToWorld(...) projects a point/space through these matrices.
//
// THIS FILE IS THE CameraSpaceHandler HOME (the type layout + the copy ctor and
// copy-assignment owned by this TU). The substantial method bodies --
// Construct, the two TransformToWorld overloads, GetSpaceTransformationMatrix,
// GetWorldOrientationMatrix, the Get/Set accessors, GetTransformToWorld, and the
// private GetTakedownToWorld / GetReverseTakedownToWorld -- live in OTHER TUs
// (ICECameraSpaceHandler.cpp and siblings) and are DECLARATION-ONLY here. They
// EXTEND this home; do not re-home them.
//
// Layout authority: references/DecFIGS/dwarfdump/SDKs/Packages/ICE/
// ICECameraSpaceHandler.hpp -- eight rw::math::vpu::Matrix44Affine members in
// order, then the const BehaviourHandle<BehaviourGameplayExternal>* back-pointer.
// Member placement is corroborated by the X360 copy ctor (@0x821FAA88) and
// operator= (@0x8252B5A8), which both flat-copy the object: 32 lvx128/stvx128
// 16-byte vector moves over offsets 0x000..0x1F0 (8 matrices * 64 bytes = 512
// bytes), then a single lwz/stw word copy at offset 0x200 (the mpGamePlayCam
// pointer). The two functions are byte-identical (same 512 + 4 copy).
//
// (DWARF spells the scalars float32_t / uint8_t; those are NOT project types ->
// f32 / u8, per the type vocabulary in types.hpp.)
// ============================================================================

// mpGamePlayCam is a POINTER, so an incomplete type is sufficient here --
// forward-declared rather than pulling the whole BrnDirector::Camera behaviour header
// into every ICE TU. The TU that DEREFERENCES it (ICECameraSpaceHandler.cpp, for
// GetTransformToWorld's eICE_GAMEPLAY_SPACE arm) includes BrnBehaviourManager.h itself.
//
// ⚠️ FIXED 2026-08-01: this used to forward-declare a GLOBAL `template <typename T>
// class BehaviourHandle;`, which is a DIFFERENT type from the real
// BrnDirector::Camera::BehaviourHandle<> that BrnBehaviourManager.h:128/422 defines and
// that every producer of this pointer actually holds. Nothing had noticed because
// CameraSpaceHandler::Construct still has no caller in the tree, so the global template
// was never required to be complete -- it named a type that is defined NOWHERE. Declared
// in its real namespace now, so the pointer means what the DWARF says it means
// (ICECameraSpaceHandler.hpp:118 `const BehaviourHandle<BrnDirector::Camera::
// BehaviourGameplayExternal>* mpGamePlayCam`).
namespace BrnDirector
{
namespace Camera
{
    class BehaviourGameplayExternal;
    template <typename TBehaviour> class BehaviourHandle;
}
}

namespace ICE
{

// Convenience alias so the member declarations read exactly as the DWARF (bare
// `Matrix44Affine`). The type itself is owned by the vendor rw math header.
typedef rw::math::vpu::Matrix44Affine Matrix44Affine;

// ---------------------------------------------------------------------------
// ICE::CameraSpaceHandler (DWARF ICECameraSpaceHandler.hpp:37).
//
// LAYOUT (DWARF :109-118, corroborated by the X360 flat-copy ctor/operator=):
//   @0x000  Matrix44Affine mCarToWorld
//   @0x040  Matrix44Affine mCar2ToWorld
//   @0x080  Matrix44Affine mTrafficLightToWorld
//   @0x0C0  Matrix44Affine mSceneToWorld
//   @0x100  Matrix44Affine mImpactToWorld
//   @0x140  Matrix44Affine mHeadingToWorld
//   @0x180  Matrix44Affine mLooseHeadingToWorld
//   @0x1C0  Matrix44Affine mHeading2ToWorld
//   @0x200  const BehaviourHandle<BehaviourGameplayExternal>* mpGamePlayCam
// (8 * 64 = 512 bytes of matrices, then a 4-byte pointer at +0x200.)
// ---------------------------------------------------------------------------
struct CameraSpaceHandler
{
private:
    // Eight reference-space transforms, IN DWARF ORDER (each a 16-aligned 4x4
    // affine = 64 bytes).
    Matrix44Affine mCarToWorld;          // @0x000
    Matrix44Affine mCar2ToWorld;         // @0x040
    Matrix44Affine mTrafficLightToWorld; // @0x080
    Matrix44Affine mSceneToWorld;        // @0x0C0
    Matrix44Affine mImpactToWorld;       // @0x100
    Matrix44Affine mHeadingToWorld;      // @0x140
    Matrix44Affine mLooseHeadingToWorld; // @0x180
    Matrix44Affine mHeading2ToWorld;     // @0x1C0

    // @0x200  Back-pointer to the gameplay camera behaviour; copied verbatim by
    // the copy ctor / operator= (the trailing lwz/stw word). Forward-declared
    // template handle -> pointer-only here.
    const BrnDirector::Camera::BehaviourHandle<BrnDirector::Camera::BehaviourGameplayExternal>* mpGamePlayCam;

public:
    // Default constructor: the eight matrices + the back-pointer are left to their
    // members' default-init (the real per-frame values are written by Construct / the
    // copy assignment). Declared because the user-provided copy ctor below otherwise
    // suppresses the implicit default ctor, and CameraSpaceHandler is embedded by value
    // (e.g. BrnDirector::ICEWrapper::mCameraSpaceHandler) so it must be default-constructible.
    CameraSpaceHandler() = default;

    // === Owned by THIS TU =================================================
    // Copy constructor (@0x821FAA88) and copy-assignment (@0x8252B5A8). The X360
    // bodies are a flat bitwise copy of the whole object: 32 lvx128/stvx128
    // 16-byte vector moves over the 512-byte matrix block, then a single word
    // copy of mpGamePlayCam at +0x200. Because every member (the eight
    // Matrix44Affine and the pointer) is trivially copyable, the defaulted
    // special members generate the IDENTICAL flat member-wise (memcpy-shaped)
    // copy -- exact semantic parity with the lvx/stvx + lwz/stw sequence -- so we
    // model them as `= default` (cleanest form that reproduces the copy).
    CameraSpaceHandler(const CameraSpaceHandler& lrOther) = default;
    CameraSpaceHandler& operator=(const CameraSpaceHandler& lrOther) = default;

    // === Owned by OTHER TUs -- DECLARATION-ONLY ===========================
    // FLAG: the entire method set below is declaration-only here; bodies live in
    // ICECameraSpaceHandler.cpp / sibling TUs and EXTEND this home. Shapes
    // (return types, params, trailing const) follow the DWARF, not Hex-Rays.
    // ⭐ BODIED 2026-08-01 in ICECameraSpaceHandler.cpp (the DWARF's own home for it:
    // ICECameraSpaceHandler.cpp:2). ⚠️ THE EIGHT MATRICES ARE `const&`, NOT BY VALUE: the
    // .hpp line of the DWARF drops the reference decoration, the .cpp definition line carries
    // it (`const rw::math::vpu::Matrix44Affine & lCarToWorld`, ...), and the X360 @0x8252B950
    // agrees -- every argument arrives as a POINTER that the body lvx128's four rows out of.
    // This declaration used to say by-value, which was a 512-byte-per-call stack copy the
    // console does not make.
    void Construct(const Matrix44Affine& lrCarToWorld,
                   const Matrix44Affine& lrCar2ToWorld,
                   const Matrix44Affine& lrTrafficLightToWorld,
                   const Matrix44Affine& lrSceneToWorld,
                   const Matrix44Affine& lrImpactToWorld,
                   const Matrix44Affine& lrHeadingToWorld,
                   const Matrix44Affine& lrLooseHeadingToWorld,
                   const Matrix44Affine& lrHeading2ToWorld,
                   const BrnDirector::Camera::BehaviourHandle<BrnDirector::Camera::BehaviourGameplayExternal>* lpGamePlayCam);

    Vector3 TransformToWorld(Vector3 lvPoint, eICESpace leSpace) const;
    Vector3 TransformToWorld(Vector3 lvPoint, u8 lu8A, u8 lu8B, u8 lu8C, u8 lu8D, f32 lfBlend) const;

    void GetSpaceTransformationMatrix(Matrix4* lpMatrix, u8 lu8Space) const;
    void GetWorldOrientationMatrix(Matrix4* lpMatrix, u8 lu8SpaceA, u8 lu8SpaceB) const;

    const Matrix44Affine& GetCarToWorld() const;

    void SetCarToWorld(Matrix44Affine lMatrix);
    void SetCar2ToWorld(Matrix44Affine lMatrix);
    void SetHeadingToWorld(Matrix44Affine lMatrix);
    void SetHeading2ToWorld(Matrix44Affine lMatrix);

    const Matrix44Affine GetTransformToWorld(eICESpace leSpace) const;

private:
    const Matrix44Affine GetTakedownToWorld() const;
    const Matrix44Affine GetReverseTakedownToWorld() const;
};

} // namespace ICE

#endif // SDKS_PACKAGES_ICE_ICECAMERASPACEHANDLER_HPP
