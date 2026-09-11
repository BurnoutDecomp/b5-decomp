// ============================================================================
// SDKs/Packages/ICE/ICEWrapper.cpp
//
// Runtime bodies for BrnDirector::ICEWrapper (the director-side ICE owner). The home
// (member layout) is GameSource/Director/BrnDirectorICEWrapper.h; members are accessed
// BY NAME here -- the struct-relative offsets quoted in comments are provenance only,
// never used as casts. The two functions left in this TU:
//   Update           per-frame: cache spaces, scale sim time, advance + render + drive mover
//   UpdateAction     queue this frame's dev-tools input actions
//
// The ctor / EditorOn / EditorOff / ReconstructCameraMover bodies live in the sibling
// TU GameSource/Director/BrnDirectorICEWrapper.cpp (same home); Construct, Destruct,
// PlayMovie, GetCurrentMovie and IsPlayingMovie are split into ICEWrapper_wG_11.cpp so
// they can be on the link while this TU cannot -- the two converter tables UpdateAction
// indexes are still unhomed (see the FLAG below).
// ============================================================================

#include "GameSource/Director/BrnDirectorICEWrapper.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatusInterface (sim-time step)

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// FLAG (unhomed dev-tools dependency): the two control->action converter tables
// UpdateAction maps inputs through. The recorded code reads them as two file-scope
// arrays (NOT this-relative members) indexed by control 0..21: the press table for
// just-pressed controls, the pad/held table for is-pressed controls. Their element is
// ICE::EControlToICEAction (a single action id). Their CONTENTS are not recovered, so
// they are declared extern (the definitions are the not-yet-reconstructed static
// tables; the per-TU `cl /c` gate does not link). Replace with the real homed tables
// when the dev-tools ICE-input mapping TU lands.
// ----------------------------------------------------------------------------
extern const ICE::EControlToICEAction maPressConverter[];
extern const ICE::EControlToICEAction maPadConverter[];

namespace
{
    // Control loop bounds + the release-only control / its action, from UpdateAction's
    // recorded loop (controls 0..21 inclusive; control 21 also handles just-released).
    const s32 KI_NUM_INPUT_CONTROLS    = 22;
    const s32 KI_RELEASE_CONTROL       = 21;
    const s32 KI_RELEASE_ACTION_ID     = 28;
    const f32 KF_RELEASE_ACTION_DATA   = 1.0f;

    // The take channel Update samples for the mover's per-frame integer value.
    const s32 KI_MOVER_VALUE_CHANNEL   = 41;
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEWrapper::Update
//
// Per-frame tick:
//   * copy the incoming reference spaces into the cache,
//   * compute the sim-time scale from the game timer (base step * multiplier),
//   * advance the ICE manager and render the editor,
//   * if a movie is playing OR the editor is active, drive the camera mover: advance
//     its sim time, sample the active take's per-frame value (channel 41) into the
//     mover, then finish the mover's frame.
//
// Member map (provenance): CameraSpaceHandler::operator= (+0x11ED0 = lrSpace);
// mfTimeScale = game-timer-status (+8) * (+4) (+0x9B20); ICEManager::Update (+0xA40);
// ICEController::Render (mController, +0x2750); the gate reads mbPlaybackDataSet
// (manager +0x1CE0) || editor menus-active (+0x2BA8); the mover work is at +0x11BD0,
// its take pointer at +0x110, its per-frame value at +0x188.
// ----------------------------------------------------------------------------
void ICEWrapper::Update(const CgsSystem::TimerStatusInterface* lpTimer,
                        const ICE::CameraSpaceHandler& lrSpace)
{
    // Cache this frame's reference spaces.
    mCameraSpaceHandler = lrSpace;

    // Sim-time scale is the PRODUCT of the game timer status' two step fields read
    // directly off the timer (the +8 multiplier times the +4 base step). FLAG: field
    // names are mfTimeStepMultiplier (+8) and mfBaseTimeStep (+4) of the game TimerStatus,
    // which sits at the head of the TimerStatusInterface.
    const CgsSystem::TimerStatus* lpGameStatus = lpTimer->GetGameTimerStatus();
    mfTimeScale = lpGameStatus->GetTimeStepMultiplier() * lpGameStatus->GetBaseTimeStep();

    // Advance the manager, then let the editor draw its overlay.
    mICEManager.Update();
    mICEManager.GetEditor().Render();

    // Drive the mover while a movie is playing or the editor is up.
    if (mICEManager.IsPlaybackDataSet() || mICEManager.GetEditor().AreMenusActive())
    {
        const f32 lfSimTime = 1.0f;

        ICE::ICETake* lpTake = mCameraMover.GetTake();
        if (lpTake != 0)
        {
            mCameraMover.UpdateSimTime(lfSimTime);

            lpTake = mCameraMover.GetTake();
            if (lpTake->GetParameter() <= 0.0f)
                mCameraMover.SetCurrentTakeValueInt(0);

            mCameraMover.SetCurrentTakeValueInt(lpTake->GetValueInt(KI_MOVER_VALUE_CHANNEL));
        }

        mCameraMover.UpdateFrameEnd(lfSimTime);
    }
}

// ----------------------------------------------------------------------------
// BrnDirector::ICEWrapper::UpdateAction
//
// Translate this frame's dev-tools controller input into queued ICE actions (only when
// input is accepted). The action queue is Cleared, then for each control 0..21:
//   * just-pressed  -> push { press-converter[control], control value }
//   * is-pressed    -> push { pad-converter[control],   control value }
// and, for the release control (21) only, just-released -> push { 28, 1.0 }.
// The press / held pushes are bounded by the queue's capacity (skipped when full);
// the release push checks IsFull explicitly. Each push asserts the queue was
// Construct/Clear'd (the Stack's used-before-Construct invariant).
//
// Member map (provenance): the accept-input gate is mbAcceptInput (+0x12108); the
// queue is mActionQueue (+0x11B28, length word +0x11BC8); the converter tables are the
// two file-scope arrays; the control value is controller+4*control.
// ----------------------------------------------------------------------------
void ICEWrapper::UpdateAction(const Camera::Utils::DebugController& lrController)
{
    // The control index is an int in the recorded loop; the query accessors take the
    // controller's own enum, so each index is named through it explicitly.
    typedef Camera::Utils::DebugController::EControl EControl;

    if (!mbAcceptInput)
        return;

    mActionQueue.Clear();

    for (s32 liControl = 0; liControl < KI_NUM_INPUT_CONTROLS; ++liControl)
    {
        const EControl leControl = static_cast<EControl>(liControl);

        if (lrController.GetJustPressed(leControl))
        {
            ICE::ActionRef lAction;
            lAction.SetID(maPressConverter[liControl].mAction);
            lAction.SetData(lrController.GetControlValue(liControl));
            // Bounded by the queue's capacity (the recorded code skips the push when full).
            if (!mActionQueue.IsFull())
                mActionQueue.Push(lAction);
        }

        if (lrController.GetIsPressed(leControl))
        {
            ICE::ActionRef lAction;
            lAction.SetID(maPadConverter[liControl].mAction);
            lAction.SetData(lrController.GetControlValue(liControl));
            if (!mActionQueue.IsFull())
                mActionQueue.Push(lAction);
        }

        if (liControl == KI_RELEASE_CONTROL && lrController.GetJustReleased(static_cast<EControl>(KI_RELEASE_CONTROL)))
        {
            ICE::ActionRef lAction;
            lAction.SetID(KI_RELEASE_ACTION_ID);
            lAction.SetData(KF_RELEASE_ACTION_DATA);
            if (!mActionQueue.IsFull())
                mActionQueue.Push(lAction);
        }
    }
}

} // namespace BrnDirector
