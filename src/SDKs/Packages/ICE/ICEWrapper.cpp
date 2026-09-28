// ============================================================================
// SDKs/Packages/ICE/ICEWrapper.cpp
//
// Runtime bodies for BrnDirector::ICEWrapper (the director-side ICE owner). The home
// (member layout) is GameSource/Director/BrnDirectorICEWrapper.h; members are accessed
// BY NAME here -- the struct-relative offsets quoted in comments are provenance only,
// never used as casts. The one function left in this TU:
//   UpdateAction     queue this frame's dev-tools input actions
// (Update moved to ICEWrapper_wG_11.cpp on 2026-09-27.)
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

}

// ----------------------------------------------------------------------------
// BrnDirector::ICEWrapper::Update -- MOVED 2026-09-27 (OWNERLIST lane L5) to
// SDKs/Packages/ICE/ICEWrapper_wG_11.cpp, which is in the link: it indexes neither converter
// table, and MainDirector::UpdateICE @0x82238FC0 calls it every frame.
// ----------------------------------------------------------------------------

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
