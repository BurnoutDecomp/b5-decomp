#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_TESTBED_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_TESTBED_H

#include "types.hpp"
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle<T>
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"             // Camera::Behaviour::Parameters (mpParameters)
#include "GameSource/Director/Camera/Camera.h"                           // Camera::Camera::ShotReference (mpShotRef)

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateTestbed.h
//
// BrnDirector::ArbStateTestbed -- the developer "camera testbed" arbitrator state, embedded by
// value in the Arbitrator (console arbitrator +0x30). It is debug-menu driven: every block of
// the behaviour parameter bank is registered as a "Testbed" action (GenericActivateCam, through
// TestbedSetupSerialiser) and every ICE-anim take of a shot group as another
// (ActivateIceCam, through RegisterIceAnimsWithDebugComponent). Activating one arms the state;
// the next Update allocates the matching behaviour through the BehaviourManager, mirrors its
// parameter block into the debug menu (RegisterParameters) and drives it; Deactivate releases.
//
// The activate / deactivate actions are debug-menu callbacks, so they are static and reach the
// one live testbed through spTestbed (set by Construct); RegisterParameters /
// UnregisterParameters reach the director's debug component through spDebugComponent (set by
// SetDebugComponent).
//
// Layout (declaration order; console offsets, provenance only -- the host Camera and handles
// are wider):
//   +0x010  ArbitratorState::mCamera
//   +0x180  the fifteen BehaviourHandle<> members, 0x14 apart
//   +0x2AC  mpShotRef   +0x2B0 mpParameters   +0x2B4 mpCamera   +0x2B8 mfRunningTime
//   +0x2BC  meState     +0x2C0 mbLoopIceMovies +0x2C1 mbUseSlomo
//
// Console vtable: Construct, Prepare (returns true), Update, Release, Destruct (empty),
// CanRun (the base's), GetName.
// ----------------------------------------------------------------------------

namespace Attrib { namespace Gen { class shotgroup; } }

namespace BrnDirector
{
    class DebugComponent;                 // DirectorModule/BrnDirectorModuleDebugCompononent.h
    class DirectorResourceManager;        // BrnDirectorResourceManager.h

    namespace Camera
    {
        class  BehaviourAftertouchCam;
        class  BehaviourAftertouchCrash;
        class  BehaviourRig;
        class  BehaviourHeliCam;
        class  BehaviourBystanderCam;
        class  BehaviourGyroCam;
        class  BehaviourGameplayExternal;
        class  BehaviourFailsafe;
        struct BehaviourPassengerCam;
        class  BehaviourLooseAttachment;
        class  BehaviourFixedCam;
        class  BehaviourIceAnim;
        class  BehaviourRotateAboutVehicle;
        class  BehaviourSpirallingDeathcam;
        class  BehaviourRoadRunner;
        class  TestbedSetupSerialiser;
    }

    class ArbStateTestbed : public ArbitratorState
    {
    public:
        // The testbed state machine; the Update switch key.
        enum EState
        {
            E_STATE_INACTIVE        = 0,
            E_STATE_GENERIC_PREPARE = 1,
            E_STATE_GENERIC_UPDATE  = 2,
            E_STATE_RELEASING       = 3,

            E_NUM_STATES            = 4
        };

        // ---- ArbitratorState virtual overrides -------------------------------------------
        void        Construct() override;
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override;
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override;
        void        Destruct() override;
        const char* GetName() const override;

        // Is a behaviour or take armed or running.
        bool IsActive() const;

        // Bind the debug component RegisterParameters / UnregisterParameters mirror the active
        // parameter block into (spDebugComponent).
        void SetDebugComponent(DebugComponent* lpDebugComponent);

        // Register every ICE-anim take of lpShotGroup as a debug-menu action (ActivateIceCam)
        // under lpcMenuPath.
        void RegisterIceAnimsWithDebugComponent(const Attrib::Gen::shotgroup* lpShotGroup,
                                                const DirectorResourceManager& lrResources,
                                                const char* lpcMenuPath);

    private:
        // Mirror the active parameter block into / out of the debug menu
        // (SerialiseBehaviourParameters<DebugMenuSerialiser>, add / remove mode).
        void RegisterParameters();
        void UnregisterParameters();

        // The debug-menu actions (DebugCallbackFunction shape; they act on spTestbed).
        static void Deactivate(void* lpUnused);
        static void GenericActivateCam(void* lpParameters);   // lpParameters: Camera::Behaviour::Parameters*
        static void ActivateIceCam(void* lpShotRef);          // lpShotRef: Camera::Camera::ShotReference*

        // TestbedSetupSerialiser registers GenericActivateCam for every bank block; the
        // director's DebugComponent::OnActivate registers Deactivate.
        friend class Camera::TestbedSetupSerialiser;
        friend class DebugComponent;

        // ---- members, declaration order ----------------------------------------------------------
        Camera::BehaviourHandle<Camera::BehaviourAftertouchCam>       mAftertouch;            // +0x180
        Camera::BehaviourHandle<Camera::BehaviourAftertouchCrash>     mAftertouchCrash;       // +0x194
        Camera::BehaviourHandle<Camera::BehaviourRig>                 mRigCam;                // +0x1A8
        Camera::BehaviourHandle<Camera::BehaviourHeliCam>             mHeliCam;               // +0x1BC
        Camera::BehaviourHandle<Camera::BehaviourBystanderCam>        mBystander;             // +0x1D0
        Camera::BehaviourHandle<Camera::BehaviourGyroCam>             mGyroCam;               // +0x1E4
        Camera::BehaviourHandle<Camera::BehaviourGameplayExternal>    mGameplayExternal;      // +0x1F8
        Camera::BehaviourHandle<Camera::BehaviourFailsafe>            mFailsafe;              // +0x20C
        Camera::BehaviourHandle<Camera::BehaviourPassengerCam>        mPassenger;             // +0x220
        Camera::BehaviourHandle<Camera::BehaviourLooseAttachment>     mLooseAttachment;       // +0x234
        Camera::BehaviourHandle<Camera::BehaviourFixedCam>            mFixedCam;              // +0x248
        Camera::BehaviourHandle<Camera::BehaviourIceAnim>             mIceCam;                // +0x25C
        Camera::BehaviourHandle<Camera::BehaviourRotateAboutVehicle>  mRotateAboutVehicleCam; // +0x270
        Camera::BehaviourHandle<Camera::BehaviourSpirallingDeathcam>  mSpirallingDeathCam;    // +0x284
        Camera::BehaviourHandle<Camera::BehaviourRoadRunner>          mRoadRunner;            // +0x298

        Camera::Camera::ShotReference*     mpShotRef;        // +0x2AC  the armed ICE take
        Camera::Behaviour::Parameters*     mpParameters;     // +0x2B0  the armed parameter block
        const Camera::Camera*              mpCamera;         // +0x2B4  the active behaviour's camera
        f32                                mfRunningTime;    // +0x2B8
        EState                             meState;          // +0x2BC
        bool                               mbLoopIceMovies;  // +0x2C0
        bool                               mbUseSlomo;       // +0x2C1

        static ArbStateTestbed* spTestbed;          // the live testbed (Construct)
        static DebugComponent*  spDebugComponent;   // the director's debug component (SetDebugComponent)

    public:
        // NEVER CALLED. Pins the member order the console layout fixes.
        static void _AssertLayout();
    };

    inline void ArbStateTestbed::_AssertLayout()
    {
        typedef ArbStateTestbed T;
        static_assert(offsetof(T, mAftertouch) < offsetof(T, mRigCam) &&
                      offsetof(T, mRigCam) < offsetof(T, mGyroCam) &&
                      offsetof(T, mGyroCam) < offsetof(T, mIceCam) &&
                      offsetof(T, mIceCam) < offsetof(T, mRoadRunner),
                      "ArbStateTestbed: the fifteen handles in console order");
        static_assert(offsetof(T, mRoadRunner) < offsetof(T, mpShotRef) &&
                      offsetof(T, mpShotRef) < offsetof(T, mpParameters) &&
                      offsetof(T, mpParameters) < offsetof(T, mpCamera) &&
                      offsetof(T, mpCamera) < offsetof(T, mfRunningTime) &&
                      offsetof(T, mfRunningTime) < offsetof(T, meState) &&
                      offsetof(T, meState) < offsetof(T, mbLoopIceMovies),
                      "ArbStateTestbed: the tail members in console order");
        static_assert(offsetof(T, mbUseSlomo) == offsetof(T, mbLoopIceMovies) + 1,
                      "ArbStateTestbed: mbUseSlomo is the byte after mbLoopIceMovies (+0x2C0 / +0x2C1)");
    }
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_TESTBED_H
