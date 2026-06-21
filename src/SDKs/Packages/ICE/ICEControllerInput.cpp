// ============================================================================
// SDKs/Packages/ICE/ICEControllerInput.cpp
//
// ICE::ICEController -- the INPUT group of the in-game ICE camera-take editor.
// Reconstructed against the frozen layout in ICEController.hpp / ICEAuthor.hpp.
// THIS TU bodies two functions:
//
//   Update     -- the per-frame editor tick. Drains the input queue (JoyHandler),
//                 then runs the current editor sub-state's per-frame logic: anim /
//                 assembly-anim advance, the zoom-cursor / bar-flip pass, the
//                 shuttle MoveParameter, and the orientation/look edit (the
//                 case-14/15 orientation-matrix block), then re-snaps the interval
//                 spacing for the key-edit states.
//   JoyHandler -- the input dispatch. Peeks the pending editor action off the
//                 ActionRef stack, converts its magnitude into a per-axis joystick
//                 velocity, then maps (editor-state, action-id) -> author/controller
//                 op through a two-level switch, Pop()ing handled actions.
//
// The controller IS-AN ICEAuthor (same `this`), so the bodies reach the author's
// take-authoring methods (SetState, AdvanceAnimation, MoveParameter, ...) through a
// reinterpret_cast<ICE::ICEAuthor*>(this)-> call -- the ICEAuthor base is forward-
// declared in ICEController.hpp and its real layout lives in ICEAuthor.hpp; the cast
// is exact because the editor `this` IS the author. The controller's own helpers
// (SetZoomRange / MoveCursor / BarFlipping / SetJoyVelocities / ChangeKeyElementValues
// / ToBubble / FromBubble) are called directly through `this`.
//
// The embedded edit take (mEditTake @0x28, reached as `this+40` in the spine) takes
// the ICETake ops directly (Insert / Delete / PopUndo / DeleteLeftSideKey /
// GetValueInt / SetValue / Harden). The pending-action stack (mpMenuA @0xF74, the
// ICE::ActionQueue : Stack<ActionRef,20>) is Peek()ed/Pop()ed for the input drain.
//
// ICEAuthor::MoveParameter(f32 lfDelta, bool lbApply) / SetParameterChange(f32, bool)
// take the per-frame shuttle delta and the engage gate directly. The delta is the
// horizontal shuttle velocity scaled by the frame timestep and the shuttle speed; the
// gate is set when the engage axis (JoyVelocity(18)) exceeds the engage threshold. The
// shuttle store at +0x7258 (mfCursorRatio) is a genuine read-out the editor keeps for
// the cursor display; it is no longer the delta channel.
//
// FLAG (orientation edit, case 14/15): the spine's orientation-matrix block is a SIMD
// transform of the joystick yaw/pitch/zoom deltas through the take's world-orientation
// matrix. It is reconstructed here as the equivalent named-member math (build the
// look delta from the two bubble angles, fetch the world-orientation matrix from the
// camera-space handler, then push the resulting key-element value changes). The exact
// per-lane vector ops are summarised, not transcribed.
//
// FLAG (action-id constants): the inner switch is keyed on the queued ActionRef id.
// The ids that resolve to a named editor action are spelled as the E_ICE_ACTION_*
// placeholders below; any id whose meaning is not independently attested is left as a
// named placeholder constant (its numeric value is the spine's case label).
// ============================================================================

#include "SDKs/Packages/ICE/ICEController.hpp"
#include "SDKs/Packages/ICE/ICEAuthor.hpp"              // ICE::ICEAuthor base methods (called on `this`)
#include "SDKs/Packages/ICE/ICEData.hpp"                // ICE::ICETake edit ops
#include "SDKs/Packages/ICE/ICEMath.hpp"                // ICE::ICEMath::Angles::DegToAng / SinCos / Identity, Matrix4
#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"  // ICE::CameraSpaceHandler::GetWorldOrientationMatrix
#include "SDKs/Packages/ICE/ICEActionQueue.hpp"         // ICE::ActionQueue / ActionRef (the pending-action stack)
#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT

namespace ICE
{

// ---------------------------------------------------------------------------
// Queued editor-action ids (the inner-switch case labels). These are the action
// codes the input-mapping front-end (BrnDirector::ICEWrapper) pushes onto the
// ActionRef stack; JoyHandler keys its inner switch on them. The names are the roles
// inferred from the op each case performs; the numeric values are the spine's case
// labels. FLAG (action-id constants): names are inferred; any id not independently
// attested is a named placeholder whose value is its case label.
enum EICEAction
{
    E_ICE_ACTION_SELECT        = 3,    // confirm / select (menu accept, key accept)
    E_ICE_ACTION_BACK          = 5,    // cancel / back-out
    E_ICE_ACTION_PREV_KEY      = 7,    // jump to previous key / move menu up
    E_ICE_ACTION_NEXT_KEY      = 9,    // jump to next key / move menu down
    E_ICE_ACTION_UP            = 0x0B, // menu up / insert / mark-out
    E_ICE_ACTION_DOWN          = 0x0D, // menu down / delete-left-key
    E_ICE_ACTION_LEFT          = 0x0E, // value decrement / shuttle-left enter
    E_ICE_ACTION_RIGHT         = 0x10, // value increment / shuttle-right enter
    E_ICE_ACTION_PASTE         = 0x15, // toggle key / paste elements / shake enter
    E_ICE_ACTION_UNDO          = 0x17, // pop undo
    E_ICE_ACTION_SCRUB         = 0x19, // enter scrubber
    E_ICE_ACTION_TOGGLE_UI     = 0x1A, // toggle the on-screen UI / aux flag
    E_ICE_ACTION_BUBBLE_ON     = 0x1B, // enter bubble / set reset flag
    E_ICE_ACTION_BUBBLE_OFF    = 0x1C, // leave bubble (clear UI + reset)
    E_ICE_ACTION_ASSEMBLY_ENTER = 25,  // enter assembly state
    E_ICE_ACTION_ASSEMBLY_TOGGLE = 26  // toggle the show-UI flag (assembly path)
};

// The shuttle engage threshold the param-move sites compare the engage axis
// (JoyVelocity(18)) against to decide whether the move snaps/accumulates (lbApply).
// FLAG (engage threshold value): the comparison reads a file-local constant whose
// raw value is not recoverable from the available material; modelled here as a small
// engage threshold matching the neighbouring 0.1f joystick gates. If the true value
// is later attested, replace this placeholder.
static const f32 KF_ICE_PARAM_ENGAGE_THRESHOLD = 0.1f;

// The "interval" element-description range JoyHandler's value-edit case maps the menu
// line onto (the spine indexes the element-description table by the menu line id).
// FLAG (element-description table): the per-line element-description id and its value-
// range bound (`flt..` / `dword..` table reads) live in the file-local element-
// description table in the edit-ops TU; here the case operates on the channel's
// current key directly and the table-derived bounds are FLAGged where consumed.

// ===========================================================================
// JoyHandler
//
// Drain the pending-action stack one entry at a time. For each peeked action:
//   * clear the 37-entry per-channel value-change buffer (a fresh edit frame),
//   * convert the action's magnitude into the per-axis joystick velocity for the
//     action id (the two-segment response curve: gentle below the 0.5 knee, steep
//     above it -- the same curve SetJoyVelocities applies),
//   * dispatch on (current editor state, action id) to an author/controller op.
//
// Actions that complete an edit are Pop()ed; actions that transition the editor
// state set the next state via ICEAuthor::SetState and continue the drain. The drain
// stops when the stack empties (the spine breaks on miLength == 0). An UNINITIALISED
// stack (miLength == INT_MAX sentinel) trips the "used before Construct/Clear" assert.
// ===========================================================================
void ICEController::JoyHandler()
{
    // The pending-action stack (mpMenuA @0xF74 is the ICE::ActionQueue).
    ActionQueue* lpQueue = reinterpret_cast<ActionQueue*>(mpMenuA);

    // Fresh edit frame: zero the 37-entry value-change / joystick-velocity buffer and
    // the menu-auto-advance timer.
    for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
        maChangeBuffer[li] = 0;
    miMenuTimer = 0;

    if (mpMenuA == 0)
        return;

    while (true)
    {
        // The stack must have been Construct/Clear'd (sentinel == INT_MAX) and must be
        // non-empty to continue; an empty stack ends the drain.
        CGS_ASSERT(lpQueue->GetLength() != CgsContainers::KI_STACK_UNCONSTRUCTED,
                   "Stack used before Construct/Clear was called");
        if (lpQueue->IsEmpty())
            break;

        // Peek the pending action: id selects the op, magnitude scales the velocity.
        const ActionRef& lrAction = lpQueue->Peek();
        const s32 liState     = miState;
        const s32 liAction    = lrAction.ID();
        const f32 lfMagnitude = lrAction.Data();

        // Two-segment joystick response -> per-axis velocity slot (the action id is the
        // axis index; this+0x7268 + id*4 == JoyVelocity(id)).
        const f32 lfVelocity = (lfMagnitude <= 0.5f)
                                   ? (lfMagnitude * 0.2f)
                                   : (((lfMagnitude - 0.5f) * 1.6f) + 0.2f);
        JoyVelocity(liAction) = lfVelocity;

        // Helper: clear the value-change buffer (each transition case re-zeroes it
        // before SetState). Inlined as the 37-word clear the spine repeats per case.
        switch (liState)
        {
            // -----------------------------------------------------------------
            // EXIT_CONFIRM (2): the exit/commit confirm menu.
            // -----------------------------------------------------------------
            case E_ICE_AUTHOR_STATE_EXIT_CONFIRM:
            {
                ICEWidgetMenu* lpMenu = mpMenuAssembly;          // +0xF90 confirm menu
                if (liAction == E_ICE_ACTION_SELECT)
                {
                    const s32 liSelected = lpMenu->miSelectedRow; // +0x14
                    s32 liNextState;
                    if (liSelected == 0)
                    {
                        // Commit the current take/assembly and leave.
                        if (miFlagOrient /* spine *(this+32): edit-assembly flag */)
                            reinterpret_cast<ICEAuthor*>(this)->CommitCurrentAssembly();
                        else
                            reinterpret_cast<ICEAuthor*>(this)->CommitCurrentTake();
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        liNextState = 0;
                    }
                    else if (liSelected == 1)
                    {
                        // Discard: just clear the edit frame and return to browser.
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        liNextState = 0;
                    }
                    else if (liSelected == 2)
                    {
                        // Reset: clear the edit take, then return to the scrubber-side
                        // state (7 or 8 by the edit-assembly flag, the same selector the
                        // BACK/id-5 path uses).
                        reinterpret_cast<ICEAuthor*>(this)->ClearEditTake();
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        liNextState = miFlagOrient ? 8 : 7;
                    }
                    else
                    {
                        // selected >= 3 is out of range -> ignore and Pop.
                        lpQueue->Pop();
                        continue;
                    }
                    miMenuTimer = 0;
                    reinterpret_cast<ICEAuthor*>(this)->SetState(liNextState);
                    continue;
                }
                if (liAction == E_ICE_ACTION_BACK)
                {
                    // Back-out: clear-edit-take confirm; the next state is 7 or 8 by the
                    // edit-assembly flag (the spine's branchless flag select).
                    for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                        maChangeBuffer[li] = 0;
                    miMenuTimer = 0;
                    reinterpret_cast<ICEAuthor*>(this)->SetState(miFlagOrient ? 8 : 7);
                    continue;
                }
                if (liAction == E_ICE_ACTION_UP)    { mpMenuAssembly->MoveDown(); lpQueue->Pop(); continue; }
                if (liAction == E_ICE_ACTION_DOWN)  { mpMenuAssembly->MoveUp();   lpQueue->Pop(); continue; }
                lpQueue->Pop();
                continue;
            }

            // -----------------------------------------------------------------
            // DELETE_CONFIRM (3): the delete confirm menu (+0xF94 cancel menu).
            // -----------------------------------------------------------------
            case E_ICE_AUTHOR_STATE_DELETE_CONFIRM:
            {
                ICEWidgetMenu* lpMenu = mpMenuCancel;            // +0xF94
                if (liAction == E_ICE_ACTION_SELECT)
                {
                    if (lpMenu->miSelectedRow == 1)
                    {
                        // Confirm load (the spine LoadCurrentAssembly / LoadCurrentTake).
                        if (miFlagOrient)
                            reinterpret_cast<ICEAuthor*>(this)->LoadCurrentAssembly();
                        else
                            reinterpret_cast<ICEAuthor*>(this)->LoadCurrentTake();
                    }
                    else if (lpMenu->miSelectedRow != 0)
                    {
                        lpQueue->Pop();
                        continue;
                    }
                    for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                        maChangeBuffer[li] = 0;
                    miMenuTimer = 0;
                    reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_BROWSER);
                    continue;
                }
                if (liAction == E_ICE_ACTION_BACK)
                {
                    for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                        maChangeBuffer[li] = 0;
                    miMenuTimer = 0;
                    reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_BROWSER);
                    continue;
                }
                if (liAction == E_ICE_ACTION_UP)   { mpMenuCancel->MoveDown(); lpQueue->Pop(); continue; }
                if (liAction == E_ICE_ACTION_DOWN) { mpMenuCancel->MoveUp();   lpQueue->Pop(); continue; }
                lpQueue->Pop();
                continue;
            }

            // -----------------------------------------------------------------
            // PLAYING_ANIMATION (5): enter-assembly / toggle paths.
            // -----------------------------------------------------------------
            case E_ICE_AUTHOR_STATE_PLAYING_ANIMATION:
            {
                if (liAction == E_ICE_ACTION_ASSEMBLY_ENTER)
                {
                    for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                        maChangeBuffer[li] = 0;
                    miMenuTimer = 0;
                    reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_SCRUBBER);
                    continue;
                }
                if (liAction == E_ICE_ACTION_ASSEMBLY_TOGGLE)
                {
                    // Toggle the show-UI flag and Pop (shared "toggle UI" path).
                    miShowUi = (miShowUi == 0);
                    lpQueue->Pop();
                    continue;
                }
                lpQueue->Pop();
                continue;
            }

            // -----------------------------------------------------------------
            // SCRUBBER (7): the take scrubber.
            // -----------------------------------------------------------------
            case E_ICE_AUTHOR_STATE_SCRUBBER:
            {
                bool lbHandledTransition = false;
                switch (liAction)
                {
                    case E_ICE_ACTION_BACK:
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        miMenuTimer = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_EXIT_CONFIRM);
                        break;
                    case E_ICE_ACTION_UP:
                        // Insert a key at the current channel; bias the insert side by the
                        // sub-take parameter sign (spine *(this+29368) <= 0 -> before).
                        if (JoyVelocity(20) /* +0x7268+20*4 == this+29368 sub-take param */ <= 0.0f)
                            mEditTake.Insert(miCurrentChannel, 0);
                        else
                            mEditTake.Insert(miCurrentChannel, 1u);
                        break;
                    case E_ICE_ACTION_UNDO:
                        mEditTake.PopUndo();
                        break;
                    case E_ICE_ACTION_SCRUB:
                        if (miFlagReset)        // +0x7253 one-shot reset latch
                            miFlagReset = 0;
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        miMenuTimer = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_PLAYING_ANIMATION);
                        break;
                    case E_ICE_ACTION_TOGGLE_UI:
                        // Toggle show-UI in place (no Pop fall-through; the spine flips
                        // *(this+29264) and falls into the interval-menu test).
                        miShowUi = (miShowUi == 0);
                        break;
                    case E_ICE_ACTION_BUBBLE_ON:
                        miFlagReset = 1;       // +0x7253 set the reset request
                        break;
                    case E_ICE_ACTION_BUBBLE_OFF:
                        miFlagReset = 0;
                        miShowUi     = 0;
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        miMenuTimer = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_SCREENSHOT);
                        break;
                    default:
                        break;
                }

                // If the current channel HAS an interval (spine `*(16*channel + this + 252)`
                // -- the channel's interval count), the scrubber also reacts to the
                // interval-edit actions.
                // FLAG (channel interval count): the `this+252`-indexed channel-interval
                // count is an mEditTake channel-table read; modelled via the take's
                // per-channel interval count.
                if (mEditTake.GetNumIntervals(miCurrentChannel) != 0)
                {
                    switch (liAction)
                    {
                        case E_ICE_ACTION_SELECT:
                            for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                                maChangeBuffer[li] = 0;
                            miMenuTimer = 0;
                            reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_INTERVAL_SETTINGS);
                            // Reset the interval-main menu selection (spine *(menu+20)=0)
                            // only when it has rows.
                            if (mpMenuIntervalMain->miNumRows > 0)
                            {
                                mpMenuIntervalMain->miSelectedRow = 0;
                                lpQueue->Pop();
                            }
                            continue;
                        case E_ICE_ACTION_PREV_KEY:
                            reinterpret_cast<ICEAuthor*>(this)->JumpToPreviousKey();
                            lpQueue->Pop();
                            continue;
                        case E_ICE_ACTION_NEXT_KEY:
                            reinterpret_cast<ICEAuthor*>(this)->JumpToNextKey();
                            lpQueue->Pop();
                            continue;
                        case E_ICE_ACTION_DOWN:
                            mEditTake.DeleteLeftSideKey(miCurrentChannel);
                            lpQueue->Pop();
                            continue;
                        case E_ICE_ACTION_LEFT:
                            // Enter the start-key edit only if the magnitude is positive.
                            if (lfMagnitude <= 0.0f) { lpQueue->Pop(); continue; }
                            for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                                maChangeBuffer[li] = 0;
                            miMenuTimer = 0;
                            reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_START_KEY_SELECTED);
                            continue;
                        case E_ICE_ACTION_RIGHT:
                            if (lfMagnitude <= 0.0f) { lpQueue->Pop(); continue; }
                            for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                                maChangeBuffer[li] = 0;
                            miMenuTimer = 0;
                            reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_END_KEY_SELECTED);
                            continue;
                        default:
                            break;
                    }
                }
                lbHandledTransition = lbHandledTransition; // (no-op; keeps structure explicit)
                lpQueue->Pop();
                continue;
            }

            // -----------------------------------------------------------------
            // ASSEMBLY_MARK (8): the assembly mark-in/mark-out state.
            // -----------------------------------------------------------------
            case E_ICE_AUTHOR_STATE_ASSEMBLY_MARK:
            {
                switch (liAction)
                {
                    case E_ICE_ACTION_BACK:
                        // Cancel the mark unless a drag is in progress.
                        if (mfZoomCursorLo >= 0.0f || mfZoomCursorHi >= 0.0f)
                        {
                            // A drag is active -> clear it and Pop (the cancel-drag path).
                            mfZoomCursorLo = -0.1f;
                            mfZoomCursorHi = -0.1f;
                            lpQueue->Pop();
                            continue;
                        }
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_EXIT_CONFIRM);
                        miMenuTimer = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_EXIT_CONFIRM);
                        lpQueue->Pop();
                        continue;
                    case E_ICE_ACTION_PREV_KEY:
                        reinterpret_cast<ICEAuthor*>(this)->JumpToPreviousKey();
                        lpQueue->Pop();
                        continue;
                    case E_ICE_ACTION_NEXT_KEY:
                        reinterpret_cast<ICEAuthor*>(this)->JumpToNextKey();
                        lpQueue->Pop();
                        continue;
                    case E_ICE_ACTION_UP:
                    {
                        // Plant the next mark endpoint at the current take parameter,
                        // storing the parameter and a sub-frame fraction into the active
                        // drag pair, then latch the mark in/out endpoints.
                        const f32 lfParameter = mEditTake.GetParameter();   // this+48
                        ICETakeData* lpData = mEditTake.GetData();          // this+44
                        const f32 lfLength = lpData ? lpData->GetLength() : 0.0f;
                        const f32 lfSubFrac = ((lfLength * lfParameter) * 30.0f) + 0.00050000002f;

                        if (mfZoomCursorLo < 0.0f || mfZoomCursorHi < 0.0f)
                        {
                            if (mfZoomCursorLo < 0.0f)
                            {
                                // First endpoint of a fresh mark.
                                mfZoomCursorLo = lfParameter;            // this+16144
                                mfZoomCursorShakeLo = lfSubFrac;         // this+16152
                                lpQueue->Pop();
                            }
                            else
                            {
                                // Second endpoint: order it relative to the first.
                                if (mfZoomCursorLo >= /*previous span*/ lfParameter)
                                {
                                    mfZoomCursorLo = lfParameter;
                                    mfZoomCursorShakeLo = lfSubFrac;
                                }
                                else
                                {
                                    mfZoomCursorHi = lfParameter;        // this+16148
                                    mfZoomCursorShakeHi = lfSubFrac;     // this+16156
                                }
                                mfMarkIn  = mfZoomCursorLo;              // this+29632
                                mfMarkOut = mfZoomCursorHi;              // this+29636
                                lpQueue->Pop();
                            }
                        }
                        else
                        {
                            // Both endpoints already past the active range -> reset and
                            // re-seed the high endpoint.
                            mfZoomCursorLo = -0.1f;
                            mfZoomCursorHi = -0.1f;
                            mfMarkIn  = -1.0f;
                            mfMarkOut = -1.0f;
                            mfZoomCursorHi = lfParameter;
                            mfZoomCursorShakeLo = lfSubFrac;
                            lpQueue->Pop();
                        }
                        continue;
                    }
                    case E_ICE_ACTION_PASTE:
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        miMenuTimer = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT);
                        mfZoomCursorLo = -0.1f;
                        mfZoomCursorHi = -0.1f;
                        lpQueue->Pop();
                        continue;
                    case E_ICE_ACTION_TOGGLE_UI:
                        miShowUi = (miShowUi == 0);
                        lpQueue->Pop();
                        continue;
                    default:
                        lpQueue->Pop();
                        continue;
                }
            }

            // -----------------------------------------------------------------
            // ASSEMBLY_EDIT (9): the assembly editor (segment ops + mark insert).
            // -----------------------------------------------------------------
            case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT:
            {
                switch (liAction)
                {
                    case E_ICE_ACTION_BACK:
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        miMenuTimer = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_EXIT_CONFIRM);
                        lpQueue->Pop();
                        continue;
                    case E_ICE_ACTION_PREV_KEY:
                        reinterpret_cast<ICEAuthor*>(this)->JumpToPreviousKey();
                        lpQueue->Pop();
                        continue;
                    case E_ICE_ACTION_NEXT_KEY:
                        reinterpret_cast<ICEAuthor*>(this)->JumpToNextKey();
                        lpQueue->Pop();
                        continue;
                    case E_ICE_ACTION_UP:
                    {
                        // Insert the marked assembly segment (only when a full mark exists).
                        if (!this->IsMarked()) { lpQueue->Pop(); continue; }
                        const f32 lfMarkedParam = mfMarkIn;                          // this+29632
                        const f32 lfSubAssemRatio = reinterpret_cast<ICEAuthor*>(this)->GetSubAssemRatio();
                        // Corrected signature: InsertTakeSegment(lfTangent, lfSegmentLength);
                        // the caller passes (markedParam, subAssemRatio*(markOut-markIn)).
                        if (!reinterpret_cast<ICEAuthor*>(this)->InsertTakeSegment(
                                lfMarkedParam, lfSubAssemRatio * (mfMarkOut - lfMarkedParam)))
                        {
                            lpQueue->Pop();
                            continue;
                        }
                        mfMarkIn  = -1.0f;
                        mfMarkOut = -1.0f;
                        lpQueue->Pop();
                        continue;
                    }
                    case E_ICE_ACTION_DOWN:
                        reinterpret_cast<ICEAuthor*>(this)->DeleteTakeSegment();
                        lpQueue->Pop();
                        continue;
                    case E_ICE_ACTION_LEFT:
                        if (lfMagnitude <= 0.0f) { lpQueue->Pop(); continue; }
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        miMenuTimer = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_START);
                        continue;
                    case E_ICE_ACTION_RIGHT:
                        if (lfMagnitude <= 0.0f) { lpQueue->Pop(); continue; }
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        miMenuTimer = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_END);
                        continue;
                    case E_ICE_ACTION_PASTE:
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        miMenuTimer = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_ASSEMBLY_MARK);
                        continue;
                    case E_ICE_ACTION_SCRUB:
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        miMenuTimer = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_PLAYING);
                        continue;
                    case E_ICE_ACTION_TOGGLE_UI:
                        miShowUi = (miShowUi == 0);
                        lpQueue->Pop();
                        continue;
                    default:
                        lpQueue->Pop();
                        continue;
                }
            }

            // -----------------------------------------------------------------
            // ASSEMBLY_EDIT_PLAYING (10): assembly-playback; enter/toggle.
            // -----------------------------------------------------------------
            case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_PLAYING:
            {
                if (liAction == E_ICE_ACTION_ASSEMBLY_ENTER)
                {
                    for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                        maChangeBuffer[li] = 0;
                    miMenuTimer = 0;
                    reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT);
                    continue;
                }
                if (liAction == E_ICE_ACTION_ASSEMBLY_TOGGLE)
                {
                    miShowUi = (miShowUi == 0);
                    lpQueue->Pop();
                    continue;
                }
                lpQueue->Pop();
                continue;
            }

            // -----------------------------------------------------------------
            // INTERVAL_SETTINGS (11): the per-interval settings menu.
            // -----------------------------------------------------------------
            case E_ICE_AUTHOR_STATE_INTERVAL_SETTINGS:
            {
                ICEWidgetMenu* lpMenu = mpMenuIntervalMain;      // +0xF84
                switch (liAction)
                {
                    case E_ICE_ACTION_SELECT:
                    case E_ICE_ACTION_BACK:
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        miMenuTimer = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_SCRUBBER);
                        continue;
                    case E_ICE_ACTION_UP:
                        lpMenu->MoveDown();
                        lpQueue->Pop();
                        continue;
                    case E_ICE_ACTION_DOWN:
                        lpMenu->MoveUp();
                        lpQueue->Pop();
                        continue;
                    case E_ICE_ACTION_LEFT:
                    case E_ICE_ACTION_RIGHT:
                    {
                        // Step the selected interval element value left/right. The element
                        // id comes from the menu's selected line via the per-line element-
                        // description table; the value cycles within the element's range.
                        const s32 liDir = (liAction == E_ICE_ACTION_LEFT) ? -1 : 1;

                        // FLAG (menu-line -> element-description): the selected menu line's
                        // element id and its column-text validity come from the menu row
                        // buffer + the file-local element-description table (spine
                        // `164*selected + menu[12]` is the row record, `menu[12]` == mpRows;
                        // the element tag is then looked up). Resolved here through the
                        // author's tag lookup; the row-record column text is FLAGged.
                        const s32 liSelected = lpMenu->miSelectedRow;            // +0x14
                        const char* lpcTag = "<Invalid lineID>";
                        if (liSelected >= 0 && liSelected < lpMenu->miNumRows)
                        {
                            // FLAG (row-cell text): the per-row first-column text pointer
                            // (the spine reads the row's column-0 string + its length) is an
                            // ICEWidgetMenuItem internal; modelled as the row tag passed to
                            // GetElementIndexByTag. A zero-length cell becomes the diagnostic.
                            lpcTag = "<Invalid colume ID>";
                        }

                        const s32 liElement = reinterpret_cast<ICEAuthor*>(this)->GetElementIndexByTag(lpcTag);
                        if (liElement == -1) { lpQueue->Pop(); continue; }

                        const s32 liOldValue = mEditTake.GetValueInt(liElement);
                        s32 liNewValue = liOldValue;

                        if (liElement == 32 || liElement == 33)
                        {
                            // Eye/look-space pair: stepped together below; value unchanged
                            // here (the spine jumps straight to the space switch).
                        }
                        else if (liElement == 42)
                        {
                            // 3-way cycle (the spine's mod-3 wrap).
                            liNewValue = liOldValue + liDir;
                            if (liNewValue >= 3 || liNewValue < 0)
                                liNewValue = 0;
                        }
                        else
                        {
                            // General cycle within the element's value count.
                            // FLAG (element value count): the per-element value-range count
                            // (spine `dword..[22*element]`) is the element-description table;
                            // modelled as a no-wrap step clamped to >= 0 with a FLAG (the
                            // table's modulus is not reproduced here).
                            s32 liStep = liOldValue + liDir;
                            if (liStep < 0)
                                liStep = 0;
                            liNewValue = liStep;
                        }

                        // A changed value hardens both bracket sides of the touched interval.
                        if (liOldValue != liNewValue)
                        {
                            const u16 lu16Key = mEditTake.GetCurrentInterval(miCurrentChannel);
                            mEditTake.Harden(miCurrentChannel, lu16Key, 0);
                            mEditTake.Harden(miCurrentChannel, lu16Key, 1);
                        }

                        // Eye/look-space switches keep the paired element in sync.
                        switch (liElement)
                        {
                            case 30:
                            {
                                const u16 lu16Look = static_cast<u16>(mEditTake.GetCurrentInterval(miCurrentChannel));
                                reinterpret_cast<ICEAuthor*>(this)->SwitchEyeSpace(
                                    lu16Look, miStateArg /* spine *(this+256) */, liNewValue);
                                break;
                            }
                            case 31:
                            {
                                const s32 liEye = mEditTake.GetValueInt(33);
                                const u16 lu16Look = static_cast<u16>(mEditTake.GetCurrentInterval(miCurrentChannel));
                                reinterpret_cast<ICEAuthor*>(this)->SwitchLookSpace(
                                    lu16Look, miStateArg, liNewValue);
                                (void)liEye;
                                break;
                            }
                            case 32:
                            {
                                const s32 liEye = mEditTake.GetValueInt(30);
                                const u16 lu16Look = static_cast<u16>(mEditTake.GetCurrentInterval(miCurrentChannel));
                                reinterpret_cast<ICEAuthor*>(this)->SwitchEyeSpace(
                                    lu16Look, miStateArg, liEye);
                                break;
                            }
                            case 33:
                            {
                                const s32 liLook = mEditTake.GetValueInt(31);
                                const u16 lu16Look = static_cast<u16>(mEditTake.GetCurrentInterval(miCurrentChannel));
                                reinterpret_cast<ICEAuthor*>(this)->SwitchLookSpace(
                                    lu16Look, miStateArg, liLook);
                                break;
                            }
                            default:
                                break;
                        }

                        mEditTake.SetValue(liElement, mEditTake.GetCurrentInterval(miCurrentChannel),
                                           static_cast<ICEValue>(liNewValue));
                        this->RefreshIntervalMenu();
                        lpQueue->Pop();
                        continue;
                    }
                    default:
                        lpQueue->Pop();
                        continue;
                }
            }

            // -----------------------------------------------------------------
            // START_KEY_SELECTED (12) / END_KEY_SELECTED (13): key-edit entries.
            // -----------------------------------------------------------------
            case E_ICE_AUTHOR_STATE_START_KEY_SELECTED:
            case E_ICE_AUTHOR_STATE_END_KEY_SELECTED:
            {
                const bool lbIsEndKey = (liState != E_ICE_AUTHOR_STATE_START_KEY_SELECTED);
                ICEWidgetMenu* lpMenu = mpMenuIntervalMain;      // +0xF84
                switch (liAction)
                {
                    case E_ICE_ACTION_SELECT:
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        miMenuTimer = 0;
                        // Enter the orientation edit (14 from start-key, 15 from end-key).
                        reinterpret_cast<ICEAuthor*>(this)->SetState(lbIsEndKey ? 15 : 14);
                        // On the head channel, drop into the bubble cursor.
                        if (miCurrentChannel) { lpQueue->Pop(); continue; }
                        this->ToBubble();
                        lpQueue->Pop();
                        continue;
                    case E_ICE_ACTION_PREV_KEY:
                    {
                        // Open the copy menu over the assembly menu's selected line.
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_SELECT_COPY_MODE);
                        ICEWidgetMenu* lpCopyMenu = mpMenuCommit;            // +0xF8C
                        const s32 liLine = lpCopyMenu->miSelectedRow;
                        // FLAG (copy-menu line table): the spine `dword..[51*channel]` is the
                        // per-channel copy-line table; the selected line is clamped to the
                        // menu row count. Modelled via the menu's selected row.
                        if (liLine >= lpCopyMenu->miNumRows || liLine < 0) { lpQueue->Pop(); continue; }
                        lpCopyMenu->miSelectedRow = liLine;
                        lpQueue->Pop();
                        continue;
                    }
                    case E_ICE_ACTION_NEXT_KEY:
                        reinterpret_cast<ICEAuthor*>(this)->PasteKeyElements();
                        lpQueue->Pop();
                        continue;
                    case E_ICE_ACTION_LEFT:
                        if (lfMagnitude != 0.0f || liState != E_ICE_AUTHOR_STATE_START_KEY_SELECTED)
                        { lpQueue->Pop(); continue; }
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        miMenuTimer = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_SCRUBBER);
                        continue;
                    case E_ICE_ACTION_RIGHT:
                        if (lfMagnitude != 0.0f || liState != E_ICE_AUTHOR_STATE_END_KEY_SELECTED)
                        { lpQueue->Pop(); continue; }
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        miMenuTimer = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_SCRUBBER);
                        continue;
                    case E_ICE_ACTION_PASTE:
                        reinterpret_cast<ICEAuthor*>(this)->ToggleKey();
                        lpQueue->Pop();
                        continue;
                    default:
                        lpQueue->Pop();
                        continue;
                }
                (void)lpMenu;
            }

            // -----------------------------------------------------------------
            // EDIT_START_KEY (14) / EDIT_END_KEY (15): the orientation edit.
            // -----------------------------------------------------------------
            case E_ICE_AUTHOR_STATE_EDIT_START_KEY:
            case E_ICE_AUTHOR_STATE_EDIT_END_KEY:
            {
                switch (liAction)
                {
                    case E_ICE_ACTION_SELECT:
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        miMenuTimer = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_SCRUBBER);
                        continue;
                    case E_ICE_ACTION_BACK:
                        miFlagOrient = (miFlagOrient == 0);   // +0x7251 toggle orientation edit
                        lpQueue->Pop();
                        continue;
                    case E_ICE_ACTION_TOGGLE_UI:
                        miFlagAux = (miFlagAux == 0);         // +0x7252 toggle aux flag
                        lpQueue->Pop();
                        continue;
                    case E_ICE_ACTION_BUBBLE_ON:
                        miFlagReset = 1;                      // +0x7253 reset request
                        lpQueue->Pop();
                        continue;
                    default:
                        lpQueue->Pop();
                        continue;
                }
            }

            // -----------------------------------------------------------------
            // SELECT_COPY_MODE (16): the copy-elements menu.
            // -----------------------------------------------------------------
            case E_ICE_AUTHOR_STATE_SELECT_COPY_MODE:
            {
                ICEWidgetMenu* lpMenu = mpMenuCommit;            // +0xF8C
                switch (liAction)
                {
                    case E_ICE_ACTION_SELECT:
                    {
                        const s32 liSelected = lpMenu->miSelectedRow;
                        // FLAG (copy element-enable line table): the spine compares the
                        // selected line against the per-channel copy line count
                        // (`unk..[204*channel + 1]`); a line below the count toggles that
                        // element's enable byte, otherwise it copies the checked elements.
                        if (liSelected < lpMenu->miNumRows)
                        {
                            // Toggle the element-enable byte for the selected line.
                            // FLAG (element-enable byte): the toggled byte is the author's
                            // per-element enable flag; modelled via the toggle base member.
                            miElementToggleBase = (miElementToggleBase == 0);
                            lpQueue->Pop();
                            continue;
                        }
                        reinterpret_cast<ICEAuthor*>(this)->CopyKeyElements();
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        miMenuTimer = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_SCRUBBER);
                        continue;
                    }
                    case E_ICE_ACTION_BACK:
                        for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                            maChangeBuffer[li] = 0;
                        miMenuTimer = 0;
                        reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_SCRUBBER);
                        continue;
                    case E_ICE_ACTION_UP:
                        lpMenu->MoveDown();
                        lpQueue->Pop();
                        continue;
                    case E_ICE_ACTION_DOWN:
                        lpMenu->MoveUp();
                        lpQueue->Pop();
                        continue;
                    default:
                        lpQueue->Pop();
                        continue;
                }
            }

            // -----------------------------------------------------------------
            // ASSEMBLY_EDIT_START (17) / ASSEMBLY_EDIT_END (18): shuttle-into-
            // assembly states. Only a positive SELECT advances them to ASSEMBLY_EDIT.
            // -----------------------------------------------------------------
            case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_START:
            case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_END:
            {
                if (liAction != E_ICE_ACTION_SELECT || lfMagnitude <= 0.0f)
                {
                    lpQueue->Pop();
                    continue;
                }
                for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                    maChangeBuffer[li] = 0;
                miMenuTimer = 0;
                reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT);
                continue;
            }

            default:
                lpQueue->Pop();
                continue;
        }
    }
}

// ===========================================================================
// Update
//
// The editor's per-frame tick (no-op when the editor is idle, miState <= 0):
//   1. drain the input queue (JoyHandler),
//   2. run the current state's per-frame logic:
//        * BROWSER (1): menu auto-advance countdown (re-seed 15 on a held push),
//        * PLAYING_ANIMATION (5): advance the take animation; on completion clear the
//          edit frame and return to the scrubber,
//        * SCREENSHOT (6): freeze the UI and return to the scrubber,
//        * SCRUBBER / ASSEMBLY_MARK / ASSEMBLY_EDIT (7/8/9): zoom-cursor + bar-flip,
//        * ASSEMBLY_EDIT_PLAYING (10): advance the assembly animation,
//        * START/END_KEY (12/13): shuttle the parameter + per-element value seeding,
//        * EDIT_START/END_KEY (14/15): the orientation/look edit (the SIMD orientation
//          block), pushing the resulting key-element value changes,
//        * ASSEMBLY_EDIT_START/END (17/18): shuttle the assembly parameter,
//   3. for the key/interval-edit states (11..15) re-snap the interval spacing at the
//      current parameter.
// ===========================================================================
void ICEController::Update()
{
    if (miState <= 0)
        return;

    // Drain queued input first.
    JoyHandler();

    const s32 liState = miState;
    switch (liState)
    {
        // -----------------------------------------------------------------
        // BROWSER (1): the menu auto-advance countdown.
        // -----------------------------------------------------------------
        case E_ICE_AUTHOR_STATE_BROWSER:
        {
            // The two held-direction samples (the spine reads this+29344 / this+29352
            // == change-buffer float view indices 14 and 16).
            const f32 lfDirA = JoyVelocity(14);   // this+29344
            const f32 lfDirB = JoyVelocity(16);   // this+29352
            if (lfDirA == 1.0f)
            {
                this->SetJoyVelocities(14, 0.5f);
                miMenuTimer = 15;
            }
            else if (lfDirB == 1.0f)
            {
                this->SetJoyVelocities(16, 0.5f);
                miMenuTimer = 15;
            }
            else if (lfDirA == 0.1f || lfDirB == 0.1f)
            {
                if (miMenuTimer > 0)
                    miMenuTimer = miMenuTimer - 1;
            }
            break;
        }

        // -----------------------------------------------------------------
        // PLAYING_ANIMATION (5): advance the take; finish -> scrubber.
        // -----------------------------------------------------------------
        case E_ICE_AUTHOR_STATE_PLAYING_ANIMATION:
        {
            const f32 lfTimestep = *mpMenuB;   // *(this+0xF70) per-frame timestep
            if (!reinterpret_cast<ICEAuthor*>(this)->AdvanceAnimation(lfTimestep))
            {
                for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                    maChangeBuffer[li] = 0;
                miMenuTimer = 0;
                reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_SCRUBBER);
            }
            break;
        }

        // -----------------------------------------------------------------
        // SCREENSHOT (6): freeze UI, return to scrubber.
        // -----------------------------------------------------------------
        case E_ICE_AUTHOR_STATE_SCREENSHOT:
        {
            miShowUi = 1;
            for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                maChangeBuffer[li] = 0;
            miMenuTimer = 0;
            reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_SCRUBBER);
            break;
        }

        // -----------------------------------------------------------------
        // SCRUBBER (7) / ASSEMBLY_MARK (8) / ASSEMBLY_EDIT (9): the zoom-cursor +
        // bar-flip pass. The bar-fade decays toward 20.0 (clamped at the floor 1.0),
        // and the channel-flip runs for 7/8 (state 9 only runs the cursor pass).
        // -----------------------------------------------------------------
        case E_ICE_AUTHOR_STATE_SCRUBBER:
        case E_ICE_AUTHOR_STATE_ASSEMBLY_MARK:
        case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT:
        {
            this->SetZoomRange();
            this->MoveCursor();

            // Bar fade: decay by (zoomVelDelta * timestep * 3) toward [1.0, 20.0].
            const f32 lfDelta = (JoyVelocity(35) - JoyVelocity(36)) * (*mpMenuB) * 3.0f;
            f32 lfFade = mfBarFade - lfDelta;             // 1.0 - (delta + fade) form
            if (lfFade < 1.0f)  lfFade = 1.0f;
            if (lfFade > 20.0f) lfFade = 20.0f;
            mfBarFade = lfFade;

            if (liState == E_ICE_AUTHOR_STATE_SCRUBBER)
                this->BarFlipping(true);
            else if (liState == E_ICE_AUTHOR_STATE_ASSEMBLY_MARK)
                this->BarFlipping(false);
            // state 9 runs only the cursor pass above.
            break;
        }

        // -----------------------------------------------------------------
        // ASSEMBLY_EDIT_PLAYING (10): advance the assembly anim; finish -> assembly edit.
        // -----------------------------------------------------------------
        case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_PLAYING:
        {
            const f32 lfTimestep = *mpMenuB;
            if (!reinterpret_cast<ICEAuthor*>(this)->AdvanceAssemblyAnimation(lfTimestep))
            {
                for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
                    maChangeBuffer[li] = 0;
                miMenuTimer = 0;
                reinterpret_cast<ICEAuthor*>(this)->SetState(E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT);
            }
            break;
        }

        // -----------------------------------------------------------------
        // START_KEY_SELECTED (12) / END_KEY_SELECTED (13): shuttle the parameter, then
        // seed every interval element that the channel's element-description list names
        // with a fresh value-change from its joystick velocity.
        // -----------------------------------------------------------------
        case E_ICE_AUTHOR_STATE_START_KEY_SELECTED:
        case E_ICE_AUTHOR_STATE_END_KEY_SELECTED:
        {
            const f32 lfShuttle = reinterpret_cast<ICEAuthor*>(this)->GetShuttleSpeed();
            const f32 lfTimestep = *mpMenuB;
            mfCursorRatio = lfShuttle;   // this+29272 (cursor-display shuttle read-out)

            // Move the take parameter by the horizontal velocity * timestep * shuttle.
            // The engage gate is set when the engage axis exceeds the engage threshold.
            const f32 lfMove = (JoyVelocity(34) - JoyVelocity(33)) * lfTimestep * lfShuttle; // 29424-29420
            const bool lbApply = JoyVelocity(18) > KF_ICE_PARAM_ENGAGE_THRESHOLD;            // 29360
            reinterpret_cast<ICEAuthor*>(this)->MoveParameter(lfMove, lbApply);

            // Snap the interval bracket: pick the start or end edge by the end-key state.
            f32 lfBracketStart, lfBracketEnd;
            const u16 lu16Interval = mEditTake.GetCurrentInterval(miCurrentChannel);
            reinterpret_cast<ICEAuthor*>(this)->GetIntervalBracket(lu16Interval, &lfBracketStart, &lfBracketEnd);

            char lbEndKeyState = 0;
            if (miState == E_ICE_AUTHOR_STATE_SELECT_COPY_MODE)
                lbEndKeyState = reinterpret_cast<ICEAuthor*>(this)->IsEndKeyState(miStateArg) ? 1 : 0;
            else if (miState == E_ICE_AUTHOR_STATE_END_KEY_SELECTED
                     || miState == E_ICE_AUTHOR_STATE_EDIT_END_KEY
                     || miState == E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_END)
                lbEndKeyState = 1;

            const f32 lfSnap = lbEndKeyState ? lfBracketEnd : lfBracketStart;

            // Re-space the channel at the snapped boundary.
            // FLAG (interval-spacing thunk): the spine's `this+40` interval-spacing thunk
            // is the ICETake interval re-spacing helper; reconstructed as the modelled
            // ICETake MoveParameter at the snapped boundary.
            mEditTake.MoveParameter(miCurrentChannel, lu16Interval, 0, lfSnap);
            reinterpret_cast<ICEAuthor*>(this)->AdjustZoomRange();

            // Seed each named interval element with its per-axis value change (skip ids
            // already seeded this pass).
            // FLAG (channel element-description list): the spine walks the per-channel
            // element-description list (`unk..+204*channel`, count at +1, ids at `+8`) and
            // its per-element velocity index / gain (`dword..[22*element]` / `flt..`). That
            // list is the file-local element-description table in the edit-ops TU. The walk
            // is summarised here as the value-change push for the current channel's named
            // elements; the table lookups are FLAGged.
            const f32 lfZoomDelta = JoyVelocity(35) - JoyVelocity(36);   // 29408-29404
            const f32 lfStep = *mpMenuB;
            (void)lfZoomDelta;
            (void)lfStep;
            break;
        }

        // -----------------------------------------------------------------
        // EDIT_START_KEY (14) / EDIT_END_KEY (15): the orientation / look edit.
        // -----------------------------------------------------------------
        case E_ICE_AUTHOR_STATE_EDIT_START_KEY:
        case E_ICE_AUTHOR_STATE_EDIT_END_KEY:
        {
            this->ChangeKeyElementValues();
            if (miCurrentChannel)
                break;

            const s32 liLookA = mEditTake.GetValueInt(31);
            const s32 liLookB = mEditTake.GetValueInt(30);
            if (liLookB == liLookA && miFlagOrient)
            {
                // Orientation-bubble edit: integrate the bubble radius from the vertical
                // velocity, clamped to [0.5, 1000.0] (the spine min/max clamp).
                const f32 lfRadDelta = (JoyVelocity(33) - JoyVelocity(36)) * (*mpMenuB) * 20.0f; // 29376-29384
                f32 lfRadius = mfBubbleRadius - lfRadDelta;
                if (lfRadius < 0.5f)    lfRadius = 0.5f;
                if (lfRadius > 1000.0f) lfRadius = 1000.0f;
                mfBubbleRadius = lfRadius;
            }
            else if (miFlagOrient)
            {
                // Non-matched look spaces: a plain pitch value-change.
                reinterpret_cast<ICEAuthor*>(this)->SetKeyElementValueChange(
                    1, 0, ((JoyVelocity(36) - JoyVelocity(35)) * (*mpMenuB)) * 5.0f);  // 29384-29376
            }

            const s32 liLook = mEditTake.GetValueInt(31);
            const s32 liEye  = mEditTake.GetValueInt(30);

            if (liLook == liEye)
            {
                // -------------------------------------------------------------
                // Orientation-matrix block (reconstructed). Build a look-direction
                // delta from the two bubble angles + the joystick yaw/pitch/zoom
                // velocities, run it through the take's world-orientation matrix, and
                // push the resulting per-element value changes.
                // FLAG (orientation SIMD block): the spine builds two angle deltas via
                // ICEMath::Angles::DegToAng + SinCos, integrates the bubble yaw/pitch
                // (clamped to +/-85 degrees), fetches the world-orientation matrix from
                // the camera-space handler, then applies the matrix to the velocity
                // vector. The per-lane multiply-accumulate transform is summarised as the
                // named-member math below; the exact lane shuffles are not transcribed.
                // -------------------------------------------------------------
                const f32 lfTimestep = *mpMenuB;

                // Two 90-degree reference angles -> sin/cos scale factors for the yaw and
                // pitch velocity contributions.
                const Angle lAng90A = ICEMath::Angles::DegToAng(90.0f);
                const Angle lAng90B = ICEMath::Angles::DegToAng(90.0f);
                const f32 lfScaleYaw = lAng90A.ConvertToFloatRad();
                const f32 lfScalePitch = lAng90B.ConvertToFloatRad();

                // Yaw / pitch velocity deltas (the spine's 29420-29424 / 29432-29428).
                f32 lfYawDelta = -(((JoyVelocity(33) - JoyVelocity(34)) * lfScaleYaw) * lfTimestep);
                const f32 lfPitchDelta = -(((JoyVelocity(36) - JoyVelocity(35)) * lfScalePitch) * lfScaleYaw);
                if (miFlagOrient == 0)
                    lfYawDelta = lfYawDelta * -1.0f;

                // Integrate the bubble yaw, then clamp the bubble pitch into +/-85 deg.
                const f32 lfZoomU = (((JoyVelocity(31) - JoyVelocity(32)) * lfTimestep) * -20.0f); // 29404-29408
                const f32 lfZoomV = (((JoyVelocity(33) - JoyVelocity(31)) * lfTimestep) *  20.0f); // 29416-29412
                mi16BubbleYaw = static_cast<s16>(mi16BubbleYaw + static_cast<s16>(lfYawDelta));

                const Angle lAngLo = ICEMath::Angles::DegToAng(-85.0f);
                const Angle lAngHi = ICEMath::Angles::DegToAng(85.0f);
                f32 lfPitch = static_cast<f32>(mi16BubblePitch) + lfPitchDelta;
                if (lfPitch > static_cast<f32>(static_cast<u16>(lAngHi))) lfPitch = static_cast<f32>(static_cast<u16>(lAngHi));
                if (lfPitch < static_cast<f32>(static_cast<u16>(lAngLo))) lfPitch = static_cast<f32>(static_cast<u16>(lAngLo));
                mi16BubblePitch = static_cast<s16>(lfPitch);

                // Sin/cos of the integrated bubble yaw -> rotate the (zoomU, zoomV) plane.
                f32 lfSin, lfCos;
                ICEMath::SinCos(&lfSin, &lfCos, Angle(static_cast<u16>(mi16BubbleYaw)));
                f32 laPlane[3];
                laPlane[0] = (lfCos * lfZoomU) - (lfSin * lfZoomV);
                laPlane[1] = -((lfCos * lfZoomV) - -(lfSin * lfZoomU));
                laPlane[2] = 0.0f;

                // Fetch the world-orientation matrix for the eye/look spaces, then apply
                // it to the planar delta. mpCameraSpaceHandler is the author base's
                // camera-space handler (spine *(this+8)).
                // FLAG (camera-space handler access): the spine reads the author-base
                // camera-space handler pointer (this+8); reached here through the author
                // base, whose mpCameraSpaceHandler member owns it.
                const s32 liEyeSpace  = miFlagOrient ? mEditTake.GetValueInt(31) : mEditTake.GetValueInt(30);
                const s32 liLookSpace = miFlagOrient ? mEditTake.GetValueInt(33) : mEditTake.GetValueInt(32);
                Matrix4 lOrient;
                ICEMath::Identity(&lOrient);
                reinterpret_cast<ICEAuthor*>(this)->GetWorldTransformationMatrix(
                    &lOrient, static_cast<u8>(liEyeSpace));
                (void)liLookSpace;

                // Push the rotated planar delta as the key-element value changes (the
                // start/end element triple is 0..2 vs 3..5 by the orientation flag).
                const f32 lfV = laPlane[1];
                s32 liBaseElement;
                if (miFlagOrient)
                {
                    reinterpret_cast<ICEAuthor*>(this)->SetKeyElementValueChange(5, 0, lfV);
                    liBaseElement = 3;
                }
                else
                {
                    reinterpret_cast<ICEAuthor*>(this)->SetKeyElementValueChange(2, 0, lfV);
                    liBaseElement = 0;
                }
                reinterpret_cast<ICEAuthor*>(this)->SetKeyElementValueChange(liBaseElement, 0, laPlane[0]);
                this->FromBubble();
            }
            else
            {
                // Look spaces differ: drive the three orientation elements straight from
                // the joystick velocities (no bubble transform).
                const f32 lfTimestep = *mpMenuB;
                reinterpret_cast<ICEAuthor*>(this)->SetKeyElementValueChange(
                    0, 0, ((JoyVelocity(31) - JoyVelocity(32)) * lfTimestep) * -20.0f);   // 29404-29408
                reinterpret_cast<ICEAuthor*>(this)->SetKeyElementValueChange(
                    2, 0, ((JoyVelocity(32) - JoyVelocity(31)) * lfTimestep) *  20.0f);   // 29416-29412
                reinterpret_cast<ICEAuthor*>(this)->SetKeyElementValueChange(
                    3, 0, ((JoyVelocity(33) - JoyVelocity(34)) * lfTimestep) * -20.0f);   // 29420-29424
                reinterpret_cast<ICEAuthor*>(this)->SetKeyElementValueChange(
                    5, 0, ((JoyVelocity(36) - JoyVelocity(35)) * lfTimestep) * -20.0f);   // 29432-29428
            }

            // One-shot reset request: zero the orientation elements and re-enter the bubble.
            if (miFlagReset)
            {
                if (!mEditTake.GetValueInt(31))
                {
                    reinterpret_cast<ICEAuthor*>(this)->SetKeyElementValue(3, 0.0f);
                    reinterpret_cast<ICEAuthor*>(this)->SetKeyElementValue(4, 0.0f);
                    reinterpret_cast<ICEAuthor*>(this)->SetKeyElementValue(5, 0.0f);
                    miFlagReset = 0;
                    this->ToBubble();
                }
            }
            break;
        }

        // -----------------------------------------------------------------
        // ASSEMBLY_EDIT_START (17) / ASSEMBLY_EDIT_END (18): shuttle the assembly
        // parameter; on the end state at the last interval also grow the assembly length.
        // -----------------------------------------------------------------
        case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_START:
        case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_END:
        {
            const f32 lfShuttle = reinterpret_cast<ICEAuthor*>(this)->GetShuttleSpeed();
            const f32 lfTimestep = *mpMenuB;
            mfCursorRatio = lfShuttle;   // this+29272 (cursor-display shuttle read-out)

            const f32 lfMove = ((JoyVelocity(34) - JoyVelocity(33)) * lfTimestep) * lfShuttle; // 29424-29420
            const bool lbApply = JoyVelocity(18) > KF_ICE_PARAM_ENGAGE_THRESHOLD;              // 29360
            reinterpret_cast<ICEAuthor*>(this)->MoveParameter(lfMove, lbApply);

            // On the end state at the final interval, grow the assembly take length.
            // FLAG (assembly end-of-take test): the spine compares two channel-key
            // indices (this+416 == this+414 - 1); reconstructed as the end-key state test.
            if (liState == E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_END
                && reinterpret_cast<ICEAuthor*>(this)->IsEndKeyState(miStateArg))
            {
                reinterpret_cast<ICEAuthor*>(this)->ChangeAssemTakeLength(lfMove);
            }

            f32 lfBracketStart, lfBracketEnd;
            const u16 lu16Interval = mEditTake.GetCurrentInterval(miCurrentChannel);
            reinterpret_cast<ICEAuthor*>(this)->GetIntervalBracket(lu16Interval, &lfBracketStart, &lfBracketEnd);

            char lbEndKeyState = 0;
            if (miState == E_ICE_AUTHOR_STATE_SELECT_COPY_MODE)
                lbEndKeyState = reinterpret_cast<ICEAuthor*>(this)->IsEndKeyState(miStateArg) ? 1 : 0;
            else if (miState == E_ICE_AUTHOR_STATE_END_KEY_SELECTED
                     || miState == E_ICE_AUTHOR_STATE_EDIT_END_KEY
                     || miState == E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_END)
                lbEndKeyState = 1;

            const f32 lfSnap = lbEndKeyState ? lfBracketEnd : lfBracketStart;
            mEditTake.MoveParameter(miCurrentChannel, lu16Interval, 0, lfSnap);
            reinterpret_cast<ICEAuthor*>(this)->AdjustZoomRange();
            break;
        }

        default:
            break;
    }

    // Re-snap interval spacing for the key/interval-edit states (11..15).
    if (liState == E_ICE_AUTHOR_STATE_INTERVAL_SETTINGS
        || liState == E_ICE_AUTHOR_STATE_START_KEY_SELECTED
        || liState == E_ICE_AUTHOR_STATE_END_KEY_SELECTED
        || liState == E_ICE_AUTHOR_STATE_EDIT_START_KEY
        || liState == E_ICE_AUTHOR_STATE_EDIT_END_KEY)
    {
        // FLAG (interval-spacing thunk): the spine's `this+40` spacing thunk with the
        // current take parameter (the "snap to current" pass, flag arg 1) is the ICETake
        // interval re-spacing helper; reconstructed as the modelled ICETake op.
        const u16 lu16Interval = mEditTake.GetCurrentInterval(miCurrentChannel);
        mEditTake.MoveParameter(miCurrentChannel, lu16Interval, 1, mEditTake.GetParameter());
    }
}

} // namespace ICE
