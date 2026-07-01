#include "GameSource/Director/Arbitrator/States/BrnArbStateDriveThru.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT (drive-thru asserts)
#include "GameShared/GameClasses/Development/CgsStrStream.h"                    // CgsDev::StrStream (default-case assert)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorUtils.h"          // ArbUtils::ChangeToState
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h" // ArbitratorStateContainer::EState
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"            // BrnDirector::GameState
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"                 // Camera::EnsureEffectIsPlaying
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"          // DirectorResourceManager slice (GetDriveThru*Shots) + Camera::Behaviour
#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"                // SharedCameraContainer (primary-active/suspended gate)
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"                        // Camera::BehaviourManager (complete)
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"                   // Attrib::Gen::shotgroup
#include "rw/math/vpu/types.h"                                                  // Matrix44Affine::Pos()/At()
#include "rw/math/vpu/vector3_operation.h"                                      // Dot / operator-

// ============================================================================
// BrnDirector::ArbStateDriveThru -- reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity)
//   Construct @0x8225AE10
//   GetName   @0x821F6330
//   Prepare   @0x8226E938
//   Update    @0x82235DB0
//   Release   @0x82235F00
//
// The director arbitrator state that runs the camera while the player is inside a
// drive-thru shop. Prepare resolves the shop-specific shot-group, works out whether the
// bay approach is "reversed" from the bay/player-car transforms, and allocates a generic
// camera Behaviour to play the resolved shot. Update drives that behaviour each frame and,
// once it reports finished, plays the one-shot "Car_Reset" flash before handing control
// back to the roaming state (BrnDirector::ArbUtils::ChangeToState). All member access is
// BY NAME; GameState's drive-thru snapshot is reached through its named members.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace
    {
        // The blend the "Car_Reset" camera effect plays at (flt_82001C98 == 1.0; shared with
        // BrnArbStatePostEvent's / BrnArbStateRaceIntro's KF_CAR_RESET_BLEND).
        const f32 KF_CAR_RESET_BLEND = 1.0f;

        // The default attrib data-area size requested when a shot's parameter block is absent
        // (X360: li r3, 0x18 -> Attrib::DefaultDataArea(0x18)). Same as the sibling arbitrator
        // states (race-intro / rank-up / online-car-select).
        const u32 KU_SHOT_DEFAULT_DATA_AREA_SIZE = 0x18u;

        // The two trailing selectors the BehaviourManager::NewBehaviour<TBehaviour> allocation
        // request carries (X360 li r7,0 / li r8,1). Same as the sibling arbitrator states.
        const s32 KI_NEW_BEHAVIOUR_ARG_A = 0;
        const s32 KI_NEW_BEHAVIOUR_ARG_B = 1;

        // The dirty-flag bit Update raises on the state's camera while a behaviour is driving
        // it (X360 mCamera.mState_uFlags |= 2; same bit as the sibling arbitrator states).
        const s32 KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN = 2;
    }

    // ------------------------------------------------------------------------
    // Two X360 helper statics whose owning TU is not yet identified (the ledger carries them
    // as external/unknown -- no attribution to any reconstructed home). They resolve
    // ArbStateDriveThru::Update's live-behaviour handle (address-of mDriveThruBehaviourHandle
    // passed as the sole argument, matching a member-style call with `this` in r3) to (a) the
    // camera the behaviour produced this frame and (b) the live Behaviour* itself. Modelled as
    // opaque externs called EXACTLY as the asm does, rather than guessing a named accessor --
    // per project policy, a callee whose identity is not attested is stubbed/declared, not
    // fabricated. FLAG: bodies land once their owning TU is identified; the per-TU `cl /c`
    // gate does not link them.
    //   sub_821FCE10 @0x821FCE10 -- BehaviourHandle<Camera::Behaviour>-style produced-camera
    //     resolver (X360: bl sub_821FCE10 with r3 = &handle; result fed to Camera::operator=).
    //   sub_821FCDA8 @0x821FCDA8 -- BehaviourHandle<Camera::Behaviour>-style live-behaviour
    //     resolver (X360: bl sub_821FCDA8 with r3 = &handle; result is a Camera::Behaviour*
    //     whose +0xC byte -- Camera::Behaviour::mbBaseFlagC, the canonical DWARF-attested
    //     field this same offset names in GameSource/Director/Camera/Behaviours/Behaviour.cpp
    //     -- Update reads).
    extern "C"
    {
        const Camera::Camera* sub_821FCE10(void* lpHandle);   // @ 0x821FCE10
        Camera::Behaviour*    sub_821FCDA8(void* lpHandle);   // @ 0x821FCDA8
    }

    // ------------------------------------------------------------------------
    // Construct @0x8225AE10 -- build the camera, clear the base camera flags, and zero the
    // behaviour handle / interpolation handle+params / state machine / reversed flag.
    // ------------------------------------------------------------------------
    void ArbStateDriveThru::Construct()
    {
        GetNonConstCamera().Construct();   // X360 Camera::Construct(this+0x10)

        ResetBaseCameraFlags();            // X360 stb 0, +0x170 / +0x171

        meState = E_STATE_INACTIVE;        // +0x1B8 = 0

        // The two behaviour handles start unallocated (five-word blocks zeroed).
        mDriveThruBehaviourHandle = BehaviourHandle<Camera::Behaviour>();
        mInterpolater             = BehaviourHandle<Camera::BehaviourInterpolate>();

        // mInterpolaterParams (+0x1A8): the X360 seeds the four selector words {8, 0, 0, 1}.
        mInterpolaterParams.mauParams[0] = 8u;
        mInterpolaterParams.mauParams[1] = 0u;
        mInterpolaterParams.mauParams[2] = 0u;
        mInterpolaterParams.mauParams[3] = 1u;

        mbIsReversed = false;   // +0x1BC = 0
    }

    // ------------------------------------------------------------------------
    // GetName @0x821F6330
    // ------------------------------------------------------------------------
    const char* ArbStateDriveThru::GetName() const
    {
        return "ArbStateDriveThru";
    }

    // ------------------------------------------------------------------------
    // Prepare @0x8226E938 -- enter the drive-thru state: assert the drive-thru snapshot is
    // valid, work out whether the bay approach is reversed from the bay/player-car transforms,
    // resolve the shop-specific shot-group, and allocate the camera behaviour to play it. Only
    // runs the one-time setup while meState == E_STATE_INACTIVE (0); unconditionally reports
    // "not yet ready" on the frame that runs it (X360 always returns 0 from this branch).
    // ------------------------------------------------------------------------
    bool ArbStateDriveThru::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        if (meState != E_STATE_INACTIVE)   // X360: *(a1+440) != 0
        {
            return true;
        }

        meState = E_STATE_PREPARING;   // +0x1B8 = 1

        GameState& lrGameState = *lrSharedInfo.mpGameState;

        CGS_ASSERT(lrGameState.mbDriveThruActive, "lSharedInfo.mpGameState->mbDriveThruActive");
        CGS_ASSERT(lrGameState.meDriveThruType != GameState::E_DRIVETHRU_INVALID,
                   "lSharedInfo.mpGameState->meDriveThruType != GameState::E_DRIVETHRU_INVALID");

        // ---- resolve whether the bay approach is reversed ---------------------------------
        // X360: dot(mDriveThruTransform.Pos() - mpPlayerCarTransform->Pos(), mDriveThruTransform.At()) < 0.
        const rw::math::vpu::Matrix44Affine& lrPlayerTransform =
            *static_cast<const rw::math::vpu::Matrix44Affine*>(lrSharedInfo.mpPlayerCarTransform);

        const rw::math::vpu::Vector3 lv3BayToPlayer =
            lrGameState.mDriveThruTransform.Pos() - lrPlayerTransform.Pos();

        mbIsReversed =
            rw::math::vpu::Dot(lv3BayToPlayer, lrGameState.mDriveThruTransform.At()) < 0.0f;

        // The gas-station bay is approached from the opposite side, so its reversed sense is
        // inverted (X360: `if (meDriveThruType == 3) mbIsReversed = !mbIsReversed;`).
        if (lrGameState.meDriveThruType == GameState::E_DRIVETHRU_GAS_STATION)
        {
            mbIsReversed = !mbIsReversed;
        }

        // ---- allocate the shop-shot camera behaviour (only once) --------------------------
        if (!mDriveThruBehaviourHandle.IsAllocated())   // +0x180 block
        {
            const DirectorResourceManager& lrResourceManager = *lrSharedInfo.mpDirectorResourceManager;

            // Resolve the shop-specific shot-group (X360: a switch on meDriveThruType that
            // reads one of five manager-embedded shotgroups, one per shop kind).
            const Attrib::Gen::shotgroup* lpShotGroup = nullptr;
            switch (lrGameState.meDriveThruType)
            {
            case GameState::E_DRIVETHRU_AUTO_PARTS:
                lpShotGroup = &lrResourceManager.GetDriveThruAutoPartsShots();
                break;
            case GameState::E_DRIVETHRU_BODY_SHOP:
                lpShotGroup = &lrResourceManager.GetDriveThruBodyShopShots();
                break;
            case GameState::E_DRIVETHRU_GAS_STATION:
                lpShotGroup = &lrResourceManager.GetDriveThruGasStationShots();
                break;
            case GameState::E_DRIVETHRU_TUNING_SHOP:
                lpShotGroup = &lrResourceManager.GetDriveThruTuningShopShots();
                break;
            case GameState::E_DRIVETHRU_TIRE_SHOP:
                lpShotGroup = &lrResourceManager.GetDriveThruTireShopShots();
                break;
            default:
            {
                // X360 streams "unhandled drivethru type: <n>\n" into the assert buffer (same
                // idiom as BrnRouteRequestManager.cpp's default-case assert).
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStream << "unhandled drivethru type: " << static_cast<s32>(lrGameState.meDriveThruType);
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lacMessageBuffer,
                                           "..\\..\\..\\GameSource\\Director/Arbitrator/States/BrnArbStateDriveThru.cpp",
                                           111);
                CgsDev::Assert::EndAssert();

                // X360 falls through to the E_DRIVETHRU_TIRE_SHOP slot on any unhandled value.
                lpShotGroup = &lrResourceManager.GetDriveThruTireShopShots();
                break;
            }
            }

            const Attrib::Gen::shotgroup& lrShotGroup = *lpShotGroup;

            CGS_ASSERT(lrShotGroup.Num_ShotList() > 0, "lpShotGroup->Num_ShotList()>0");

            // When the group has at least two shots and the approach is reversed, use the
            // second shot (the group's reverse-approach take); otherwise the first.
            const bool lbUseSecondShot =
                (lrShotGroup.Num_ShotList() >= 2) && mbIsReversed;

            if (lbUseSecondShot)
            {
                CGS_ASSERT(lrShotGroup.Num_ShotList() >= 2, "lpShotGroup->Num_ShotList() >= 2");
            }

            const void* lpShotData = lrShotGroup.GetShotListData(lbUseSecondShot);
            if (!lpShotData)
                lpShotData = Attrib::DefaultDataArea(KU_SHOT_DEFAULT_DATA_AREA_SIZE);

            // TBehaviour == the generic Camera::Behaviour base (not a concrete behaviour type
            // already pinned at the call site, unlike the sibling states' NewBehaviour<BehaviourIceAnim>
            // calls), so this uses the attribute-taking NewBehaviour overload: the manager reads
            // the shot data to pick which concrete behaviour to allocate (X360: r4 = AttributePointer
            // is passed directly into NewBehaviour, not to a follow-up SetParameters call).
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::Behaviour>(
                mDriveThruBehaviourHandle, lpShotData, this,
                KI_NEW_BEHAVIOUR_ARG_A, KI_NEW_BEHAVIOUR_ARG_B);
        }

        return false;
    }

    // ------------------------------------------------------------------------
    // Update @0x82235DB0 -- per-frame drive-thru state machine.
    // ------------------------------------------------------------------------
    void ArbStateDriveThru::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        Camera::Camera& lrCamera = GetNonConstCamera();

        switch (meState)
        {
        case E_STATE_INACTIVE:
            break;

        case E_STATE_PREPARING:
            // Run Prepare; on success advance to ACTIVE and fall through into its work (X360
            // goto LABEL_4). Prepare() here is the polymorphic virtual call the asm dispatches
            // through the vtable (`(*(*result+4))(result, a2)`), i.e. this same override.
            if (Prepare(lrSharedInfo))
            {
                meState = E_STATE_ACTIVE;   // +0x1B8 = 2
                // FALLTHROUGH
            }
            else
            {
                break;
            }
            // FALLTHROUGH
        case E_STATE_ACTIVE:
        {
            // Drive the state camera from the behaviour and mark it behaviour-driven.
            lrCamera = *sub_821FCE10(&mDriveThruBehaviourHandle);
            lrCamera.mState_uFlags |= KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN;

            Camera::Behaviour* lpBehaviour = sub_821FCDA8(&mDriveThruBehaviourHandle);

            // "flag C" (Camera::Behaviour +0xC; see Behaviour.cpp) gates the hand-off.
            if (!lpBehaviour->GetBaseFlagC())
            {
                break;
            }

            // X360: `!mpSharedCameraContainer->mbPrimaryActive || mpSharedCameraContainer->
            // mbPrimarySuspended` selects whether to skip straight to the hand-off (when the
            // primary gameplay camera is active AND suspended, the flash has already played
            // elsewhere -- go straight to CHANGING_TO_ROAMING); otherwise play the one-shot
            // flash first and advance next frame.
            const SharedCameraContainer& lrSharedCameraContainer = *lrSharedInfo.mpSharedCameraContainer;
            const bool lbSkipFlash =
                lrSharedCameraContainer.mbPrimaryActive && lrSharedCameraContainer.mbPrimarySuspended;

            if (!lbSkipFlash)
            {
                Camera::EnsureEffectIsPlaying(lrCamera, *lrSharedInfo.mpEffectInterface,
                                              "Car_Reset", KF_CAR_RESET_BLEND);
                meState = E_STATE_CHANGING_TO_ROAMING;   // +0x1B8 = 3
                break;
            }

            // FALLTHROUGH (X360 goto LABEL_11): skip the flash and hand off immediately.
            ArbUtils::ChangeToState<EState>(
                this, lrSharedInfo, ArbitratorStateContainer::E_STATE_ROAMING,
                meState, E_STATE_CHANGING_TO_ROAMING);
            break;
        }

        case E_STATE_CHANGING_TO_ROAMING:
            ArbUtils::ChangeToState<EState>(
                this, lrSharedInfo, ArbitratorStateContainer::E_STATE_ROAMING,
                meState, E_STATE_CHANGING_TO_ROAMING);
            break;

        default:
            CGS_ASSERT(false, "unhandled state");
            break;
        }
    }

    // ------------------------------------------------------------------------
    // Release @0x82235F00 -- leave the drive-thru state: clear the state machine, release the
    // shop-shot behaviour back to the manager, and assert no behaviours remain allocated.
    // ------------------------------------------------------------------------
    bool ArbStateDriveThru::Release(ArbStateSharedInfo& lrSharedInfo)
    {
        meState = E_STATE_INACTIVE;   // +0x1B8 = 0

        if (mDriveThruBehaviourHandle.IsAllocated())   // +0x180 block
        {
            mDriveThruBehaviourHandle.mpManager->UnSetBehaviourUsedByHandle(
                mDriveThruBehaviourHandle.muAllocationKey);
            mDriveThruBehaviourHandle.muHelperIndex = 0;
            mDriveThruBehaviourHandle.mpManager     = 0;
            mDriveThruBehaviourHandle.mpBehaviour   = 0;
            mDriveThruBehaviourHandle.mbAllocated   = false;
        }

        lrSharedInfo.mpBehaviourManager->CheckNoBehavioursAreAllocatedByState(this);
        return true;
    }
}
