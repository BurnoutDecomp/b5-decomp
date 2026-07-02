// ============================================================================
// SDKs/Packages/ICE/ICEAuthor.cpp
//
// ICE::ICEAuthor -- the in-game ICE camera-take/assembly editor. Reconstructed
// against the frozen layout in ICEAuthor.hpp. THIS TU bodies the FOUNDATIONAL
// subset only (the state machine, the trivial getters, the interval delegators,
// the editor-on entry, and Construct); the take/assembly/key/segment/space
// operations are declaration-only here and get bodies in the fan-out TUs
// (ICEAuthorTakeOps.cpp / ICEAuthorAssemblyOps.cpp / ICEAuthorKeyElementOps.cpp /
// ICEAuthorSegmentOps.cpp / ICEAuthorSpaceOps.cpp -- see the manifest). The per-TU
// `cl /c` gate does not link, so the un-bodied declarations are fine.
//
// Reconstruction reverses the compiler inlining of the embedded ICETake accesses
// (mEditTake @0x28 / mScratchTake @0x760) back into named member calls; the byte
// offsets in ICEAuthor.hpp are the layout facts, not used as casts here.
// ============================================================================

#include "SDKs/Packages/ICE/ICEAuthor.hpp"

namespace ICE
{

// ---------------------------------------------------------------------------
// Construct
//
// Initialise the editor from the ICEPointers bundle: cache the three subsystem
// pointers (file/text sink a2[1], camera-space handler a2[3], resource manager
// a2[5]), zero the head scalars + state machine, default the scrub/range/zoom
// scalars (range [0,1], zoom [0,1], saved -1 selected-take guid), build the two
// embedded takes against the resource manager (the scratch take additionally gets
// a fresh edit buffer + a one-key insert + a flushed undo), self-link the
// edited-takes list head, and set the 28 per-element enable flags to 1.
//
// Init stores: state/prev/channel (+0xE98/+0xE9C/+0xEA0)=0, the head pointers
// (+0x10/+0x14/+0x18)=0, the bundle pointers (+0x04/+0x08/+0x0C) from a2[1/3/5];
// the widget side-table (28 dwords @+0xEA4) zeroed; the scalar block defaulted;
// +0x1244 (selected-take guid) = -1; the list head (+0x1248/+0x124C) self-linked;
// the 28 enable bytes (+0x1250) set to 1. ICETake::Construct(mEditTake,resMgr) and
// ICETake::Construct(mScratchTake,resMgr); the scratch take is given an edit buffer
// sized by ICETakeData::ComputeEditSize, a SetParameter(force) and a FlushUndo.
// ---------------------------------------------------------------------------
void ICEAuthor::Construct(const ICEPointers* lpICEPointers)
{
    // ---- cache the bundle pointers (a2[1] / a2[3] / a2[5]) ----
    mpFileHandler        = lpICEPointers->mpICEFileHandler;
    // The camera-space handler the take spaces project through is carried by the
    // camera-anchor slot (a2[3]); stored as the CameraSpaceHandler* the Space/Transform
    // ops pass to CameraSpaceHandler::GetTransformToWorld. FLAG: the bundle slot is
    // typed ICECameraAnchor* in ICEMemory.hpp; the author treats +0x08 as the space
    // handler. The reinterpretation is the documented bundle->author wiring (the anchor
    // owns/yields the space handler); kept as a cast so the foundational TU compiles
    // without the ICECameraAnchor home.
    mpCameraSpaceHandler = reinterpret_cast<CameraSpaceHandler*>(lpICEPointers->mpICECameraAnchor);
    mpResourceManager    = const_cast<IResourceManager*>(lpICEPointers->mpResourceManager);

    // ---- zero the head scalars + state machine ----
    mpEditTakeData     = 0;
    mpAssemblyTakeData = 0;
    mpSubTakeData      = 0;
    miState     = E_ICE_AUTHOR_STATE_OFF;
    miPrevState = E_ICE_AUTHOR_STATE_OFF;
    miCurrentChannel = 0;

    // ---- zero the per-element widget side-table (28 dwords) ----
    for (s32 liElement = 0; liElement < KI_ICE_AUTHOR_ELEMENTS; ++liElement)
    {
        maiElementWidget[liElement] = 0;
    }

    // ---- default the scrub / range / zoom scalars ----
    miPad3896 = 0;
    miPad3900 = 0;
    miPad3904 = 0;
    mfMoveAccumulator  = 0.0f;
    miSelectedTakeGuid = -1;
    mfSavedParameter   = 0.0f;
    mfRangeLo          = 0.0f;
    mfSavedRangeLo     = 0.0f;
    mfRangeHi          = 1.0f;
    mfSavedRangeHi     = 1.0f;
    mfZoomLo           = 0.0f;
    mfZoomHi           = 1.0f;

    // ---- self-link the edited-takes list head (empty -> Next/Prev point at head) ----
    mEditedTakeList[0] = &mEditedTakeList[0];
    mEditedTakeList[1] = &mEditedTakeList[0];

    // ---- build the two embedded takes ----
    mEditTake.Construct(mpResourceManager);
    mScratchTake.Construct(mpResourceManager);

    // The scratch take is given a fresh edit buffer (allocated from the ICE memory
    // manager, bound, seeded at parameter 0, with its undo history cleared). The build
    // inlines the GetMemory(ComputeEditSize) / ICETakeData::Construct / SetDataPointers
    // / SetParameter(force) / FlushUndo sequence; the public ICETake::NewEditBuffer()
    // performs exactly that whole edit-buffer set-up, so it is the named entry point.
    //
    // FLAG: the inlined sequence ends with ICETake::FlushUndo, which is PRIVATE on
    // ICETake (ICEData.hpp). NewEditBuffer() encapsulates the same buffer-allocate +
    // undo-flush; if the body phase finds the author needs a standalone FlushUndo on
    // the scratch take distinct from NewEditBuffer, ICETake must EXPOSE it (grow
    // ICEData.hpp) -- do NOT poke the private member from here.
    mScratchTake.NewEditBuffer();

    // ---- set the 28 per-element enable flags to 1 ----
    for (s32 liElement = 0; liElement < KI_ICE_AUTHOR_ELEMENTS; ++liElement)
    {
        mabElementEnabled[liElement] = 1;
    }
}

// ---------------------------------------------------------------------------
// EditorOn
//
// Enter take-edit mode for the given source take. No-op if already on for the same
// source (state > OFF and the edit source unchanged). Otherwise: record the new
// edit source, reset to OFF, then walk OFF -> BROWSER -> load the take -> SCRUBBER.
// ---------------------------------------------------------------------------
void ICEAuthor::EditorOn(ICETakeData* lpTakeData)
{
    if (miState <= E_ICE_AUTHOR_STATE_OFF || mpEditTakeData != lpTakeData)
    {
        mpEditTakeData = lpTakeData;
        SetState(E_ICE_AUTHOR_STATE_OFF);
        SetState(E_ICE_AUTHOR_STATE_BROWSER);
        LoadCurrentTake();
        SetState(E_ICE_AUTHOR_STATE_SCRUBBER);
    }
}

// ---------------------------------------------------------------------------
// GetStateName
//
// Map an editor-state enum to its debug name (used by the editor info list). Any
// unrecognised state reads "UNKNOWN".
// ---------------------------------------------------------------------------
const char* ICEAuthor::GetStateName(s32 liState) const
{
    switch (liState)
    {
        case E_ICE_AUTHOR_STATE_BROWSER:               return "BROWSER";
        case E_ICE_AUTHOR_STATE_EXIT_CONFIRM:          return "EXIT_CONFIRM";
        case E_ICE_AUTHOR_STATE_DELETE_CONFIRM:        return "DELETE_CONFIRM";
        case E_ICE_AUTHOR_STATE_LOADING_ANIMATION:     return "LOADING_ANIMATION";
        case E_ICE_AUTHOR_STATE_PLAYING_ANIMATION:     return "PLAYING_ANIMATION";
        case E_ICE_AUTHOR_STATE_SCREENSHOT:            return "SCREENSHOT";
        case E_ICE_AUTHOR_STATE_SCRUBBER:              return "SCRUBBER";
        case E_ICE_AUTHOR_STATE_ASSEMBLY_MARK:         return "ASSEMBLY_MARK";
        case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT:         return "ASSEMBLY_EDIT";
        case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_PLAYING: return "ASSEMBLY_EDIT_PLAYING";
        case E_ICE_AUTHOR_STATE_INTERVAL_SETTINGS:     return "INTERVAL_SETTINGS";
        case E_ICE_AUTHOR_STATE_START_KEY_SELECTED:    return "START_KEY_SELECTED";
        case E_ICE_AUTHOR_STATE_END_KEY_SELECTED:      return "END_KEY_SELECTED";
        case E_ICE_AUTHOR_STATE_EDIT_START_KEY:        return "EDIT_START_KEY";
        case E_ICE_AUTHOR_STATE_EDIT_END_KEY:          return "EDIT_END_KEY";
        case E_ICE_AUTHOR_STATE_SELECT_COPY_MODE:      return "SELECT_COPY_MODE";
        case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_START:   return "ASSEMBLY_EDIT_START";
        case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_END:     return "ASSEMBLY_EDIT_END";
        default:                                       return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// IsEndKeyState
//
// True when the editor is on an "end key" of a key pair. liKey == -1 means "use the
// live state": loop pulling the current state, and while it is SELECT_COPY_MODE
// (16) chase back to the previous state. The terminal states that count as end-key
// are END_KEY_SELECTED (13), EDIT_END_KEY (15), ASSEMBLY_EDIT_END (18).
// ---------------------------------------------------------------------------
bool ICEAuthor::IsEndKeyState(s32 liKey) const
{
    s32 liState = liKey;
    while (liState == -1)
    {
        liState = miState;
        if (liState != E_ICE_AUTHOR_STATE_SELECT_COPY_MODE)
        {
            break;
        }
        liState = miPrevState;
    }

    if (liState == E_ICE_AUTHOR_STATE_END_KEY_SELECTED)   { return true; }
    if (liState == E_ICE_AUTHOR_STATE_EDIT_END_KEY)       { return true; }
    if (liState == E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_END)  { return true; }
    return false;
}

// ---------------------------------------------------------------------------
// GetValueFloat
//
// Read element liElement's current value as a float, straight out of the edit
// take's per-element value table (mEditTake.mValues[liElement]). ICETake::GetValueFloat
// already carries the FIXED/FLOAT-vs-everything-else type dispatch, so it is called
// directly with the raw element index -- NOT the element's description channel
// number, which drives a different (per-channel) table.
// ---------------------------------------------------------------------------
f32 ICEAuthor::GetValueFloat(s32 liElement) const
{
    return mEditTake.GetValueFloat(liElement);
}

// ---------------------------------------------------------------------------
// GetCurrentSubtakeGuid
//
// If the assembly channel (the fixed channel that carries the take's interval
// breakdown, KI_ICE_ASSEMBLY_CHANNEL) has any intervals and the current interval
// carries a sub-take (element 47, the sub-take flag), return the sub-take guid
// (element 46); else -1.
// ---------------------------------------------------------------------------
s32 ICEAuthor::GetCurrentSubtakeGuid() const
{
    static const s32 KI_ICE_ASSEMBLY_CHANNEL = 10;

    if (mEditTake.GetNumIntervals(KI_ICE_ASSEMBLY_CHANNEL) != 0 && mEditTake.GetValueInt(47) != 0)
    {
        return mEditTake.GetValueInt(46);
    }
    return -1;
}

// ---------------------------------------------------------------------------
// AdjustZoomRange
//
// Keep the active zoom/play window enclosing the current playback parameter. The
// window is the zoom range (mfZoomLo/Hi) in ASSEMBLY_MARK state (8), otherwise the
// play range (mfRangeLo/Hi). If the parameter falls below the window's low, slide
// the whole window down to start at the parameter (preserving its width); if it
// rises above the window's high, slide it up to end at the parameter.
// ---------------------------------------------------------------------------
void ICEAuthor::AdjustZoomRange()
{
    const f32 lfParameter = mEditTake.GetParameter();

    f32* lpfWindow = (miState == E_ICE_AUTHOR_STATE_ASSEMBLY_MARK) ? &mfZoomLo : &mfRangeLo;

    if (lfParameter >= lpfWindow[0])
    {
        if (lfParameter > lpfWindow[1])
        {
            const f32 lfWidth = lfParameter - lpfWindow[1];
            lpfWindow[1] = lfParameter;
            lpfWindow[0] = lfWidth + lpfWindow[0];
        }
    }
    else
    {
        const f32 lfWidth = lpfWindow[0] - lfParameter;
        const f32 lfHigh  = lpfWindow[1];
        lpfWindow[0] = lfParameter;
        lpfWindow[1] = lfHigh - lfWidth;
    }
}

// ---------------------------------------------------------------------------
// GetShuttleSpeed
//
// The playback speed scale (window width / take length). Uses the zoom window in
// ASSEMBLY_MARK state (8), else the play window. Returns 1.0 if there is no take or
// the take has zero length.
// ---------------------------------------------------------------------------
f32 ICEAuthor::GetShuttleSpeed() const
{
    const f32* lpfWindow = (miState == E_ICE_AUTHOR_STATE_ASSEMBLY_MARK) ? &mfZoomLo : &mfRangeLo;

    const ICETakeData* lpTakeData = mEditTake.GetData();
    if (lpTakeData == 0)
    {
        return 1.0f;
    }
    if (lpTakeData->GetLength() <= 0.0f)
    {
        return 1.0f;
    }
    const f32 lfLength = mEditTake.GetData()->GetLength();
    return (lpfWindow[1] - lpfWindow[0]) / lfLength;
}

// ---------------------------------------------------------------------------
// GetSubAssemRatio
//
// Ratio of the sub-take parameter span to the edit-take length -- 0 when there is
// no take or a zero-length take. (mEditTake.GetSubParameter scaled by the sub-take
// length over the edit take length, as the runtime reads the two takes' lengths.)
// ---------------------------------------------------------------------------
f32 ICEAuthor::GetSubAssemRatio() const
{
    const ICETakeData* lpTakeData = mEditTake.GetData();
    f32 lfRatio = 0.0f;
    if (lpTakeData != 0 && lpTakeData->GetLength() != 0.0f)
    {
        const f32 lfSubLength = mEditTake.GetSubTakeLength();
        const f32 lfLength    = mEditTake.GetData()->GetLength();
        return lfSubLength / lfLength;
    }
    return lfRatio;
}

// ---------------------------------------------------------------------------
// GetIntervalBracket
//
// Fill (start, end) parameters bracketing interval lu16Interval of the current
// channel. The sentinel ICE_INVALID_INTERVAL (0xFFFF) brackets the whole take
// [0, 1]. Otherwise both bounds come from the channel's interval parameters.
// ---------------------------------------------------------------------------
void ICEAuthor::GetIntervalBracket(u16 lu16Interval, f32* lpfStart, f32* lpfEnd) const
{
    if (lu16Interval == ICE_INVALID_INTERVAL)
    {
        *lpfStart = 0.0f;
        *lpfEnd   = 1.0f;
    }
    else
    {
        // The channel's interval parameters bracket the interval (the +1 boundary is
        // the end). Delegated to the current channel of the edit take.
        mEditTake.GetIntervalBracket(miCurrentChannel, lu16Interval, lpfStart, lpfEnd);
    }
}

// ---------------------------------------------------------------------------
// GetIntervalEnd
//
// End parameter of interval li16Interval on the current channel: -1 -> 0.0 (before
// the first), past the last interval -> 1.0, else the (interval+1) key's parameter.
// ---------------------------------------------------------------------------
f32 ICEAuthor::GetIntervalEnd(s16 li16Interval) const
{
    if (li16Interval == -1)
    {
        return 0.0f;
    }
    return mEditTake.GetIntervalEnd(miCurrentChannel, (u16)li16Interval);
}

// ---------------------------------------------------------------------------
// GetIntervalKey
//
// Key index bounding interval lu16Interval on the current channel: interval 0 -> key
// 0, an interior interval -> its stored boundary key, the last -> the final key.
// ---------------------------------------------------------------------------
s32 ICEAuthor::GetIntervalKey(u16 lu16Interval) const
{
    if (lu16Interval == 0)
    {
        return 0;
    }
    return mEditTake.GetIntervalKey(miCurrentChannel, lu16Interval);
}

// ---------------------------------------------------------------------------
// GetIntervalStart
//
// Start parameter of interval lu16Interval on the current channel: interval 0 -> 0.0,
// past the last -> 1.0, else the interval's start key parameter.
// ---------------------------------------------------------------------------
f32 ICEAuthor::GetIntervalStart(u16 lu16Interval) const
{
    if (lu16Interval == 0)
    {
        return 0.0f;
    }
    return mEditTake.GetIntervalStart(miCurrentChannel, lu16Interval);
}

} // namespace ICE
