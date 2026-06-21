// ============================================================================
// SDKs/Packages/ICE/ICEAuthorKeyElementOps.cpp
//
// ICE::ICEAuthor -- the key-element-ops + state-machine group of the in-game ICE
// camera-take editor. Reconstructed against the frozen layout in ICEAuthor.hpp.
// THIS TU bodies one fan-out group of ICEAuthor (the declarations are in
// ICEAuthor.hpp; the foundational subset -- Construct / state getters / interval
// delegators / IsEndKeyState / GetIntervalKey / GetIntervalBracket / AdjustZoomRange
// -- is bodied in ICEAuthor.cpp). The per-TU `cl /c` gate does not link, so the
// un-bodied declarations from the other groups are fine.
//
// Group bodies:
//   ToggleKey, CopyKeyElements, PasteKeyElements, SetKeyElementValue,
//   SetKeyElementValueChange, GetElementIndexByTag, SetState.
//
// The editor edits the live take (mEditTake) and a clipboard/scratch take
// (mScratchTake); per element it caches the edited value in the 28-wide
// value/widget side-table (ElementValueCache(), base @0xEA4) and gates copy/paste
// on the 28-wide per-element enable bytes (mabElementEnabled). The element schema
// is the shared ICEElementDescriptions[48] table (ICEDataEnums.hpp); two file-local
// data tables (a per-channel copy plan and the per-channel-filtered SetState sweep)
// are reconstructed here -- FLAGGED below where their contents are not recoverable.
// ============================================================================

#include "SDKs/Packages/ICE/ICEAuthor.hpp"
#include "SDKs/Packages/ICE/ICEData.hpp"            // ICE::ICETake editor ops / ICEElementDescription / ICEValue
#include "SDKs/Packages/ICE/ICEMemory.hpp"          // ICE::ICEPointers (pulled by the layout)
#include "rw/core/stdc/stdc.h"                       // rw::core::stdc::StringCompare

namespace ICE
{

// ---------------------------------------------------------------------------
// File-local: per-channel COPY PLAN table.
//
// CopyKeyElements / PasteKeyElements index a per-channel plan keyed by
// miCurrentChannel. Each plan record carries a count of elements to copy/paste and
// a list of element indices; the body walks the list, gates each entry on the
// per-element enable byte (mabElementEnabled), and copies/pastes that element.
//
// FLAG (PLACEHOLDER): the runtime plan table's record stride is 204 bytes
// (KU_ICE_COPY_PLAN_STRIDE below): word[1] is the element count and word[2..] are
// the element-index list (matching record+4 = count, record+8 = list base). The
// concrete contents (which channels copy which elements) live in a data-segment
// table that is NOT recovered here, so the table is modeled as an empty-but-shaped
// accessor: GetCopyPlan() returns a zero-count plan for every channel, which makes
// CopyKeyElements / PasteKeyElements no-op (return success) until the table data is
// reconstructed. Reconstructing the real per-channel element lists is a follow-up.
// ---------------------------------------------------------------------------
struct ICECopyPlan
{
    s32 miElementCount;          // record+4: number of elements in this plan
    const s32* mpiElementList;   // record+8..: element indices to copy/paste
};

// Record stride of the runtime per-channel plan table (204 bytes per channel).
static const s32 KU_ICE_COPY_PLAN_STRIDE = 204;

// Empty (zero-count) plan stand-in for every channel until the real table data is
// recovered. FLAG: placeholder -- the element lists are not modeled.
static const ICECopyPlan KrEmptyCopyPlan = { 0, 0 };

static const ICECopyPlan& GetCopyPlan(s32 /*liChannel*/)
{
    // Touch the stride so the documented record size stays referenced.
    (void)KU_ICE_COPY_PLAN_STRIDE;
    return KrEmptyCopyPlan;
}

// ---------------------------------------------------------------------------
// IsEndKeyState dispatch helper (shared by the key-element ops and SetState).
//
// While in SELECT_COPY_MODE (16) the "is this an end key" question is answered for
// the PREVIOUS state via the live-state IsEndKeyState walk (seeded from miPrevState).
// The END_KEY_SELECTED (13) / EDIT_END_KEY (15) / ASSEMBLY_EDIT_END (18) states are
// unconditional end-key states. Every other state is a start-key state (false).
// This mirrors the inline state test the bodies share.
// ---------------------------------------------------------------------------
static bool ResolveEndKey(const ICEAuthor& lrAuthor, s32 liState, s32 liPrevState)
{
    if (liState == E_ICE_AUTHOR_STATE_SELECT_COPY_MODE)
    {
        return lrAuthor.IsEndKeyState(liPrevState);
    }
    if (liState == E_ICE_AUTHOR_STATE_END_KEY_SELECTED)  { return true; }
    if (liState == E_ICE_AUTHOR_STATE_EDIT_END_KEY)      { return true; }
    if (liState == E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_END) { return true; }
    return false;
}

// ---------------------------------------------------------------------------
// ToggleKey
//
// Flip the current channel's current-interval boundary between a soft join and a
// hard cut for the active key edge. The active edge is the end key in the end-key
// states (resolved through the SELECT_COPY_MODE indirection) and the start key
// otherwise. If the boundary is currently soft, harden it (insert a key); if it is
// already a hard cut, soften it (remove the key). Operates on the live edit take's
// current channel / current interval.
// ---------------------------------------------------------------------------
void ICEAuthor::ToggleKey()
{
    const bool lbEndKey = ResolveEndKey(*this, miState, miPrevState);

    // The current interval of the current channel (the channel's own cached
    // current-interval) is the boundary being toggled.
    const u16 lu16Interval = mEditTake.GetCurrentInterval(miCurrentChannel);

    // A hard cut at this boundary for the active edge -> soften it; a soft boundary
    // -> harden it. ICEChannel::IsHardCut takes the edge offset (0 start / 1 end).
    if (mEditTake.IsHardCut(miCurrentChannel, lu16Interval, lbEndKey ? 1 : 0))
    {
        mEditTake.Soften(miCurrentChannel, lu16Interval, lbEndKey ? 1 : 0);
    }
    else
    {
        mEditTake.Harden(miCurrentChannel, lu16Interval, lbEndKey ? 1 : 0);
    }
}

// ---------------------------------------------------------------------------
// CopyKeyElements
//
// Copy the enabled elements of the current channel's current interval OUT of the
// live edit take into the scratch/clipboard take (direction: mEditTake ->
// mScratchTake). Walks the channel's copy plan; for each planned element that is
// enabled (mabElementEnabled), copies it at the active key edge offset (end-key 1 /
// start-key 0). Stops and returns false if any copy fails; returns true when the
// whole plan copies (or the channel has no plan).
// ---------------------------------------------------------------------------
bool ICEAuthor::CopyKeyElements()
{
    const ICECopyPlan& lrPlan = GetCopyPlan(miCurrentChannel);
    if (lrPlan.miElementCount <= 0)
    {
        return true;
    }

    for (s32 liIndex = 0; liIndex < lrPlan.miElementCount; ++liIndex)
    {
        const s32 liElement = lrPlan.mpiElementList[liIndex];
        if (ElementEnabled(liElement))
        {
            const bool lbEndKey = ResolveEndKey(*this, miState, miPrevState);
            if (!mEditTake.CopyElement(&mScratchTake, miCurrentChannel, (s16)(lbEndKey ? 1 : 0), liElement))
            {
                return false;
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// PasteKeyElements
//
// Paste the enabled elements of the current channel back INTO the live edit take
// from the scratch/clipboard take (direction: mScratchTake -> mEditTake). Same plan
// walk and enable gating as CopyKeyElements, but pastes at the active key edge
// offset. Stops and returns false if any paste fails; returns true when the whole
// plan pastes (or the channel has no plan).
// ---------------------------------------------------------------------------
bool ICEAuthor::PasteKeyElements()
{
    const ICECopyPlan& lrPlan = GetCopyPlan(miCurrentChannel);
    if (lrPlan.miElementCount <= 0)
    {
        return true;
    }

    for (s32 liIndex = 0; liIndex < lrPlan.miElementCount; ++liIndex)
    {
        const s32 liElement = lrPlan.mpiElementList[liIndex];
        if (ElementEnabled(liElement))
        {
            const bool lbEndKey = ResolveEndKey(*this, miState, miPrevState);
            if (!mEditTake.PasteElement(&mScratchTake, miCurrentChannel, (s16)(lbEndKey ? 1 : 0), liElement))
            {
                return false;
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// SetKeyElementValue
//
// Set element liElement to an absolute value on the live edit take, at the active
// key edge of the current channel's current interval. The destination key is the
// current interval's bracket key (0 when the channel has no current interval) plus
// the edge offset (end-key 1 / start-key 0). The edited value is cached in the
// per-element value cache (ElementValueCache, the widget side-table) before writing
// it through to the take.
// ---------------------------------------------------------------------------
void ICEAuthor::SetKeyElementValue(s32 liElement, f32 lfValue)
{
    // Bracket key of the current channel's current interval (its own cached
    // current-interval): interval 0 -> key 0, an interior interval -> its stored
    // boundary key, the final interval -> the last key.
    const u16 lu16Interval = mEditTake.GetCurrentInterval(miCurrentChannel);
    u16 lu16BracketKey = 0;
    if (lu16Interval != 0)
    {
        lu16BracketKey = (u16)mEditTake.GetIntervalKey(miCurrentChannel, lu16Interval);
    }

    const bool lbEndKey = ResolveEndKey(*this, miState, miPrevState);
    const u16 lu16Key = (u16)(lu16BracketKey + (u16)(lbEndKey ? 1 : 0));

    // Cache the edited value (the widget side-table doubles as the f32 value cache).
    ElementValueCache()[liElement] = lfValue;

    mEditTake.SetValue(liElement, lu16Key, ICEValue(lfValue));
}

// ---------------------------------------------------------------------------
// SetKeyElementValueChange
//
// Apply a relative value delta to element liElement on the live edit take. No-op
// when the delta is zero. Adds lfDelta to the cached element value, clamps the
// result to the element's [mMin, mMax] range (the description's min/max fields at
// +0x18/+0x1C), re-caches it, then writes the clamped value through to the take at
// the active key edge -- same key-edge resolution as SetKeyElementValue. liChange is
// the schema-token disambiguator carried by callers.
// ---------------------------------------------------------------------------
void ICEAuthor::SetKeyElementValueChange(s32 liElement, s32 /*liChange*/, f32 lfDelta)
{
    if (lfDelta == 0.0f)
    {
        return;
    }

    const u16 lu16Interval = mEditTake.GetCurrentInterval(miCurrentChannel);
    u16 lu16BracketKey = 0;
    if (lu16Interval != 0)
    {
        lu16BracketKey = (u16)mEditTake.GetIntervalKey(miCurrentChannel, lu16Interval);
    }

    const bool lbEndKey = ResolveEndKey(*this, miState, miPrevState);
    const u16 lu16Key = (u16)(lu16BracketKey + (u16)(lbEndKey ? 1 : 0));

    // The element-description min/max bounds clamp the accumulated value (stride 0x58
    // per description): lower-bound max(val, mMin) then upper-bound min(val, mMax),
    // applied to (cachedValue + delta). mMin/mMax are ICEValue, read as their f32 view.
    const ICEElementDescription& lrDesc = ICEElementDescriptions[liElement];
    const f32 lfLow  = lrDesc.mMin.GetFloat();   // low clamp bound  (description +0x18)
    const f32 lfHigh = lrDesc.mMax.GetFloat();   // high clamp bound (description +0x1C)

    f32 lfValue = ElementValueCache()[liElement] + lfDelta;
    if (lfValue < lfLow)  { lfValue = lfLow; }
    if (lfValue > lfHigh) { lfValue = lfHigh; }

    ElementValueCache()[liElement] = lfValue;

    mEditTake.SetValue(liElement, lu16Key, ICEValue(lfValue));
}

// ---------------------------------------------------------------------------
// GetElementIndexByTag
//
// Find the element-description index whose channel matches the current channel and
// whose display name matches lpcTag. Scans the element schema from index 28 (the
// editor's tag-searchable region begins past the 28 fixed widget elements); returns
// the absolute element index on a match, -1 if none.
//
// NOTE (name vs field): despite the "ByTag" name the scan compares the element's
// DISPLAY NAME field (mpDisplayName, +0x04) against lpcTag, not mpTag (+0x00). The
// scan base is the +0x04 field of ICEElementDescriptions[28] and walks at stride 0x58;
// the string compare reads that +0x04 field (mpDisplayName) and the equality gate
// reads the following +0x08 field (miChannelNumber). Reconstructed with the named
// members that resolve to those same offsets.
// ---------------------------------------------------------------------------
s32 ICEAuthor::GetElementIndexByTag(const char* lpcTag) const
{
    const s32 liBase = KI_ICE_AUTHOR_ELEMENTS; // scan starts at element index 28
    for (s32 liElement = liBase; liElement < eICE_NUM_ELEMENTS; ++liElement)
    {
        const ICEElementDescription& lrDesc = ICEElementDescriptions[liElement];
        if (lrDesc.miChannelNumber == miCurrentChannel
            && rw::core::stdc::StringCompare(lrDesc.mpDisplayName, lpcTag) == 0)
        {
            return liElement;
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// SetState
//
// The editor state-machine transition. No-op when the target state equals the
// current state. Otherwise:
//   1. Save the current state as the previous state.
//   2. LEAVE-from-key (current state > INTERVAL_SETTINGS): restore the live take to
//      the saved playback parameter and restore the saved play range.
//   3. ENTER per-old-state cleanup (switch on the still-current state):
//        OFF (0)        -> reset channel/play/zoom ranges, new edit buffer
//        ASSEMBLY_MARK(8)-> reset zoom range to [0,1]
//        ASSEMBLY_EDIT(9)-> reset the sub-parameter
//        INTERVAL_SETTINGS..EDIT_END_KEY (11..15) -> if the take is unchanged from
//                          its newest undo snapshot, discard that snapshot
//   4. Commit the new state.
//   5. ENTER-to-key (new state > INTERVAL_SETTINGS): snapshot the play range, move
//      the take parameter onto the active key edge of the current interval bracket,
//      and re-fit the zoom window.
//   6. ENTER per-new-state setup (switch on the new state):
//        OFF (0)        -> free the edit buffer
//        ASSEMBLY_MARK(8)-> select assembly channel 0, bind the sub-take
//        ASSEMBLY_EDIT(9)/ASSEMBLY_EDIT_PLAYING(10) -> select assembly channel 10,
//                          move the take parameter to the current parameter
//        INTERVAL_SETTINGS(11) -> push an undo snapshot
//        START/END_KEY_SELECTED (12/13) and EDIT_START/END_KEY (14/15) -> refresh
//                          the per-element value cache from the take at the resolved
//                          key edge (over the current channel's elements), then push
//                          an undo snapshot
// ---------------------------------------------------------------------------
void ICEAuthor::SetState(s32 liState, f32 lfKey)
{
    const s32 liOldState = miState;
    if (liOldState == liState)
    {
        return;
    }

    miPrevState = liOldState;

    // ---- 2. LEAVE-from-key: restore parameter + play range ----
    if (liOldState > E_ICE_AUTHOR_STATE_INTERVAL_SETTINGS)
    {
        mEditTake.SetParameter(mfSavedParameter, false, false);
        mfRangeLo = mfSavedRangeLo;
        mfRangeHi = mfSavedRangeHi;
    }

    // ---- 3. per-old-state cleanup (state still == liOldState here) ----
    switch (miState)
    {
        case E_ICE_AUTHOR_STATE_OFF:
            miCurrentChannel = 0;
            mfRangeLo = 0.0f;
            mfZoomLo  = 0.0f;
            mfRangeHi = 1.0f;
            mfZoomHi  = 1.0f;
            mEditTake.NewEditBuffer();
            break;

        case E_ICE_AUTHOR_STATE_ASSEMBLY_MARK:
            mfZoomLo = 0.0f;
            mfZoomHi = 1.0f;
            break;

        case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT:
            mEditTake.SetSubParameter(0.0f);
            break;

        case E_ICE_AUTHOR_STATE_INTERVAL_SETTINGS:
        case E_ICE_AUTHOR_STATE_START_KEY_SELECTED:
        case E_ICE_AUTHOR_STATE_END_KEY_SELECTED:
        case E_ICE_AUTHOR_STATE_EDIT_START_KEY:
        case E_ICE_AUTHOR_STATE_EDIT_END_KEY:
            if (!mEditTake.DataChanged())
            {
                mEditTake.DiscardUndo();
            }
            break;

        default:
            break;
    }

    // ---- 4. commit the new state ----
    miState = liState;

    // ---- 5. ENTER-to-key: snapshot range, seek to the key edge, re-fit zoom ----
    if (liState > E_ICE_AUTHOR_STATE_INTERVAL_SETTINGS)
    {
        mfSavedParameter = mEditTake.GetParameter();
        mfSavedRangeLo   = mfRangeLo;
        mfSavedRangeHi   = mfRangeHi;

        const u16 lu16Interval = mEditTake.GetCurrentInterval(miCurrentChannel);

        bool lbEndKey;
        if (liState == E_ICE_AUTHOR_STATE_SELECT_COPY_MODE)
        {
            lbEndKey = IsEndKeyState((s32)lfKey);
        }
        else
        {
            lbEndKey = (liState == E_ICE_AUTHOR_STATE_END_KEY_SELECTED)
                    || (liState == E_ICE_AUTHOR_STATE_EDIT_END_KEY)
                    || (liState == E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_END);
        }

        f32 lfStart = 0.0f;
        f32 lfEnd   = 1.0f;
        GetIntervalBracket(lu16Interval, &lfStart, &lfEnd);

        const f32 lfTarget = lbEndKey ? lfEnd : lfStart;
        mEditTake.SetParameter(lfTarget, true, false);
        AdjustZoomRange();
    }

    // ---- 6. per-new-state setup ----
    switch (miState)
    {
        case E_ICE_AUTHOR_STATE_OFF:
            mEditTake.FreeEditBuffer();
            break;

        case E_ICE_AUTHOR_STATE_ASSEMBLY_MARK:
            miCurrentChannel = 0;
            mEditTake.SetSubTake(mpSubTakeData, true);
            break;

        case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT:
            miCurrentChannel = 10;
            mEditTake.SetParameter(mEditTake.GetParameter(), true, false);
            break;

        case E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_PLAYING:
            mEditTake.SetParameter(mEditTake.GetParameter(), true, false);
            break;

        case E_ICE_AUTHOR_STATE_INTERVAL_SETTINGS:
            mEditTake.PushUndo();
            break;

        case E_ICE_AUTHOR_STATE_START_KEY_SELECTED:
        case E_ICE_AUTHOR_STATE_END_KEY_SELECTED:
            RefreshElementValueCache(lfKey);
            mEditTake.PushUndo();
            break;

        case E_ICE_AUTHOR_STATE_EDIT_START_KEY:
        case E_ICE_AUTHOR_STATE_EDIT_END_KEY:
            RefreshElementValueCache(lfKey);
            mEditTake.PushUndo();
            break;

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// RefreshElementValueCache (SetState helper for the key-selected/edit-key states)
//
// Reload the per-element value cache (the widget side-table) from the live take at
// the active key edge, over the elements that drive the current channel. The active
// key is the current interval's bracket key plus the end-key offset (resolved with
// the same SELECT_COPY_MODE indirection as the rest of the state machine). Each
// element-description whose channel matches the current channel has its take value
// (read as float at that key) written into its cache slot.
//
// FLAG (PLACEHOLDER element selector): the runtime sweeps the FIRST 28 element
// descriptions (ICEElementDescriptions[0..27], the editor's widget-element region)
// and writes their values into the matching value-cache slots, filtering on
// miChannelNumber == miCurrentChannel. The 28 bound is the widget side-table width
// (KI_ICE_AUTHOR_ELEMENTS); modeled directly from the schema rather than from a
// recovered data-segment selector table.
// ---------------------------------------------------------------------------
void ICEAuthor::RefreshElementValueCache(f32 lfKey)
{
    const u16 lu16Interval = mEditTake.GetCurrentInterval(miCurrentChannel);
    u16 lu16BracketKey = 0;
    if (lu16Interval != 0)
    {
        lu16BracketKey = (u16)mEditTake.GetIntervalKey(miCurrentChannel, lu16Interval);
    }

    bool lbEndKey;
    if (miState == E_ICE_AUTHOR_STATE_SELECT_COPY_MODE)
    {
        lbEndKey = IsEndKeyState((s32)lfKey);
    }
    else
    {
        lbEndKey = (miState == E_ICE_AUTHOR_STATE_END_KEY_SELECTED)
                || (miState == E_ICE_AUTHOR_STATE_EDIT_END_KEY)
                || (miState == E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_END);
    }
    const u16 lu16Key = (u16)(lu16BracketKey + (u16)(lbEndKey ? 1 : 0));

    f32* lpfCache = ElementValueCache();
    for (s32 liElement = 0; liElement < KI_ICE_AUTHOR_ELEMENTS; ++liElement)
    {
        const ICEElementDescription& lrDesc = ICEElementDescriptions[liElement];
        if (lrDesc.miChannelNumber == miCurrentChannel)
        {
            lpfCache[liElement] = mEditTake.GetValueFloat(liElement, lu16Key);
        }
    }
}

} // namespace ICE
