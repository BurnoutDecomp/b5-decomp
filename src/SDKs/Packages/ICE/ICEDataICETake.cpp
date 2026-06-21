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
}
