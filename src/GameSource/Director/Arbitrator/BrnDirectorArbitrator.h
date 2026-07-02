#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_BRN_DIRECTOR_ARBITRATOR_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_BRN_DIRECTOR_ARBITRATOR_H

#include "types.hpp"
#include "rw/math/vpu/types.h"                                              // rw::math::vpu::Vector3 / Matrix44Affine
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"      // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h" // ArbitratorStateContainer (mStateContainer by value)
#include "GameSource/Director/Arbitrator/States/BrnArbStateCrashNav.h"      // BrnDirector::ArbStateCrashNav (real layout, by value)
#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"            // BrnDirector::SharedCameraContainer (by value)
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"                    // BrnDirector::Camera::BehaviourHandle<> / BehaviourManager / BehaviourHelperIndex

// ============================================================================
// GameSource/Director/Arbitrator/BrnDirectorArbitrator.h
//
// BrnDirector::Arbitrator -- the Director camera ARBITRATOR: the top-level camera state
// machine that owns the embedded arbitrator-state container plus the handful of special
// "out of band" states (crash-nav / attract-mode / render-metrics / final-elite ICE cam /
// the debug fly/orbit cameras), and each frame PICKS which arbitrator state drives the
// output Camera. Its EState (PREPARE -> PRE_NORMAL -> NORMAL -> ... -> RELEASE) is the
// outer dispatch key; the NORMAL state delegates the per-section selection to
// mStateContainer (the 11 ArbitratorState siblings), while the CRASH_NAV / ATTRACT_MODE /
// RENDER_METRICS / FINAL_ELITE_SEQUENCE states run their own embedded behaviours.
//
// Construct / Update / UpdateCameraCycleControl are reconstructed in
// BrnDirectorArbitrator.cpp from the ARTIST X360 asm (Construct @0x82254E90,
// Update @0x8226ADA0, UpdateCameraCycleControl @0x821F5D28). Destruct / UpdateDebugCameras
// are DECLARATION-ONLY (no X360 asm in this TU's function set -- only DWARF variable hints).
//
// LAYOUT: member NAMES + DWARF declaration order come from the DecFIGS DWARF
// (BrnDirectorArbitrator.h). Parity is BY NAMED MEMBER (the project's x64-gate rule): the
// X360 4-byte-pointer offsets the asm proves (mStateContainer @+0x310, the crash-nav state
// @+0x3910, attract-mode @+0x41A0, render-metrics @+0x4340, mFinalEliteCam @+0x44E0,
// mfTimeCycleCameraHeld @+0x44F4, meState @+0x44F8, mfSlomoFactor @+0x4504) are provenance;
// on the x64 host the embedded states / handles / Camera widen, so absolute offsets shift --
// the member ROLES are what is reproduced.
//
// FLAG: ArbStateAttractMode / ArbStateRenderMetrics / ArbStateTestbed have no reconstructed
//   home TU yet -- declared here as minimal ArbitratorState subclasses (same convention the
//   container uses for its not-yet-homed states) purely so the Arbitrator can embed them by
//   value and dispatch their virtual Construct() / Update() / Release(). GROW each into its
//   real layout (additively) when its own TU lands; this TU never touches their per-state
//   members by name.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    // The output camera type the arbitrator copies the selected state's camera into. This TU
    // reaches it BY NAME (forward-declared; the real definition is Camera.h, included via the
    // state headers above through Camera::Camera).
    namespace Camera
    {
        class Camera;
        // The three special-cam behaviours the arbitrator's debug + final-elite handles own.
        // Forward-declared (the handles only need the type name to be embeddable; the bodies
        // that touch the live behaviour live in the declaration-only Update / UpdateDebugCameras).
        // FLAG: no reconstructed home TU yet for these three behaviour types.
        class BehaviourDebugOrbitPlayer;
        class BehaviourDebugFlyWorld;
        class BehaviourIceAnim;
        class BehaviourManager;
    }

    struct DirectorOutputInterface;

    // ---- not-yet-homed embedded arbitrator states (minimal placeholder homes) -------------
    // Each is a distinct ArbitratorState subtype embedded by value in the Arbitrator; the
    // arbitrator constructs each (vtable slot 0) and the special-state Update paths dispatch
    // their virtuals. The real layouts / overrides land with each state's own TU.
    // FLAG: minimal placeholder homes (by-name parity; this TU touches no per-state member).
    class ArbStateTestbed      : public ArbitratorState {};
    class ArbStateAttractMode  : public ArbitratorState {};
    class ArbStateRenderMetrics: public ArbitratorState {};

    class Arbitrator
    {
    public:
        // DWARF EState (BrnDirectorArbitrator.h:72). The OUTER arbitrator state machine: the
        // switch key the Update asm reads from meState (X360 +0x44F8). Values are the X360
        // jump-table case indices / the immediates the asm stores into meState.
        enum EState
        {
            E_STATE_PREPARE                  = 0,
            E_STATE_PRE_NORMAL               = 1,
            E_STATE_NORMAL                   = 2,
            E_STATE_CRASH_NAV                = 3,
            E_STATE_CRASH_NAV_ICE_CAMERAS    = 4,
            E_STATE_CHANGING_TO_ATTRACT_MODE = 5,
            E_STATE_ATTRACT_MODE             = 6,
            E_STATE_FINAL_ELITE_SEQUENCE     = 7,
            E_STATE_RENDER_METRICS           = 8,
            E_STATE_RELEASE                  = 9
        };

        // X360 0x82254E90. Construct the arbitrator: build the embedded state container, the
        // shared-camera container, the three special-cam states, the final-elite / debug
        // camera handles, and seed the outer state machine to PREPARE. (BrnDirectorArbitrator.h:59)
        void Construct();

        // X360 0x8226ADA0. Per-frame: pick + run the active arbitrator state for the current
        // EState, copying its produced camera into lrCameraInOut and driving the
        // pause / slow-down / jump / black-fade post-FX. (BrnDirectorArbitrator.h:67)
        //
        // DECLARATION-ONLY + FLAG: the body is the outer state machine (X360 @0x8226ADA0). It
        //   dispatches across a deep, NOT-YET-HOMED camera/effect API surface --
        //   SharedCameraContainer::{SetUsingGameplayExternal,SetLookbackOverride,
        //   GetGameplayExternal,GetGameplayCamera}, Camera::Camera::operator= (the take copy),
        //   Camera::{EnsureEffectIsStopped}, CameraEffects::{SetStart/StopHookNameString,
        //   SetSimTimeScale,SetRequestedPostFX,ClearStart/StopHookNameString},
        //   DepthOfField::SetBlurriness, DirectorOutputInterface::SetPauseRequest,
        //   BehaviourManager::NewBehaviour<>, HookNameStringWrapper::Set,
        //   BehaviourGameplayExternal::SnapToCar, the BehaviourIceAnim final-elite seed, and the
        //   gameplay-external collision-policy resets -- none of which have reconstructed homes /
        //   attested signatures yet. Writing the body would require FABRICATING that entire API
        //   (signatures + the per-camera-flag offset->name mappings the trimmed DWARF omits),
        //   which the reconstruction rules forbid. Left declaration-only until those camera /
        //   effect / behaviour-manager TUs land; the dispatch structure is documented in the .cpp.
        void Update(bool lbPaused, Camera::Camera& lrCameraInOut,
                    ArbStateSharedInfo& lrSharedInfo, bool lbCycleCamera, bool lbCycleCameraHeld);

        // X360 (no asm in this TU's function set -- DWARF variable hints only). Tear the
        // arbitrator down. (BrnDirectorArbitrator.h:70) DECLARATION-ONLY.
        void Destruct();

        // Whether a debug camera (fly-world / orbit-player) is currently driving the output.
        // (BrnDirectorArbitrator.h:87) DECLARATION-ONLY (no asm in this TU's function set).
        bool IsDebugCamera();

        // Point the debug fly-world camera at lLookAt from lEye. (BrnDirectorArbitrator.h:92)
        // @0x82234738 (class TU; body in the .cpp).
        void DebugCameraFlyWorldLookAt(rw::math::vpu::Vector3 lEye, rw::math::vpu::Vector3 lLookAt);

        // The current debug fly-world camera transform. (BrnDirectorArbitrator.h:95)
        // DECLARATION-ONLY (no asm in this TU's function set).
        rw::math::vpu::Matrix44Affine GetDebugFlyWorldTransform();

        // The shared-camera container the gameplay cameras live in. (BrnDirectorArbitrator.h:98)
        SharedCameraContainer& GetSharedCameras() { return mSharedCameraContainer; }

        // GROWN for DirectorDevTools::GameTalkMsgHandler (@0x822095A0): the GameTalk
        // Start/StopRenderMetrics commands poke the render-metrics request pair
        // (X360 arbitrator +0x44FE / +0x44FF) directly.
        void SetDoRenderMetrics(bool lbDoRenderMetrics) { mbDoRenderMetrics = lbDoRenderMetrics; }
        void SetRenderMetricsArg(u8 luArg)              { muRenderMetricsArg = luArg; }

    private:
        // The currently-driving "normal" gameplay camera (the container's selected state's
        // camera). (BrnDirectorArbitrator.h:150) @0x821F5BD8 (class TU; body in the .cpp).
        const Camera::Camera& GetNormalCamera() const;

        // Advance the normal-camera cycle one step. (BrnDirectorArbitrator.h:153)
        // @0x822087F0 (class TU; body in the .cpp).
        void CycleNormalCamera();

        // X360 0x821F5D28. Drive the "hold to slow-mo / tap to cycle" camera-cycle control.
        // While lbCycleCameraHeld, accumulate the held time; once it crosses the [1.0, 2.0)
        // window, snap the timer to 2.0, FLIP the normal-camera selector toggle and report
        // "changed this frame". On release, if the timer was a short tap ((0.0, 1.0)) report
        // "cycle camera"; either way reset the held timer to 0. (BrnDirectorArbitrator.h:160)
        void UpdateCameraCycleControl(f32 lfTimestep, bool lbCycleCameraHeld,
                                      bool& lrbCycleCameraOut, bool& lrbChangedThisFrameOut);

        // X360 (no asm in this TU's function set -- DWARF variable hints only). Drive the
        // fly-world / orbit-player debug cameras + their tweakers. (BrnDirectorArbitrator.h:168)
        // DECLARATION-ONLY.
        void UpdateDebugCameras(Camera::Camera& lrCameraInOut, Camera::BehaviourManager& lrBehaviourManager,
                                bool lbCycleCamera, bool lbChangedThisFrame);

        // ---- members, DWARF order; X360 4-byte-pointer offsets in comments (provenance) ----
        Camera::BehaviourHandle<Camera::BehaviourDebugOrbitPlayer> mDebugCameraOrbitPlayer; // X360 +0x00
        Camera::BehaviourHandle<Camera::BehaviourDebugFlyWorld>    mDebugCameraFlyWorld;     // X360 +0x14

        ArbStateTestbed          mArbStateTestbed;            // X360 +0x30 (vtable Construct @+0x30)

        // The normal-camera source selector (X360 +0x2EC): CycleNormalCamera /
        // GetNormalCamera @0x822087F0/@0x821F5BD8 route to the TESTBED state's camera
        // (mArbStateTestbed.mCamera == arb +0x40) when this reads 2, else to the
        // container's current state. FLAG: name inferred from that role (the trimmed
        // DWARF omits the member; only the ==2 compare is attested).
        s32                      miDebugCameraMode;           // X360 +0x2EC

        // FLAG: two Arbitrator scratch flags the trimmed DWARF omits but the asm PROVES exist
        //   (X360 +0x300 / +0x301): Construct seeds +0x300 = 1 and +0x301 = 0, and
        //   UpdateCameraCycleControl reads/writes +0x301 as the normal-camera cycle selector
        //   (the (cntlzw & 0x20) toggle). Named here from their role; only mbCycleNormalCamSelector
        //   is touched by a reconstructed body in this TU.
        bool                     mbCameraCycleInitialised;    // X360 +0x300 : Construct = 1
        bool                     mbCycleNormalCamSelector;    // X360 +0x301 : cycle selector toggle

        bool                     mbUseDebugFlyWorld;          // X360 ~+0x302 (DWARF order; not written by Construct)
        bool                     mbUseDebugCameras;           // X360 ~+0x303 (DWARF order; not written by Construct)

        ArbitratorStateContainer mStateContainer;             // X360 +0x310
        SharedCameraContainer    mSharedCameraContainer;       // X360 +0x38E0 region
        ArbStateCrashNav         mArbStateCrashNav;            // X360 +0x3910 (vtable Construct)
        ArbStateAttractMode      mArbStateAttractMode;         // X360 +0x41A0 (vtable Construct)
        ArbStateRenderMetrics    mArbStateRenderMetrics;       // X360 +0x4340 (vtable Construct)

        Camera::BehaviourHandle<Camera::BehaviourIceAnim> mFinalEliteCam;   // X360 +0x44E0

        f32                      mfTimeCycleCameraHeld;        // X360 +0x44F4
        EState                   meState;                      // X360 +0x44F8

        bool                     mbStartOfGame;                // X360 +0x44FC
        bool                     mbDoAttractMode;              // X360 +0x44FD
        bool                     mbDoRenderMetrics;            // X360 +0x44FE
        u8                       muRenderMetricsArg;           // X360 +0x44FF (the StartRenderMetrics content's
                                                               //   first byte -- an ASCII digit from the tool;
                                                               //   consumed by the render-metrics state, role
                                                               //   un-attested by this TU)
        bool                     mbWasDoingTrainingLastFrame;  // X360 +0x4500
        f32                      mfSlomoFactor;                // X360 +0x4504
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_BRN_DIRECTOR_ARBITRATOR_H
