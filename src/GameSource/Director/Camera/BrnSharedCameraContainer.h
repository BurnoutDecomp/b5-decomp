#pragma once

// Home for BrnDirector::SharedCameraContainer -- the container the Director arbitrator
// states query for the gameplay camera header. DWARF home (nominal):
// GameSource/Director/Camera/BrnSharedCameraContainer.h.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @0x82219718 (GetGameplayCameraHe),
// semantic-parity (not byte-matching). Called by ArbStateRoaming::Update,
// B3ClassicTakedownPlayer::{Update,Prepare}, ShutdownTakedownPlayer::Prepare and
// ArbStateCrashNav::Prepare to fetch whichever gameplay-camera header is currently live.
//
// Layout the asm proves:
//   +0x00  bool  mbPrimaryActive    -- lbz 0(this); first selection test
//   +0x01  bool  mbPrimarySuspended -- lbz 1(this); when set, fall back to the secondary
//   +0x04  ref   mPrimaryRef        -- handle to the primary gameplay camera header
//   +0x18  ref   mSecondaryRef      -- handle to the fallback (e.g. crash/takedown) header
// The selection picks the primary only when it is active AND not suspended; otherwise the
// secondary. Each ref is the BrnBehaviourManager Reference<> handle whose held pointer
// lives at handle+0x04 (the two callee accessors, sub_822122F0 / sub_822124A0, both assert
// IsAllocated() then copy that held pointer into the output) -- modelled here as the
// deref-by-name accessor so no raw-offset cast is needed.
//
// The header pointee and the wider container layout are owned by their own TUs; this home
// models only the named fields the asm touches, at the proven offsets, with offsetof
// static_asserts. Members are reached BY NAME.

#include "types.hpp"
#include <cstddef>   // offsetof

namespace BrnDirector
{
    namespace Camera { struct Camera; }

    // The gameplay-camera header the container hands out. Opaque to this TU -- the caller
    // receives a pointer to it; its layout belongs to the camera-header TU.
    struct GameplayCameraHeader;

    // The container's output holder: a single pointer to the selected header. The X360
    // accessor returns this object (r3) after writing the header pointer into its first
    // word.
    struct GameplayCameraHeaderRef
    {
        const GameplayCameraHeader* mpHeader;   // +0x00 : written by GetGameplayCameraHe
    };

    // A BrnBehaviourManager Reference<> handle (subset). The callee accessors assert the
    // held pointer is allocated, then return it; only the held pointer (X360 handle+0x04,
    // its 4-byte-pointer slot) is modelled here. mpHeld is a real pointer so the deref is
    // by-name and type-correct; its absolute x64 offset (8-byte pointer alignment) differs
    // from the X360 +0x04 -- not pinned (see note below).
    struct CameraHeaderHandle
    {
        unsigned char               maReserved0[0x04];  // +0x00 (X360) : handle book-keeping (own TU)
        const GameplayCameraHeader*  mpHeld;            // X360 +0x04 : the held header pointer
    };

    // BrnDirector::SharedCameraContainer (subset). Two live-state flags + the primary and
    // fallback header handles, at the offsets the X360 asm reads (X360 4-byte-pointer
    // layout; the byte-flag offsets are size-stable and pinned, the handle offsets are not
    // -- x64's 8-byte pointers shift them).
    struct SharedCameraContainer
    {
        bool                mbPrimaryActive;     // +0x00 : lbz 0(this)
        bool                mbPrimarySuspended;  // +0x01 : lbz 1(this)
        unsigned char       maReserved0[0x02];   // +0x02 : padding to the handle
        CameraHeaderHandle  mPrimaryRef;         // X360 +0x04
        unsigned char       maReserved1[0x10];   // intervening members (own TU)
        CameraHeaderHandle  mSecondaryRef;       // X360 +0x18

        // @0x82219718. Select the live gameplay-camera header: the primary handle when the
        // primary is active and not suspended, otherwise the fallback handle. Writes the
        // chosen (asserted-allocated) header pointer into lrOut and returns &lrOut.
        GameplayCameraHeaderRef* GetGameplayCameraHe(GameplayCameraHeaderRef& lrOut) const;

        // The Camera of the currently-selected gameplay behaviour: pick the primary handle
        // when the primary is active and not suspended, otherwise the secondary, then resolve
        // that behaviour through its manager and return its camera (asserts the handle is
        // allocated). This is the selection ArbStateRoaming::Prepare copies into mCamera; the
        // X360 inlines it as the primary/secondary test + the two handle-resolve accessors
        // (sub_822122x). DECLARATION-ONLY (body lands with the SharedCameraContainer TU).
        const Camera::Camera& GetSelectedGameplayCamera() const;
    };

    // Pin only the size-stable offsets the X360 asm proves (the two selection-flag bytes).
    // The handle offsets (X360 +0x04 / +0x18, both 4-byte-pointer slots) are not pinned on
    // x64, where 8-byte pointers and pointer alignment shift them; the fields are reached by
    // name, so the absolute offset is not load-bearing for correctness.
    static_assert(offsetof(SharedCameraContainer, mbPrimaryActive)    == 0x00, "primary-active flag @+0x00");
    static_assert(offsetof(SharedCameraContainer, mbPrimarySuspended) == 0x01, "primary-suspended flag @+0x01");
}
