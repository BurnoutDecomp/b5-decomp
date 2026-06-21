// ============================================================================
// SDKs/Packages/ICE/ICEDataICETake.cpp
//
// ICE::ICETake -- the camera-take EDITOR methods (the class:ICE::ICETake TU),
// separate from the runtime/eval ICETake methods that live in ICEData.cpp. This
// file holds the interval-bracket queries and the sub-take channel mark; the
// undo stack, element insert/delete/copy/paste, key harden/soften and resize
// operations are reconstructed in companion rounds.
// ============================================================================

#include "SDKs/Packages/ICE/ICEData.hpp"           // ICETake, ICEChannel, ICE_INVALID_INTERVAL
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
#include "SDKs/Packages/ICE/ICEMemory.hpp"          // ICE::spICEMemory (GetMemory / mEditHeap.Free)
#include "rw/core/stdc/stdc.h"                       // rw::core::stdc::MemCopy / MemCompare

namespace ICE
{
    // ------------------------------------------------------------------------
    // ICETake::GetIntervalKey -- map an interval index to its left-edge key
    // index: interval 0 starts at key 0; an interior interval n starts at the
    // stored key-index table entry [n-1]; the final interval starts at the
    // second-to-last key (numKeys - 2).
    // ------------------------------------------------------------------------
    u16 ICETake::GetIntervalKey(s32 liChannel, u16 lu16Interval) const
    {
        const ICEChannel& lrChannel = mChannels[liChannel];

        if (lu16Interval == 0)
            return 0;

        if ((u16)(lu16Interval + 1) < (u16)lrChannel.GetNumIntervals())
            return (u16)lrChannel.GetKeyIndex((u16)(lu16Interval - 1));

        return (u16)(lrChannel.GetNumKeys() - 2);
    }

    // ------------------------------------------------------------------------
    // ICETake::GetIntervalBracket -- return the parameter-space [start,end]
    // bracket of an interval. The invalid-interval sentinel brackets the whole
    // take (0..1); otherwise the start is the interval's parameter and the end is
    // the next interval's parameter.
    // ------------------------------------------------------------------------
    void ICETake::GetIntervalBracket(s32 liChannel, u16 lu16Interval,
                                     f32* lpfStart, f32* lpfEnd) const
    {
        const ICEChannel& lrChannel = mChannels[liChannel];

        if (lu16Interval == ICE_INVALID_INTERVAL)
        {
            *lpfStart = 0.0f;
            *lpfEnd   = 1.0f;
        }
        else
        {
            *lpfStart = lrChannel.GetIntervalParameter(lu16Interval);
            *lpfEnd   = lrChannel.GetIntervalParameter((u16)(lu16Interval + 1));
        }
    }

    // ------------------------------------------------------------------------
    // ICETake::MarkChannelFromSubTake -- flag a channel as sourced from the
    // sub-take rather than the primary take, by setting its bit in the sub-take
    // channel mask. SetDataPointers uses this in edit mode for channels with no
    // primary-take data.
    // ------------------------------------------------------------------------
    void ICETake::MarkChannelFromSubTake(s32 liChannel)
    {
        CGS_ASSERT(liChannel < 32, "liChannel < 32");
        mxSubTakeChannels |= (1 << liChannel);
    }

    // ------------------------------------------------------------------------
    // ICETake::FlushUndo -- drop every snapshot. Walk the intrusive undo list from
    // the head; for each node, splice it out of the ring and free it back to the
    // edit heap. The head is the self-linked sentinel (&mUndoList): when its Next
    // points back at itself the list is empty. mUndoList[0] is Next, mUndoList[1]
    // is Prev; each node's node[0]/node[1] are its own Next/Prev.
    // ------------------------------------------------------------------------
    void ICETake::FlushUndo()
    {
        void** lppHead = mUndoList;
        while (lppHead[0] != lppHead)
        {
            void** lpNode = (void**)lppHead[0];
            void*  lpNext = lpNode[0];
            void** lpPrev = (void**)lpNode[1];
            lpPrev[0]           = lpNext;            // node->Prev->Next = node->Next
            ((void**)lpNext)[1] = lpPrev;            // node->Next->Prev = node->Prev
            spICEMemory->mEditHeap.Free(lpNode);
        }
        muUndoUsedBytes = 0;
    }

    // ------------------------------------------------------------------------
    // ICETake::DiscardUndo -- drop only the newest snapshot (the head node) without
    // restoring it: unlink it, subtract its size from the running total, free it.
    // ------------------------------------------------------------------------
    void ICETake::DiscardUndo()
    {
        void** lppHead = mUndoList;
        if (lppHead[0] != lppHead)
        {
            void** lpNode = (void**)lppHead[0];
            void*  lpNext = lpNode[0];
            void** lpPrev = (void**)lpNode[1];
            lpPrev[0]           = lpNext;
            ((void**)lpNext)[1] = lpPrev;
            muUndoUsedBytes -= ((ICETakeData*)lpNode)->ComputeActualSize();
            spICEMemory->mEditHeap.Free(lpNode);
        }
    }

    // ------------------------------------------------------------------------
    // ICETake::PushUndo -- snapshot the live editable take onto the undo list.
    // No-op if there is no take data. Otherwise evict the OLDEST snapshots (the
    // tail = head's Prev) until the new snapshot fits the ICE_MAX_UNDO_SIZE budget,
    // then allocate a snapshot of ComputeActualSize() bytes from the edit heap, copy
    // the whole take into it, and link it at the HEAD. The take's 8-byte node base
    // doubles as the intrusive Next/Prev linkage.
    // ------------------------------------------------------------------------
    void ICETake::PushUndo()
    {
        if (mpTakeData == 0)
            return;

        const u32 luNewSize = mpTakeData->ComputeActualSize();
        void** lppHead = mUndoList;

        // Evict oldest snapshots (tail = head's Prev) until the new one fits.
        while (lppHead[0] != lppHead)
        {
            if (luNewSize + muUndoUsedBytes <= ICE_MAX_UNDO_SIZE)
                break;

            void** lpOldest = (void**)lppHead[1];    // mUndoList[1] = Prev = tail
            void*  lpNext   = lpOldest[0];
            void** lpPrev   = (void**)lpOldest[1];
            lpPrev[0]           = lpNext;
            ((void**)lpNext)[1] = lpPrev;
            muUndoUsedBytes -= ((ICETakeData*)lpOldest)->ComputeActualSize();
            spICEMemory->mEditHeap.Free(lpOldest);
        }

        if (luNewSize + muUndoUsedBytes <= ICE_MAX_UNDO_SIZE)
        {
            void* lpSnapshot = spICEMemory->GetMemory(luNewSize);
            if (lpSnapshot != 0)
            {
                rw::core::stdc::MemCopy(lpSnapshot, mpTakeData, luNewSize);

                // Link at head: newNode takes the old first node as its Next, becomes
                // that node's Prev, and head->Next points at it.
                void** lpNode  = (void**)lpSnapshot;
                void*  lpFirst = lppHead[0];         // old head->Next
                lppHead[0]           = lpNode;       // head->Next = newNode
                ((void**)lpFirst)[1] = lpNode;       // oldFirst->Prev = newNode
                lpNode[0]            = lpFirst;       // newNode->Next = oldFirst
                lpNode[1]            = lppHead;       // newNode->Prev = &head
                muUndoUsedBytes += luNewSize;
            }
        }
    }

    // ------------------------------------------------------------------------
    // ICETake::PopUndo -- restore the newest snapshot onto the live take. No-op
    // (false) if the list is empty. Otherwise the take must be editable: unlink the
    // head snapshot, subtract its size, assign it onto the live take data (copies the
    // payload, not the node base), free the node, rebind the runtime pointers over
    // the restored data and re-seed the playback parameter.
    // ------------------------------------------------------------------------
    bool ICETake::PopUndo()
    {
        void** lppHead = mUndoList;
        if (lppHead[0] == lppHead)
            return false;

        CGS_ASSERT(IsEditable(), "IsEditable()");

        void**       lpNode = (void**)lppHead[0];
        ICETakeData* lpData = mpTakeData;
        void*        lpNext = lpNode[0];
        void**       lpPrev = (void**)lpNode[1];
        lpPrev[0]           = lpNext;
        ((void**)lpNext)[1] = lpPrev;

        muUndoUsedBytes -= ((ICETakeData*)lpNode)->ComputeActualSize();
        *lpData = *(ICETakeData*)lpNode;             // ICETakeData::operator=
        spICEMemory->mEditHeap.Free(lpNode);

        const f32 lfParameter = mfParameter;
        SetDataPointers(lpData, false);
        SetParameter(lfParameter, true, false);
        return true;
    }

    // ------------------------------------------------------------------------
    // ICETake::DataChanged -- true if the live editable take differs from its newest
    // undo snapshot: true when there is no snapshot (head's Next is the self-linked
    // head), when the serialised sizes differ, or when the payloads past the 8-byte
    // node base differ.
    // ------------------------------------------------------------------------
    bool ICETake::DataChanged() const
    {
        const void* const* lppHead = mUndoList;
        if (lppHead[0] == lppHead)
            return true;

        const ICETakeData* lpLive     = mpTakeData;
        const ICETakeData* lpSnapshot = (const ICETakeData*)lppHead[0];

        const u32 luSize = lpLive->ComputeActualSize();
        if (luSize != lpSnapshot->ComputeActualSize())
            return true;

        return rw::core::stdc::MemCompare((const u8*)lpLive + 8,
                                          (const u8*)lpSnapshot + 8,
                                          luSize - 8) != 0;
    }

    // ------------------------------------------------------------------------
    // ICETake::CopyKeyElement -- copy one element value from a source take/key into
    // this take at a destination key. The destination key must lie within the
    // destination channel's key count; the value is read from the source take via
    // GetValue and written here via SetValue. liElement is the element-description
    // index (the GetValue/SetValue first-arg convention); liChannel bounds the key.
    // ------------------------------------------------------------------------
    s32 ICETake::CopyKeyElement(s32 liChannel, s32 liDestKey, s32 liElement,
                                const ICETake* lpSrc, u16 lu16SrcKey)
    {
        CGS_ASSERT((s16)liDestKey <= mChannels[liChannel].GetNumKeys(),
                   "(dest_key) <= channel.GetNumKeys()");

        const ICEValue lValue = lpSrc->GetValue(liElement, lu16SrcKey);
        SetValue(liElement, (u16)liDestKey, lValue);
        return 1;
    }

    // ------------------------------------------------------------------------
    // ICETake::MoveParameter -- move an interval-boundary parameter, keeping a
    // minimum spacing of one 30fps frame from each neighbouring boundary, then
    // re-enforce spacing across the channel. liDelta offsets lu16Interval to select
    // the boundary moved; lfParameter is the requested parameter shift. Returns
    // whether the boundary parameter actually changed.
    //
    // FLAG: the two-sided clamp (lower = neighbour[-1] + spacing, upper =
    //       neighbour[+1] - spacing) is reconstructed by intent -- the underlying
    //       packed float selects don't expose the neighbour reads literally, but the
    //       direction matches the interval-bracket pattern used elsewhere. The gate,
    //       range guard, min-spacing formula, interior-parameter write and the
    //       changed-result are exact.
    // ------------------------------------------------------------------------
    bool ICETake::MoveParameter(s32 liChannel, u16 lu16Interval, s32 liDelta,
                                f32 lfParameter)
    {
        CGS_ASSERT(IsEditable(), "IsEditable()");

        ICEChannel& lrChannel = mChannels[liChannel];

        const s16 li16NumIntervals = lrChannel.GetNumIntervals();
        const s32 liBoundary = (s32)lu16Interval + liDelta;

        if (li16NumIntervals <= 0 || liBoundary < 0 || liBoundary >= (s32)li16NumIntervals)
            return false;

        const u16 lu16Boundary = (u16)liBoundary;

        // Minimum spacing between boundaries: one frame at 30fps over the take
        // length, expressed in unit-interval parameter space.
        const f32 lfMinSpacing = 1.0f / (mpTakeData->GetLength() * 30.0f);

        const f32 lfCurrent = lrChannel.GetIntervalParameter(lu16Boundary);
        const f32 lfTarget  = lfCurrent + lfParameter;

        // Clamp the target between the neighbouring boundaries, leaving at least one
        // frame of spacing on each side.
        const f32 lfLowerBound = lrChannel.GetIntervalParameter((u16)(lu16Boundary - 1)) + lfMinSpacing;
        const f32 lfUpperBound = lrChannel.GetIntervalParameter((u16)(lu16Boundary + 1)) - lfMinSpacing;

        f32 lfNew = lfTarget;
        if (lfNew < lfLowerBound)
            lfNew = lfLowerBound;
        if (lfNew > lfUpperBound)
            lfNew = lfUpperBound;

        // Interior boundaries store their parameter in mpParameters[boundary - 1];
        // boundary 0 is the implicit 0.0 start and has no stored parameter.
        if (lu16Boundary != 0 && lu16Boundary < (u16)lrChannel.GetNumIntervals())
            lrChannel.GetParameterData()[lu16Boundary - 1].SetValue(lfNew);

        lrChannel.EnforceSpacing(lu16Boundary, lfMinSpacing);

        return lfCurrent != lfNew;
    }

    // ------------------------------------------------------------------------
    // ICETake::ChangeSize -- resize one channel's key/interval storage by building
    // a fresh max-sized edit buffer, splicing the channel's key-index and interval-
    // parameter tables across the inserted/removed run, carrying every element's
    // values across, snapshotting the prior take for undo, then copying the rebuilt
    // take back over the live one and rebinding.
    //
    // liKeyDelta/liIntervalDelta grow (>0) or shrink (<0) the channel's key/interval
    // counts; liKeyAt/liIntervalAt position the run; liLeftHardCut/liRightHardCut
    // bias the inserted interval-index run. A resize whose resulting count would
    // leave the editable range [0,100]/[0,99] is REJECTED (returns false) -- the
    // clamp only detects the out-of-range case. Returns true if resized.
    //
    // FLAG: the inserted interval-index value (loop C) is built from liKeyAt with a
    //       per-interval stride of liLeftHardCut plus the constant liRightHardCut;
    //       the operands are certain but which of the two is the stride vs the bias
    //       is reconstructed by intent (constrained, not fully pinned, by the
    //       Insert/Soften/Harden call sites).
    // FLAG: the per-element value-carry head/tail bounds follow the same insertion
    //       structure as the index/parameter rebuild (head before the gap, tail
    //       shifted by the delta, gap default-filled); the interval-element path is
    //       solid, the key-element path mirrors it by intent (the key-branch arg
    //       wiring was not bit-confirmable from the packed value-carry path).
    // ------------------------------------------------------------------------
    bool ICETake::ChangeSize(s32 liChannel, s32 liKeyDelta, s32 liKeyAt,
                             s32 liIntervalDelta, s32 liIntervalAt,
                             s32 liLeftHardCut, s32 liRightHardCut)
    {
        if (liKeyDelta == 0 && liIntervalDelta == 0)
            return true;

        ICEChannel& lrSrcChannel = mChannels[liChannel];

        const s32 liNewKeys      = (s32)lrSrcChannel.GetNumKeys()      + liKeyDelta;
        const s32 liNewIntervals = (s32)lrSrcChannel.GetNumIntervals() + liIntervalDelta;

        // Reject (do not clamp) a resize that would leave the editable range.
        // The editable maxima are one below the buffer capacities (ICE_MAX_EDIT_KEYS
        // /INTERVALS size the edit buffer; the resize ceiling is one less).
        s32 liClampedKeys = 0;
        if (liNewKeys >= 0)
            liClampedKeys = (liNewKeys > (ICE_MAX_EDIT_KEYS - 1)) ? (ICE_MAX_EDIT_KEYS - 1) : liNewKeys;
        if (liNewKeys != liClampedKeys)
            return false;

        s32 liClampedIntervals = 0;
        if (liNewIntervals >= 0)
            liClampedIntervals = (liNewIntervals > (ICE_MAX_EDIT_INTERVALS - 1))
                               ? (ICE_MAX_EDIT_INTERVALS - 1) : liNewIntervals;
        if (liNewIntervals != liClampedIntervals)
            return false;

        // Allocate a fresh max-sized edit buffer, seed it from the current take's
        // 100-byte header, then overwrite this channel's counts (GetElementCounts()
        // is const-only, so write the resized counts via direct member access).
        const u32 luEditSize = mpTakeData->ComputeEditSize();
        ICETakeData* lpNewData = (ICETakeData*)spICEMemory->GetMemory(luEditSize);
        rw::core::stdc::MemCopy(lpNewData, mpTakeData, 100);
        lpNewData->mElementCounts[liChannel].mu16Intervals = (u16)liNewIntervals;
        lpNewData->mElementCounts[liChannel].mu16Keys      = (u16)liNewKeys;

        // A scratch ICETake binds the new buffer so its per-channel index/parameter
        // pointers point into the rebuilt variable-data region. Raw-storage stand-in
        // (the FixDown idiom): ICETake is not default-constructible against the frozen
        // header, and SetDataPointers writes every member it later reads.
        alignas(ICETake) u8 laScratchStorage[sizeof(ICETake)];
        ICETake& lScratch = *reinterpret_cast<ICETake*>(laScratchStorage);
        lScratch.Construct(mpResourceManager);
        lScratch.SetDataPointers(lpNewData, false);

        const u16 lu16IntervalAt   = (u16)liIntervalAt;
        const s32 liInsertPositive = (liIntervalDelta < 0) ? 0 : liIntervalDelta;
        const s32 liTailStart      = (s32)lu16IntervalAt + liInsertPositive;

        // ---- Per-channel index/parameter rebuild. ----
        for (s32 liCh = 0; liCh < 12; ++liCh)
        {
            ICEChannel&       lrSrc = mChannels[liCh];
            const ICEChannel& lrDst = lScratch.mChannels[liCh];

            if (liCh == liChannel)
            {
                // (A) Intervals before the inserted run [0, intervalAt): copied
                // unchanged (index biased by liKeyDelta once at/after liKeyAt).
                for (s32 liInterval = 0; liInterval < (s32)lu16IntervalAt; ++liInterval)
                {
                    const u16 lu16N = (u16)liInterval;

                    s16 li16KeyIndex;
                    if (lu16N == 0)                                   li16KeyIndex = 0;
                    else if (lu16N + 1 < (u16)lrSrc.GetNumIntervals()) li16KeyIndex = (s16)lrSrc.GetKeyData()[lu16N - 1];
                    else                                              li16KeyIndex = (s16)(lrSrc.GetNumKeys() - 2);

                    f32 lfParameter;
                    if (lu16N == 0)                                  lfParameter = 0.0f;
                    else if (lu16N < (u16)lrSrc.GetNumIntervals())   lfParameter = lrSrc.GetParameterData()[lu16N - 1].GetValue();
                    else                                             lfParameter = 1.0f;

                    if (lu16N != 0 && lu16N < (u16)lrDst.GetNumIntervals())
                        lrDst.GetParameterData()[lu16N - 1].SetValue(lfParameter);

                    const s16 li16Bias = ((u16)li16KeyIndex < (u16)liKeyAt) ? 0 : (s16)liKeyDelta;
                    if (lu16N != 0 && lu16N < (u16)(lrDst.GetNumIntervals() - 1))
                        lrDst.GetKeyData()[lu16N - 1] = (u16)(li16KeyIndex + li16Bias);
                }

                // (B) Intervals after the inserted run: dst [tailStart, dstNumIntervals),
                // each reading source interval (dst - liIntervalDelta).
                {
                    s32 liDst = liTailStart;
                    s32 liSrc = liTailStart - liIntervalDelta;
                    for (; liDst < (s32)lrDst.GetNumIntervals(); ++liDst, ++liSrc)
                    {
                        const u16 lu16Src = (u16)liSrc;

                        s16 li16KeyIndex;
                        if (lu16Src == 0)                                   li16KeyIndex = 0;
                        else if (lu16Src + 1 < (u16)lrSrc.GetNumIntervals()) li16KeyIndex = (s16)lrSrc.GetKeyData()[lu16Src - 1];
                        else                                                li16KeyIndex = (s16)(lrSrc.GetNumKeys() - 2);

                        f32 lfParameter;
                        if (lu16Src == 0)                                lfParameter = 0.0f;
                        else if (lu16Src < (u16)lrSrc.GetNumIntervals()) lfParameter = lrSrc.GetParameterData()[lu16Src - 1].GetValue();
                        else                                             lfParameter = 1.0f;

                        const u16 lu16Dst = (u16)liDst;
                        if (lu16Dst != 0 && lu16Dst < (u16)lrDst.GetNumIntervals())
                            lrDst.GetParameterData()[lu16Dst - 1].SetValue(lfParameter);

                        const s16 li16Bias = ((u16)li16KeyIndex < (u16)liKeyAt) ? 0 : (s16)liKeyDelta;
                        if (lu16Dst != 0 && lu16Dst < (u16)(lrDst.GetNumIntervals() - 1))
                            lrDst.GetKeyData()[lu16Dst - 1] = (u16)(li16KeyIndex + li16Bias);
                    }
                }

                // (C) Inserted run [0, liIntervalDelta): parameter = current playback
                // parameter; interval index = liKeyAt + liLeftHardCut*run + liRightHardCut.
                for (s32 liRun = 0; liRun < liIntervalDelta; ++liRun)
                {
                    const u16 lu16Dst = (u16)(lu16IntervalAt + liRun);

                    if (lu16Dst != 0 && lu16Dst < (u16)lrDst.GetNumIntervals())
                        lrDst.GetParameterData()[lu16Dst - 1].SetValue(mfParameter);

                    if (lu16Dst != 0 && lu16Dst < (u16)(lrDst.GetNumIntervals() - 1))
                        lrDst.GetKeyData()[lu16Dst - 1] =
                            (u16)((u16)liKeyAt + liLeftHardCut * liRun + liRightHardCut);
                }

                // (D) Shrink fix-up: when removing intervals, re-emit the boundary
                // parameter at intervalAt if the source key index there is below liKeyAt.
                if ((s32)lu16IntervalAt < (s32)lrDst.GetNumIntervals() && liIntervalDelta < 0)
                {
                    s16 li16KeyIndex;
                    if (lu16IntervalAt == 0)                                   li16KeyIndex = 0;
                    else if (lu16IntervalAt + 1 < (u16)lrSrc.GetNumIntervals()) li16KeyIndex = (s16)lrSrc.GetKeyData()[lu16IntervalAt - 1];
                    else                                                       li16KeyIndex = (s16)(lrSrc.GetNumKeys() - 2);

                    if ((u16)liKeyAt > (u16)li16KeyIndex)
                    {
                        f32 lfParameter;
                        if (lu16IntervalAt == 0)                                lfParameter = 0.0f;
                        else if (lu16IntervalAt < (u16)lrSrc.GetNumIntervals()) lfParameter = lrSrc.GetParameterData()[lu16IntervalAt - 1].GetValue();
                        else                                                    lfParameter = 1.0f;

                        if (lu16IntervalAt != 0 && lu16IntervalAt < (u16)lrDst.GetNumIntervals())
                            lrDst.GetParameterData()[lu16IntervalAt - 1].SetValue(lfParameter);
                    }
                }
            }
            else
            {
                // Non-resized channel: re-emit its interval tables verbatim (high to
                // low so an in-place write never clobbers a not-yet-read entry).
                for (s32 liInterval = lrSrc.GetNumIntervals() - 1; liInterval >= 0; --liInterval)
                {
                    const u16 lu16N = (u16)liInterval;

                    s16 li16KeyIndex;
                    if (lu16N == 0)                                   li16KeyIndex = 0;
                    else if (lu16N + 1 < (u16)lrSrc.GetNumIntervals()) li16KeyIndex = (s16)lrSrc.GetKeyData()[lu16N - 1];
                    else                                              li16KeyIndex = (s16)(lrSrc.GetNumKeys() - 2);

                    if (lu16N != 0 && lu16N < (u16)(lrDst.GetNumIntervals() - 1))
                        lrDst.GetKeyData()[lu16N - 1] = (u16)li16KeyIndex;

                    f32 lfParameter;
                    if (lu16N == 0)                                  lfParameter = 0.0f;
                    else if (lu16N < (u16)lrSrc.GetNumIntervals())   lfParameter = lrSrc.GetParameterData()[lu16N - 1].GetValue();
                    else                                             lfParameter = 1.0f;

                    if (lu16N != 0 && lu16N < (u16)lrDst.GetNumIntervals())
                        lrDst.GetParameterData()[lu16N - 1].SetValue(lfParameter);
                }
            }
        }

        // ---- Per-element value carry. The resized channel's element values are
        //      spliced across the inserted/removed gap (key elements use the key
        //      insertion point, interval elements the interval one); other elements'
        //      packed value runs are bulk-copied. ----
        for (s32 liElement = 0; liElement < eICE_NUM_ELEMENTS; ++liElement)
        {
            const ICEElementDescription& lrDesc = ICEElementDescriptions[liElement];
            const s32 liElemChannel = lrDesc.miChannelNumber;

            const u16 lu16NewCount = (liElement >= 28)
                                   ? lpNewData->mElementCounts[liElemChannel].mu16Intervals
                                   : lpNewData->mElementCounts[liElemChannel].mu16Keys;

            if (liElemChannel == liChannel)
            {
                const bool lbInterval = (liElement >= 28);
                const s32  liGapStart = lbInterval ? (s32)lu16IntervalAt : liKeyAt;
                const s32  liGapLen   = lbInterval ? liIntervalDelta : liKeyDelta;
                const s32  liTailDst  = liGapStart + ((liGapLen < 0) ? 0 : liGapLen);

                // (1) Head run unchanged: [0, gapStart).
                for (s32 liK = 0; liK < liGapStart; ++liK)
                    lScratch.SetValue(liElement, (u16)liK, GetValue(liElement, (u16)liK));

                // (2) Tail run: dst d <- src (d - gapLen), for [tailDst, newCount).
                for (s32 liDst = liTailDst; liDst < (s32)lu16NewCount; ++liDst)
                    lScratch.SetValue(liElement, (u16)liDst, GetValue(liElement, (u16)(liDst - liGapLen)));

                // (3) Gap fill (only when inserting): each inserted slot in
                // [gapStart, gapStart+gapLen) takes the take's current decoded value
                // for this element (mValues[liElement]).
                const ICEValue lFill = mValues[liElement];
                for (s32 liG = liGapStart; liG < liGapStart + liGapLen; ++liG)
                    lScratch.SetValue(liElement, (u16)liG, lFill);
            }
            else
            {
                // Other element: bulk-copy the whole packed value run (bit-packed
                // values rounded up to a whole 4-byte word).
                const u32 luBytes = (u32)(((lrDesc.miDataBits * lu16NewCount + 31) >> 3) & ~3);
                rw::core::stdc::MemCopy(lScratch.mpElementData[liElement],
                                        mpElementData[liElement], luBytes);
            }
        }

        // ---- Commit: snapshot the prior take, copy the rebuilt buffer back over the
        //      live take, rebind the runtime pointers and re-seed the parameter. ----
        PushUndo();
        const u32 luActual = lpNewData->ComputeActualSize();
        rw::core::stdc::MemCopy(mpTakeData, lpNewData, luActual);

        const f32 lfParameter = mfParameter;
        SetDataPointers(mpTakeData, false);
        SetParameter(lfParameter, true, false);
        return true;
    }

    // ------------------------------------------------------------------------
    // ICETake::SetSize -- set channel liChannel to exactly liNumKeys keys /
    // liNumIntervals intervals by converting each target count into a delta from the
    // channel's current count and delegating to ChangeSize (run at 0, no hard-cut).
    // ------------------------------------------------------------------------
    bool ICETake::SetSize(s32 liChannel, s32 liNumKeys, s32 liNumIntervals)
    {
        return ChangeSize(liChannel,
                          liNumKeys - (s32)mChannels[liChannel].GetNumKeys(), 0,
                          liNumIntervals - (s32)mChannels[liChannel].GetNumIntervals(), 0,
                          0, 0);
    }
}
