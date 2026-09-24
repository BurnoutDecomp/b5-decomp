#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2 (rw::math::vpu) alias

// BrnDirector::Camera::Utils::Camera2DRotationController -- the debug/look "2D rotation"
// controller that maps a look-stick vector to a smoothed on-screen rotation angle. It
// holds the last normalised stick direction, a hold timer, and the accumulated rotation
// angle (degrees), easing toward the stick heading while pushed and back to centre once
// released past a minimum hold time. It also latches the look-back button for edge
// detection. DWARF home BrnCamera2DRotationController.h:49.
//
// This TU bodies Update @0x8220BFE0 (called by MainDirector::UpdateCameraBehavioursPreScene
// / PostScene and BehaviourManager::UpdateAllBehaviours). Construct and the const
// accessors (IsRotated / GetAdjustedStickVector / GetRawStickVector / GetRotationAngleDegs
// / GetRotationAngleRads / IsLookback / IsStartingLookbackThisFrame / IsEndingLookbackThisFrame)
// are their own ledger functions (declaration-only here).
//
// Offsets pinned by the X360 Update asm: mStickVector @+0x00 (lvx/stvx128 r31 -- a full
// 16-byte Vector2, alignas(16) {x,y,z,w}), the four f32 tunables/state @+0x10..+0x20
// (lfs/stfs displacements), the three bool flags @+0x24..+0x26 (lbz/stb). The four kf*
// class-scope constants are DWARF static const floats (h:52..55); their exact rodata
// values are consumed inside Update as 0.5 (dead-zone) and as the default blend/return/
// min-time seeds set by Construct (declaration-only here).
namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    struct Camera2DRotationController
    {
        // DWARF h:52..55 -- class-scope tuning defaults (declaration-only; defined in the
        // Construct/data TU). FLAG: exact rodata values not attested in this TU beyond the
        // 0.5 dead-zone radius that Update compares the stick magnitude against.
        static const f32 kfDefaultBlendFactor;        // h:52
        static const f32 kfDefaultReturnBlendFactor;  // h:53
        static const f32 kfDefaultMinRotationTime;    // h:54
        static const f32 kfDeadZoneRadius;            // h:55  (== 0.5, flt_82001DA0)

        // DWARF h:59 -- declaration-only (its own ledger function).
        void Construct();

        // @0x8220BFE0 (this TU, DWARF h:66 / cpp:55) -- fold this frame's look-stick input
        // into the smoothed rotation angle (see cpp).
        void Update(f32 lfTimestep, Vector2 lStick, bool lbLookback, bool lbPaused);

        // DWARF h:69..90 -- declaration-only (their own ledger functions).
        bool    IsRotated() const;
        Vector2 GetAdjustedStickVector() const;
        // BODIED INLINE (2026-09-24, FX-CAMRIG). No standalone symbol: its one inliner in the
        // DecFIGS line table is BehaviourAftertouchCrash::Update (PS3 0x5C794/0x5C79C, attributed
        // to this header's :134), and the X360 rig reads it as a plain 16-byte load of the shared
        // info's head -- `lvx128 v0, r0, r26` @0x822285B0, i.e. mStickVector at +0x00.
        Vector2 GetRawStickVector() const { return mStickVector; }                // +0x00
        f32     GetRotationAngleDegs() const;
        f32     GetRotationAngleRads() const;
        // BODIED INLINE (2026-09-16). The console has no standalone symbol for it: every
        // caller inlines the single byte fetch -- BehaviourGameplayBumper::Update
        // @0x82226778 reads it as `lbz r9, 0x26(r31)` straight off the shared info, and
        // 0x26 is this member at its asm-pinned offset. Same treatment, and the same
        // justification, as the sibling CameraSphericalRotationController::IsLookback.
        bool    IsLookback() const { return mbIsLookback; }                     // +0x26
        // The two edge tests, bodied from their only ARTIST reader that spells them out --
        // BehaviourGameplayBumper::Update @0x82227158..0x82227178, which tests the shared
        // controller's bytes directly (`lbz +0x26` / `lbz +0x25`): starting == lookback now
        // and not last frame; ending == not now and last frame. Declared here since
        // 2026-08-02, bodied 2026-09-17 when the bumper's flag tail landed.
        bool    IsStartingLookbackThisFrame() const { return mbIsLookback && !mbIsLookbackLastFrame; }
        bool    IsEndingLookbackThisFrame() const   { return !mbIsLookback && mbIsLookbackLastFrame; }

    private:
        Vector2 mStickVector;                 // +0x00 (h:95)  normalised held stick dir (16B)
        f32     mfTimeSinceRotation;          // +0x10 (h:96)
        f32     mfMinRotationTime;            // +0x14 (h:97)
        f32     mfRotationAngleDegs;          // +0x18 (h:98)  accumulated, wrapped (-180,180]
        f32     mfRotationBlendFactor;        // +0x1C (h:99)
        f32     mfRotationReturnBlendFactor;  // +0x20 (h:100)
        bool    mbIsRotated;                  // +0x24 (h:101)
        bool    mbIsLookbackLastFrame;        // +0x25 (h:102)
        bool    mbIsLookback;                 // +0x26 (h:103)
    };
}
}
}
