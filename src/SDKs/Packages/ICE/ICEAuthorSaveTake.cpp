// ============================================================================
// SDKs/Packages/ICE/ICEAuthorSaveTake.cpp
//
// [marked deviation -- FILE SPLIT, not a code change] ICEAuthor::SaveTake and its one
// caller ICEAuthor::CommitCurrentTake, moved VERBATIM out of ICEAuthorTakeOps.cpp on
// 2026-08-01 so that TU could be mounted for ICEAuthor::FindEditedTakeFromGuid
// @0x82531878 -- the editor-take lookup the ICE camera family (KeyAnimController /
// BehaviourIceAnim / DirectorResourceManager::GetKeyAnimFromGuid) needs.
//
// ⭐ WHY THIS BOUNDARY. SaveTake is the ONLY thing in the take-ops group that touches
// the editor's TEXT SINK, and the sink is what is expensive:
//     ICE::ICEFileHandler::FileClose        real body in ICEFileClose.cpp -- which is
//                                           the SOLE EA::GameTalk user in the whole ICE
//                                           package and MEASURED at +5 unresolved
//                                           (GameTalkMessage ctor/dtor/AddKeyContent,
//                                           GameTalkManager::GetInstance/SendMessage)
//     rw::core::stdc::Snprintf              declared in the vendor stdc header, bodied
//                                           nowhere (X360 @0x82BC97E8 is a plain
//                                           va_start + Vsnprintf forwarder)
// CommitCurrentTake comes along only because it CALLS SaveTake -- leaving it behind
// would swap one unresolved external for another. Nothing else in the group referenced
// either symbol, so the remaining four bodies (ClearEditTake, LoadCurrentTake,
// CreateNewTake, FindEditedTakeFromGuid) now open ZERO externals.
//
// This is the same shape, for the same reason, as the split that already exists one
// directory over: ICEFileClose.cpp was carved out of ICEFile.cpp precisely so ICEFile.cpp
// could mount without dragging GameTalk in. Nothing is invented, nothing is stubbed --
// both bodies below keep their real, faithful implementations.
//
// ⚠️ NOT MOUNTED. Mount this WITH ICEFileClose.cpp + the EA::GameTalk message API + a
// rw::core::stdc::Snprintf body, and merge it straight back into ICEAuthorTakeOps.cpp at
// the same time -- the split has no reason to outlive the blocker.
// DELETE-WHEN: EA::GameTalk lands (or the debug XML dump path is retired) AND
// rw::core::stdc::Snprintf is bodied.
// ============================================================================

#include "SDKs/Packages/ICE/ICEAuthor.hpp"
#include "SDKs/Packages/ICE/ICEData.hpp"     // ICE::ICETake / ICETakeData edit ops
#include "SDKs/Packages/ICE/ICEMemory.hpp"   // ICE::spICEMemory edit heap, bNode/bTList
#include "SDKs/Packages/ICE/ICEFile.hpp"     // ICE::ICEFileHandler (SaveTake text sink)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/core/stdc/stdc.h"               // rw::core::stdc::Snprintf

namespace ICE
{

// ---------------------------------------------------------------------------
// CommitCurrentTake
//
// Commit the live edit take's edited data back to the persistent edit-source take.
// Asserts a source take exists, dumps the edited take through the text sink
// (SaveTake), then either copies the edited data over the source take in place OR --
// when the source take cannot hold the edited data (the source is not yet allocated,
// or the edited take is larger than the source) -- links the edited take record into
// the edited-takes list and installs a fresh edit buffer.
//
// FLAG (copy direction): the in-place path is mpEditTakeData->operator=(edited data),
// i.e. edited edit-take data -> persistent source take (the reverse of LoadCurrentTake).
//
// FLAG (SaveTake signature): the take dump here forwards the EDITED take's data to
// SaveTake. The frozen header declares SaveTake(ICETakeData*), the 1-arg form bodied
// below; this call matches that signature. (CommitCurrentAssembly uses a wider
// SaveTake call shape -- see the SaveTake FLAG.)
// ---------------------------------------------------------------------------
void ICEAuthor::CommitCurrentTake()
{
    ICETakeData* lpSource = mpEditTakeData;
    CGS_ASSERT(lpSource != 0, "lpTakeData!=NULL");

    ICETakeData* lpEdited = mEditTake.GetData();

    // Dump the edited take through the text sink.
    SaveTake(lpEdited);

    // In-place commit when the source take can already hold the edited data (it is
    // allocated, or its actual size is at least the edited take's actual size).
    if (lpSource->IsAllocated()
        || lpSource->ComputeActualSize() >= lpEdited->ComputeActualSize())
    {
        // Copy the edited data back over the persistent source take.
        *lpSource = *lpEdited;
    }
    else
    {
        // The source take cannot hold the edited data: hand the edited take record to
        // the edited-takes list (link at the tail) and install a fresh edit buffer.
        //
        // The list head (mEditedTakeList) is the bNode sentinel: [0]=Head/Next,
        // [1]=Tail/Prev. AddTail: old-tail->Next = new node; tail = new node; new
        // node->Next = head sentinel; new node->Prev = old tail.
        bNode* lpSentinel = reinterpret_cast<bNode*>(&mEditedTakeList[0]);
        bNode* lpNode     = reinterpret_cast<bNode*>(lpEdited);
        bNode* lpOldTail  = reinterpret_cast<bNode*>(mEditedTakeList[1]);

        lpOldTail->Next    = lpNode;
        mEditedTakeList[1] = lpNode;
        lpNode->Next       = lpSentinel;
        lpNode->Prev       = lpOldTail;

        mEditTake.NewEditBuffer();
    }
}

// ---------------------------------------------------------------------------
// SaveTake
//
// Dump one take to the editor text sink (mpFileHandler @0x04): format the take guid
// as a decimal string, emit the opening <TAKE name="..." guid="..."> tag through the
// file handler, write the take body (ICETakeData::SaveData), and flush/close the
// handler. No-op when the take's guid is the unset sentinel (-1).
//
// FLAG (SaveTake signature / two call shapes): the frozen header declares the 1-arg
// SaveTake(ICETakeData*) bodied here, matching CommitCurrentTake's call. There is
// also a 6-arg call shape (CommitCurrentAssembly forwards extra trailing args; the
// take-dump body shows trailing register-width args that the 1-arg call site does not
// pass -- typical of a varargs-ish / spilled frame, the Snprintf vararg tail). The
// header is NOT changed; the 1-arg form is bodied. If a fan-out body genuinely needs
// the wide form,
// FLAG and grow the header there.
// ---------------------------------------------------------------------------
void ICEAuthor::SaveTake(ICETakeData* lpTakeData)
{
    const s32 liGuid = lpTakeData->GetGuid();
    if (liGuid == -1)
    {
        return;
    }

    // Format the guid as a decimal string (capped at the 32-byte take-name budget; the
    // scratch buffer is 40 bytes to leave headroom for the formatter).
    char lacGuid[40];
    rw::core::stdc::Snprintf(lacGuid, 32, "%d", liGuid);

    // Emit the opening tag, the take body, then flush/close the text sink.
    mpFileHandler->FilePrintf("<TAKE name=\"%s\" guid=\"%s\">", lpTakeData->GetName(), lacGuid);
    lpTakeData->SaveData(mpFileHandler, 0);
    mpFileHandler->FileClose();
}

} // namespace ICE
