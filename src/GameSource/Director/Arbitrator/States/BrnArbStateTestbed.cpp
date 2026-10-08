// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateTestbed.cpp
//
// BrnDirector::ArbStateTestbed -- the developer camera testbed arbitrator state: the class
// statics, the vtable members, the debug-menu activate / deactivate actions, the parameter
// block's debug-menu mirror and the ICE-anim take registration.
// ============================================================================

#include "GameSource/Director/Arbitrator/States/BrnArbStateTestbed.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                           // CgsCore::SnPrintf
#include "GameSource/AttribSys/Generated/classes/aftertouchcam.h"                 // Attrib::Gen::aftertouchcam
#include "GameSource/AttribSys/Generated/classes/iceanim.h"                       // Attrib::Gen::iceanim
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"                     // Attrib::Gen::shotgroup
#include "GameSource/Director/BrnDirectorResourceManager.h"                       // GetAfterTouchCam / GetKeyAnimFromGuid
#include "GameSource/Director/Camera/Camera.h"                                    // Camera::Camera (the base mCamera)
#include "GameSource/Director/Camera/BrnCameraState.h"                            // CameraState::E_FLAG_VALID
#include "GameSource/Director/Camera/BrnBehaviourManager.h"                       // NewBehaviour<> / BehaviourHandle<>
#include "GameSource/Director/Camera/Behaviours/Serialisation.h"                  // DebugMenuSerialiser / SerialiseBehaviourParameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h"
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.h"
#include "GameSource/Director/Camera/Behaviours/BehaviourBystanderCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.h"
#include "GameSource/Director/Camera/Behaviours/BehaviourPassengerCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourSpirallingDeathcam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"  // DebugComponent::RegisterFunction
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h"      // DebugPrinter::Print
#include "SDKs/Packages/ICE/ICEData.hpp"                                          // ICE::ICETakeData::GetName

namespace BrnDirector
{
    ArbStateTestbed* ArbStateTestbed::spTestbed        = nullptr;
    DebugComponent*  ArbStateTestbed::spDebugComponent = nullptr;

    namespace
    {
        // The behaviour-type tags of the two parameter blocks whose headers name no enumerator
        // (each block's Parameters::Construct stores the value into its type word).
        const u32 KU_BEHAVIOUR_TYPE_ROAD_RUNNER          = 16u;
        const u32 KU_BEHAVIOUR_TYPE_ROTATE_ABOUT_VEHICLE = 18u;
    }

    // ------------------------------------------------------------------------
    // Construct -- build the base camera, clear its two flags, publish this state as the live
    // testbed and reset the state machine. Of the fifteen behaviour handles the console resets
    // eight here (rig, heli, bystander, gyro, external, failsafe, rotate-about-vehicle,
    // road-runner); the other seven keep whatever they hold (zero from the handle constructor).
    // mpCamera is not written.
    // ------------------------------------------------------------------------
    void ArbStateTestbed::Construct()
    {
        GetNonConstCamera().Construct();
        ResetBaseCameraFlags();
        spTestbed = this;
        meState   = E_STATE_INACTIVE;

        mRigCam                = Camera::BehaviourHandle<Camera::BehaviourRig>();
        mHeliCam               = Camera::BehaviourHandle<Camera::BehaviourHeliCam>();
        mBystander             = Camera::BehaviourHandle<Camera::BehaviourBystanderCam>();
        mGyroCam               = Camera::BehaviourHandle<Camera::BehaviourGyroCam>();
        mGameplayExternal      = Camera::BehaviourHandle<Camera::BehaviourGameplayExternal>();
        mFailsafe              = Camera::BehaviourHandle<Camera::BehaviourFailsafe>();
        mRotateAboutVehicleCam = Camera::BehaviourHandle<Camera::BehaviourRotateAboutVehicle>();
        mRoadRunner            = Camera::BehaviourHandle<Camera::BehaviourRoadRunner>();

        mfRunningTime   = 0.0f;
        mpParameters    = nullptr;
        mpShotRef       = nullptr;
        mbUseSlomo      = false;
        mbLoopIceMovies = false;
    }

    // The console vtable shares the one-instruction `return true` body here.
    bool ArbStateTestbed::Prepare(ArbStateSharedInfo& /*lrSharedInfo*/)
    {
        return true;
    }

    // ------------------------------------------------------------------------
    // Update -- the testbed state machine. An activate action arms GENERIC_PREPARE; the next
    // Update releases whatever ran before, allocates the behaviour the armed ICE take or
    // parameter block names, adopts the block (mirroring it into the debug menu) and goes to
    // GENERIC_UPDATE, which copies that behaviour's camera out every frame. Deactivate arms
    // RELEASING. INACTIVE does nothing.
    // ------------------------------------------------------------------------
    void ArbStateTestbed::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        switch (meState)
        {
        case E_STATE_INACTIVE:
            break;

        case E_STATE_GENERIC_PREPARE:
        {
            Release(lrSharedInfo);

            Camera::BehaviourManager* const lpBehaviourManager = lrSharedInfo.mpBehaviourManager;

            if (mpShotRef != nullptr)
            {
                lpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(mIceCam, this, 0, 1);
                mIceCam.GetBehaviour()->SetParameters(mpShotRef);
                mIceCam.GetBehaviour()->SetTakeLooping(mbLoopIceMovies);
                mIceCam.GetBehaviour()->SetUseCollisionPolicy(true);
                mpCamera = &mIceCam.GetProducedCamera();
            }
            else if (mpParameters != nullptr)
            {
                RegisterParameters();

                switch (mpParameters->GetType())
                {
                case Camera::eBehaviourAftertouchCam:
                {
                    lpBehaviourManager->NewBehaviour<Camera::BehaviourAftertouchCam>(mAftertouch, this, 0, 1);
                    mAftertouch.GetBehaviour()->SetParameters(
                        static_cast<const Camera::BehaviourAftertouchCam::Parameters*>(mpParameters));
                    const Attrib::Gen::aftertouchcam lAftertouchCam(
                        lrSharedInfo.mpDirectorResourceManager->GetAfterTouchCam(), 0);
                    mAftertouch.GetBehaviour()->SetSourceShot(lAftertouchCam);
                    mpCamera = &mAftertouch.GetProducedCamera();
                    break;
                }

                case Camera::eBehaviourAftertouchCrash:
                    lpBehaviourManager->NewBehaviour<Camera::BehaviourAftertouchCrash>(mAftertouchCrash, this, 0, 1);
                    mAftertouchCrash.GetBehaviour()->SetParameters(
                        static_cast<const Camera::BehaviourAftertouchCrash::Parameters*>(mpParameters));
                    mpCamera = &mAftertouchCrash.GetProducedCamera();
                    break;

                case Camera::eBehaviourRig:
                    lpBehaviourManager->NewBehaviour<Camera::BehaviourRig>(mRigCam, this, 0, 1);
                    mRigCam.GetBehaviour()->SetParameters(
                        static_cast<const Camera::BehaviourRig::Parameters*>(mpParameters));
                    mpCamera = &mRigCam.GetProducedCamera();
                    mRigCam.AttachTweaker();
                    break;

                case Camera::eBehaviourHeliCam:
                    lpBehaviourManager->NewBehaviour<Camera::BehaviourHeliCam>(mHeliCam, this, 0, 1);
                    mHeliCam.GetBehaviour()->SetParameters(
                        static_cast<const Camera::BehaviourHeliCam::Parameters*>(mpParameters));
                    mpCamera = &mHeliCam.GetProducedCamera();
                    break;

                case Camera::eBehaviourBystanderCam:
                    lpBehaviourManager->NewBehaviour<Camera::BehaviourBystanderCam>(mBystander, this, 0, 1);
                    mBystander.GetBehaviour()->SetParameters(
                        static_cast<const Camera::BehaviourBystanderCam::Parameters*>(mpParameters));
                    mBystander.GetBehaviour()->SetTarget(
                        static_cast<EActiveRaceCarIndex>(lrSharedInfo.mePlayerActiveRaceCarIndex));
                    mpCamera = &mBystander.GetProducedCamera();
                    break;

                case Camera::eBehaviourGyroCam:
                    lpBehaviourManager->NewBehaviour<Camera::BehaviourGyroCam>(mGyroCam, this, 0, 1);
                    mGyroCam.GetBehaviour()->SetParameters(
                        static_cast<const Camera::BehaviourGyroCam::Parameters*>(mpParameters));
                    mpCamera = &mGyroCam.GetProducedCamera();
                    break;

                case Camera::eBehaviourGameplayExternal:
                    lpBehaviourManager->NewBehaviour<Camera::BehaviourGameplayExternal>(mGameplayExternal, this, 0, 1);
                    mGameplayExternal.GetBehaviour()->SetParameters(mpParameters);
                    mGameplayExternal.SetUpdatesDuringPause(true);
                    mpCamera = &mGameplayExternal.GetProducedCamera();
                    break;

                case Camera::eBehaviourFailsafe:
                    lpBehaviourManager->NewBehaviour<Camera::BehaviourFailsafe>(mFailsafe, this, 0, 1);
                    mFailsafe.GetBehaviour()->SetParameters(
                        static_cast<const Camera::BehaviourFailsafe::Parameters*>(mpParameters));
                    mpCamera = &mFailsafe.GetProducedCamera();
                    break;

                case Camera::eBehaviourPassengerCam:
                    lpBehaviourManager->NewBehaviour<Camera::BehaviourPassengerCam>(mPassenger, this, 0, 1);
                    mPassenger.GetBehaviour()->SetParameters(
                        static_cast<const Camera::BehaviourPassengerCam::Parameters*>(mpParameters));
                    mpCamera = &mPassenger.GetProducedCamera();
                    break;

                case Camera::eBehaviourLooseAttachment:
                    lpBehaviourManager->NewBehaviour<Camera::BehaviourLooseAttachment>(mLooseAttachment, this, 0, 1);
                    mLooseAttachment.GetBehaviour()->SetParameters(
                        static_cast<const Camera::BehaviourLooseAttachment::Parameters*>(mpParameters));
                    mLooseAttachment.GetBehaviour()->AttachTo(lrSharedInfo.mePlayerActiveRaceCarIndex);
                    mLooseAttachment.GetBehaviour()->GetImpactEffect().RegisterImpact(1.0f);
                    mpCamera = &mLooseAttachment.GetProducedCamera();
                    break;

                case Camera::eBehaviourFixedCam:
                    lpBehaviourManager->NewBehaviour<Camera::BehaviourFixedCam>(mFixedCam, this, 0, 1);
                    mFixedCam.GetBehaviour()->SetParameters(
                        static_cast<const Camera::BehaviourFixedCam::Parameters*>(mpParameters));
                    mpCamera = &mFixedCam.GetProducedCamera();
                    mFixedCam.AttachTweaker();
                    break;

                case KU_BEHAVIOUR_TYPE_ROTATE_ABOUT_VEHICLE:
                    lpBehaviourManager->NewBehaviour<Camera::BehaviourRotateAboutVehicle>(mRotateAboutVehicleCam, this, 0, 1);
                    mRotateAboutVehicleCam.GetBehaviour()->SetParameters(
                        static_cast<const Camera::BehaviourRotateAboutVehicle::Parameters*>(mpParameters));
                    mpCamera = &mRotateAboutVehicleCam.GetProducedCamera();
                    break;

                case Camera::BehaviourSpirallingDeathcam::eBehaviourSpirallingDeathcam:
                    lpBehaviourManager->NewBehaviour<Camera::BehaviourSpirallingDeathcam>(mSpirallingDeathCam, this, 0, 1);
                    mSpirallingDeathCam.GetBehaviour()->SetParameters(
                        static_cast<const Camera::BehaviourSpirallingDeathcam::Parameters*>(mpParameters));
                    mSpirallingDeathCam.GetBehaviour()->Start();
                    mpCamera = &mSpirallingDeathCam.GetProducedCamera();
                    break;

                case KU_BEHAVIOUR_TYPE_ROAD_RUNNER:
                    lpBehaviourManager->NewBehaviour<Camera::BehaviourRoadRunner>(mRoadRunner, this, 0, 1);
                    mRoadRunner.GetBehaviour()->SetParameters(
                        static_cast<const Camera::BehaviourRoadRunner::Parameters*>(mpParameters));
                    mpCamera = &mRoadRunner.GetProducedCamera();
                    break;

                default:
                    CGS_ASSERT(false, "unhandled case");
                    break;
                }
            }
            else
            {
                CGS_ASSERT(false, "No ShotRef or Parameters!");
            }

            mfRunningTime = 0.0f;
            meState       = E_STATE_GENERIC_UPDATE;
            break;
        }

        case E_STATE_GENERIC_UPDATE:
        {
            CGS_ASSERT(mpCamera != nullptr, "mpCamera");

            GetNonConstCamera() = *mpCamera;
            GetNonConstCamera().mState_uFlags |= (1 << Camera::CameraState::E_FLAG_VALID);

            if (ShouldCycleCameraThisFrame())
            {
                mbUseSlomo = !mbUseSlomo;
            }

            char lacText[32];
            CgsCore::SnPrintf(lacText, sizeof(lacText), "Running time: %f", mfRunningTime);
            lrSharedInfo.mpDebugPrinter->Print(lacText);

            if (mIceCam.IsAllocated() && mIceCam.GetBehaviour()->HasFinishedOrFailed())
            {
                break;
            }

            mfRunningTime += lrSharedInfo.mfSimTimestep;
            break;
        }

        case E_STATE_RELEASING:
            Release(lrSharedInfo);
            meState = E_STATE_INACTIVE;
            break;

        default:
            CGS_ASSERT(false, "unhandled state");
            break;
        }
    }

    // ------------------------------------------------------------------------
    // Release -- drop back to INACTIVE: take the parameter block out of the debug menu, detach
    // the tweaker from the three behaviours that take one, give back every behaviour handle and
    // check the manager holds nothing on this state's behalf.
    // ------------------------------------------------------------------------
    bool ArbStateTestbed::Release(ArbStateSharedInfo& lrSharedInfo)
    {
        meState = E_STATE_INACTIVE;
        UnregisterParameters();

        if (mRigCam.IsAllocated())
        {
            mRigCam.DetachTweaker();
        }
        if (mGyroCam.IsAllocated())
        {
            mGyroCam.DetachTweaker();
        }
        if (mFixedCam.IsAllocated())
        {
            mFixedCam.DetachTweaker();
        }

        mAftertouch.Release();
        mAftertouchCrash.Release();
        mRigCam.Release();
        mHeliCam.Release();
        mBystander.Release();
        mGyroCam.Release();
        mGameplayExternal.Release();
        mFailsafe.Release();
        mPassenger.Release();
        mLooseAttachment.Release();
        mFixedCam.Release();
        mIceCam.Release();
        mRotateAboutVehicleCam.Release();
        mSpirallingDeathCam.Release();
        mRoadRunner.Release();

        lrSharedInfo.mpBehaviourManager->CheckNoBehavioursAreAllocatedByState(this);
        return true;
    }

    // The console vtable shares the empty body here.
    void ArbStateTestbed::Destruct()
    {
    }

    const char* ArbStateTestbed::GetName() const
    {
        return "ArbStateTestbed";
    }

    void ArbStateTestbed::SetDebugComponent(DebugComponent* lpDebugComponent)
    {
        spDebugComponent = lpDebugComponent;
    }

    // ------------------------------------------------------------------------
    // RegisterIceAnimsWithDebugComponent -- one "Activate" action per shot of the group, named
    // after the shot's take and filed under lpcMenuPath. The take is the editor's edited copy
    // when one exists, else the loaded one.
    // ------------------------------------------------------------------------
    void ArbStateTestbed::RegisterIceAnimsWithDebugComponent(const Attrib::Gen::shotgroup* lpShotGroup,
                                                             const DirectorResourceManager& lrResources,
                                                             const char* lpcMenuPath)
    {
        CGS_ASSERT(lpShotGroup != NULL, "lpShotgroup != NULL");
        CGS_ASSERT(spDebugComponent != NULL, "spDebugComponent != NULL");

        const u32 luNumShots = lpShotGroup->Num_ShotList();
        for (u32 luLoop = 0; luLoop < luNumShots; ++luLoop)
        {
            CGS_ASSERT(lpShotGroup->ShotList(luLoop)->GetClassKey()
                           == static_cast<u64>(Attrib::Gen::iceanim::ClassKey()),
                       "lpShotgroup->ShotList(luLoop).GetClassKey() == Attrib::Gen::iceanim::ClassKey()");

            const Attrib::Gen::iceanim lIceAnim(*lpShotGroup->ShotList(luLoop), 0);
            const ICE::ICETakeData* const lpTakeData = lrResources.GetKeyAnimFromGuid(lIceAnim.GetAnimGuid());
            CGS_ASSERT(lpTakeData != NULL, "lpTakeData != NULL");

            spDebugComponent->RegisterFunction(&ArbStateTestbed::ActivateIceCam,
                                               const_cast<Attrib::RefSpec*>(lpShotGroup->ShotList(luLoop)),
                                               lpcMenuPath,
                                               lpTakeData->GetName());
        }
    }

    // ------------------------------------------------------------------------
    // RegisterParameters / UnregisterParameters -- mirror the armed parameter block into / out
    // of the debug menu under its own debug name. Unregister is a no-op with nothing armed.
    // ------------------------------------------------------------------------
    void ArbStateTestbed::RegisterParameters()
    {
        CGS_ASSERT(mpParameters, "mpParameters");
        CGS_ASSERT(spDebugComponent, "spDebugComponent");

        Camera::DebugMenuSerialiser lSerialiser;
        lSerialiser.Construct(spDebugComponent, Camera::DebugMenuSerialiser::E_MODE_ADD_TO_MENU);
        Camera::SerialiseBehaviourParameters(mpParameters->GetDebugName(), *mpParameters, lSerialiser);
    }

    void ArbStateTestbed::UnregisterParameters()
    {
        if (mpParameters != nullptr)
        {
            CGS_ASSERT(spDebugComponent, "spDebugComponent");

            Camera::DebugMenuSerialiser lSerialiser;
            lSerialiser.Construct(spDebugComponent, Camera::DebugMenuSerialiser::E_MODE_REMOVE_FROM_MENU);
            Camera::SerialiseBehaviourParameters(mpParameters->GetDebugName(), *mpParameters, lSerialiser);
        }
    }

    // ------------------------------------------------------------------------
    // The debug-menu actions. Each takes the armed block out of the menu first, then arms the
    // state (Deactivate only asks for the release).
    // ------------------------------------------------------------------------
    void ArbStateTestbed::Deactivate(void* /*lpUnused*/)
    {
        CGS_ASSERT(spTestbed != NULL, "spTestbed != NULL");
        spTestbed->meState = E_STATE_RELEASING;
    }

    void ArbStateTestbed::GenericActivateCam(void* lpParameters)
    {
        CGS_ASSERT(spTestbed != NULL, "spTestbed != NULL");
        spTestbed->UnregisterParameters();
        spTestbed->mpParameters = static_cast<Camera::Behaviour::Parameters*>(lpParameters);
        spTestbed->mpShotRef    = nullptr;
        spTestbed->meState      = E_STATE_GENERIC_PREPARE;
    }

    void ArbStateTestbed::ActivateIceCam(void* lpShotRef)
    {
        CGS_ASSERT(spTestbed != NULL, "spTestbed != NULL");
        spTestbed->UnregisterParameters();
        spTestbed->mpShotRef    = static_cast<Camera::Camera::ShotReference*>(lpShotRef);
        spTestbed->mpParameters = nullptr;
        spTestbed->meState      = E_STATE_GENERIC_PREPARE;
    }
}
