// ============================================================================
// SDKs/Packages/ICE/ICEAuthorParamOps.cpp
//
// ICE::ICEAuthor -- the PARAMETER-MOVE group of the in-game ICE camera-take editor.
// Reconstructed against the frozen layout in ICEAuthor.hpp. THIS TU bodies the two
// per-frame parameter-shuttle methods the editor's controller spine calls every
// frame while a key is selected / the cursor is moved:
//
//   MoveParameter       -- nudge the current channel's interval-boundary parameter
//                          by a per-frame delta (key-edit shuttle).
//   SetParameterChange  -- fold a per-frame delta into the live playback parameter
//                          (cursor-move shuttle).
//
// SIGNATURES (resolved from the call sites): both take a per-frame f32 delta and a
// bool gate. The delta arrives in the FP argument register and the gate in a byte
// argument register; both are SPILLED across the helper-call prologue, which is why
// a flat by-address recovery types them away. The two callers (MoveParameter <-
// ICEController::Update; SetParameterChange <- ICEController::MoveCursor) both:
//   - compute the delta as (yawVelHi - yawVelLo) * timestep * shuttleSpeed and pass
//     it in fp,
//   - pass a byte gate (1 when the editor's shuttle is active, the held value
//     otherwise).
// So the frozen no-arg declarations were wrong; the header is updated to
// MoveParameter(f32, bool) / SetParameterChange(f32, bool) and the controller spine
// (not edited here) must pass (lfDelta, lbApply) instead of staging the delta on a
// member. See the file-end reconciliation note.
//
// SHARED SHAPE: both methods, when the gate is set and the delta is non-zero and a
// channel is selected, derive a snap tolerance from the visible range width
// (mfRangeHi - mfRangeLo) * 0.015, bracket the current interval, and -- if the
// current parameter sits exactly on a bracket boundary -- snap by accumulating the
// delta into mfMoveAccumulator and only releasing a move once half the accumulator
// magnitude exceeds the tolerance (a hysteresis that keeps a boundary "sticky").
// The remaining delta is then forwarded: MoveParameter re-spaces the channel's
// boundary (ICETake::MoveParameter); SetParameterChange seeks the live take
// parameter (ICETake::SetParameter), clamped to [0, 1].
//
// The byte offsets in ICEAuthor.hpp / ICEData.hpp are the layout facts; they are not
// used as casts here -- the accesses go through the named members and the ICETake
// methods declared in ICEData.hpp.
// ============================================================================

#include "SDKs/Packages/ICE/ICEAuthor.hpp"
#include "SDKs/Packages/ICE/ICEData.hpp"            // ICE::ICETake editor ops / ICEChannel / ICEValue
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT (parity with the sibling ops TUs)
#include "SDKs/Packages/ICE/ICEMemory.hpp"          // ICE::ICEPointers (pulled by the layout)
#include "rw/core/stdc/stdc.h"                       // rw stdc (pulled by the ICE includes)

namespace ICE
{

// ---------------------------------------------------------------------------
// File-local parameter-move constants.
//
// The primary channel is index 0; the snap-tolerance probe runs against it. The
// snap tolerance is (visible range width) * 0.015 -- a fraction of the on-screen
// timeline width, so a boundary is "sticky" within ~1.5% of the visible window.
// The accumulator releases its move once HALF the accumulated magnitude exceeds the
// tolerance (the 0.5 hysteresis factor).
// ---------------------------------------------------------------------------
static const s32 KI_ICE_PRIMARY_CHANNEL    = 0;
static const f32 KF_ICE_RANGE_SNAP_FACTOR  = 0.015f;   // tolerance = rangeWidth * this
static const f32 KF_ICE_ACCUM_HALF         = 0.5f;     // release threshold = |accum| * this

// ---------------------------------------------------------------------------
// IsEndKeyState dispatch (shared shape with the other ICEAuthor op groups): while in
// SELECT_COPY_MODE (16) the question defers to the PREVIOUS state via the live-state
// walk seeded from miPrevState; END_KEY_SELECTED (13) / EDIT_END_KEY (15) /
// ASSEMBLY_EDIT_END (18) are unconditional end-key states; everything else is a
// start-key state. Mirrors the inline state test in MoveParameter's tail.
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

// ===========================================================================
// MoveParameter
//
// Per-frame key-edit shuttle. lbApply gates the snap/accumulate hysteresis; when it
// is set, the delta is non-zero, and a channel is selected, the move snaps to the
// current channel's primary-interval bracket boundary using a range-derived
// tolerance and folds the leftover into mfMoveAccumulator. The resulting move is
// then forwarded to the current channel's interval boundary via ICETake::MoveParameter
// (delta side = the resolved end-key state, parameter = the released move).
//
// Snap test: bracket the primary channel's current interval. If the current
// parameter (mEditTake.GetParameter) lands exactly on either bracket edge (within
// the floating compare), engage the accumulator: lfMove becomes (edge - param) (==0
// on the edge); the accumulated delta is held in mfMoveAccumulator until half its
// magnitude exceeds the tolerance, at which point the accumulator is zeroed and the
// half-magnitude is released as the move. Off a boundary the raw delta passes through.
// ===========================================================================
void ICEAuthor::MoveParameter(f32 lfDelta, bool lbApply)
{
    f32 lfMove = lfDelta;

    if (lbApply && lfDelta != 0.0f && miCurrentChannel != 0)
    {
        const f32 lfParam     = mEditTake.GetParameter();
        const f32 lfTolerance = (mfRangeHi - mfRangeLo) * KF_ICE_RANGE_SNAP_FACTOR;

        // Bracket the primary channel's current interval.
        f32 lfBracketStart, lfBracketEnd;
        const u16 lu16Interval = mEditTake.GetCurrentInterval(KI_ICE_PRIMARY_CHANNEL);
        mEditTake.GetIntervalBracket(KI_ICE_PRIMARY_CHANNEL, lu16Interval, &lfBracketStart, &lfBracketEnd);

        // On the start edge? Test param == clamp(param, edge-tol, edge+tol): first
        // raise to max(edge-tol, param), then lower to min(edge+tol, that). param
        // survives the clamp iff it sits within the tolerance band of the edge.
        f32 lfOnEdge = lfBracketStart;
        f32 lfLow  = (lfBracketStart - lfTolerance < lfParam) ? lfParam : (lfBracketStart - lfTolerance);
        f32 lfHigh = (lfBracketStart + lfTolerance < lfLow)   ? (lfBracketStart + lfTolerance) : lfLow;
        bool lbOnBoundary = (lfParam == lfHigh);

        if (!lbOnBoundary)
        {
            // Otherwise test the end edge the same way.
            lfOnEdge = lfBracketEnd;
            lfLow  = (lfBracketEnd - lfTolerance < lfParam) ? lfParam : (lfBracketEnd - lfTolerance);
            lfHigh = (lfBracketEnd + lfTolerance < lfLow)   ? (lfBracketEnd + lfTolerance) : lfLow;
            lbOnBoundary = (lfParam == lfHigh);
        }

        if (lbOnBoundary)
        {
            // Sitting on a boundary: the immediate move is the residual to the edge
            // (0 on the edge); accumulate the delta until it overcomes the tolerance.
            lfMove = lfOnEdge - lfParam;
        }

        if (lfMove == 0.0f)
        {
            mfMoveAccumulator = lfDelta + mfMoveAccumulator;
        }

        const f32 lfHalfAccum = mfMoveAccumulator * KF_ICE_ACCUM_HALF;
        bool lbRelease;
        if (mfMoveAccumulator <= 0.0f)
        {
            lbRelease = (lfHalfAccum < -lfTolerance);
        }
        else
        {
            lbRelease = (lfHalfAccum > lfTolerance);
        }
        if (lbRelease)
        {
            mfMoveAccumulator = 0.0f;
            lfMove = lfHalfAccum;
        }
    }

    // Forward to the current channel's interval boundary. The delta side is the
    // resolved end-key state (start edge == 0, end edge == 1).
    const bool lbEndKey = ResolveEndKey(*this, miState, miPrevState);
    const u16 lu16CurrentInterval = mEditTake.GetCurrentInterval(miCurrentChannel);
    mEditTake.MoveParameter(miCurrentChannel, lu16CurrentInterval, lbEndKey ? 1 : 0, lfMove);
}

// ===========================================================================
// SetParameterChange
//
// Per-frame cursor-move shuttle. Same snap/accumulate hysteresis as MoveParameter,
// but the released move is added to the live playback parameter (rather than to an
// interval boundary) and re-seeked via ICETake::SetParameter, clamped to [0, 1]. The
// seek forces no key (force == false) and wraps only in ASSEMBLY_MARK state (8).
//
// Bracket source: the current channel's CURRENT interval is bracketed first
// (ICEAuthor::GetIntervalBracket over miCurrentChannel). When the current channel is
// the primary (0) or the assembly channel (10) the bracket is used as-is; otherwise
// the primary channel's bracket is intersected in (start = max(curStart, primStart),
// end = min(curEnd, primEnd)) so the cursor cannot leave the primary interval.
// ===========================================================================
void ICEAuthor::SetParameterChange(f32 lfDelta, bool lbApply)
{
    f32 lfMove = lfDelta;

    if (lfDelta != 0.0f)
    {
        if (lbApply)
        {
            const f32 lfParam     = mEditTake.GetParameter();
            const f32 lfTolerance = (mfRangeHi - mfRangeLo) * KF_ICE_RANGE_SNAP_FACTOR;

            // Bracket the current channel's current interval.
            f32 lfBracketStart, lfBracketEnd;
            const u16 lu16Interval = mEditTake.GetCurrentInterval(miCurrentChannel);
            GetIntervalBracket(lu16Interval, &lfBracketStart, &lfBracketEnd);

            f32 lfEdgeLo = lfBracketStart;
            f32 lfEdgeHi = lfBracketEnd;
            if (miCurrentChannel != 0 && miCurrentChannel != 10)
            {
                // Intersect with the primary channel's bracket so the cursor stays
                // inside the primary interval.
                f32 lfPrimStart, lfPrimEnd;
                const u16 lu16PrimInterval = mEditTake.GetCurrentInterval(KI_ICE_PRIMARY_CHANNEL);
                mEditTake.GetIntervalBracket(KI_ICE_PRIMARY_CHANNEL, lu16PrimInterval, &lfPrimStart, &lfPrimEnd);
                if (lfBracketStart < lfPrimStart) { lfEdgeLo = lfPrimStart; }
                if (lfBracketEnd   > lfPrimEnd)   { lfEdgeHi = lfPrimEnd; }
            }

            // On the low edge? Same clamp-and-compare tolerance test as MoveParameter.
            f32 lfLow  = (lfEdgeLo - lfTolerance < lfParam) ? lfParam : (lfEdgeLo - lfTolerance);
            f32 lfHigh = (lfEdgeLo + lfTolerance < lfLow)   ? (lfEdgeLo + lfTolerance) : lfLow;
            if (lfParam == lfHigh)
            {
                lfMove = lfEdgeLo - lfParam;
            }
            else
            {
                // Else the high edge.
                lfLow  = (lfEdgeHi - lfTolerance < lfParam) ? lfParam : (lfEdgeHi - lfTolerance);
                lfHigh = (lfEdgeHi + lfTolerance < lfLow)   ? (lfEdgeHi + lfTolerance) : lfLow;
                if (lfParam == lfHigh)
                {
                    lfMove = lfEdgeHi - lfParam;
                }
            }

            if (lfMove == 0.0f)
            {
                mfMoveAccumulator = mfMoveAccumulator + lfDelta;
            }

            const f32 lfHalfAccum = mfMoveAccumulator * KF_ICE_ACCUM_HALF;
            bool lbRelease;
            if (mfMoveAccumulator > 0.0f)
            {
                lbRelease = (lfHalfAccum > lfTolerance);
            }
            else
            {
                lbRelease = (lfHalfAccum < -lfTolerance);
            }
            if (lbRelease)
            {
                mfMoveAccumulator = 0.0f;
                lfMove = lfHalfAccum;
            }
        }

        // Seek the live parameter to (current + move), clamped into [0, 1]. Wrap
        // only in ASSEMBLY_MARK state.
        f32 lfTarget = mEditTake.GetParameter() + lfMove;
        if (lfTarget < 0.0f) { lfTarget = 0.0f; }
        if (lfTarget > 1.0f) { lfTarget = 1.0f; }
        mEditTake.SetParameter(lfTarget, false, miState == E_ICE_AUTHOR_STATE_ASSEMBLY_MARK);
    }
}

} // namespace ICE
