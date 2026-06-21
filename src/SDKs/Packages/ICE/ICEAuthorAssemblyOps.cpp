// ============================================================================
// SDKs/Packages/ICE/ICEAuthorAssemblyOps.cpp
//
// ICE::ICEAuthor -- the assembly-ops group of the in-game ICE camera-take editor.
// Reconstructed against the frozen layout in ICEAuthor.hpp. THIS TU bodies one
// fan-out group of ICEAuthor (the declarations live in ICEAuthor.hpp; the
// foundational subset -- Construct / state getters / interval delegators /
// IsEndKeyState / AdjustZoomRange / GetSubAssemRatio -- is bodied in ICEAuthor.cpp,
// and the key-element + state-machine group is bodied in ICEAuthorKeyElementOps.cpp).
// The per-TU `cl /c` gate does not link, so the un-bodied declarations from the
// other groups are fine.
//
// Group bodies:
//   EditAssembly, CommitCurrentAssembly, LoadCurrentAssembly,
//   ChangeAssemTakeLength, SetAssemblySourceTake.
//   (GetSubAssemRatio is bodied in ICEAuthor.cpp and is NOT re-bodied here.)
//
// The assembly path edits the live take (mEditTake) as an ASSEMBLY take: channel 10
// is the assembly channel (the soft-key run of sub-take references), and the
// assembly source take (mpAssemblyTakeData @0x14) is the take-data the editor edits
// into. CommitCurrentAssembly copies the edited take back over the source (or
// appends a fresh edited-take to the edited-takes list when it grew),
// LoadCurrentAssembly rebinds the live take to the assembly source + sub-take,
// ChangeAssemTakeLength rescales every assembly-channel interval boundary to a new
// take length, and SetAssemblySourceTake swaps the sub-take referenced by the
// current assembly interval.
//
// FLAG (offset 0x1C vs 0x20 -- header member mismatch, NOT edited here):
//   The frozen header declares mfInsertTangent @0x1C and miSubTakeGuid @0x20. The
//   assembly bodies use these two slots the OTHER way round:
//     * @0x1C (word index 7) is where SetAssemblySourceTake stores the looked-up
//       sub-take guid before the resource-manager lookup (`v2[7] = guid`). i.e.
//       @0x1C is the sub-take guid scratch, not an insert tangent.
//     * @0x20 (word index 8) is an ASSEMBLY-MODE flag: LoadCurrentAssembly writes 1
//       and LoadCurrentTake writes 0 there, and SetAssemblySourceTake gates on it
//       being non-zero (`if (*(this+0x20))`). i.e. @0x20 is a bool mode flag, not
//       the sub-take guid.
//   Because the header is FROZEN (no .hpp edits allowed here), these bodies access
//   the slots through their EXISTING header names (mfInsertTangent for the @0x1C
//   guid scratch via a reinterpret, miSubTakeGuid for the @0x20 mode flag). The
//   header members should be RENAMED/RETYPED to match (a @0x1C s32 sub-take-guid
//   scratch + a @0x20 s32/bool assembly-mode flag) when the header is next grown;
//   do NOT keep mfInsertTangent/miSubTakeGuid as-is. Flagged for the header steward.
//
// FLAG (mpResourceManager take-lookup functor in SetAssemblySourceTake):
//   The body stashes the guid into the @0x1C scratch, then invokes mpResourceManager
//   (@0x0C) as a NULLARY virtual functor passing only itself: `v4 = *(this+0x0C);
//   result = (**v4)(v4)`. The guid is read back from the stashed scratch by the
//   functor; the result is an ICETakeData* stored to mpSubTakeData (@0x18). The
//   frozen IResourceManager (ICEData.hpp) only declares GetTakeData(CgsResource::ID)
//   / GetTakeData(s32 liIndex) -- it has NO nullary stash-then-call functor. This
//   body reconstructs the semantically-equivalent take-lookup-by-guid as
//   mpResourceManager->GetTakeData(liGuid). FLAG: the raw shape is a stateful
//   functor (stash guid member -> vtable slot 0 with only `this`), not a parametered
//   GetTakeData; if IResourceManager is later grown with that functor entry, rebind.
// ============================================================================

#include "SDKs/Packages/ICE/ICEAuthor.hpp"
#include "SDKs/Packages/ICE/ICEData.hpp"            // ICE::ICETake assembly ops / ICETakeData operator= / ComputeActualSize
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
#include "SDKs/Packages/ICE/ICEMemory.hpp"          // ICE::ICEPointers (pulled by the layout)
#include "rw/core/stdc/stdc.h"                       // rw core stdc (string/mem helpers used across the ICE TUs)

namespace ICE
{

// The assembly channel index (channel 10 carries the assembly soft-key run of
// sub-take references). ChangeAssemTakeLength / LoadCurrentAssembly / SetState's
// assembly entries all operate on this channel.
static const s32 KI_ICE_ASSEMBLY_CHANNEL = 10;

// Key-storage fixed-point scale: the assembly-channel key values are stored as u16
// quantised parameters and converted to the [0,1] normalised parameter by this
// reciprocal (1.0 / 65535.0). ChangeAssemTakeLength reads the raw key table and
// rescales each boundary to the new take length.
static const f32 KF_ICE_KEY_TO_PARAMETER = 0.000015259022f;

// Frames-per-second basis the assembly take length is expressed in: a delta in
// seconds is converted to frame units (x30) before being added to the current take
// length, and the resulting length is clamped to at least one frame.
static const f32 KF_ICE_FRAMES_PER_SECOND = 30.0f;

// ---------------------------------------------------------------------------
// EditAssembly
//
// Enter the assembly editor on lpAssemblyTakeData. No-op when the editor is already
// live (miState > 0) AND already editing this exact assembly source (the stored
// mpAssemblyTakeData @0x14 already equals the arg). Otherwise it stores the new
// assembly source, cycles the state machine OFF (0) -> BROWSER (1), loads the
// assembly into the live take, and settles into ASSEMBLY_MARK (8).
//
// FLAG (arg type): the ledger renders the arg as a register-width word; the body's
// `*(this+0x14) != a2` comparison against the assembly-take-data pointer fixes it as
// ICETakeData* (matching the frozen EditAssembly(ICETakeData*) declaration).
// ---------------------------------------------------------------------------
void ICEAuthor::EditAssembly(ICETakeData* lpAssemblyTakeData)
{
    // Re-entry guard: already-on AND already on this source -> nothing to do.
    if (miState > 0 && mpAssemblyTakeData == lpAssemblyTakeData)
    {
        return;
    }

    mpAssemblyTakeData = lpAssemblyTakeData;
    SetState(E_ICE_AUTHOR_STATE_OFF);          // 0
    SetState(E_ICE_AUTHOR_STATE_BROWSER);      // 1
    LoadCurrentAssembly();
    SetState(E_ICE_AUTHOR_STATE_ASSEMBLY_MARK);// 8
}

// ---------------------------------------------------------------------------
// CommitCurrentAssembly
//
// Commit the live edited assembly take (mEditTake's take-data) back onto the
// assembly source take (mpAssemblyTakeData @0x14). Asserts the assembly source is
// non-null, then saves the live take into it (SaveTake). If the source is already
// allocated, or it is at least as large as the edited take, the edited take is
// copied back over the source in place (ICETakeData::operator=). Otherwise the
// edited take has grown beyond the source: the edited take-data is APPENDED to the
// edited-takes list (the intrusive bTList<ICETakeData> whose head/tail are
// mEditedTakeList[0]/[1]) and a fresh edit buffer is started.
//
// Mirrors CommitCurrentTake exactly (same shape), differing only in committing the
// ASSEMBLY source (mpAssemblyTakeData @0x14) instead of the edit source
// (mpEditTakeData @0x10).
// ---------------------------------------------------------------------------
void ICEAuthor::CommitCurrentAssembly()
{
    ICETakeData* lpSource = mpAssemblyTakeData;          // @0x14
    CGS_ASSERT(lpSource != NULL, "lpTakeData!=NULL");

    ICETakeData* lpEdited = mEditTake.GetData();         // a1[11] == mEditTake.mpTakeData @0x2C

    SaveTake(lpEdited);

    if (lpSource->IsAllocated()
        || lpSource->ComputeActualSize() >= lpEdited->ComputeActualSize())
    {
        // Fits in place: copy the edited take back over the source.
        *lpSource = *lpEdited;
    }
    else
    {
        // Grew: append the edited take-data to the edited-takes list tail, then start
        // a fresh edit buffer. mEditedTakeList[0]==Head sentinel, [1]==Tail. The
        // ICETakeData node base (mPadNodeBase) provides the Next/Prev linkage:
        // node-word[0]==Next, node-word[1]==Prev.
        void** lppNode = reinterpret_cast<void**>(lpEdited);
        void*  lpOldTail = mEditedTakeList[1];
        *reinterpret_cast<void**>(lpOldTail) = lpEdited;      // oldTail->Next = edited
        mEditedTakeList[1] = lpEdited;                        // Tail = edited
        lppNode[0] = &mEditedTakeList[0];                     // edited->Next = head sentinel
        lppNode[1] = lpOldTail;                               // edited->Prev = oldTail
        mEditTake.NewEditBuffer();
    }
}

// ---------------------------------------------------------------------------
// LoadCurrentAssembly
//
// Rebind the live edit take (mEditTake @0x28) to the current assembly source. When
// an assembly source is set (mpAssemblyTakeData @0x14 non-null): clear/copy the
// source take-data into the live take, bind the live take's primary data pointers to
// the assembly source (edit=false) and its sub-take data pointers to mpSubTakeData
// (edit=true), then seek the parameter to 0. When no assembly source is set: clear
// the edit take, seed a single assembly-channel (10) interval, bind the sub-take,
// and seek to 0. Finally mark the editor as being in ASSEMBLY mode (the @0x20 flag).
//
// FLAG (@0x20 mode flag): the trailing `*(this+0x20) = 1` write lands on the slot the
// frozen header names miSubTakeGuid. It is the assembly-mode flag (LoadCurrentTake
// writes 0 there), NOT a guid -- see the file-top FLAG. Written through miSubTakeGuid
// here because the header is frozen.
//
// FLAG (parameter-seek helper): the live-take parameter seek used here and in
// Construct/LoadCurrentTake is a take-parameter setter taking
// (this, <unused-slot>, bForce, bWrap, fParameter); it maps to
// ICETake::SetParameter(f32 lfParameter, bool lbForce, bool lbWrap). The extra
// leading slot is a spilled/undefined argument, dropped here. The two int flags
// become (lbForce, lbWrap) and the floating-point argument becomes lfParameter (0.0).
// ---------------------------------------------------------------------------
void ICEAuthor::LoadCurrentAssembly()
{
    if (mpAssemblyTakeData != NULL)                  // @0x14
    {
        // Copy the assembly source into the live take's take-data, then bind.
        *mEditTake.GetData() = *mpAssemblyTakeData;  // ICETakeData::operator= (a1[44]=mEditTake.mpTakeData)
        mEditTake.SetDataPointers(mpAssemblyTakeData, false); // primary = assembly source (edit=false)
        mEditTake.SetDataPointers(mpSubTakeData, true);       // sub-take = mpSubTakeData (edit=true)
    }
    else
    {
        // No assembly source: clear, seed a single assembly-channel interval, bind sub.
        ClearEditTake();
        mEditTake.SetParameter(0.0f, false, false);
        mEditTake.Insert(KI_ICE_ASSEMBLY_CHANNEL, 0); // seed assembly channel (10)
        mEditTake.SetDataPointers(mpSubTakeData, true);
    }

    mEditTake.SetParameter(0.0f, true, true);

    // Mark assembly mode (the @0x20 flag; written through the frozen header name).
    miSubTakeGuid = 1;
}

// ---------------------------------------------------------------------------
// ChangeAssemTakeLength
//
// Rescale the assembly take to a new length by lfDeltaSeconds. No-op when the delta
// is zero. Computes the new length as (currentLength + deltaSeconds * 30 frames),
// clamped to at least one frame (1.0). Then, for each assembly-channel (10) interval
// boundary, the boundary's normalised parameter is recomputed against the new length
// and the boundary is nudged into place via ICETake::MoveParameter(channel 10, ...).
// Finally the take-data length field is set to the new length.
//
// FLAG (arg is the FP delta): the ledger types the arg as `int a1` (this) only and
// reads the delta from an unassigned `double v2` -- the seconds delta is the FP
// argument register, not an int. The frozen declaration ChangeAssemTakeLength(f32)
// fixes it as the delta-seconds float.
//
// The interval key table is read inline: the channel's interval count is at the
// take-data (mElementCounts region) and the per-interval key indices are the u16
// key table; each key index k maps to the parameter (k * KF_ICE_KEY_TO_PARAMETER),
// boundary 0 -> parameter 0, the final boundary -> parameter 1.
// ---------------------------------------------------------------------------
void ICEAuthor::ChangeAssemTakeLength(f32 lfDeltaSeconds)
{
    if (lfDeltaSeconds == 0.0f)
    {
        return;
    }

    ICETakeData* lpData = mEditTake.GetData();           // *(this+44) == mEditTake.mpTakeData
    const f32 lfCurrentLength = (lpData != NULL) ? lpData->GetLength() : 0.0f;

    f32 lfNewLength = (lfDeltaSeconds * KF_ICE_FRAMES_PER_SECOND) + lfCurrentLength;
    if (lfNewLength < 1.0f)
    {
        lfNewLength = 1.0f;
    }

    // Walk every assembly-channel interval boundary (index 1 .. numKeys-1) and rescale
    // it to the new length. The boundary's old normalised parameter is read from the
    // key table; the new MoveParameter delta is (oldBoundaryParam * scaledByOldLength)
    // re-expressed against the new length, minus the old normalised parameter.
    const u32 luNumKeys = (u32)mEditTake.GetNumKeys(KI_ICE_ASSEMBLY_CHANNEL); // *(this+414)
    if (luNumKeys > 1)
    {
        const u16* lpu16Keys = mEditTake.GetKeyData(KI_ICE_ASSEMBLY_CHANNEL); // *(this+424)
        for (s32 liKey = 1; liKey < (s32)luNumKeys; ++liKey)
        {
            f32 lfBoundaryParam;
            if (liKey < (s32)mEditTake.GetNumKeys(KI_ICE_ASSEMBLY_CHANNEL))
            {
                const u16 lu16KeyIndex = lpu16Keys[liKey - 1]; // 2*v9 + base - 2
                lfBoundaryParam = (f32)lu16KeyIndex * KF_ICE_KEY_TO_PARAMETER;
            }
            else
            {
                lfBoundaryParam = 1.0f;
            }

            const f32 lfOldLength = (lpData != NULL) ? lpData->GetLength() : 0.0f;
            const f32 lfMove = ((lfOldLength * lfBoundaryParam) * (1.0f / lfNewLength)) - lfBoundaryParam;
            mEditTake.MoveParameter(KI_ICE_ASSEMBLY_CHANNEL, (u16)liKey, 0, lfMove);
        }
    }

    if (lpData != NULL)
    {
        lpData->SetLength(lfNewLength);                  // *(*(this+44) + 44) = newLength
    }
}

// ---------------------------------------------------------------------------
// SetAssemblySourceTake
//
// Swap the sub-take referenced by the current assembly interval to the take with
// guid liGuid. Active only while the editor is live (miState > 0) AND in assembly
// mode (the @0x20 flag set). No-op when there is no assembly source, or when the
// assembly source's guid already equals liGuid. Otherwise it looks the new take up
// through the resource manager, records it as the sub-take (mpSubTakeData @0x18),
// and -- when the editor is in ASSEMBLY_MARK (8) -- rebinds the live take's sub-take
// to it.
//
// FLAG (arg type): the body compares liGuid against the assembly source's guid
// (`*(mpAssemblyTakeData + 8) != a2`, take-data miGuid @0x08), so the arg is the s32
// guid -- matching the frozen SetAssemblySourceTake(s32 liGuid) declaration, NOT an
// ICETakeData*.
//
// FLAG (@0x20 mode gate + @0x1C guid scratch + nullary mpResourceManager functor):
// see the two file-top FLAGs. The gate `if (*(this+0x20))` reads the assembly-mode
// flag (accessed via miSubTakeGuid). The guid stash `v2[7] = guid` writes the @0x1C
// scratch (accessed via mfInsertTangent reinterpreted). The lookup
// `result = (**(this+0x0C))(this+0x0C)` is the nullary stash-then-call resource
// functor, reconstructed as mpResourceManager->GetTakeData(liGuid).
// ---------------------------------------------------------------------------
void ICEAuthor::SetAssemblySourceTake(s32 liGuid)
{
    if (miState <= 0)                       // *(this+3736) > 0
    {
        return;
    }
    if (miSubTakeGuid == 0)                 // @0x20 assembly-mode flag (frozen-name access)
    {
        return;
    }

    ICETakeData* lpSource = mpAssemblyTakeData;   // @0x14
    if (lpSource == NULL)
    {
        return;
    }
    if (lpSource->GetGuid() == liGuid)            // *(v3 + 8) != a2
    {
        return;
    }

    // Stash the requested guid into the @0x1C scratch (frozen-name access via the
    // tangent slot), then resolve the take through the resource-manager functor.
    *reinterpret_cast<s32*>(&mfInsertTangent) = liGuid;   // v2[7] = guid (@0x1C)

    const ICETakeData* lpResolved = (mpResourceManager != NULL)
        ? mpResourceManager->GetTakeData(liGuid)         // (**(this+0x0C))(this+0x0C)
        : NULL;

    mpSubTakeData = const_cast<ICETakeData*>(lpResolved); // v2[6] = result (@0x18)

    if (miState == E_ICE_AUTHOR_STATE_ASSEMBLY_MARK)      // v5 == 8
    {
        mEditTake.SetSubTake(mpSubTakeData, true);        // (v2+10), result, 1
    }
}

} // namespace ICE
