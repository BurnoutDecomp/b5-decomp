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
//   * Update -- a 15-way behaviour-allocation dispatch over the un-homed BehaviourManager pool and
//     the un-homed Camera::Behaviour::Parameters block (its leading type-tag word selects the
//     behaviour). Each arm calls BehaviourManager::NewBehaviour<TBehaviour> then resolves the live
//     behaviour through unrecovered manager-pool accessor thunks (the X360 sub_821Fxxxx helpers)
//     and seeds it via the behaviour's SetParameters. Reconstructing the body faithfully needs the
//     BehaviourManager pool layout + the per-behaviour resolve thunks, none of which are homed; a
//     body would have to fabricate them. Per the project's no-raw-offset-poke-into-an-un-homed-
//     aggregate + no-fabrication rules it is left declaration-only.
//   * RegisterParameters / UnregisterParameters -- both forward to
//     BrnDirector::Camera::SerialiseBehaviourParameters<BrnDirector::Camera::DebugMenuSerialiser>,
//     which has no reconstructed home (the serialiser template + the DebugMenuSerialiser visitor are
//     un-homed). Declaration-only until that TU lands.
//   * RegisterIceAnimsWithDebugComponent -- registers each shot-group ICE-anim take as a debug-menu
//     callback through the un-homed BrnDirector::DebugComponent (CgsDev::DebugComponent::
//     RegisterFunction) and indexes the un-homed DirectorResourceManager aggregate
//     (*(resources+560)+10064 / *(resources+544)). Declaration-only until DebugComponent lands.
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
    //   thunks (X360 sub_821Fxxxx), and seeds it via the behaviour's SetParameters. The
    //   BehaviourManager pool layout + those resolve thunks are NOT homed; the +0x2B0 parameter
    //   block is the un-homed Camera::Behaviour::Parameters. No faithful body without fabrication.

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
    //   BrnResource::ICEList::GetICETakeDataFromGuid (homed) indexed off the un-homed
    //   DirectorResourceManager aggregate (*(resources+560)+10064 / *(resources+544)), and registers
    //   each as a debug-menu callback bound to ActivateIceCam through the un-homed
    //   BrnDirector::DebugComponent (CgsDev::DebugComponent::RegisterFunction). The DebugComponent +
    //   DirectorResourceManager field layouts the asm indexes are not homed; declaration-only until
    //   they land.
}
