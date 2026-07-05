#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"                  // BrnDirector::Moment (base); pulls BrnBehaviourManager.h (BehaviourHandle / the BehaviourInterpolate slice)
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"             // Camera::BehaviourRig(::Parameters) + the Behaviour base slice
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourBystanderCam.h" // Camera::BehaviourBystanderCam(::Parameters)
#include "GameShared/GameClasses/Containers/CgsArray.h"                     // Array<T,N> (the collections' shot tables)

// BrnDirector::MomentPlayerJumping - the "player jumping" camera moment: while
// the player is airborne off a jump, cut to one of several authored shot
// sequences (ICE rig / dropped rig / bystander / attached rig). Class shape /
// member names / method set / enums verbatim from the DecFIGS DWARF
// (BrnMomentPlayerJumping.h:134/:173-:225 + the BehaviourCollection template
// @h:41); gated on the X360 ledger. This TU bodies the eight exported
// functions; SetParameters/Destruct/UpdateCamera and every BehaviourCollection
// method are their own ledger functions (declaration-only here -- the DWARF
// places the template bodies in this TU's .cpp, but they are separate exported
// instantiations, e.g. AddShot<Rig,6> @0x8224B1B8, reconstructed with the
// collection's own claim).
//
// NOTE (mutual exclusion, the MomentHitTraffic precedent): BrnMomentSubclasses.h
// carries a layout-stubbed MomentPlayerJumping for the NewMoment TU; this real
// home and that stub define the same class and CANNOT share a TU.
namespace Attrib { struct RefSpec; }   // the ice collection's TParameters (Camera.h fwd-declares it too)

namespace BrnDirector
{
    namespace Camera { class BehaviourIceAnim; }   // template arg only (real home BrnBehaviourIceAnim.h is mutually exclusive with BehaviourRig.h's Behaviour slice)

    // ------------------------------------------------------------------------
    // BehaviourCollection<TBehaviour, TParameters, N> -- a small bank of up to N
    // authored "shots" (a typed behaviour handle + its parameter block each) an
    // owning moment/arbitrator-state allocates and drives as one unit. DWARF
    // home: BrnMomentPlayerJumping.h:41 (members h:114-:121, methods h:50-:95).
    // ALL methods are DECLARATION-ONLY here: the DWARF places the bodies in
    // BrnMomentPlayerJumping.cpp as template definitions, and their per-type
    // instantiations are their own X360 ledger functions (AddShot @0x8224B1B8/
    // 0x8224B0A8/0x8224B130, Prepare @0x82273360/0x82267D88/0x82267ED0/
    // 0x82267C40, Release @0x8222DF20/0x8222DDB0/0x8222DE68/0x8222E030,
    // HasFailed @0x821FD5B8/0x821FD118/0x821FD238/0x821FD358, CanSwitchToMeNow
    // @0x821FD2C8.., the shot-table Append @0x8222FBE8) -- reconstructed with
    // the collection's own claim, not this moment TU.
    // ------------------------------------------------------------------------
    template <typename TBehaviour, typename TParameters, u32 N>
    class BehaviourCollection
    {
    public:
        void Construct(const ArbitratorState* lpArbStateOwner,
                       const Moment* lpMomentOwner);                     // h:50
        bool Prepare(Camera::BehaviourManager& lrBehaviourManager);     // h:54
        bool Release();                                                  // h:57
        s32  GetNumShots() const;                                        // h:60
        bool HasActiveCamera();                                          // h:63
        bool CanSwitchToMeNow();                                         // h:66
        bool CanSwitchFromMeNow();                                       // h:69
        bool HasFailed();                                                // h:72
        void AddShot(const TParameters* lpParams);                       // h:76
        Camera::BehaviourHandle<TBehaviour>& GetBehaviourHandle();       // h:79
        TBehaviour& GetBehaviour();                                      // h:87
        const Camera::Camera& GetCamera() const;                         // h:95

        // The shots-already-allocated latch (MomentPlayerJumping::Prepare's ice
        // loop asserts on it before each AddShot -- the X360 inlines the read).
        // FLAG: accessor name inferred from that use; trivial by-name read.
        bool HasAllocatedShots() const { return mbAllocatedShots; }

    private:
        // DWARF h:106-:108 -- one authored shot: the typed handle + its params.
        struct Shot
        {
            Camera::BehaviourHandle<TBehaviour> mHandle;   // h:107
            const TParameters*                  mpParams;  // h:108
        };

        Array<Shot, N>         maShots;            // h:114
        const ArbitratorState* mpArbStateOwner;    // h:115
        const Moment*          mpMomentOwner;      // h:116
        s32                    miCurrentCamera;    // h:118
        bool                   mbHasCurrentCamera; // h:119
        bool                   mbAllocatedShots;   // h:121
    };

    // BehaviourCollection<TBehaviour,TParameters,N>::GetCamera() const @ X360 0x8222CF38
    // (declared h:60/h:95). The X360 asserts carry the .h source path
    // (BrnMomentPlayerJumping.h:89/90), so the body genuinely lives in the header (unlike
    // HasActiveCamera whose asserts carry the .cpp). N=5 (mIceCollection) is the copy the
    // ledger forces.
    //
    // asm 0x8222CF38: lbz mbAllocatedShots@+0x89 (assert :89), lbz mbHasCurrentCamera@+0x88
    // (assert :90), then maShots[miCurrentCamera] (Array<Shot,N>::operator[] const) ->
    // mHandle.GetProducedCamera(). GetProducedCamera itself carries the single IsAllocated()
    // assert (BrnBehaviourManager.h:589) plus the GetBehaviourSlotFromHandle resolve + deref,
    // so NO separate explicit IsAllocated assert is emitted here (that would double-fire).
    template <typename TBehaviour, typename TParameters, u32 N>
    inline const Camera::Camera&
    BehaviourCollection<TBehaviour, TParameters, N>::GetCamera() const
    {
        CGS_ASSERT(mbAllocatedShots, "mbAllocatedShots");       // BrnMomentPlayerJumping.h:89
        CGS_ASSERT(mbHasCurrentCamera, "mbHasCurrentCamera");   // BrnMomentPlayerJumping.h:90
        return maShots[static_cast<u32>(miCurrentCamera)].mHandle.GetProducedCamera();
    }

    // DWARF: BrnMomentPlayerJumping.h:134.
    class MomentPlayerJumping : public Moment
    {
    public:
        // DWARF h:197 -- what GetCameraStatus reports about the active collection.
        enum EStatus
        {
            E_STATUS_WAITING = 0,
            E_STATUS_READY   = 1,
            E_STATUS_FAILED  = 2
        };

        // DWARF h:211 -- which authored sequence this jump is running.
        enum ESequenceType
        {
            E_INVALID_TYPE                                = -1,
            E_TYPE_ICE_RIG                                = 0,
            E_TYPE_DROPPED_RIG_THEN_ATTACHED_RIG_SEQUENCE = 1,
            E_TYPE_BYSTANDER_THEN_ATTACHED_RIG_SEQUENCE   = 2,
            E_TYPE_BYSTANDER_SHOT                         = 3,
            E_TYPE_DROPPED_RIG_SHOT                       = 4,
            E_TYPE_ATTACHED_RIG_SHOT                      = 5,
            E_NUM_TYPES                                   = 6
        };

        // DWARF h:235 -- the (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            // DWARF h:239 -- its own ledger function (declaration-only).
            void Construct();
        };

        // @0x8225F1F0 (this TU, DWARF cpp:177) -- the inlined base Construct, the
        // four collection Constructs, the interpolate handle clear + parameter
        // defaults (rotate-about-player-car / exponential-out-x-cubed), the
        // cooldown seed and the latch resets.
        virtual void Construct();

        // @0x82251048 (this TU, DWARF cpp:209) -- enter SEARCHING and, first time
        // only, load every collection's authored shots (rig/bystander blocks from
        // the parameter bank; up to five ice ShotList refs off the resource
        // manager's player-jumping shot group). Reports the prepared latch.
        virtual bool Prepare(void* lrBehaviourController);

        // @0x82275988 (this TU, DWARF cpp:261) -- the per-frame state machine
        // (see the .cpp).
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // @0x8223AED0 (this TU, DWARF cpp:680) -- zero the cooldown, release all
        // four collections + the interpolate handle, clear the gates, raise the
        // searching head bit, park at SEARCHING.
        virtual bool Release();

        // DWARF cpp:705 / cpp:719 -- declaration-only (own ledger functions).
        virtual void Destruct();
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        // @0x821F7630 (this TU, DWARF cpp:745).
        virtual const char* GetName() const;

    protected:
        // @0x821F7628 (this TU, DWARF cpp:731) -- E_MOMENT_PLAYER_JUMPING (7).
        virtual EType GetInstanceType();

    private:
        // @0x822755A0 (this TU, DWARF cpp:410) -- Prepare the active sequence's
        // collection(s), asserting each Prepare. Always reports true.
        bool PrepareCameras(Camera::BehaviourManager& lrBehaviourManager);

        // @0x8220A358 (this TU, DWARF cpp:465) -- the active collection's status:
        // FAILED when it failed, else READY once it can be switched to.
        EStatus GetCameraStatus();

        // DWARF cpp:555 -- fill the frame's camera from the active collection.
        // Declaration-only (its own ledger function; the ICF dump misattributes
        // the folded body to CgsArray.h).
        bool UpdateCamera(Camera::Camera& lrCamera);

        // DWARF member layout (h:173-:225; X360 offsets in comments; access BY NAME).
        const Parameters* mpParameters;                                              // h:173 +0x180
        BehaviourCollection<Camera::BehaviourBystanderCam,
                            Camera::BehaviourBystanderCam::Parameters, 2>
            mBystanderCollection;                                                    // h:180 +0x184
        BehaviourCollection<Camera::BehaviourRig,
                            Camera::BehaviourRig::Parameters, 4>
            mDroppedRigCollection;                                                   // h:181 +0x1C8
        BehaviourCollection<Camera::BehaviourRig,
                            Camera::BehaviourRig::Parameters, 6>
            mRigCollection;                                                          // h:182 +0x23C
        BehaviourCollection<Camera::BehaviourIceAnim, const Attrib::RefSpec, 5>
            mIceCollection;                                                          // h:183 +0x2E0
        Camera::BehaviourInterpolate::Parameters mInterpolaterParams;                // h:185 +0x36C
        Camera::BehaviourHandle<Camera::BehaviourInterpolate> mInterpolater;         // h:186 +0x37C
        f32           mfJumpTimer;                                                   // h:188 +0x390
        f32           mfCooldownTimer;                                               // h:189 +0x394
        bool          mbPrepared;                                                    // h:191 +0x398
        ESequenceType meType;                                                        // h:225 +0x39C (shadows the base's private meType, per the DWARF)
    };
}
