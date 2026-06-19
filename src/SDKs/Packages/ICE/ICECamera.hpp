#ifndef SDKS_PACKAGES_ICE_ICECAMERA_HPP
#define SDKS_PACKAGES_ICE_ICECAMERA_HPP

// ============================================================================
// SDKs/Packages/ICE/ICECamera.hpp
//
// ICE::ICECamera -- the ICE (in-game cinematic engine) camera wrapper around the
// director-side BrnDirector::Camera::Camera. DWARF home
// references/DecFIGS/dwarfdump/SDKs/Packages/ICE/ICECamera.hpp:51.
//
// Layout (DWARF member order; the two pinned offsets come from the X360
// ICE::ICECamera::SetCameraMatrix asm @0x82531AC8):
//   ICEOverlay mICEOverlay;   // +0x00  (u32)
//   bool       mbShowOverlay; // +0x04
//   <pad to 16 -- mCamera is alignas(16)>
//   ICECamera::Camera mCamera;// +0x10  (== BrnDirector::Camera::Camera)
//
// The director camera's take transform sits at the camera's +0x00, landing the
// ICECamera transform at this+0x10; the camera's dirty-flags word sits at the
// camera's +0x140, landing the flags at this+0x150 -- exactly the offsets the asm
// pokes (matrix copy to this+0x10, `ld/ori 2/std` at this+0x150).
//
// `ICECamera::Camera` is the DWARF's `typedef Camera Camera` (ICECamera.hpp:39) --
// it imports the director camera, so mCamera is a BrnDirector::Camera::Camera.
//
// Only SetCameraMatrix has a reconstructed body (its TU, ICECamera.cpp). The rest
// of the method set is DECLARATION-ONLY -- the per-TU `cl /c` gate does not link,
// so the declarations are enough; each lands a body with its own ledger TU.
// ============================================================================

#include "types.hpp"
#include "rw/math/vpu/types.h"                       // rw::math::vpu::Matrix44Affine
#include "SDKs/Packages/ICE/ICEOverlays.hpp"         // ICE::ICEOverlay (by value)
#include "GameSource/Director/Camera/Camera.h"       // BrnDirector::Camera::Camera (by value)
#include <cstddef>                                   // offsetof

namespace ICE
{
    // DWARF: ICECamera.hpp:51 (struct ICE::ICECamera).
    struct ICECamera
    {
    public:
        // ICECamera.hpp:39: the director camera, imported under ICE's namespace.
        typedef BrnDirector::Camera::Camera Camera;

        // --- The X360-shaped methods (DWARF ICECamera.hpp). Declaration-only except
        //     SetCameraMatrix (reconstructed in ICECamera.cpp). ----------------------
        void Construct();                                                   // :53
        void Destruct();                                                    // :54
        Camera* GetCamera();                                                // :56
        const Camera* GetCamera() const;                                    // :57
        void SetCamera(Camera*);                                            // :58
        void ClearVelocity();                                               // :60
        void SetSimTimeMultiplier(f32);                                     // :62
        void SetFadeColor(f32, f32, f32, f32);                             // :69
        void SetLetterBox(f32);                                            // :71
        void SetDepthOfField(f32, f32, f32, f32, f32);                     // :78
        void SetFocalDistance(f32);                                        // :81
        void SetDOFFalloff(f32);                                           // :83
        void SetDOFMaxIntensity(f32);                                      // :85
        void SetFieldOfView(f32);                                          // :87
        void SetNearZ(f32);                                                // :89
        void SetNoiseAmplitude1(f32, f32, f32, f32);                       // :94
        void SetNoiseFrequency1(f32, f32, f32, f32);                       // :99
        void SetNoiseAmplitude2(f32, f32, f32, f32);                       // :104
        void SetNoiseFrequency2(f32, f32, f32, f32);                       // :109
        void SetBloom(f32, f32);                                           // :116
        void SetTargetDistance(f32);                                       // :120

        // ICECamera.hpp:123. Copies the take matrix into the director camera's
        // transform, revalidates it, and marks the camera dirty. Body: ICECamera.cpp.
        // Returns the validated-transform pointer (X360 asm forwards the
        // ValidateTransformWithDebugInfo result; see ICECamera.cpp note).
        rw::math::vpu::Matrix44Affine* SetCameraMatrix(const rw::math::vpu::Matrix44Affine& lrMatrix);

        void SetOverlay(s32);                                             // :127
        void ClearOverlay();                                              // :128
        void SetHideOverlay(bool);                                        // :130

    private:
        ICEOverlay mICEOverlay;     // ICECamera.hpp:133  (+0x00)
        bool       mbShowOverlay;   // ICECamera.hpp:134  (+0x04)
        Camera     mCamera;         // ICECamera.hpp:137  (+0x10, alignas(16))
    };

    // Pin the two X360-proven offsets (asm @0x82531AC8: matrix copy to this+0x10,
    // dirty-flags OR at this+0x150). The actual offsetof checks (which need private
    // access to mCamera) live in ICECamera.cpp inside the member function body.
}

#endif // SDKS_PACKAGES_ICE_ICECAMERA_HPP
