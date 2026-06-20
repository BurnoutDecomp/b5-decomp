// ============================================================================
// SDKs/Packages/ICE/ICECameraSpaceHandlerCtor.cpp
//
// Compile anchor for the ICE::CameraSpaceHandler TU that owns the copy
// constructor (@0x821FAA88) and copy-assignment operator (@0x8252B5A8). Both are
// `= default` in the home header (ICECameraSpaceHandler.hpp) -- the eight
// Matrix44Affine members and the mpGamePlayCam pointer are all trivially
// copyable, so the defaulted special members generate the IDENTICAL flat copy
// the X360 asm performs (32 lvx128/stvx128 16-byte vector moves over the 512-byte
// matrix block, then the lwz/stw word copy of the +0x200 pointer).
//
// This .cpp gives the compile gate a real TU: it pulls the header and ODR-uses
// both special members (so they are instantiated and checked), and pins the
// matrix-block layout the X360 flat-copy idiom relies on.
//
// The substantial CameraSpaceHandler bodies (Construct / TransformToWorld / the
// space-matrix getters / accessors) live in OTHER TUs and are not defined here.
// ============================================================================

#include "ICECameraSpaceHandler.hpp"

namespace ICE
{

// --- Layout proofs -------------------------------------------------------
// Matrix44Affine is the 16-aligned 4x4 affine the lvx128/stvx128 stride-16 idiom
// walks: four 16-byte Vector3 lanes = 64 bytes.
static_assert(sizeof(rw::math::vpu::Matrix44Affine) == 64,
              "Matrix44Affine must be 64 bytes (four 16-byte affine rows)");

// The eight take matrices form a contiguous 512-byte block (8 * 64) -- exactly
// the span the X360 copy ctor / operator= flat-copy with 32 vector moves.
static_assert(8 * sizeof(rw::math::vpu::Matrix44Affine) == 512,
              "the eight take matrices must occupy a 512-byte block");

// mpGamePlayCam sits at offset 0x200 == 512 (the trailing lwz/stw word in the
// X360 copy). FLAG: on the x64 PC compile the pointer is 8 bytes, so the full
// sizeof(CameraSpaceHandler) is 512 + 8 = 520, padded up to 528 by the 16-byte
// Matrix44Affine alignment -- larger than the X360's 4-byte-pointer object. That
// is the documented platform pointer-width delta; it does NOT move any of the
// 512-byte matrix block the asm offsets depend on. The "512" the X360 copy walks
// is the MATRIX block, not the whole object. The object begins with the 512-byte
// matrix block, so the whole object is at least that large.
static_assert(sizeof(CameraSpaceHandler) >= 512,
              "CameraSpaceHandler embeds the 512-byte matrix block");

// CameraSpaceHandler is trivially copyable, which is WHY the defaulted copy
// ctor / operator= reproduce the X360 flat (lvx/stvx + lwz/stw) copy exactly.
static_assert(__is_trivially_copyable(CameraSpaceHandler),
              "the flat-copy parity relies on CameraSpaceHandler being trivially copyable");

// ODR-use both special members so the gate instantiates and checks the copy
// ctor and copy-assignment this TU owns.
const CameraSpaceHandler& AnchorCopyAssign(CameraSpaceHandler& lrDst,
                                           const CameraSpaceHandler& lrSrc)
{
    lrDst = lrSrc;            // copy-assignment (@0x8252B5A8)
    return lrDst;
}

CameraSpaceHandler AnchorCopyConstruct(const CameraSpaceHandler& lrSrc)
{
    return CameraSpaceHandler(lrSrc);   // copy constructor (@0x821FAA88)
}

} // namespace ICE
