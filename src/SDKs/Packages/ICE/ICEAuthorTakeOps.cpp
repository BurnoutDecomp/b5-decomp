// ============================================================================
// SDKs/Packages/ICE/ICEAuthorTakeOps.cpp
//
// ICE::ICEAuthor -- the TAKE-OPS group of the in-game ICE camera-take editor.
// Reconstructed against the frozen layout in ICEAuthor.hpp. THIS TU bodies one
// fan-out group of ICEAuthor (the declarations are in ICEAuthor.hpp; the
// foundational subset -- Construct / state getters / interval delegators -- is
// bodied in ICEAuthor.cpp, and the key-element group in ICEAuthorKeyElementOps.cpp).
// The per-TU `cl /c` gate does not link, so the un-bodied declarations from the
// other groups are fine.
//
// Group bodies:
//   CreateNewTake, ClearEditTake, LoadCurrentTake, FindEditedTakeFromGuid.
//
// ⚠️ CommitCurrentTake and SaveTake used to be here too; they were moved VERBATIM to
// ICEAuthorSaveTake.cpp on 2026-08-01 (see that file's banner) because SaveTake is the
// only body in the group that touches the editor TEXT SINK, and the sink costs
// ICEFileHandler::FileClose (+5 unresolved: the sole EA::GameTalk user in the ICE
// package) plus an unbodied rw::core::stdc::Snprintf. With them out, THIS TU opens ZERO
// externals and can be mounted for FindEditedTakeFromGuid, which the ICE camera family
// needs. Merge them back when GameTalk + Snprintf land.
//
// The editor owns a live edit take (mEditTake @0x28) whose ICETakeData is held in
// the ICE edit heap (spICEMemory->mEditHeap), the persistent edit-source take data
// pointer (mpEditTakeData @0x10), and an intrusive list of newly created / edited
// takes (mEditedTakeList @0xF48: [0]=Head/Next sentinel, [1]=Tail/Prev). Each take
// record is an ICETakeData deriving from bTNode<ICETakeData> (an 8-byte bNode base:
// Next @+0, Prev @+4), so the list splices below link through that node base. The
// frozen header models the list head as a 2-pointer buffer (void* mEditedTakeList[2]);
// the splices reinterpret that head as the bNode sentinel the layout documents.
// ============================================================================

#include "SDKs/Packages/ICE/ICEAuthor.hpp"
#include "SDKs/Packages/ICE/ICEData.hpp"     // ICE::ICETake / ICETakeData edit ops
#include "SDKs/Packages/ICE/ICEMemory.hpp"   // ICE::spICEMemory edit heap, bNode/bTList iterator
#include "SDKs/Packages/ICE/ICEFile.hpp"     // ICE::ICEFileHandler (SaveTake text sink)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/core/stdc/stdc.h"               // rw::core::stdc::StringnCopy / Snprintf

namespace ICE
{

// ---------------------------------------------------------------------------
// ClearEditTake
//
// Drop the live edit take's editable data and rebuild a fresh, empty edit buffer.
// If the edit take currently holds an allocated take (mEditTake.GetData() non-null
// and that take is allocated), the take record is freed back to the ICE edit heap,
// the edit-buffer data binding is cleared, and the take's undo history is reset.
// Either way a new (empty) edit buffer is installed. Returns through NewEditBuffer.
//
// The edit-take data lives in spICEMemory's embedded edit heap (mEditHeap), the same
// heap GetMemory allocates from and FreeEditBuffer frees to.
//
// FLAG (FlushUndo private): the take-take undo reset is ICETake::FlushUndo, which is
// PRIVATE on ICETake (ICEData.hpp). The public ICETake::NewEditBuffer() encapsulates
// the same buffer-allocate + undo-flush, so the undo reset is routed through
// NewEditBuffer() rather than poking the private member. The data-pointer clear that
// preceded the private FlushUndo is subsumed by NewEditBuffer rebuilding the buffer.
// ---------------------------------------------------------------------------
void ICEAuthor::ClearEditTake()
{
    ICETakeData* lpData = mEditTake.GetData();
    if (lpData != 0 && lpData->IsAllocated())
    {
        // Free the held take record back to the ICE edit heap (the +0x520 mEditHeap
        // the take buffers are allocated from).
        spICEMemory->mEditHeap.Free(lpData);
    }

    // Rebuild a fresh (empty) edit buffer; this also clears the take's undo history
    // (NewEditBuffer subsumes the private FlushUndo the inlined sequence ended with).
    mEditTake.NewEditBuffer();
}

// ---------------------------------------------------------------------------
// LoadCurrentTake
//
// Load the persistent edit-source take (mpEditTakeData @0x10) into the live edit
// take. If there is a source take, copy its data into the edit take's record,
// re-point the edit take at that record, and force the playback parameter to the
// start. If there is no source take, clear the edit take to an empty buffer. Either
// way the sub-take data pointer (mpSubTakeData @0x18) is cleared.
//
// FLAG (copy direction): ICETakeData::operator= copies mpEditTakeData INTO the edit
// take's data record (source take -> edit-take data). This is the opposite direction
// of CommitCurrentTake (which copies the edited data back into the source take).
//
// FLAG (SetParameter reconstruction): after rebinding, the edit take's playback
// parameter is forced to the start. The post-rebind reseed renders with a force flag
// and no wrap; modelled as ICETake::SetParameter(0.0f, true, false) -- the same
// force-seek shape SetState uses. The exact parameter value is a reconstruction
// choice (0.0 == start of take); force=true, wrap=false match the call shape.
// ---------------------------------------------------------------------------
void ICEAuthor::LoadCurrentTake()
{
    ICETakeData* lpSource = mpEditTakeData;
    if (lpSource != 0)
    {
        // Copy the persistent source take into the edit take's data record, then bind
        // the edit take to that record (non-edit bind: the third arg is 0/false).
        *mEditTake.GetData() = *lpSource;
        mEditTake.SetDataPointers(mEditTake.GetData(), false);
        mEditTake.SetParameter(0.0f, true, false);
    }
    else
    {
        ClearEditTake();
    }

    // Mark the editor as NOT in assembly mode: clear the @0x20 assembly-mode flag
    // (the mirror of LoadCurrentAssembly's set-to-1). SetAssemblySourceTake gates on
    // this flag. (This write targets @0x20, the flag the header names miSubTakeGuid;
    // it does NOT touch mpSubTakeData @0x18.)
    miSubTakeGuid = 0;
}

// ---------------------------------------------------------------------------
// CreateNewTake
//
// Allocate a new, named take in the live edit take's edit buffer, link it into the
// edited-takes list, and return its record. The live edit take is first reset to a
// fresh edit buffer (freeing any held take record back to the ICE edit heap, as in
// ClearEditTake), then the freshly built take record is named (lpcName, capped at the
// 32-byte take-name field) and stamped with liGuid, linked at the tail of the
// edited-takes list, and a second fresh edit buffer is installed so the editor keeps
// a working buffer distinct from the just-created record.
//
// FLAG (FlushUndo private): same as ClearEditTake -- the inlined free + data-clear +
// undo reset is routed through NewEditBuffer() (FlushUndo is private on ICETake).
// ---------------------------------------------------------------------------
ICETakeData* ICEAuthor::CreateNewTake(const char* lpcName, s32 liGuid)
{
    // Reset the live edit take to a fresh edit buffer (free any held record first).
    ICETakeData* lpHeld = mEditTake.GetData();
    if (lpHeld != 0 && lpHeld->IsAllocated())
    {
        spICEMemory->mEditHeap.Free(lpHeld);
    }
    mEditTake.NewEditBuffer();

    // The fresh edit buffer's take record is the new take; name + guid it.
    ICETakeData* lpNew = mEditTake.GetData();
    rw::core::stdc::StringnCopy(lpNew->macTakeName, lpcName, KI_MAX_TAKENAME_LENGTH);
    lpNew->miGuid = liGuid;

    // Link the new take at the tail of the edited-takes list (bNode sentinel:
    // [0]=Head/Next, [1]=Tail/Prev -- same AddTail splice as CommitCurrentTake).
    bNode* lpSentinel = reinterpret_cast<bNode*>(&mEditedTakeList[0]);
    bNode* lpNode     = reinterpret_cast<bNode*>(lpNew);
    bNode* lpOldTail  = reinterpret_cast<bNode*>(mEditedTakeList[1]);

    lpOldTail->Next    = lpNode;
    mEditedTakeList[1] = lpNode;
    lpNode->Next       = lpSentinel;
    lpNode->Prev       = lpOldTail;

    // Install a second fresh edit buffer so the editor's working buffer is distinct
    // from the just-created take record.
    mEditTake.NewEditBuffer();

    return lpNew;
}

// ---------------------------------------------------------------------------
// FindEditedTakeFromGuid
//
// Walk the edited-takes list (mEditedTakeList @0xF48) looking for the take record
// whose guid (miGuid @+0x8 of ICETakeData) matches liGuid. Returns the matching
// record, or null if no edited take carries that guid.
//
// The walk uses the bTList<ICETakeData>::iterator cursor (the post-increment that the
// ICEMemoryList TU instantiates): { _Ptr = current node, _Lst = owning list }. The
// head is the past-the-end sentinel (mEditedTakeList reinterpreted as the list head);
// iteration stops when the cursor reaches it.
// ---------------------------------------------------------------------------
ICETakeData* ICEAuthor::FindEditedTakeFromGuid(s32 liGuid)
{
    // The list head reinterpreted as the bTList the iterator walks; its EndOfList()
    // sentinel is the head node itself (Head/Next @ mEditedTakeList[0]).
    bTList<ICETakeData>* lpList = reinterpret_cast<bTList<ICETakeData>*>(&mEditedTakeList[0]);
    bNode* lpEnd = reinterpret_cast<bNode*>(&mEditedTakeList[0]);

    // Seed the cursor at the first node (head's Next).
    bTList<ICETakeData>::iterator lIterator;
    lIterator._Ptr = reinterpret_cast<bNode*>(mEditedTakeList[0]);
    lIterator._Lst = lpList;

    // ⛔ THE NULL-CURSOR TERMINATOR IS LOAD-BEARING (added 2026-08-01, junkyard-fire wave).
    // A bTList that has been CONSTRUCTED and is empty self-links its head (head->Next == head),
    // so `_Ptr != lpEnd` alone terminates it. A list that has never been constructed has a ZERO
    // head->Next, and `0 != lpEnd` walked straight into `lpNode->miGuid` -- an access violation
    // in FindEditedTakeFromGuid, reached from DirectorResourceManager::GetKeyAnimFromGuid ->
    // KeyAnimController::Prepare the first time an ICE-anim camera behaviour was ever prepared.
    // That is exactly the state the PC build is in: nothing constructs the in-game ICE editor's
    // author store, so its edited-take list head is null.
    // Returning 0 here is not a papering-over -- it is the function's OWN documented miss result,
    // and GetKeyAnimFromGuid's next line is the on-disk fallback
    // (`ICEList::GetICETakeDataFromGuid`, the 549-take resource list that IS loaded). The
    // console's own precondition assert below already declares `_Ptr != 0`; this stops the code
    // running past it into the fault.
    while (lIterator._Ptr != 0 && lIterator._Ptr != lpEnd)
    {
        // Guard the live cursor (the iterator's own post-increment also asserts this).
        CGS_ASSERT(lIterator._Ptr != 0 && lIterator._Lst != 0 && lIterator._Ptr != lpEnd,
            "_Ptr && _Lst && _Ptr!=_Lst->EndOfList()");

        ICETakeData* lpNode = reinterpret_cast<ICETakeData*>(lIterator._Ptr);
        if (lpNode->miGuid == liGuid)
        {
            return lpNode;
        }

        // Advance (post-increment returns the pre-advance copy; the side effect is the
        // cursor stepping to _Ptr->Next).
        lIterator++;
    }

    return 0;
}

} // namespace ICE
