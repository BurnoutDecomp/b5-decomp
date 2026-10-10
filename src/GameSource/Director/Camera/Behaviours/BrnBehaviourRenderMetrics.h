#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                        // Vector3
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"       // the Camera::Behaviour base
#include "GameSource/Director/Utils/BrnDirectorPostOfficeTypes.h"  // BrnDirector::LineTestNearestPostBox
#include "SDKs/EA/GameTalk/GameTalk.h"                             // EA::GameTalk::GameTalkMessage

#include <cstddef>   // offsetof (the layout pins)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourRenderMetrics.h
//
// BrnDirector::Camera::BehaviourRenderMetrics -- the render-metrics probe camera. An authoring
// tool drives it over the GameTalk "Camera" channel: "GetMetrics" names a world position, the
// behaviour drops a line test straight down from it to find the ground, parks the camera 2 m
// above the hit and then turns it through eight horizontal look directions, raising the camera
// state's record-metrics flag at each so the renderer can sample its metrics there, and the
// send-metrics flag after the last. Between runs it sits on an optional idle camera the tool
// places with "SetIdleMetricsCamera". ArbStateRenderMetrics allocates it.
//
// It derives the canonical Camera::Behaviour and overrides slots 0, 1, 2, 6 and 7 of the base's
// table; slots 3, 4 and 5 are the base's. Like the road runner it introduces its own virtual
// SetParameters / GetParameters pair over Behaviour::Parameters, which become slots 8 and 9.
//
// Layout (declaration order; console offsets, the host widens the base head and the pointer):
//   +0x020 mLineTest          +0x070 mCamPosition      +0x080 mGroundNormal
//   +0x090 mIdlePosition      +0x0A0 mIdleDirection    +0x0B0 mbIdleCamOn
//   +0x0B4 mfYOffset          +0x0B8 mpParameters      +0x0BC meCameraState
//   +0x0C0 meAfterWaitState   +0x0C4 mfTimeWaited      +0x0C8 mfTimeToWait
//   +0x0CC muAnglesChecked    +0x0CD mbWaitingForLineTest +0x0CE mbTargetReady
//   +0x0D0 mInitialPosition   (console size 0xE0)
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// The camera-behaviour type tag SetParameters asserts on (the block's leading word is compared
// against 17).
enum EBehaviourTypeRenderMetrics
{
    eBehaviourRenderMetrics = 17
};

class BehaviourRenderMetrics : public Behaviour
{
public:
    // The probe's state machine (the Update switch key).
    enum ECameraState
    {
        E_WAITING_TO_START = 0,   // idle: announce readiness to the tool, show the idle camera
        E_START            = 1,   // a position was requested: start the ground line test
        E_WAIT             = 2,   // wait for the line test, then for the camera to settle
        E_RECORD_METRICS   = 3,   // record this direction's metrics, turn to the next
        E_FINISHED         = 4
    };

    // The render-metrics parameter block: the Behaviour::Parameters head only.
    class Parameters : public Behaviour::Parameters
    {
    };

    // ---- the Behaviour virtual interface -----------------------------------------------------
    void        Construct() override;                                                    // slot 0
    bool        Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo) override;        // slot 1
    bool        Update(Camera& lrCamera, const BehaviourSharedInfo& lrSharedInfo) override; // slot 2
    void        SetupTweaker(Utils::Tweaker& lrTweaker) override;                         // slot 6
    const char* GetName() const override;                                                 // slot 7

    // ---- this class's own virtuals (slots 8 and 9; the base's non-virtual pair is hidden) ----
    virtual void                         SetParameters(const Behaviour::Parameters* lpParameters);
    virtual const Behaviour::Parameters* GetParameters() const;

    // The GameTalk "Camera" channel callback Prepare registers with this behaviour as the
    // context; the manager hands back the address of its context slot. Per key of the message:
    //   "GetMetrics"           content = 3 float32s, a world position -> SetToGetMetrics.
    //   "SetIdleMetricsCamera" content = 6 float32s (asserted): the idle camera's position and
    //                          look direction; the idle camera goes on.
    //   "StopRenderMetrics"    back to E_WAITING_TO_START.
    static void GameTalkMessageReceiver(EA::GameTalk::GameTalkMessage* lpMessage, void* lpContext);

private:
    // Wait lfTimeToWait seconds (counted only while the world is fully streamed), then go to
    // leAfterWaitState.
    void SetToWait(f32 lfTimeToWait, ECameraState leAfterWaitState);

    // A "GetMetrics" request: start a run at lPosition unless one is already in progress.
    void SetToGetMetrics(Vector3 lPosition);

    // Line-test straight down from lPosition for the ground under it.
    void RequestPosition(const BehaviourSharedInfo& lrSharedInfo, Vector3 lPosition);

    // Tell the tool this target is ready (activation id, address, build stamp, platform).
    void SendReadyMessage(const BehaviourSharedInfo& lrSharedInfo);

    static const u8  KU_NUM_ANGLES = 8;
    static const f32 KF_ROTATION_WAIT;   // settle time after each turn
    static const f32 KF_MOVE_WAIT;       // settle time after the move to the probe position

    // The eight horizontal directions (45 degrees apart) each look direction is crossed from.
    static Vector3 saAngles[KU_NUM_ANGLES];

    BrnDirector::LineTestNearestPostBox mLineTest;
    Vector3                             mCamPosition;
    Vector3                             mGroundNormal;
    Vector3                             mIdlePosition;
    Vector3                             mIdleDirection;
    bool                                mbIdleCamOn;
    f32                                 mfYOffset;
    const Parameters*                   mpParameters;
    ECameraState                        meCameraState;
    ECameraState                        meAfterWaitState;
    f32                                 mfTimeWaited;
    f32                                 mfTimeToWait;
    u8                                  muAnglesChecked;
    bool                                mbWaitingForLineTest;
    bool                                mbTargetReady;
    Vector3                             mInitialPosition;

public:
    // NEVER CALLED. Pins the member order the console layout fixes.
    static void _AssertLayout()
    {
        typedef BehaviourRenderMetrics T;
        static_assert(offsetof(T, mLineTest) < offsetof(T, mCamPosition) &&
                      offsetof(T, mCamPosition) < offsetof(T, mGroundNormal) &&
                      offsetof(T, mGroundNormal) < offsetof(T, mIdlePosition) &&
                      offsetof(T, mIdlePosition) < offsetof(T, mIdleDirection) &&
                      offsetof(T, mIdleDirection) < offsetof(T, mbIdleCamOn) &&
                      offsetof(T, mbIdleCamOn) < offsetof(T, mfYOffset) &&
                      offsetof(T, mfYOffset) < offsetof(T, mpParameters) &&
                      offsetof(T, mpParameters) < offsetof(T, meCameraState) &&
                      offsetof(T, meCameraState) < offsetof(T, meAfterWaitState) &&
                      offsetof(T, meAfterWaitState) < offsetof(T, mfTimeWaited) &&
                      offsetof(T, mfTimeWaited) < offsetof(T, mfTimeToWait) &&
                      offsetof(T, mfTimeToWait) < offsetof(T, muAnglesChecked) &&
                      offsetof(T, mInitialPosition) > offsetof(T, mbTargetReady),
                      "BehaviourRenderMetrics: members in console order");
        static_assert(offsetof(T, mbWaitingForLineTest) == offsetof(T, muAnglesChecked) + 1 &&
                      offsetof(T, mbTargetReady) == offsetof(T, muAnglesChecked) + 2,
                      "BehaviourRenderMetrics: the three byte flags are adjacent (+0xCC / +0xCD / +0xCE)");
    }
};

} // namespace Camera
} // namespace BrnDirector
