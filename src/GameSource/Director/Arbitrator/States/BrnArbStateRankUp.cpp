#include "GameSource/Director/Arbitrator/States/BrnArbStateRankUp.h"

#include "types.hpp"
#include <cstring>                                                              // std::strcmp (Checkpoint effect-name compare)
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT (rank-up asserts)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h" // ArbitratorStateContainer (hand-off)
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"            // BrnDirector::GameState (rank-up flags)
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"                 // Camera::RequestStartEffectHook / EffectInterface
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"          // Camera::BehaviourIceAnim + DirectorResourceManager slice
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"                        // Camera::BehaviourManager (complete)
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"                   // Attrib::Gen::shotgroup + Attrib::DefaultDataArea

// ============================================================================
// BrnDirector::ArbStateRankUp -- reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity)
//   Construct  @0x8225B2B8
//   GetName    @0x821F6710
//   Prepare    @0x82270EE8
//   Update     @0x82236380
//   Release    @0x82236650
//
// The director's "rank up" arbitrator state. On Prepare it allocates an ICE-anim camera
// behaviour to play the rank-up shot-group, anchoring it to the current rival's car. Update
// walks the rank-up state machine, copying the behaviour's produced camera into the state's
// own camera each frame; while the game is cycling rivals it advances the rival index and
// swaps the take to that rival's shot (rewinding the controller to the take start). Once the
// take has finished (and the rank-up is no longer driving), it either requests the
// "Checkpoint" camera effect or hands control back to the roaming state.
//
// All member access is BY NAME. The three rank-up control fields it reacts to live in the
// GameState snapshot's trailing sub-object region, whose DecFIGS DWARF field layout does not
// line up with the byte/word offsets the asm uses (documented on BrnDirectorGameState.h); they
// are reached through the named GameState accessors IsRankUpIntroRunning() /
// IsNewRankUpRivalThisFrame() / GetRankUpRivalRaceCarIndex() (the owning type encapsulates the
// documented-offset reads), so this consumer never reinterprets the blob itself.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace
    {
        // The default attrib data-area size requested when a shot's parameter block is absent
        // (X360 li r3, 0x18 -> Attrib::DefaultDataArea(0x18)). Same as the race-intro state.
        const u32 KU_SHOT_DEFAULT_DATA_AREA_SIZE = 0x18u;

        // The two trailing arguments the BehaviourManager::NewBehaviour<TBehaviour> allocation
        // request carries (X360 li r6,0 / li r7,1). RETYPED 2026-07-29: r6 is NewBehaviour's
        // OWNER slot -- a `const void*` (the arbitrator states pass null there; the moments pass
        // their Moment*) -- and r7 is the s32 reference LIMIT. Declaring the owner as `const s32`
        // meant the call matched no overload at all: a `const s32` variable is not an integer
        // LITERAL, so it is not a null-pointer constant and will not convert to `const void*`.
        // (That is the Step-0 defect the previous wave recorded as "one of the two call sites'
        // arg lists is wrong" -- it was the TYPE, not the count.)
        const void* const KPC_NEW_BEHAVIOUR_OWNER   = 0;
        const s32         KI_NEW_BEHAVIOUR_REFLIMIT = 1;

        // The dirty-flag bit Update raises on the state's camera while the rank-up behaviour is
        // driving it (X360 mCamera.mState_uFlags |= 2). Same as the race-intro state.
        const s32 KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN = 2;

        // The blend the end-of-take "Checkpoint" camera effect hook plays at (flt_82001C98 == 1.0).
        const f32 KF_CHECKPOINT_BLEND = 1.0f;

        // The parametric time a freshly-swapped take is rewound to each frame the rank-up
        // advances to the next rival's shot (flt_82001CC0 == 0.0).
        const f32 KF_TAKE_START_PARAMETRIC_TIME = 0.0f;

        // The end-of-take camera effect hook name the rank-up state requests / checks against.
        const char* const KPC_CHECKPOINT_HOOK = "Checkpoint";
    }

    // ------------------------------------------------------------------------
    // BehaviourHandle::IsBehaviourReadyToUse -- the X360's "has the take finished its initial
    // Prepare?" query (sub @0x822128A0): assert allocated, then ask the manager whether the
    // behaviour is still waiting to prepare. Defined out-of-line where BehaviourManager is
    // complete.
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    bool ArbStateRankUp::BehaviourHandle<TBehaviour>::IsBehaviourReadyToUse() const
    {
        CGS_ASSERT(mbAllocated, "mbIsAllocated");
        return !mpManager->IsBehaviourWaitingToPrepare(muAllocationKey);
    }

    // ------------------------------------------------------------------------
    // Construct @0x8225B2B8 -- build the camera, clear the base camera flags, zero the state
    // machine, and zero the behaviour handle. (miRival is left for Prepare to seed; the X360
    // Construct does not write +0x194.)
    // ------------------------------------------------------------------------
    void ArbStateRankUp::Construct()
    {
        GetNonConstCamera().Construct();   // X360 Camera::Construct(this+0x10)

        ResetBaseCameraFlags();            // X360 stb 0, +0x170 / +0x171

        meState = E_STATE_INACTIVE;        // +0x198 = 0

        // The behaviour handle starts unallocated (+0x180 block zeroed: mbAllocated,
        // muAllocationKey, muHelperIndex, mpManager, mpBehaviour).
        mIceCam = BehaviourHandle<Camera::BehaviourIceAnim>();
    }

    // ------------------------------------------------------------------------
    // GetName @0x821F6710
    // ------------------------------------------------------------------------
    const char* ArbStateRankUp::GetName() const
    {
        return "ArbStateRankUp";
    }

    // ------------------------------------------------------------------------
    // Prepare @0x82270EE8 -- enter the rank-up state: allocate and configure the ICE-anim
    // behaviour for the first rival's take. Does nothing when already ACTIVE / CHANGING_TO_-
    // ROAMING. Returns whether the freshly-allocated take is ready to use (so Update only
    // advances to ACTIVE once the behaviour has finished preparing).
    // ------------------------------------------------------------------------
    bool ArbStateRankUp::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        // Already running (ACTIVE / CHANGING_TO_ROAMING): do nothing. (X360: meState == 2 || == 3.)
        if (meState == E_STATE_ACTIVE || meState == E_STATE_CHANGING_TO_ROAMING)
        {
            return true;
        }

        const bool lbAlreadyAllocated = mIceCam.IsAllocated();

        meState = E_STATE_PREPARING;   // +0x198 = 1
        miRival = 0;                   // +0x194 = 0

        if (!lbAlreadyAllocated)
        {
            const Attrib::Gen::shotgroup& lrRankUpShots =
                lrSharedInfo.mpDirectorResourceManager->GetRankUp();

            CGS_ASSERT(lrRankUpShots.Num_ShotList() > 0,
                       "lSharedInfo.mpDirectorResourceManager->GetRankUp().Num_ShotList() > 0");

            // Allocate the ICE-anim behaviour into the handle.
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                mIceCam, this, KPC_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REFLIMIT);

            // Load the first rival's shot (index miRival == 0).
            const void* lpShotData = lrRankUpShots.GetShotListData(miRival);
            if (!lpShotData)
                lpShotData = Attrib::DefaultDataArea(KU_SHOT_DEFAULT_DATA_AREA_SIZE);

            Camera::BehaviourIceAnim* lpBehaviour = mIceCam.GetBehaviour();
            lpBehaviour->SetParameters(
                static_cast<Camera::BehaviourIceAnim::ShotReference*>(
                    const_cast<void*>(lpShotData)));

            // Anchor the take to the current rival's car (GameState rank-up rival index, read
            // through the named accessor that owns the trailing-region offset).
            lpBehaviour->SetPrimaryVehicleRefToRaceCar(
                lrSharedInfo.mpGameState->GetRankUpRivalRaceCarIndex());

            lpBehaviour->SetUseCollisionPolicy(true);    // +0xE28 = 1
            lpBehaviour->ClearBaseFirstFrameGate();      // base +0x28 = 0
        }

        // The X360 returns !IsBehaviourWaitingToPrepare(...) (sub @0x822128A0 == 0): the take is
        // "prepared" only once the manager no longer has it queued.
        return mIceCam.IsBehaviourReadyToUse();
    }

    // ------------------------------------------------------------------------
    // Update @0x82236380 -- per-frame rank-up state machine.
    // ------------------------------------------------------------------------
    void ArbStateRankUp::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        switch (meState)
        {
        case E_STATE_INACTIVE:
            break;

        case E_STATE_PREPARING:
            // Try to enter ACTIVE; on success advance and run the ACTIVE body this same frame
            // (the X360 case-1 success edge falls into case 2).
            if (!Prepare(lrSharedInfo))
            {
                break;   // still preparing
            }
            meState = E_STATE_ACTIVE;   // +0x198 = 2
            // fall through into the ACTIVE body
            [[fallthrough]];

        case E_STATE_ACTIVE:
        {
            Camera::Camera& lrCamera = GetNonConstCamera();

            // Drive the state camera from the behaviour's produced camera and mark it dirty.
            lrCamera = mIceCam.GetBehaviour()->GetProducedCamera();
            lrCamera.mState_uFlags |= KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN;

            GameState& lrGameState = *lrSharedInfo.mpGameState;

            // While the game is cycling rivals this frame, advance to the next rival's take.
            if (lrGameState.IsNewRankUpRivalThisFrame())   // X360 byte +0x1D6
            {
                ++miRival;   // +0x194

                Camera::BehaviourIceAnim* lpBehaviour = mIceCam.GetBehaviour();

                // Re-anchor the take to this rival's car (GameState rival race-car index).
                lpBehaviour->SetPrimaryVehicleRefToRaceCar(
                    lrGameState.GetRankUpRivalRaceCarIndex());   // X360 word +0x1D8

                // Swap the take to this rival's shot (index = rival % shot count).
                const Attrib::Gen::shotgroup& lrRankUpShots =
                    lrSharedInfo.mpDirectorResourceManager->GetRankUp();
                const u32 luShotCount = lrRankUpShots.Num_ShotList();   // X360 twllei n,0 guards the divide
                const s32 liShotIndex = static_cast<s32>(static_cast<u32>(miRival) % luShotCount);

                const void* lpShotData = lrRankUpShots.GetShotListData(liShotIndex);
                if (!lpShotData)
                    lpShotData = Attrib::DefaultDataArea(KU_SHOT_DEFAULT_DATA_AREA_SIZE);

                lpBehaviour->ChangeMovie(
                    static_cast<Camera::BehaviourIceAnim::ShotReference*>(
                        const_cast<void*>(lpShotData)),
                    *lrSharedInfo.mpDirectorResourceManager);

                // Rewind the freshly-swapped take to its start.
                lpBehaviour->SetControllerParametricTime0To1(KF_TAKE_START_PARAMETRIC_TIME);
            }

            // Once the rank-up is no longer actively driving (the "rank-up intro running" gate
            // is clear), check whether the take has finished and either request the end-of-take
            // "Checkpoint" effect or hand control back to roaming.
            if (!lrGameState.IsRankUpIntroRunning())   // X360 byte +0x1D4
            {
                if (mIceCam.GetBehaviour()->HasFinishedOrFailed())
                {
                    // Is the "Checkpoint" effect already the live camera effect?
                    const EffectInterface& lrEffects = *lrSharedInfo.mpEffectInterface;
                    bool lbCheckpointAlreadyLive = false;
                    if (lrEffects.HasCurrentEffectName())   // X360 byte +0xD37
                    {
                        lbCheckpointAlreadyLive =
                            std::strcmp(lrEffects.GetCurrentEffectName(), KPC_CHECKPOINT_HOOK) == 0;
                    }

                    if (!lbCheckpointAlreadyLive)
                    {
                        // Not playing "Checkpoint" yet: request the end-of-take hook on our camera.
                        Camera::RequestStartEffectHook(lrCamera, KPC_CHECKPOINT_HOOK, KF_CHECKPOINT_BLEND);
                    }
                    else
                    {
                        // "Checkpoint" is live: hand the frame back to the roaming state.
                        ArbitratorStateContainer& lrContainer = *lrSharedInfo.mpStateContainer;
                        if (lrContainer.GetState(ArbitratorStateContainer::E_STATE_ROAMING)->Prepare(lrSharedInfo))
                        {
                            lrContainer.SetCurrentState(ArbitratorStateContainer::E_STATE_ROAMING);
                            Release(lrSharedInfo);   // X360: this->Release (vtable slot 3)
                        }
                        else
                        {
                            meState = E_STATE_CHANGING_TO_ROAMING;   // +0x198 = 3
                        }
                    }
                }
            }
            break;
        }

        case E_STATE_CHANGING_TO_ROAMING:
        {
            Camera::Camera& lrCamera = GetNonConstCamera();

            // Keep driving the state camera from the behaviour while the hand-off completes.
            lrCamera = mIceCam.GetBehaviour()->GetProducedCamera();
            lrCamera.mState_uFlags |= KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN;

            ArbitratorStateContainer& lrContainer = *lrSharedInfo.mpStateContainer;
            if (lrContainer.GetState(ArbitratorStateContainer::E_STATE_ROAMING)->Prepare(lrSharedInfo))
            {
                lrContainer.SetCurrentState(ArbitratorStateContainer::E_STATE_ROAMING);
                Release(lrSharedInfo);   // X360: this->Release (vtable slot 3)
            }
            break;
        }

        default:
            CGS_ASSERT(false, "unhandled state");
            break;
        }
    }

    // ------------------------------------------------------------------------
    // Release @0x82236650 -- leave the rank-up state: reset the state machine, release the
    // ICE-anim behaviour back to the manager, and assert no behaviours remain allocated.
    // ------------------------------------------------------------------------
    bool ArbStateRankUp::Release(ArbStateSharedInfo& lrSharedInfo)
    {
        meState = E_STATE_INACTIVE;   // +0x198 = 0

        if (mIceCam.mbAllocated)      // +0x180 block
        {
            mIceCam.mpManager->UnSetBehaviourUsedByHandle(mIceCam.muAllocationKey);
            mIceCam.muHelperIndex = 0;
            mIceCam.mpManager     = 0;
            mIceCam.mpBehaviour   = 0;
            mIceCam.mbAllocated   = false;
        }

        lrSharedInfo.mpBehaviourManager->CheckNoBehavioursAreAllocatedByState(this);
        return true;
    }
}
