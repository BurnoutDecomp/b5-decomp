#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_TESTBED_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_TESTBED_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (spTestbed / handle checks)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateTestbed.h
//
// BrnDirector::ArbStateTestbed -- the developer "camera testbed" director arbitrator state. It is a
// debug-menu driven state: each director camera BEHAVIOUR (aftertouch, rig, heli, bystander, gyro,
// gameplay-external, failsafe, passenger, loose-attachment, fixed, ice-anim, rotate-about-vehicle,
// spiralling-deathcam, road-runner) can be "activated" from the debug component, and the testbed
// allocates that behaviour through the shared BehaviourManager, seeds its parameter block from the
// debug-menu serialiser, drives it, and prints the running time. It can also play an ICE-anim take
// directly (ActivateIceCam) and register every ICE-anim take in a shot-group as a debug-menu entry
// (RegisterIceAnimsWithDebugComponent). Derives from ArbitratorState (vtable order pinned by base).
//
// LAYOUT: the member NAMES + DWARF declaration order come from the DecFIGS DWARF
// (BrnArbStateTestbed.h, X360-attested for this build). The per-member X360 offsets are pinned from
// the ARTIST asm (Construct @0x82259AC0, Update @0x8226B638, Deactivate @0x821F61D8,
// GenericActivateCam @0x8226BD30, ActivateIceCam @0x82264040):
//   * mCamera is the base ArbitratorState's by-value Camera @+0x10 (this state reaches it by name
//     through the base GetNonConstCamera(); Construct calls Camera::Construct(this+0x10)).
//   * the fifteen BehaviourHandle<> members begin at +0x180 (the asm Construct zeroes the 5-word
//     blocks at +0x180.. -- e.g. mAftertouch @+0x180, mRigCam @+0x1A8, mHeliCam @+0x1BC,
//     mBystander @+0x1D0, mGyroCam @+0x1E4, mGameplayExternal @+0x1F8, mFailsafe @+0x20C,
//     mPassenger @+0x220, mLooseAttachment @+0x234, mFixedCam @+0x248, mIceCam @+0x25C,
//     mRotateAboutVehicleCam @+0x270, mSpirallingDeathCam @+0x284, mRoadRunner @+0x298). The
//     DWARF declares them in the order below; the Update jump-table activates them by parameter
//     type-tag.
//   * mpShotRef    @+0x2AC (Camera::ShotReference*; the activated debug-menu shot reference)
//   * mpParameters @+0x2B0 (Camera::Behaviour::Parameters*; the activated behaviour's param block;
//                  Update's `**(this+0x2B0)` type-tag word selects which behaviour to allocate)
//   * mpCamera     @+0x2B4 (const Camera*; the produced camera the active behaviour writes -- copied
//                  into mCamera each ACTIVE frame)
//   * mfRunningTime@+0x2B8 (f32; accumulated by the frame timestep each ACTIVE frame; printed)
//   * meState      @+0x2BC (EState; the testbed state machine value; the Update switch key. Note the
//                  X360 indexes meState as the dword at this+0x2BC == word index 175.)
//   * mbLoopIceMovies @+0x2C0 (bool; ActivateIceCam-only loop flag region)
//   * mbUseSlomo   @+0x2C1 (bool; the slow-motion toggle the ACTIVE frame flips when armed)
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the X360 4-byte-pointer offsets quoted
// above are provenance; on the x64 host the embedded Camera / handles widen, so absolute offsets
// shift -- the member ROLES are what is reproduced.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    class DebugComponent;                 // forward-decl-only (no reconstructed home; see ICEMoviePlayer.h)
    class DirectorResourceManager;        // complete: BrnDirectorResourceManager.h (consumers include it)

    namespace Camera
    {
        class BehaviourManager;
        class Behaviour;                  // Behaviour::Parameters / Behaviour::ShotReference live in the Camera TUs
        class BehaviourAftertouchCam;
        class BehaviourAftertouchCrash;
        class BehaviourRig;
        class BehaviourHeliCam;
        class BehaviourBystanderCam;
        class BehaviourGyroCam;
        class BehaviourGameplayExternal;
        class BehaviourFailsafe;
        class BehaviourPassengerCam;
        class BehaviourLooseAttachment;
        class BehaviourFixedCam;
        class BehaviourIceAnim;
        class BehaviourRotateAboutVehicle;
        class BehaviourSpirallingDeathcam;
        class BehaviourRoadRunner;
    }

    class ArbStateTestbed : public ArbitratorState
    {
    public:
        // DWARF EState (BrnArbStateTestbed.h:92). The testbed state machine; the Update switch key
        // (read as the dword this+0x2BC). Construct seeds 0 (INACTIVE); the activate helpers force 1
        // (GENERIC_PREPARE); Update's case-1 success edge stores 2 (GENERIC_UPDATE); Deactivate
        // forces 3 (RELEASING). Values are the X360 jump-table case indices / the immediates the asm
        // stores into meState.
        enum EState
        {
            E_STATE_INACTIVE        = 0,
            E_STATE_GENERIC_PREPARE = 1,
            E_STATE_GENERIC_UPDATE  = 2,
            E_STATE_RELEASING       = 3,

            E_NUM_STATES            = 4
        };

        // ---- a typed handle to a camera behaviour owned by the BehaviourManager ----------
        // The arbitrator/moment states each inline-duplicate this same 5-word layout as a nested
        // type (BrnArbStateCrashNav.h / BrnArbStateRankUp.h etc.); those nested copies are distinct
        // types and are left untouched. Block pinned from the Construct/Update asm:
        // mbAllocated(+0x00), muAllocationKey(+0x04), behaviour-lookup helper word(+0x08),
        // mpManager(+0x0C), mpBehaviour(+0x10).
        // FLAG: the +0x08 helper word's role is not fully recovered (modelled as an opaque
        //   behaviour-lookup helper index, as in the sibling arbitrator-state copies).
        template <typename TBehaviour>
        struct BehaviourHandle
        {
            BehaviourHandle()
                : mbAllocated(false), muAllocationKey(0), muHelperIndex(0),
                  mpManager(0), mpBehaviour(0) {}

            bool        IsAllocated() const { return mbAllocated; }
            TBehaviour* GetBehaviour() const
            {
                CGS_ASSERT(mbAllocated, "IsAllocated()");
                return mpBehaviour;
            }

            bool                      mbAllocated;     // +0x00
            u32                       muAllocationKey; // +0x04
            u32                       muHelperIndex;   // +0x08  FLAG: role not recovered (lookup helper)
            Camera::BehaviourManager* mpManager;       // +0x0C
            TBehaviour*               mpBehaviour;     // +0x10
        };

        // ---- ArbitratorState virtual overrides (X360 vtable order; see base) -------------
        // ONLY the three virtuals whose bodies live in THIS TU's X360 function set are overridden:
        //   Construct @0x82259AC0, Update @0x8226B638, GetName @0x821F61C8.
        // Prepare()/Release()/Destruct() are NOT in this TU's X360 function set (Update reaches
        // Prepare/Release through the vtable -- (*(*this+...))(this, info) -- so their bodies live in
        // other driver TUs); the base non-pure declarations are inherited, no override added here.
        // The DWARF also lists IsActive() / SetDebugComponent() / Destruct(); none are in this TU's
        // X360 function set and so are omitted.
        void        Construct() override;                            // @0x82259AC0
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;  // @0x8226B638  DECLARATION-ONLY (see .cpp)
        const char* GetName() const override;                        // @0x821F61C8

        // ---- testbed-specific entry points (the debug component drives these) --------------
        // Each clears the previously-activated behaviour's debug-menu params (UnregisterParameters)
        // and arms the requested behaviour / take, then forces GENERIC_PREPARE so the next Update
        // allocates it.
        //   GenericActivateCam @0x8226BD30 -- arm a generic behaviour from its parameter block.
        //   ActivateIceCam     @0x82264040 -- arm a direct ICE-anim take from its shot reference.
        //   Deactivate         @0x821F61D8 -- force RELEASING so the next Update tears the cam down.
        // The two activate helpers receive the same opaque blocks the members hold (DWARF types are
        // Camera::Behaviour::Parameters* and Camera::ShotReference* -- nested/typedef'd in the Camera
        // TUs, modelled as opaque void* here; the testbed only stows/forwards them by pointer).
        void GenericActivateCam(void* lpParameters);  // @0x8226BD30  (Camera::Behaviour::Parameters*)
        void ActivateIceCam(void* lpShotRef);         // @0x82264040  (Camera::ShotReference*)
        void Deactivate(void* lpUnused);              // @0x821F61D8

        // Register every ICE-anim take referenced by a shot-group as a debug-menu entry that calls
        // ActivateIceCam. DECLARATION-ONLY (see .cpp -- indexes the un-homed DebugComponent /
        // DirectorResourceManager aggregates).
        void RegisterIceAnimsWithDebugComponent(const void* lpShotGroup,         // const shotgroup*
                                                const DirectorResourceManager& lrResources,
                                                const char* lpcMenuPath);        // @0x8226BDB8

    private:
        // Push / pull the activated behaviour's parameter block to/from the debug-menu serialiser.
        // DECLARATION-ONLY (see .cpp -- both forward to the un-homed
        // SerialiseBehaviourParameters<DebugMenuSerialiser> template).
        void RegisterParameters();     // @0x82263F08
        void UnregisterParameters();   // @0x82263FA8

        // ---- members, DWARF order; X360 offsets in comments ------------------------------
        BehaviourHandle<Camera::BehaviourAftertouchCam>       mAftertouch;            // X360 +0x180
        BehaviourHandle<Camera::BehaviourAftertouchCrash>     mAftertouchCrash;       // X360 +0x194
        BehaviourHandle<Camera::BehaviourRig>                 mRigCam;                // X360 +0x1A8
        BehaviourHandle<Camera::BehaviourHeliCam>             mHeliCam;               // X360 +0x1BC
        BehaviourHandle<Camera::BehaviourBystanderCam>        mBystander;             // X360 +0x1D0
        BehaviourHandle<Camera::BehaviourGyroCam>             mGyroCam;               // X360 +0x1E4
        BehaviourHandle<Camera::BehaviourGameplayExternal>    mGameplayExternal;      // X360 +0x1F8
        BehaviourHandle<Camera::BehaviourFailsafe>            mFailsafe;              // X360 +0x20C
        BehaviourHandle<Camera::BehaviourPassengerCam>        mPassenger;             // X360 +0x220
        BehaviourHandle<Camera::BehaviourLooseAttachment>     mLooseAttachment;       // X360 +0x234
        BehaviourHandle<Camera::BehaviourFixedCam>            mFixedCam;              // X360 +0x248
        BehaviourHandle<Camera::BehaviourIceAnim>             mIceCam;                // X360 +0x25C
        BehaviourHandle<Camera::BehaviourRotateAboutVehicle>  mRotateAboutVehicleCam; // X360 +0x270
        BehaviourHandle<Camera::BehaviourSpirallingDeathcam>  mSpirallingDeathCam;    // X360 +0x284
        BehaviourHandle<Camera::BehaviourRoadRunner>          mRoadRunner;            // X360 +0x298

        // DWARF types: mpShotRef = Camera::ShotReference* (== const Attrib::RefSpec*);
        // mpParameters = Camera::Behaviour::Parameters*. Both are reached only as opaque pointers in
        // this TU's bodied functions (Construct nulls them; the activate helpers stow them; Update
        // -- which dereferences mpParameters' type-tag word -- is declaration-only). Modelled as
        // opaque void* to avoid forking the un-homed Camera::Behaviour::Parameters / ShotReference
        // slices into this header.
        void*                            mpShotRef;        // X360 +0x2AC  (Camera::ShotReference*)
        void*                            mpParameters;     // X360 +0x2B0  (Camera::Behaviour::Parameters*)
        const Camera::Camera*            mpCamera;         // X360 +0x2B4
        f32                              mfRunningTime;    // X360 +0x2B8
        EState                           meState;          // X360 +0x2BC
        bool                             mbLoopIceMovies;  // X360 +0x2C0
        bool                             mbUseSlomo;       // X360 +0x2C1
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_TESTBED_H
