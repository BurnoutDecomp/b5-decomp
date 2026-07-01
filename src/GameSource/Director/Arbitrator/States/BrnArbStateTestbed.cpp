#include "GameSource/Director/Arbitrator/States/BrnArbStateTestbed.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT (spTestbed / state asserts)
#include "GameSource/Director/Camera/Camera.h"                              // Camera::Camera (base mCamera complete)

// ============================================================================
// BrnDirector::ArbStateTestbed -- reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity)
//   Construct                           @0x82259AC0   (bodied)
//   GetName                             @0x821F61C8   (bodied)
//   Deactivate                          @0x821F61D8   (bodied)
//   GenericActivateCam                  @0x8226BD30   (bodied)
//   ActivateIceCam                      @0x82264040   (bodied)
//   UnregisterParameters                @0x82263FA8   (DECLARATION-ONLY -- FLAG)
//   RegisterParameters                  @0x82263F08   (DECLARATION-ONLY -- FLAG)
//   Update                              @0x8226B638   (DECLARATION-ONLY -- FLAG)
//   RegisterIceAnimsWithDebugComponent  @0x8226BDB8   (DECLARATION-ONLY -- FLAG)
//
// The developer camera testbed director state. Construct/GetName/Deactivate/GenericActivateCam/
// ActivateIceCam are reconstructed faithfully (they only touch this state's OWN members + the
// shared spTestbed singleton). The remaining four functions index aggregates whose LAYOUT is NOT
// homed in the committed tree and are therefore DECLARATION-ONLY (see the per-function FLAG blocks):
//   * Update -- a 15-way behaviour-allocation dispatch over the BehaviourManager pool and
//     the un-homed Camera::Behaviour::Parameters block (its leading type-tag word selects the
//     behaviour). RE-VERIFIED (2026-07-01): BrnBehaviourManager.h is now a real layout home and
//     BehaviourManager::NewBehaviour<T> is declared there, BUT every arm also calls a live-behaviour
//     RESOLVE thunk after allocation (X360 sub_821FCFB8 / BehaviourManager::BehaviourHan / ::BehaviourH
//     / ::BehaviourHandle_cl / etc. -- truncated/mangled, one per behaviour type) that is still
//     [todo]/unresolved (confirmed via `work stubs --list`: 15 NewBehaviour<T> instantiations + the
//     resolve thunks + several behaviour SetParameters overloads -- BehaviourRig, BehaviourHeliCam,
//     BehaviourBystanderCam, BehaviourGyroCam, BehaviourFailsafe, BehaviourPassengerCam,
//     BehaviourFixedCam, BehaviourRotateAboutVehicle, BehaviourLooseAttachment, BehaviourRoadRunner --
//     are all still unresolved; only Aftertouch/AftertouchCrash/SpirallingDeathcam/GameplayExternal
//     SetParameters are RECOVERED). It also needs SerialiseBehaviourParameters<DebugMenuSerialiser>
//     (see below, still un-homed) via RegisterParameters. A body would have to fabricate the resolve
//     thunks; per the project's no-fabrication rule it stays declaration-only.
//   * RegisterParameters / UnregisterParameters -- both forward to
//     BrnDirector::Camera::SerialiseBehaviourParameters<BrnDirector::Camera::DebugMenuSerialiser>,
//     which has no reconstructed home (the serialiser template + the DebugMenuSerialiser visitor are
//     un-homed -- RE-VERIFIED 2026-07-01, no hits anywhere under b5-decomp/src). Declaration-only
//     until that TU lands.
//   * RegisterIceAnimsWithDebugComponent -- registers each shot-group ICE-anim take as a debug-menu
//     callback through the un-homed BrnDirector::DebugComponent (CgsDev::DebugComponent::
//     RegisterFunction) and indexes the un-homed... wait, DirectorResourceManager IS homed
//     (BrnDirectorResourceManager.h) but the two fields the asm indexes off it
//     (*(resources+560)+10064 -> an ICEAuthor take-edit table, *(resources+544) -> the ICEList) are
//     not among its reconstructed named members yet, so they cannot be reached by name. RE-VERIFIED
//     (2026-07-01): BrnDirector::DebugComponent is STILL only forward-declared (no .cpp/.h home
//     anywhere in b5-decomp/src -- confirmed distinct from the unrelated CgsDev::DebugComponent and
//     BrnSound::DebugComponent, which do exist). CgsSceneManager::CgsCollision::
//     BaseCollisionGenerator::Destruct and Attrib::Attribute::GetLength are also still [todo]
//     (confirmed via `work stubs --list`). Declaration-only until DebugComponent (and the two
//     collision/attrib callees) land.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    // ------------------------------------------------------------------------
    // spTestbed -- the testbed singleton (X360 dword_..., set by Construct to `this`; the static
    // activate/deactivate helpers assert it and poke the live instance through it). It is file-local
    // shared state; the X360 stores `this` into it in Construct and never clears it.
    // ------------------------------------------------------------------------
    ArbStateTestbed* spTestbed = 0;

    // ------------------------------------------------------------------------
    // Construct @0x82259AC0 -- build the base camera, then zero every behaviour handle, the activated
    // shot/parameter/camera pointers, the running time, the state machine, and the two flag bytes.
    // (The X360 store range is +0x170..+0x2C1; it also records `this` into spTestbed.)
    // ------------------------------------------------------------------------
    void ArbStateTestbed::Construct()
    {
        GetNonConstCamera().Construct();   // X360 Camera::Construct(this+0x10)

        ResetBaseCameraFlags();            // X360 stb 0, +0x170 / +0x171 (mbDebugDisplayActive / mbCycleCameraThisFrame)

        spTestbed = this;                  // X360 dword_... = a1

        // The fifteen behaviour handles start unallocated (the 5-word blocks at +0x180.. zeroed).
        mAftertouch            = BehaviourHandle<Camera::BehaviourAftertouchCam>();
        mAftertouchCrash       = BehaviourHandle<Camera::BehaviourAftertouchCrash>();
        mRigCam                = BehaviourHandle<Camera::BehaviourRig>();
        mHeliCam               = BehaviourHandle<Camera::BehaviourHeliCam>();
        mBystander             = BehaviourHandle<Camera::BehaviourBystanderCam>();
        mGyroCam               = BehaviourHandle<Camera::BehaviourGyroCam>();
        mGameplayExternal      = BehaviourHandle<Camera::BehaviourGameplayExternal>();
        mFailsafe              = BehaviourHandle<Camera::BehaviourFailsafe>();
        mPassenger             = BehaviourHandle<Camera::BehaviourPassengerCam>();
        mLooseAttachment       = BehaviourHandle<Camera::BehaviourLooseAttachment>();
        mFixedCam              = BehaviourHandle<Camera::BehaviourFixedCam>();
        mIceCam                = BehaviourHandle<Camera::BehaviourIceAnim>();
        mRotateAboutVehicleCam = BehaviourHandle<Camera::BehaviourRotateAboutVehicle>();
        mSpirallingDeathCam    = BehaviourHandle<Camera::BehaviourSpirallingDeathcam>();
        mRoadRunner            = BehaviourHandle<Camera::BehaviourRoadRunner>();

        mpShotRef      = 0;                 // +0x2AC = 0
        mpParameters   = 0;                 // +0x2B0 = 0
        mpCamera       = 0;                 // +0x2B4 = 0
        mfRunningTime  = 0.0f;              // +0x2B8 = 0.0
        meState        = E_STATE_INACTIVE;  // +0x2BC = 0

        mbLoopIceMovies = false;           // +0x2C0 = 0
        mbUseSlomo      = false;           // +0x2C1 = 0
    }

    // ------------------------------------------------------------------------
    // GetName @0x821F61C8
    // ------------------------------------------------------------------------
    const char* ArbStateTestbed::GetName() const
    {
        return "ArbStateTestbed";
    }

    // ------------------------------------------------------------------------
    // Deactivate @0x821F61D8 -- force the testbed into RELEASING so the next Update tears the active
    // camera down. Asserts the singleton then writes meState through it (X360 spTestbed[175] = 3).
    // ------------------------------------------------------------------------
    void ArbStateTestbed::Deactivate(void* /*lpUnused*/)
    {
        CGS_ASSERT(spTestbed != 0, "spTestbed != NULL");
        spTestbed->meState = E_STATE_RELEASING;   // +0x2BC = 3
    }

    // ------------------------------------------------------------------------
    // GenericActivateCam @0x8226BD30 -- arm a generic camera behaviour from its parameter block:
    // clear any previously-armed behaviour's debug-menu params, stow the parameter block (clearing
    // the ICE shot ref), and force GENERIC_PREPARE so the next Update allocates it. Asserts the
    // singleton and pokes it through spTestbed (X360 spTestbed[172] = a1 / [171] = 0 / [175] = 1).
    // ------------------------------------------------------------------------
    void ArbStateTestbed::GenericActivateCam(void* lpParameters)
    {
        CGS_ASSERT(spTestbed != 0, "spTestbed != NULL");
        UnregisterParameters();

        spTestbed->mpParameters = lpParameters;        // +0x2B0 = a1  (word index 172)
        spTestbed->mpShotRef    = 0;                   // +0x2AC = 0   (word index 171)
        spTestbed->meState      = E_STATE_GENERIC_PREPARE;  // +0x2BC = 1
    }

    // ------------------------------------------------------------------------
    // ActivateIceCam @0x82264040 -- arm a direct ICE-anim take from its shot reference: clear any
    // previously-armed behaviour's debug-menu params, stow the shot reference (clearing the generic
    // parameter block), and force GENERIC_PREPARE. Asserts the singleton and pokes it through
    // spTestbed (X360 spTestbed[171] = a1 / [172] = 0 / [175] = 1).
    // ------------------------------------------------------------------------
    void ArbStateTestbed::ActivateIceCam(void* lpShotRef)
    {
        CGS_ASSERT(spTestbed != 0, "spTestbed != NULL");
        UnregisterParameters();

        spTestbed->mpShotRef    = lpShotRef;           // +0x2AC = a1  (word index 171)
        spTestbed->mpParameters = 0;                   // +0x2B0 = 0   (word index 172)
        spTestbed->meState      = E_STATE_GENERIC_PREPARE;  // +0x2BC = 1
    }

    // ========================================================================
    // DECLARATION-ONLY functions (FLAG: bodies index aggregates that are NOT homed in the committed
    // tree). The declarations are emitted so the class is complete and links against the future
    // homes; reconstructing the bodies now would require fabricating un-homed layouts / accessors,
    // which the project's no-fabrication rule forbids. See the file header for the per-function
    // rationale.
    // ========================================================================

    // Update @0x8226B638 -- DECLARATION-ONLY.
    //   FLAG: a 15-way behaviour-allocation dispatch (switch on meState, then on the activated
    //   Camera::Behaviour::Parameters type-tag word) that calls BehaviourManager::NewBehaviour<T>
    //   into each owned handle, resolves the live behaviour through unrecovered manager-pool accessor
    //   thunks (X360 sub_821Fxxxx / BehaviourManager::BehaviourHan / ::BehaviourH / etc.), and seeds
    //   it via the behaviour's SetParameters. RE-VERIFIED (2026-07-01): BrnBehaviourManager.h now
    //   homes the BehaviourManager pool LAYOUT + declares NewBehaviour<T>, but the per-behaviour
    //   resolve thunks are still [todo]/unrecovered, as are most of the per-behaviour SetParameters
    //   overloads this switch needs (only Aftertouch/AftertouchCrash/SpirallingDeathcam/
    //   GameplayExternal are RECOVERED) and SerialiseBehaviourParameters<DebugMenuSerialiser> (via
    //   RegisterParameters). The +0x2B0 parameter block is the un-homed Camera::Behaviour::Parameters.
    //   No faithful body without fabrication.

    // RegisterParameters @0x82263F08 -- DECLARATION-ONLY.
    //   FLAG: forwards to BrnDirector::Camera::SerialiseBehaviourParameters<DebugMenuSerialiser>
    //   (register pass: selector {0,0,0}) -- the serialiser template + the DebugMenuSerialiser
    //   visitor are un-homed. Asserts mpParameters + spDebugComponent first (both un-homed gates).

    // UnregisterParameters @0x82263FA8 -- DECLARATION-ONLY.
    //   FLAG: the complement of RegisterParameters (unregister pass: selector {1,0,0}); same
    //   un-homed SerialiseBehaviourParameters<DebugMenuSerialiser> dependency. Early-outs when no
    //   parameter block is armed.

    // RegisterIceAnimsWithDebugComponent @0x8226BDB8 -- DECLARATION-ONLY.
    //   FLAG: walks the shot-group's ICE-anim ShotList (Attrib::Instance / Attrib::Gen::iceanim --
    //   homed), resolves each take through ICE::ICEAuthor::FindEditedTakeFromGuid /
    //   BrnResource::ICEList::GetICETakeDataFromGuid (homed) indexed off two fields
    //   (*(resources+560)+10064 / *(resources+544)) that are not yet among
    //   BrnDirector::DirectorResourceManager's reconstructed named members (the aggregate itself IS
    //   homed -- BrnDirectorResourceManager.h -- but not these two fields), and registers each as a
    //   debug-menu callback bound to ActivateIceCam through the un-homed BrnDirector::DebugComponent
    //   (CgsDev::DebugComponent::RegisterFunction). RE-VERIFIED (2026-07-01): BrnDirector::
    //   DebugComponent is still forward-declared only, no reconstructed home anywhere in
    //   b5-decomp/src (distinct from CgsDev::DebugComponent and BrnSound::DebugComponent, which DO
    //   exist). CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct and
    //   Attrib::Attribute::GetLength are also still [todo]. Declaration-only until DebugComponent
    //   (+ those two callees + the two DirectorResourceManager fields) land.
}
