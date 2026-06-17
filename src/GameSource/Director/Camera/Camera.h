#pragma once

// MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout
// reconstructed by BrnDirector::Camera::Camera's own TU (DWARF home
// GameSource/Director/Camera/Camera.h:40). Size 256 (NOMINAL -- not byte-verified,
// grown by own TU).
//
// Opaque camera payload embedded BY VALUE in RaceCarEntityModuleIO::
// InputBuffer_PreScene (mCameraInput, IO header :255) and InputBuffer_
// GenerateDispatchLists (mCameraInput, IO header :647). The IO header only returns
// &member / copies into member (Get/SetCameraInput, the const* getter returns
// &mCameraInput), so a complete sized blob suffices here. The IO buffers reference
// this as the fully-qualified BrnDirector::Camera::Camera.
//
// DWARF (Camera.h:40) gives the full member list (abbreviated):
//   Matrix44Affine mTransform; Vector3 mSubject; const Behaviour* mpDebugInfoBehaviour;
//   ShotReference* mpSourceShot; float32_t mfFOV/mfAspectRatio/mfRunningTime;
//   const CrashAnalysis* mpCrashAnalysis; CameraEffects mEffects; DepthOfField mDepthOfField;
//   CameraState mState; float32_t mfCustomNearClipDistance; ShotSelectionInfo mShotSelectionInfo;
//   bool mbHasSubject; bool mbHasCustomNearClipDistance;
// CameraEffects/DepthOfField/CameraState/ShotSelectionInfo are Director-internal
// cascades, so collapsed to an opaque reserved blob here; the real members land
// with this type's own ledger TU.
//
// alignas(16): the DWARF shows it carries Matrix44Affine + Vector3 (SIMD).

#include "types.hpp"   // reserved blob

namespace BrnDirector
{
    namespace Camera
    {
        // DWARF: Camera.h:40 (struct BrnDirector::Camera::Camera). The director's
        // per-frame camera state (transform, FOV, effects, shot selection).
        struct alignas(16) Camera
        {
            unsigned char maReserved[256]; // NOMINAL -- full layout grown by own TU
        };
    }
}
