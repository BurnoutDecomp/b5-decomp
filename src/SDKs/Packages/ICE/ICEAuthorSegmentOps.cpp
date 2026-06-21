// ============================================================================
// SDKs/Packages/ICE/ICEAuthorSegmentOps.cpp
//
// ICE::ICEAuthor -- the SEGMENT-OPS group of the in-game ICE camera-take editor.
// Reconstructed against the frozen layout in ICEAuthor.hpp. THIS TU bodies one
// fan-out group of ICEAuthor (the declarations are in ICEAuthor.hpp; the
// foundational subset -- Construct / state getters / interval delegators -- is
// bodied in ICEAuthor.cpp; the key-element ops in ICEAuthorKeyElementOps.cpp). The
// per-TU `cl /c` gate does not link, so the un-bodied declarations from the other
// groups are fine.
//
// Group bodies:
//   InsertTakeSegment, DeleteTakeSegment, JumpToNextKey, JumpToPreviousKey.
//
// These operate on the assembly channel (10) of the live edit take (mEditTake @0x28)
// and on the current channel for the jump cursor. The compiler inlined the embedded
// ICETake / ICEChannel accesses (current-interval reads, interval-parameter quantize,
// the per-element SetValue bounds check); the reconstruction reverses that inlining
// back into the named ICETake member calls declared in ICEData.hpp. The byte offsets
// in ICEAuthor.hpp / ICEData.hpp are the layout facts, not used as casts here.
// ============================================================================

#include "SDKs/Packages/ICE/ICEAuthor.hpp"
#include "SDKs/Packages/ICE/ICEData.hpp"            // ICE::ICETake editor ops / ICEElementDescription / ICEValue / ICEChannel
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
#include "SDKs/Packages/ICE/ICEMemory.hpp"          // ICE::ICEPointers (pulled by the layout)
#include "rw/core/stdc/stdc.h"                       // rw stdc (pulled by the ICE includes)

namespace ICE
{

// ---------------------------------------------------------------------------
// File-local segment-op constants.
//
// The take's ASSEMBLY channel (channel index 10) carries the per-take-segment
// metadata. The three per-segment metadata elements live at the schema indices the
// segment ops write: TAKE_START (45), TAKE_NUMBER (46) and CONTAINS_SUBTAKE (47) --
// all on the assembly channel, per the ICEElementDescriptions[] table (ICEData.cpp:
// [45] "TAKE_START", [46] "TAKE_NUMBER", [47] "CONTAINS_SUBTAKE", all channel 10).
// Named here so the SetValue calls below read by name, not magic index.
// ---------------------------------------------------------------------------
static const s32 KI_ICE_ASSEMBLY_CHANNEL       = 10;
static const s32 KI_ICE_ELEMENT_TAKE_START     = 45;
static const s32 KI_ICE_ELEMENT_TAKE_NUMBER    = 46;
static const s32 KI_ICE_ELEMENT_CONTAINS_SUBTAKE = 47;

// The boundary-search epsilon (a fraction under one frame at 30fps): the segment
// ops snap the current playback parameter onto the nearest interval boundary using
// this tolerance window. The constant value is 0.0000099999997f.
static const f32 KF_ICE_SEGMENT_EPSILON = 0.0000099999997f;

// Magnitude of a parameter delta. The segment-insert nearest-boundary pick selects
// the value when it is non-negative, otherwise its negation.
static inline f32 SegmentAbs(f32 lfValue)
{
    return (lfValue >= 0.0f) ? lfValue : -lfValue;
}

// ===========================================================================
// InsertTakeSegment  (returns bool: true if a segment was inserted)
//
// Insert a new take-segment boundary at the current playback parameter of the
// assembly channel. Rejected (returns false) when the parameter sits on the take
// end (== 1.0), the insert tangent is negative, the resulting boundary would reach
// or pass the take end, or the tangent is non-positive.
//
// Otherwise: insert a key on the assembly channel at the current parameter, find the
// interval index whose boundary parameter is nearest the current parameter (compare
// |currentParam - boundary(n)| against |currentParam - boundary(n+1)| and pick the
// closer of n / n+1); advance the parameter past the new segment by the
// segment length and insert a second key, finding its nearest interval the same way;
// collapse any soft-key run that opened up between the two inserted boundaries; jump the
// cursor to the previous key; then stamp the segment's TAKE_START (the tangent arg),
// TAKE_NUMBER (the saved insert tangent field mfInsertTangent @0x1C) and
// CONTAINS_SUBTAKE (1) metadata onto the assembly element at the first inserted
// interval; finally force the parameter back to the segment start.
//
// The nearest-boundary pick is two magnitude selects: |currentParam - boundary(n)|
// vs |currentParam - boundary(n+1)|, then the closer index is chosen (n on a tie).
// SegmentAbs() reproduces the select-based magnitude; the comparison is <= (the tie
// goes to n). The three metadata writes go through ICETake::SetValue, which performs
// the in-range index assert and forwards to ICEElementDescription::SetValue with the
// channel's element data. The two SetParameter forwards differ only in the force flag
// (non-forced mid-insert, forced restore at the end).
//
// Two incoming floats: lfTangent (the first FP input) gates the reject and is stamped
// into TAKE_START; lfSegmentLength (the second FP input) sets the segment end and gates
// the non-positive-length reject. The caller (ICEController::JoyHandler) passes them
// explicitly: InsertTakeSegment(param, subAssemRatio * (rangeEnd - param)). TAKE_NUMBER
// is the separate member mfInsertTangent (this+0x1C), not either argument.
// ===========================================================================
bool ICEAuthor::InsertTakeSegment(f32 lfTangent, f32 lfSegmentLength)
{
    const f32 lfCurrentParam = mEditTake.GetParameter();

    const f32 lfSegmentEnd   = lfCurrentParam + lfSegmentLength;

    // Reject: at the take end, negative tangent, segment runs off the end, or a
    // non-positive segment length.
    if (lfCurrentParam == 1.0f
        || lfTangent < 0.0f
        || lfSegmentEnd >= 1.0f
        || lfSegmentLength <= 0.0f)
    {
        return false;
    }

    // ---- first boundary: insert a key at the current parameter, then pick the
    // interval whose boundary parameter is nearest the current parameter ----
    u16 lu16FirstInterval;
    if (mEditTake.Insert(KI_ICE_ASSEMBLY_CHANNEL, 0) == false)
    {
        // Insert was a no-op (parameter already on a boundary): the boundary is the
        // interval after the current one.
        lu16FirstInterval = (u16)(mEditTake.GetCurrentInterval(KI_ICE_ASSEMBLY_CHANNEL) + 1);
    }
    else
    {
        const u16 lu16Current = mEditTake.GetCurrentInterval(KI_ICE_ASSEMBLY_CHANNEL);
        // |currentParam - boundary(current)|  vs  |currentParam - boundary(current+1)|;
        // pick the closer interval (tie goes to the lower index).
        const f32 lfDistLo = SegmentAbs(lfCurrentParam - mEditTake.GetIntervalStart(KI_ICE_ASSEMBLY_CHANNEL, lu16Current));
        const f32 lfDistHi = SegmentAbs(lfCurrentParam - mEditTake.GetIntervalStart(KI_ICE_ASSEMBLY_CHANNEL, (u16)(lu16Current + 1)));
        lu16FirstInterval = (lfDistLo <= lfDistHi) ? lu16Current : (u16)(lu16Current + 1);
    }
    const u16 lu16FirstKey = (u16)mEditTake.GetIntervalKey(KI_ICE_ASSEMBLY_CHANNEL, lu16FirstInterval);

    // ---- second boundary: move the parameter to the segment end and insert there,
    // picking the nearest interval the same way ----
    mEditTake.SetParameter(lfSegmentEnd, false, false);

    u16 lu16SecondInterval;
    if (mEditTake.Insert(KI_ICE_ASSEMBLY_CHANNEL, 0) == false)
    {
        lu16SecondInterval = (u16)(mEditTake.GetCurrentInterval(KI_ICE_ASSEMBLY_CHANNEL) + 1);
    }
    else
    {
        const u16 lu16Current = mEditTake.GetCurrentInterval(KI_ICE_ASSEMBLY_CHANNEL);
        const f32 lfDistLo = SegmentAbs(lfCurrentParam - mEditTake.GetIntervalStart(KI_ICE_ASSEMBLY_CHANNEL, lu16Current));
        const f32 lfDistHi = SegmentAbs(lfCurrentParam - mEditTake.GetIntervalStart(KI_ICE_ASSEMBLY_CHANNEL, (u16)(lu16Current + 1)));
        lu16SecondInterval = (lfDistLo <= lfDistHi) ? lu16Current : (u16)(lu16Current + 1);
    }
    const u16 lu16SecondKey = (u16)mEditTake.GetIntervalKey(KI_ICE_ASSEMBLY_CHANNEL, lu16SecondInterval);

    // Collapse any soft-key run that opened between the two inserted boundaries.
    if ((s32)((u16)(lu16SecondKey - lu16FirstKey)) > 1)
    {
        mEditTake.DeleteAssemblySoftKeys(lu16FirstKey, lu16SecondKey);
    }

    JumpToPreviousKey();

    // ---- stamp the segment metadata onto the assembly element at the first
    // inserted interval (TAKE_START / TAKE_NUMBER / CONTAINS_SUBTAKE) ----
    // ICETake::SetValue forwards to ICEElementDescription::SetValue with the channel's
    // element data and asserts the index is in range (the inlined line-870 bounds
    // check). TAKE_START carries the tangent arg; TAKE_NUMBER the saved insert
    // tangent member mfInsertTangent (@0x1C); CONTAINS_SUBTAKE the flag value 1.
    mEditTake.SetValue(KI_ICE_ELEMENT_TAKE_START,       lu16FirstKey, ICEValue(lfTangent));
    mEditTake.SetValue(KI_ICE_ELEMENT_TAKE_NUMBER,      lu16FirstKey, ICEValue(mfInsertTangent));
    mEditTake.SetValue(KI_ICE_ELEMENT_CONTAINS_SUBTAKE, lu16FirstKey, ICEValue((s32)1));

    // Force the parameter back onto the segment start.
    mEditTake.SetParameter(lfCurrentParam, true, false);
    return true;
}

// ===========================================================================
// DeleteTakeSegment  (returns bool: true once the take state has been resolved)
//
// Remove the take-segment boundary at the current playback parameter of the assembly
// channel. The boundary index is the channel's current interval, bracketed by its
// neighbours (one interval either side, clamped to the channel ends). The function
// first scans the bracketed intervals: if the current parameter lands within one
// frame-epsilon of a bracket boundary's parameter it bails (returns false) -- the
// parameter is already pinned to a boundary, nothing to delete.
//
// Otherwise it deletes the segment: probe whether each neighbour is a hard cut
// (GetValueInt(47) -- CONTAINS_SUBTAKE -- at the neighbour interval); clear the
// CONTAINS_SUBTAKE flag on this boundary if it is in range; then, if this boundary is
// itself a subtake boundary, jump to the previous key and delete the left neighbour
// (unless it was a hard cut), jump to the next key and delete the right neighbour
// (unless it was a hard cut), and force the parameter back to where it started.
//
// The bracket-scan tolerance test is a clamp: the parameter is clamped into the
// window [boundary - eps, boundary + eps] (a max against the lower bound, then a min
// against the upper bound); if the clamped value equals the parameter unchanged, the
// parameter already sits on that boundary and the function bails. The bracket interval
// parameter is read by the channel-explicit GetIntervalStart (the dequantize factor
// is 1/65535 == 0.000015259022). The neighbour hard-cut probes and the this-boundary
// test are ICETake::GetValueInt of CONTAINS_SUBTAKE; the on-boundary clear goes
// through ICETake::SetValue (the in-range index assert is line 870 of ICEData.cpp).
// The two ICETake::Delete calls pass the current channel (miCurrentChannel).
//
// FLAG (signature): the frozen header declares `bool DeleteTakeSegment()` and the
// body takes only `this`; no extra arguments. Matches.
// ===========================================================================
bool ICEAuthor::DeleteTakeSegment()
{
    const s32 liInterval     = mEditTake.GetCurrentInterval(KI_ICE_ASSEMBLY_CHANNEL);
    const f32 lfCurrentParam = mEditTake.GetParameter();

    // Bracket the boundary by one interval on each side, clamped to the channel ends.
    u16 lu16BracketLo = (u16)(liInterval - 1);
    if (liInterval == 0)
    {
        lu16BracketLo = 0;
    }
    const s32 liNumIntervals = mEditTake.GetNumIntervals(KI_ICE_ASSEMBLY_CHANNEL);
    s32 liBracketHi = liInterval + 1;
    if (liInterval >= liNumIntervals - 1)
    {
        liBracketHi = liNumIntervals - 1;
    }

    // Scan the bracketed intervals: if the parameter already lands within one
    // frame-epsilon of a boundary parameter, the parameter is pinned to a boundary
    // and there is nothing to delete.
    if ((u16)lu16BracketLo <= (u16)liBracketHi)
    {
        for (u16 lu16Probe = lu16BracketLo; ; ++lu16Probe)
        {
            const f32 lfBoundary = mEditTake.GetIntervalStart(KI_ICE_ASSEMBLY_CHANNEL, lu16Probe);

            // clamp(currentParam, boundary - eps, boundary + eps): first the lower
            // bound (max against boundary-eps), then the upper bound (min against boundary+eps).
            f32 lfClamped = lfBoundary - KF_ICE_SEGMENT_EPSILON;
            if (lfClamped < lfCurrentParam) { lfClamped = lfCurrentParam; }
            const f32 lfUpper = lfBoundary + KF_ICE_SEGMENT_EPSILON;
            if (lfClamped > lfUpper) { lfClamped = lfUpper; }

            if (lfCurrentParam == lfClamped)
            {
                // The parameter sits on this boundary -- nothing to delete.
                return false;
            }
            if (lu16Probe >= (u16)liBracketHi)
            {
                break;
            }
        }
    }

    // ---- delete the boundary ----
    // Probe whether each in-range neighbour is a hard cut (a subtake boundary).
    bool lbLeftHardCut  = true;
    bool lbRightHardCut = true;
    if (liInterval > 0)
    {
        lbLeftHardCut = mEditTake.GetValueInt(KI_ICE_ELEMENT_CONTAINS_SUBTAKE, (u16)(liInterval - 1)) != 0;
    }
    if (liInterval < liNumIntervals - 1)
    {
        lbRightHardCut = mEditTake.GetValueInt(KI_ICE_ELEMENT_CONTAINS_SUBTAKE, (u16)(liInterval + 1)) != 0;
    }

    // Clear the CONTAINS_SUBTAKE flag on this boundary when at least one neighbour is
    // a hard cut and the index is in range (the inlined line-870 bounds assert).
    if (lbLeftHardCut || lbRightHardCut)
    {
        CGS_ASSERT((u16)liInterval < (u16)mEditTake.GetNumIntervals(KI_ICE_ASSEMBLY_CHANNEL),
                   "lu16Index < ( lbIsKey ? lChannel.GetNumKeys() : lChannel.GetNumIntervals())");
        mEditTake.SetValue(KI_ICE_ELEMENT_CONTAINS_SUBTAKE, (u16)liInterval, ICEValue((s32)0));
    }

    // If this boundary is a subtake boundary, drop the soft neighbours and restore
    // the parameter.
    if (mEditTake.GetValueInt(KI_ICE_ELEMENT_CONTAINS_SUBTAKE) != 0)
    {
        JumpToPreviousKey();
        if (lbLeftHardCut == false)
        {
            mEditTake.Delete(miCurrentChannel);
        }
        JumpToNextKey();
        if (lbRightHardCut == false)
        {
            mEditTake.Delete(miCurrentChannel);
        }
        mEditTake.SetParameter(lfCurrentParam, false, false);
    }
    return true;
}

// ===========================================================================
// JumpToNextKey  (void)
//
// Move the cursor (the edit take's playback parameter) forward onto the next key
// edge. Starting from the current parameter plus one frame-epsilon, take the current
// channel's current-interval END; if the cursor has already passed that end and there
// is a later interval, advance to the following interval's end. When the current
// channel is neither the primary (0) nor the assembly (10) channel, also fold in the
// primary channel's interval-end edge (its current interval, or the next one if the
// cursor has passed it), but never past the channel's own end. Force the take to the
// resolved parameter.
//
// The current-channel interval end comes from the ICEAuthor GetIntervalEnd delegator
// (which resolves the current channel). The primary-channel fold uses the channel-
// explicit ICETake::GetIntervalEnd on channel 0. The final write is ICETake::SetParameter
// with wrap enabled.
//
// FLAG (signature): the frozen header declares `void JumpToNextKey()`; the body takes
//   only `this`. Matches.
// ===========================================================================
void ICEAuthor::JumpToNextKey()
{
    const f32 lfParamPlusEps = mEditTake.GetParameter() + KF_ICE_SEGMENT_EPSILON;

    const u16 lu16Interval = mEditTake.GetCurrentInterval(miCurrentChannel);
    f32 lfIntervalEnd = GetIntervalEnd((s16)lu16Interval);
    if (lfParamPlusEps > lfIntervalEnd
        && lu16Interval < (u16)(mEditTake.GetNumIntervals(miCurrentChannel) - 1))
    {
        lfIntervalEnd = GetIntervalEnd((s16)(lu16Interval + 1));
    }

    f32 lfTarget = lfIntervalEnd;
    if (miCurrentChannel != 0 && miCurrentChannel != KI_ICE_ASSEMBLY_CHANNEL)
    {
        // Fold in the primary channel's interval-end edge.
        const u16 lu16PrimaryInterval = mEditTake.GetCurrentInterval(0);
        lfTarget = mEditTake.GetIntervalEnd(0, lu16PrimaryInterval);
        if (lfParamPlusEps > lfTarget
            && lu16PrimaryInterval < (u16)(mEditTake.GetNumIntervals(0) - 1))
        {
            lfTarget = mEditTake.GetIntervalEnd(0, (u16)(lu16PrimaryInterval + 1));
        }
        if (lfIntervalEnd < lfTarget)
        {
            lfTarget = lfIntervalEnd;
        }
    }

    mEditTake.SetParameter(lfTarget, false, true);
}

// ===========================================================================
// JumpToPreviousKey  (void)
//
// Move the cursor (the edit take's playback parameter) backward onto the previous key
// edge. Starting from the current parameter minus one frame-epsilon, take the current
// channel's current-interval START (interval 0 -> 0.0, past the end -> 1.0, else the
// quantized boundary parameter; dequantize factor 1/65535 == 0.000015259022); if the
// cursor is before that start and the interval is > 0, step back to the previous
// interval's start. When the current channel is neither the primary (0) nor the
// assembly (10) channel, also fold in the primary channel's interval-start edge (its
// current interval, or the previous one if the cursor is before it), but never before
// the channel's own start. Force the take to the resolved parameter.
//
// The current-channel interval start is read by the channel-explicit
// ICETake::GetIntervalStart; the step-back uses the ICEAuthor GetIntervalStart
// delegator. The primary-channel fold uses the channel-explicit ICETake::GetIntervalStart
// on channel 0. The final write is ICETake::SetParameter with wrap enabled.
//
// FLAG (signature): the ledger renders `JumpToPreviousKey(int a1, int a2)`, but the
//   `a2` is a spilled (dead) input -- the body reads only `this`, and the frozen
//   header declares the no-arg `void JumpToPreviousKey()`. The no-arg form is used.
// ===========================================================================
void ICEAuthor::JumpToPreviousKey()
{
    const f32 lfParamMinusEps = mEditTake.GetParameter() - KF_ICE_SEGMENT_EPSILON;

    const u16 lu16Interval = mEditTake.GetCurrentInterval(miCurrentChannel);
    // Current-channel current-interval start (the inlined GetIntervalStart): the
    // channel-explicit form on the edit take resolves the 0/end/quantized cases.
    f32 lfIntervalStart = mEditTake.GetIntervalStart(miCurrentChannel, lu16Interval);
    if (lfParamMinusEps < lfIntervalStart && lu16Interval > 0)
    {
        lfIntervalStart = GetIntervalStart((u16)(lu16Interval - 1));
    }

    f32 lfTarget = lfIntervalStart;
    if (miCurrentChannel != 0 && miCurrentChannel != KI_ICE_ASSEMBLY_CHANNEL)
    {
        // Fold in the primary channel's interval-start edge.
        const u16 lu16PrimaryInterval = mEditTake.GetCurrentInterval(0);
        lfTarget = mEditTake.GetIntervalStart(0, lu16PrimaryInterval);
        if (lfParamMinusEps < lfTarget && lu16PrimaryInterval > 0)
        {
            lfTarget = mEditTake.GetIntervalStart(0, (u16)(lu16PrimaryInterval - 1));
        }
        if (lfIntervalStart > lfTarget)
        {
            lfTarget = lfIntervalStart;
        }
    }

    mEditTake.SetParameter(lfTarget, false, true);
}

} // namespace ICE
