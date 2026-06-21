// ============================================================================
// SDKs/Packages/ICE/ICEController.cpp
//
// ICE::ICEController -- the in-game ICE camera-take editor/driver. This TU bodies
// the FOUNDATIONAL group of the controller's 23 functions: the lifecycle entry
// (Construct), the small state predicates/setters (IsMarked / IsShakeEditing /
// SetZoomRange / SetCurrentIntervalValue / SetJoyVelocities / MoveCursor /
// BarFlipping) and the tractable menu-refresh helpers. The heavy SIMD draw passes
// (Render / RenderBars / RenderMenus / RenderTargetBox), the input dispatch
// (Update / JoyHandler), the menu construct/destruct (ConstructMenus /
// DestructMenus), the bubble serialisers (ToBubble / FromBubble) and the
// element-value edit ops (ChangeKeyElementValue(s)) are left declaration-only in
// the header for the parallel fan-out (see the manifest in the layout dossier).
//
// The reconstruction reverses compiler inlining of the embedded ICETake / widget /
// author-base accesses back into named-member access against the layout in
// ICEController.hpp (the source-spine offsets are facts pinned there; the bodies access by
// name, not by cast).
//
// FLAG (ICEAuthor base): the controller IS-AN ICEAuthor. Several foundational bodies
// call ICEAuthor methods (SetParameterChange, GetIntervalKey, SetValue thunks) on
// `this`. ICEAuthor.hpp is owned by another agent and not yet present, so those calls
// are expressed against the forward-declared ICEAuthor base and FLAGged at the call
// site -- they compile under the per-TU `cl /c` gate (which does not link) and will
// resolve once the base lands. Where a body needs an unnamed source spine take-arithmetic
// thunk (the `this+40` ICETake parameter/interval helpers spelled as an unnamed thunk in the
// pseudocode), the equivalent ICETake member is used (ICETake already models them).
// ============================================================================

#include "SDKs/Packages/ICE/ICEController.hpp"
#include "SDKs/Packages/ICE/ICEAuthor.hpp"   // ICE::ICEAuthor base methods (called on `this`)

namespace ICE
{

// The shuttle engage threshold MoveCursor compares the engage axis (JoyVelocity(18))
// against to decide whether the parameter change snaps/accumulates (lbApply).
// FLAG (engage threshold value): the comparison reads a file-local constant whose raw
// value is not recoverable from the available material; modelled here as a small
// engage threshold matching the neighbouring 0.1f joystick gates. If the true value
// is later attested, replace this placeholder.
static const f32 KF_ICE_PARAM_ENGAGE_THRESHOLD = 0.1f;

// ---------------------------------------------------------------------------
// Construct
//
// Chain the ICEAuthor base constructor, cache three pointers out of the ICEPointers
// bundle (the sub-take source a2[2], the action queue a2[0], the timestep source
// a2[4] -- the source spine reads a2[2]/a2[0]/a2[4]/a2[5]), zero the editor state block, seed
// the cursor/zoom defaults, clear the 37-entry value-change buffer, seed the mark
// in/out to -1, then build the menus.
//
// source-spine word stores (this-relative): +0xF7C=a2[2], +0xF74=a2[0], +0xF70=a2[4] (the
// timestep source dereferenced as **), the status block at +0x7204/+0x7208/+0x720C
// and +0x724C cleared, the "show UI" flag +0x7250=1, the three edit bools
// +0x7251/+0x7252/+0x7253=0, the cursor floats +0x7254=0.0 / +0x7258=1.0, the bar
// fade +0x725C=3.0, the bubble radius +0x7264=0.0, the 37-word change buffer at
// +0x7268 zeroed, the menu/bar-flip state +0x73B8/+0x73BC/+0x73BD zeroed and the mark
// in/out +0x73C0/+0x73C4 = -1.0.
// ---------------------------------------------------------------------------
void ICEController::Construct(ICEPointers* lpICEPointers)
{
    // Chain the authoring base first (the controller is-an author).
    // FLAG (ICEAuthor base): ICEAuthor::Construct() is the real base ctor; until the
    // base lands, the author-region members are zero/seeded directly below. The source spine
    // calls ICEAuthor::Construct() here.

    // Cache the three subsystem pointers out of the bundle. The source spine reads a2[2]
    // (mpActionQueue is a2[2] in the bundle order) into +0xF7C, a2[0] (mpICEMemory)
    // into +0xF74 and a2[4] (mpICETimer, the timestep source) into +0xF70; the names
    // here are the manager-consumer names retained for ICEController.hpp.
    mpSubTakeSource = lpICEPointers->mpActionQueue;          // +0xF7C (a2[2])
    mpMenuA         = lpICEPointers->mpICEMemory;            // +0xF74 (a2[0], action-queue slot)
    mpMenuB         = reinterpret_cast<f32*>(lpICEPointers->mpICETimer);  // +0xF70 (a2[4], points at the per-frame timestep)
    mpMenuC         = lpICEPointers->mpICEMemory;            // +0xF78 (ICE memory manager)

    // Zero the on-screen status-text block and the spare edit flag.
    miStatusCount = 0;
    miStatusMode  = 0;
    miStatusText  = 0;
    miEditFlag724C = 0;

    // Editor UI on by default; the three edit-mode bools cleared.
    miShowUi     = 1;
    miFlagOrient = 0;
    miFlagAux    = 0;
    miFlagReset  = 0;

    // Cursor defaults: parameter origin 0.0, shuttle ratio 1.0; status-bar fade 3.0.
    mfCursorParam = 0.0f;
    mfCursorRatio = 1.0f;
    mfBarFade     = 3.0f;

    // Bubble radius cleared (the two bubble angles are seeded by the manager ctor).
    mfBubbleRadius = 0.0f;

    // Clear the 37-entry value-change / joystick-velocity buffer.
    for (s32 li = 0; li < KI_ICE_NUM_CHANGE_ELEMENTS; ++li)
        maChangeBuffer[li] = 0;

    // Menu/bar-flip state cleared.
    miMenuTimer    = 0;
    miBarFlipLatch = 0;
    miBarFlipSub   = 0;

    // Mark in/out unset.
    mfMarkIn  = -1.0f;
    mfMarkOut = -1.0f;

    // Build the on-screen editor menus and bars.
    ConstructMenus();
}

// ---------------------------------------------------------------------------
// IsMarked
//
// True when BOTH mark endpoints are set (a marked range exists): the in-point must be
// non-negative and the out-point strictly positive. Construct seeds both to -1.0
// (unset), so an unset range reports false.
// ---------------------------------------------------------------------------
bool ICEController::IsMarked() const
{
    if (mfMarkIn < 0.0f)
        return false;
    if (mfMarkOut <= 0.0f)
        return false;
    return true;
}

// ---------------------------------------------------------------------------
// IsShakeEditing
//
// True when the editor is in any of the camera-shake edit modes: the current channel
// is the shake channel (3) or the assembly-shake channel (10), or the editor state is
// the shake-adjust (10) or shake-zoom (5) state.
// ---------------------------------------------------------------------------
bool ICEController::IsShakeEditing() const
{
    if (miCurrentChannel == 3)
        return true;
    if (miCurrentChannel == 10)
        return true;
    if (miState == 10)
        return true;
    if (miState == 5)
        return true;
    return false;
}

// ---------------------------------------------------------------------------
// SetJoyVelocities
//
// Map a raw joystick magnitude (0..1) into the per-axis velocity slot for liAxis. A
// two-segment response curve: below the 0.5 dead-zone-ish knee the response is gentle
// (x*0.2); above it the response ramps steeply (0.2 + (x-0.5)*1.6). The result is
// stored into the per-axis velocity slot (the change-buffer float view at index
// liAxis -- the source spine writes `*(4*(liAxis+7322)+this)`, i.e. this+0x7268 + liAxis*4).
// ---------------------------------------------------------------------------
void ICEController::SetJoyVelocities(s32 liAxis, f32 lfMagnitude)
{
    if (lfMagnitude <= 0.5f)
        JoyVelocity(liAxis) = lfMagnitude * 0.2f;
    else
        JoyVelocity(liAxis) = ((lfMagnitude - 0.5f) * 1.6f) + 0.2f;
}

// ---------------------------------------------------------------------------
// BarFlipping
//
// Auto-advance the editor's current channel when a vertical joystick push is held: if
// either vertical velocity sample crosses the 0.3 threshold and the flip-latch is
// clear, latch it and step the current channel one slot in the requested direction,
// skipping the two reserved channel slots (10 and 11 wrap). When both samples fall
// back below 0.3 the latch is released (so the next push flips again). The two
// vertical velocity samples live in the change-buffer float view (indices that map to
// this+29412 and this+29416 -- velocity indices 31 and 32).
//
// `lbForward` chooses the step direction (the source-spine `a2` flag: forward steps up through
// the channels, back steps down).
// ---------------------------------------------------------------------------
void ICEController::BarFlipping(bool lbForward)
{
    // The two vertical velocity samples (change-buffer float view). 29412 == this +
    // 0x7268 + 31*4 -> index 31; 29416 -> index 32.
    const f32 lfVelDown = JoyVelocity(32);   // this+29416
    const f32 lfVelUp   = JoyVelocity(31);   // this+29412

    if ((lfVelDown > 0.3f || lfVelUp > 0.3f) && !miBarFlipLatch)
    {
        // Latch the flip so a held push only advances one channel.
        miBarFlipLatch = 1;

        s32 liChannel = miCurrentChannel;
        if (lfVelDown <= 0.3f)
        {
            // Upward push: step the channel forward, skipping the reserved slots.
            if (lbForward)
            {
                while (liChannel < 11)
                {
                    if (++liChannel == 11)
                    {
                        liChannel = 0;
                        break;
                    }
                    if (liChannel != 10)
                        break;
                }
                miCurrentChannel = liChannel;
            }
        }
        else
        {
            // Downward push: step the channel backward, wrapping past the reserved slots.
            if (lbForward)
            {
                do
                {
                    while (liChannel <= 0)
                        liChannel = 11;
                    --liChannel;
                }
                while (liChannel == 11 || liChannel == 10);
                miCurrentChannel = liChannel;
            }
        }

        // Re-arm the zoom cursor (clear the active drag).
        mfZoomCursorLo = -0.1f;   // this+16144 (the active zoom-drag pair, cleared)
        mfZoomCursorHi = -0.1f;   // this+16148
        return;
    }

    // Both samples fell back below the threshold -> release the latch.
    if (lfVelDown < 0.3f && lfVelUp < 0.3f)
        miBarFlipLatch = 0;
}

// ---------------------------------------------------------------------------
// MoveCursor
//
// Drive the editor's time cursor from the current take parameter: compute the cursor's
// normalised position within the visible zoom window, store the take parameter and the
// window ratio into the cursor read-outs, push the parameter into the author (a
// parameter-change request), then expand the visible window if the parameter has
// scrubbed outside it. The window pair is the shake pair when the editor state is the
// shake-zoom state (8), otherwise the non-shake pair.
//
// `*(this+48)` is the take parameter (mEditTake.GetParameter()); `*(this+44)` is the
// take-data pointer (mEditTake.GetData()); `*((take-data)+44)` is the take length.
// ---------------------------------------------------------------------------
void ICEController::MoveCursor()
{
    const f32 lfParameter = mEditTake.GetParameter();

    // Pick the visible-window pair (shake vs non-shake) by editor state.
    f32* lpWindow = (miState == 8) ? &mfZoomCursorShakeLo : &mfZoomCursorLo;

    // Window ratio = window span / take length, or 1.0 when the take has no length.
    f32 lfRatio = 1.0f;
    const ICETakeData* lpTakeData = mEditTake.GetData();
    if (lpTakeData && lpTakeData->GetLength() > 0.0f)
    {
        const f32 lfLength = lpTakeData->GetLength();
        lfRatio = (lpWindow[1] - lpWindow[0]) / lfLength;
    }

    mfCursorRatio = lfRatio;       // this+29272
    mfCursorParam = lfParameter;   // this+29268

    // Request the parameter change through the author base. The per-frame delta is the
    // horizontal shuttle velocity scaled by the frame timestep, the bar fade and the
    // window ratio; the engage gate is set when the engage axis exceeds the threshold.
    const f32 lfTimestep = *mpMenuB;
    const f32 lfMove = (JoyVelocity(34) - JoyVelocity(33)) * lfTimestep * mfBarFade * lfRatio; // 29424-29420
    const bool lbApply = JoyVelocity(18) > KF_ICE_PARAM_ENGAGE_THRESHOLD;                      // 29360
    reinterpret_cast<ICEAuthor*>(this)->SetParameterChange(lfMove, lbApply);

    // Re-fetch the window pair (the author call may have re-selected it) and expand it
    // if the parameter scrubbed outside [lo, hi].
    f32* lpWindow2 = (miState == 8) ? &mfZoomCursorShakeLo : &mfZoomCursorLo;
    const f32 lfP = mEditTake.GetParameter();
    if (lfP >= lpWindow2[0])
    {
        if (lfP > lpWindow2[1])
        {
            const f32 lfShift = lfP - lpWindow2[1];
            lpWindow2[1] = lfP;
            lpWindow2[0] = lfShift + lpWindow2[0];
        }
    }
    else
    {
        const f32 lfShift = lpWindow2[0] - lfP;
        const f32 lfHi = lpWindow2[1];
        lpWindow2[0] = lfP;
        lpWindow2[1] = lfHi - lfShift;
    }
}

// ---------------------------------------------------------------------------
// SetZoomRange
//
// Adjust the visible zoom window from the per-frame zoom velocity. The velocity delta
// is (zoomVelCur - zoomVelPrev) * timestep (the change-buffer float view at indices
// that map to this+29428 / this+29432). In the zoom-active state (7) with the zoom
// trigger held it nudges the take length directly; otherwise it grows/shrinks the
// visible window symmetrically (clamped so the window keeps a minimum 0.02 span), on
// the shake window pair when the editor state is the shake-zoom state (8).
// ---------------------------------------------------------------------------
void ICEController::SetZoomRange()
{
    // Frame timestep (mpMenuB points directly at the per-frame timestep float).
    const f32 lfTimestep = *mpMenuB;

    // Zoom velocity delta this frame. 29428 -> velocity index 35; 29432 -> index 36.
    const f32 lfZoomDelta = (JoyVelocity(35) - JoyVelocity(36)) * lfTimestep;

    // 29360 is the zoom-trigger sample (change-buffer float view, index 18).
    const f32 lfZoomTrigger = JoyVelocity(18);   // this+29360

    if (lfZoomTrigger > 0.1f && miState == 7)
    {
        // Zoom-active: nudge the bound take length directly.
        ICETakeData* lpTakeData = mEditTake.GetData();
        f32 lfLength = lpTakeData ? lpTakeData->GetLength() : 0.0f;
        if (lpTakeData)
            lpTakeData->SetLength(lfLength + lfZoomDelta);
        return;
    }

    // Otherwise grow/shrink the visible window. Pick the shake pair in state 8.
    f32 lfLo, lfHi;
    if (miState == 8)
    {
        lfLo = mfZoomCursorShakeLo;   // this+3876
        lfHi = mfZoomCursorShakeHi;   // this+3880
    }
    else
    {
        lfLo = mfZoomCursorLo;        // this+3868
        lfHi = mfZoomCursorHi;        // this+3872
    }

    // Only adjust when the delta is meaningful and the window keeps a minimum span,
    // or when zooming out (negative delta).
    if ((lfZoomDelta != 0.0f && (lfHi - lfLo) >= 0.02f) || lfZoomDelta < 0.0f)
    {
        const f32 lfParameter = mEditTake.GetParameter();   // this+48

        // Scale each edge toward the cursor by the zoom delta, clamped to [0,1].
        f32 lfNewLo = ((lfParameter - lfLo) * lfZoomDelta) + lfLo;
        f32 lfNewHi = lfHi - ((lfHi - lfParameter) * lfZoomDelta);

        // Clamp the new edges into the unit range (the source-spine fsel-clamps to 0..1).
        if (lfNewLo < 0.0f) lfNewLo = 0.0f;
        if (lfNewLo > 1.0f) lfNewLo = 1.0f;
        if (lfNewHi < 0.0f) lfNewHi = 0.0f;
        if (lfNewHi > 1.0f) lfNewHi = 1.0f;

        if (miState == 8)
        {
            mfZoomCursorShakeLo = lfNewLo;
            mfZoomCursorShakeHi = lfNewHi;
        }
        else
        {
            mfZoomCursorLo = lfNewLo;
            mfZoomCursorHi = lfNewHi;
        }
    }
}

// ---------------------------------------------------------------------------
// SetCurrentIntervalValue
//
// Set an element value at the current interval's key, but only for the "interval"
// element-description range (descriptions 28..48) and only when the destination
// channel actually has that interval (the channel's interval count is at least the
// current key). Writes the value through the edit take, then re-applies the spacing/
// harden pass for the touched interval.
//
// liElement is the element-description index; liValue is the new (integer-coded) value.
// The per-element channel id and the bound check come from the element-description
// table (the source-spine indexes the element-description table for the destination channel).
// ---------------------------------------------------------------------------
void ICEController::SetCurrentIntervalValue(s32 liElement, s32 liValue)
{
    // Only the interval element-description range (28..48 inclusive == (liElement-28)
    // unsigned <= 0x14).
    const u32 luRange = static_cast<u32>(liElement - 28);
    if (luRange > 0x14u)
        return;

    // FLAG (element-description table): the destination channel for liElement and the
    // bound check read the element-description table (source spine the element-description table[22*liElement]
    // and the channel's interval count at this+254-indexed by it). That table is a
    // file-local static in the fan-out edit-ops TU; here the channel id is taken from
    // the current channel as the conservative stand-in and FLAGged. The current key is
    // the channel's current interval key.
    const s32 liChannel  = miCurrentChannel;
    const u16 lu16Key     = mEditTake.GetCurrentInterval(liChannel);

    // Write the value at the current interval key.
    mEditTake.SetValue(liElement, lu16Key, static_cast<ICEValue>(liValue));

    // Re-apply the interval spacing/harden pass for the touched element at the current
    // take parameter (the source-spine calls the `this+40` interval-spacing thunk with the take
    // parameter; the modelled ICETake equivalent is MoveParameter on the element's
    // channel, biased to re-snap the boundary).
    // FLAG (interval-spacing thunk): the unnamed interval thunk is the ICETake interval
    // re-spacing helper; reconstructed here as the modelled ICETake interval op.
    mEditTake.MoveParameter(liElement, lu16Key, 1, mEditTake.GetParameter());
}

// ---------------------------------------------------------------------------
// RefreshIntervalMenu
//
// Re-snap the current channel's interval boundaries to the visible window. If the
// channel's current interval is a single-key interval, temporarily Insert a key so the
// re-spacing has two boundaries to work with, then push the window start (0.0), end
// (1.0) and the current parameter through the interval-spacing helper, and finally undo
// the temporary key if one was inserted.
// ---------------------------------------------------------------------------
void ICEController::RefreshIntervalMenu()
{
    bool lbInsertedTemp = false;
    const s32 liChannel = miCurrentChannel;
    const f32 lfParameter = mEditTake.GetParameter();   // this+48

    // A single-key (count == 1) current interval needs a temporary second key to span.
    // The source spine tests `*(16*channel + this + 254) == 1` -- the channel's interval count.
    if (mEditTake.GetNumIntervals(liChannel) == 1)
    {
        mEditTake.Insert(liChannel, 0);
        lbInsertedTemp = true;
    }

    // Re-space the window endpoints and the current parameter.
    // FLAG (interval-spacing thunk): the unnamed interval thunk (edit take, time, channel)
    // is the ICETake interval re-spacing helper; reconstructed here as the modelled
    // ICETake interval op at the window start (0.0), end (1.0) and current parameter.
    const u16 lu16Key = mEditTake.GetCurrentInterval(liChannel);
    mEditTake.MoveParameter(liChannel, lu16Key, 0, 0.0f);
    mEditTake.MoveParameter(liChannel, lu16Key, 0, 1.0f);
    mEditTake.MoveParameter(liChannel, lu16Key, 0, lfParameter);

    // Undo the temporary key.
    if (lbInsertedTemp)
        mEditTake.Delete(miCurrentChannel);
}

} // namespace ICE
