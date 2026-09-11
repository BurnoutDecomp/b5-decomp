#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"                  // BrnDirector::Moment (base); pulls BrnBehaviourManager.h (BehaviourHandle / the BehaviourInterpolate slice)
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"             // Camera::BehaviourRig(::Parameters) + the Behaviour base slice
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourBystanderCam.h" // Camera::BehaviourBystanderCam(::Parameters)
#include "GameShared/GameClasses/Containers/CgsArray.h"                     // Array<T,N> (the collections' shot tables)
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourInterpolate.h"  // Camera::BehaviourInterpolate(::Parameters) -- mInterpolaterParams is held BY VALUE , so the class must be COMPLETE here; BrnBehaviourManager.h only forward-declares it , which is why this TU never compiled

// BrnDirector::MomentPlayerJumping - the "player jumping" camera moment: while
// the player is airborne off a jump, cut to one of several authored shot
// sequences (ICE rig / dropped rig / bystander / attached rig). Class shape /
// member names / method set / enums verbatim from the declarations
// (BrnMomentPlayerJumping.h plus the BehaviourCollection template);
// gated in the console build ledger. This TU bodies the eight exported
// functions; SetParameters/Destruct/UpdateCamera and every BehaviourCollection
// method are their own ledger functions (declaration-only here -- the declaration
// places the template bodies in this TU's .cpp, but they are separate exported
// instantiations, e.g. AddShot<Rig,6>, reconstructed with the
// collection's own claim).
//
namespace Attrib { struct RefSpec; }   // the ice collection's TParameters (Camera.h fwd-declares it too)

namespace BrnDirector
{
    namespace Camera { class BehaviourIceAnim; }   // template arg only (real home BrnBehaviourIceAnim.h is mutually exclusive with BehaviourRig.h's Behaviour slice)

    // BehaviourCollection<TBehaviour, TParameters, N> -- a small bank of up to N
    // authored "shots" (a typed behaviour handle + its parameter block each) an
    // owning moment/arbitrator-state allocates and drives as one unit. the declaration
    // home:  (members, methods).
    // ALL methods are DECLARATION-ONLY here: the declaration places the bodies in
    // BrnMomentPlayerJumping.cpp as template definitions, and their per-type
    // instantiations are their own console ledger functions (AddShot, Prepare, Release,
    // HasFailed, CanSwitchToMeNow, the shot-table Append -- three copies each, one per
    // instantiated collection) -- reconstructed with
    // the collection's own claim, not this moment TU.
    template <typename TBehaviour, typename TParameters, u32 N>
    class BehaviourCollection
    {
    public:
        void Construct(const ArbitratorState* lpArbStateOwner,
                       const Moment* lpMomentOwner);
        bool Prepare(Camera::BehaviourManager& lrBehaviourManager);
        bool Release();
        s32  GetNumShots() const;
        bool HasActiveCamera();
        bool CanSwitchToMeNow();
        bool CanSwitchFromMeNow();
        bool HasFailed();
        void AddShot(const TParameters* lpParams);
        Camera::BehaviourHandle<TBehaviour>& GetBehaviourHandle();
        TBehaviour& GetBehaviour();
        const Camera::Camera& GetCamera() const;

        // The shots-already-allocated latch (MomentPlayerJumping::Prepare's ice
        // loop asserts on it before each AddShot -- the console build inlines the read).
        // FLAG: accessor name inferred from that use; trivial by-name read.
        bool HasAllocatedShots() const { return mbAllocatedShots; }

    private:
        //  One authored shot: the typed handle + its params.
        struct Shot
        {
            Camera::BehaviourHandle<TBehaviour> mHandle;
            const TParameters*                  mpParams;
        };

        Array<Shot, N>         maShots;
        const ArbitratorState* mpArbStateOwner;
        const Moment*          mpMomentOwner;
        s32                    miCurrentCamera;
        bool                   mbHasCurrentCamera;
        bool                   mbAllocatedShots;
    };

    // BehaviourCollection<TBehaviour,TParameters,N>::GetCamera const
    // (declared). The console asserts carry this .h's source path
    // so the body genuinely lives in the header (unlike
    // HasActiveCamera whose asserts carry the .cpp). N=5 (mIceCollection) is the copy the
    // ledger forces.
    //
    // Console body: read mbAllocatedShots at +0x89 (assert), read mbHasCurrentCamera at
    //     +0x88 (assert), then maShots[miCurrentCamera] (Array<Shot,N>::operator[] const)
    // mHandle.GetProducedCamera(). GetProducedCamera itself carries the single IsAllocated()
    // assert plus the GetBehaviourSlotFromHandle resolve + deref,
    // so NO separate explicit IsAllocated assert is emitted here (that would double-fire).
    template <typename TBehaviour, typename TParameters, u32 N>
    inline const Camera::Camera&
    BehaviourCollection<TBehaviour, TParameters, N>::GetCamera() const
    {
        CGS_ASSERT(mbAllocatedShots, "mbAllocatedShots");
        CGS_ASSERT(mbHasCurrentCamera, "mbHasCurrentCamera");
        return maShots[static_cast<u32>(miCurrentCamera)].mHandle.GetProducedCamera();
    }

    class MomentPlayerJumping : public Moment
    {
    public:
        //  What GetCameraStatus reports about the active collection.
        enum EStatus
        {
            E_STATUS_WAITING = 0,
            E_STATUS_READY   = 1,
            E_STATUS_FAILED  = 2
        };

        //  Which authored sequence this jump is running.
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

        //  The (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            //  Its own ledger function (declaration-only).
            void Construct();
        };

        // the inlined base Construct, the
        // four collection Constructs, the interpolate handle clear + parameter
        // defaults (rotate-about-player-car / exponential-out-x-cubed), the
        // cooldown seed and the latch resets.
        virtual void Construct();

        // enter SEARCHING and, first time
        // only, load every collection's authored shots (rig/bystander blocks from
        // the parameter bank; up to five ice ShotList refs off the resource
        // manager's player-jumping shot group). Reports the prepared latch.
        virtual bool Prepare(void* lrBehaviourController);

        // the per-frame state machine
        // (see the .cpp).
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // zero the cooldown, release all
        // four collections + the interpolate handle, clear the gates, raise the
        // searching head bit, park at SEARCHING.
        virtual bool Release();

        //  Declaration-only (own ledger functions).
        virtual void Destruct();
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        virtual const char* GetName() const;

    protected:
        // E_MOMENT_PLAYER_JUMPING (7).
        virtual EType GetInstanceType();

    private:
        // Prepare the active sequence's
        // collection(s), asserting each Prepare. Always reports true.
        bool PrepareCameras(Camera::BehaviourManager& lrBehaviourManager);

        // the active collection's status:
        // FAILED when it failed, else READY once it can be switched to.
        EStatus GetCameraStatus();

        //  Fill the frame's camera from the active collection.
        // Declaration-only (its own ledger function; the ICF dump misattributes
        // the folded body to CgsArray.h).
        bool UpdateCamera(Camera::Camera& lrCamera);

        // Member layout (console offsets in comments; access BY NAME).
        const Parameters* mpParameters;                                              // +0x180
        BehaviourCollection<Camera::BehaviourBystanderCam,
                            Camera::BehaviourBystanderCam::Parameters, 2>
            mBystanderCollection;                                                    // +0x184
        BehaviourCollection<Camera::BehaviourRig,
                            Camera::BehaviourRig::Parameters, 4>
            mDroppedRigCollection;                                                   // +0x1C8
        BehaviourCollection<Camera::BehaviourRig,
                            Camera::BehaviourRig::Parameters, 6>
            mRigCollection;                                                          // +0x23C
        BehaviourCollection<Camera::BehaviourIceAnim, const Attrib::RefSpec, 5>
            mIceCollection;                                                          // +0x2E0
        Camera::BehaviourInterpolate::Parameters mInterpolaterParams;                // +0x36C
        Camera::BehaviourHandle<Camera::BehaviourInterpolate> mInterpolater;         // +0x37C
        f32           mfJumpTimer;                                                   // +0x390
        f32           mfCooldownTimer;                                               // +0x394
        bool          mbPrepared;                                                    // +0x398
        ESequenceType meType;                                                        // +0x39C (shadows the base's private meType, per the declaration)
    };
}
