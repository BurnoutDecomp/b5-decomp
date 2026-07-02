#pragma once

// Home for BrnDirector::SharedCameraContainer -- the container of the two SHARED gameplay
// camera behaviours (the external "chase" cam and the in-car "bumper" cam) the Director
// arbitrator states hand off to between takes. DWARF home:
// GameSource/Director/Arbitrator/BrnDirectorArbitratorSharedCameraContainer.h:40 (this file
// predates that attribution; it stays the committed home -- extend, don't fork).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX, semantic-parity (not byte-matching):
//   SharedCameraContainer::GetGameplayCameraHelperIndex @0x82219718
//     (IDB symbol truncated to "GetGameplayCameraHe"; full name from the DWARF, h:61)
//   SharedCameraContainer::Prepare                      @0x82263D50
//     (ledger TU GameSource/Director/Arbitrator/BrnDirectorArbitratorSharedCameraContainer.cpp)
//
// Layout (DWARF h:90-94, X360-asm-attested offsets):
//   +0x00  bool mbUseGameplayExternal  -- lbz 0(this); pick the external cam when set
//   +0x01  bool mbLookbackOverride     -- lbz 1(this); when set, fall back to the bumper cam
//   +0x04  BehaviourHandle<BehaviourGameplayExternal> mGameplayExternal
//   +0x18  BehaviourHandle<BehaviourGameplayBumper>   mGameplayBumper
// (+0x18 - +0x04 == 0x14 == the console 5-word BehaviourHandle stride, cross-confirming the
// handle layout pinned in BrnBehaviourManager.h.) Members are reached BY NAME; the console
// handle offsets are not pinned on x64 (8-byte pointers shift them).

#include "types.hpp"
#include <cstddef>   // offsetof
#include "GameSource/Director/Camera/BrnBehaviourManager.h"  // Camera::BehaviourHandle / BehaviourHelperIndex
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h"    // Camera::BehaviourGameplayBumper
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"  // Camera::BehaviourGameplayExternal

namespace BrnDirector
{
    struct ArbStateSharedInfo;   // the arbitrator shared context (BrnDirectorArbitratorState.h)
    namespace Camera { struct Camera; }

    // BrnDirector::SharedCameraContainer (DWARF BrnDirectorArbitratorSharedCameraContainer.h:40).
    // Kept a struct with public members: the committed arbitrator consumers
    // (BrnDirectorArbitrator.cpp / BrnArbStateDriveThru.cpp) reproduce the X360's inlined
    // flag accesses as direct member reads/writes (the DWARF's Set/Is accessors are what got
    // inlined there).
    struct SharedCameraContainer
    {
        bool mbUseGameplayExternal;   // +0x00 (DWARF h:90) : lbz 0(this)
        bool mbLookbackOverride;      // +0x01 (DWARF h:91) : lbz 1(this)

        // The two shared gameplay camera behaviours (DWARF h:93/h:94).
        Camera::BehaviourHandle<Camera::BehaviourGameplayExternal> mGameplayExternal;  // X360 +0x04
        Camera::BehaviourHandle<Camera::BehaviourGameplayBumper>   mGameplayBumper;    // X360 +0x18

        // @0x82263D50 (this build's ledger TU BrnDirectorArbitratorSharedCameraContainer.cpp).
        // Allocate the two shared gameplay behaviours from the shared-info's manager, bind
        // each to its parameter block out of the manager's parameter bank, and mark both as
        // updating during pause. DWARF h:48 declares the param const-ref; the X360 mutates
        // the handles through it, matching the DWARF .cpp:45 non-const spelling used here.
        void Prepare(ArbStateSharedInfo& lrSharedInfo);

        // @0x82219718 (IDB-truncated symbol "GetGameplayCameraHe"; DWARF h:61). Select the
        // live gameplay camera behaviour's helper index: the external cam's when
        // mbUseGameplayExternal is set and not overridden by lookback, else the bumper cam's.
        // The X360 returns the 4-byte index object via a hidden sret pointer; by-value here.
        Camera::BehaviourHelperIndex GetGameplayCameraHelperIndex() const;

        // The Camera of the currently-selected gameplay behaviour (DWARF h:58 names this
        // GetGameplayCamera; committed consumers already call it under this name): pick the
        // external handle when mbUseGameplayExternal && !mbLookbackOverride, else the bumper,
        // then resolve that behaviour through its manager and return its camera (asserts the
        // handle is allocated). The X360 inlines it as the flag test + the two handle-resolve
        // accessors (sub_822122x). DECLARATION-ONLY (body lands with the SharedCameraContainer
        // TU).
        const Camera::Camera& GetSelectedGameplayCamera() const;

        // Force the EXTERNAL (primary) gameplay-camera behaviour to finish immediately so an
        // intro / transition camera can take over. The X360 (ArbStateRaceIntro::Update cases 1
        // and 3) resolves the external handle (this+0x04) to its behaviour via the manager,
        // then sets its remaining-time to FLT_MAX and raises its two "finished" flags
        // (behaviour-relative byte stores at +0x29E and +0xB5D). Those camera-behaviour
        // offsets belong to the gameplay-camera-behaviour TU, so this is exposed as a single
        // named operation here rather than poked by offset. DECLARATION-ONLY (body lands with
        // the SharedCameraContainer / gameplay-camera-behaviour TU).
        void ForcePrimaryGameplayBehaviourToFinish();
    };

    // Pin only the size-stable offsets the X360 asm proves (the two selection-flag bytes).
    // The handle offsets (X360 +0x04 / +0x18) are not pinned on x64, where 8-byte pointers
    // shift them; the fields are reached by name, so the absolute offset is not load-bearing.
    static_assert(offsetof(SharedCameraContainer, mbUseGameplayExternal) == 0x00, "use-external flag @+0x00");
    static_assert(offsetof(SharedCameraContainer, mbLookbackOverride)    == 0x01, "lookback-override flag @+0x01");
}
