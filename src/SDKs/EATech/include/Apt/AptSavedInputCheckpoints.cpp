// ===========================================================================
// EATech Apt -- AptSavedInputCheckpoints (the saved-input replay/debug helpers).
//
// DECOMPILED from BURNOUT_X360_ARTIST.XEX (X360 asm is authoritative):
//   AptSavedInputCheckpoints::AllLinked               @ 0x82AFADB0
//   AptSavedInputCheckpoints::CanContinueSavedInputs  @ 0x82AEE980
//   AptSavedInputCheckpoints::CanLinkPendingFiles     @ 0x82AEE938
//   AptSavedInputCheckpoints::Checkpoint              @ 0x82AFD3D0
//
// These are a namespace of free functions over the saved-input list (the
// AptFileSavedInputStateVector, = the X360 BasicString<StringAsVectorEncoding<
// AptFileSavedInputState>>); each takes the list as the implicit `this` / first
// __fastcall arg. See AptSavedInputCheckpoints.h for the container lineage + the
// named-member (x64 semantic-parity) treatment; the console grows the backing array
// through StringAsVectorPolicy<AptFileSavedInputState>::ReAlloc, whose own
// allocator (sub_82C08C00) is the global `operator new` -- so the typed-vector
// reconstruction below manages its storage with global new/delete + explicit
// element lifetimes, the faithful equivalent of the template's bodies.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptSavedInputCheckpoints.h"
#include "SDKs/EATech/include/Apt/AptLoader.h"        // AptLoader::IsLoaded / AptFilePtr
#include "SDKs/EATech/include/Apt/AptTarget.h"        // AptTarget::GetLoader (full type for the member call)
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"     // AptSharedPtrDecRef / AptSharedPtrDelete

#include <new>        // placement new (element construction in raw storage)

// ---------------------------------------------------------------------------
// The current AptTarget the saved-input system runs against (ARTIST off_8324E574
// -- the same global AptLinker::Update reads). off_8324E574's LIVE definition is
// `gpAptTarget` (AptTarget.cpp, declared by the included AptTarget.h, stamped by
// AptInit's target bring-up + AptUpdate); the old `gpCurrentAptTarget` extern
// bound AptGlobals.cpp's dead never-written duplicate of the same console slot,
// so Checkpoint dereferenced null. The loader is reached through the named
// GetLoader accessor (console: target+0x1C).
// ---------------------------------------------------------------------------

// The single global checkpoint list (declared in the header; homed by the boot TU).
AptFileSavedInputStateVector* gpAptSavedInputCheckpoints = nullptr;   // dword_8324D810

// ===========================================================================
// AptFileSavedInputStateVector -- the StringAsVectorPolicy growth/teardown ops,
// reconstructed as the typed-vector equivalents (Insert @0x82AF8460 / ReAlloc
// @0x82AEFC00 grow to a doubled capacity through the global operator new; the
// AllLinked swap-destruct frees the block). Elements are non-trivially
// destructible (EAStringC), so their lifetimes are managed explicitly over raw
// storage -- mirroring the console's count-prefixed raw allocation + per-element
// construct/destruct.
// ===========================================================================
void AptFileSavedInputStateVector::Reserve(int32_t nCount)
{
    if (mnCapacity >= nCount)
        return;

    int32_t newCap = (mnCapacity > 0) ? mnCapacity * 2 : 1;
    if (newCap < nCount)
        newCap = nCount;

    AptFileSavedInputState* pNew = static_cast<AptFileSavedInputState*>(
        ::operator new(sizeof(AptFileSavedInputState) * static_cast<size_t>(newCap)));

    // Move the existing records into the new storage, then tear the old ones down.
    for (int32_t i = 0; i < mnSize; ++i)
    {
        ::new (&pNew[i]) AptFileSavedInputState(mpData[i]);
        mpData[i].~AptFileSavedInputState();
    }
    if (mpData)
        ::operator delete(mpData);

    mpData     = pNew;
    mnCapacity = newCap;
}

void AptFileSavedInputStateVector::PushBack(const AptFileSavedInputState& rState)
{
    Reserve(mnSize + 1);
    ::new (&mpData[mnSize]) AptFileSavedInputState(rState);
    ++mnSize;
}

void AptFileSavedInputStateVector::Clear()
{
    for (int32_t i = 0; i < mnSize; ++i)
        mpData[i].~AptFileSavedInputState();
    if (mpData)
        ::operator delete(mpData);
    mpData     = nullptr;
    mnSize     = 0;
    mnCapacity = 0;
}

// ===========================================================================
// AptSavedInputCheckpoints -- the saved-input checkpoint operations.
// ===========================================================================
namespace AptSavedInputCheckpoints
{

// ---------------------------------------------------------------------------
// AllLinked @0x82AFADB0 -- clear the checkpoint list.
//
// The X360 builds an empty temporary StringAsVector (inline storage), swaps the
// live vector into it (the SBO-aware BasicString assignment `String` @0x82AEC4C8),
// then destroys the temporary (`StringAsVec` @0x82AF83F8, which frees the swapped-
// out elements + backing block); the leftover return value is discarded by the
// caller. Net effect: the live vector is emptied and its storage freed -- exactly
// Clear(). (Symbol name kept verbatim though the body is a clear.)
// ---------------------------------------------------------------------------
void AllLinked(AptFileSavedInputStateVector& rList)
{
    rList.Clear();
}

// ---------------------------------------------------------------------------
// CanContinueSavedInputs @0x82AEE980 -- true unless some record is still in a
// state other than LOADED(2) or PLAYED(4). (Walks begin..begin+count, stride 8;
// reads *(element+4) = meState. The X360 short-circuits on the first failure.)
// ---------------------------------------------------------------------------
bool CanContinueSavedInputs(const AptFileSavedInputStateVector& rList)
{
    for (const AptFileSavedInputState* pRec = rList.begin(); pRec != rList.end(); ++pRec)
    {
        if (pRec->meState != E_APT_SAVED_INPUT_PLAYED &&
            pRec->meState != E_APT_SAVED_INPUT_LOADED)
        {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// CanLinkPendingFiles @0x82AEE938 -- true unless some record is still in a state
// other than LOADED(2) or LINKED(3) (every pending file is ready for the linker).
// ---------------------------------------------------------------------------
bool CanLinkPendingFiles(const AptFileSavedInputStateVector& rList)
{
    for (const AptFileSavedInputState* pRec = rList.begin(); pRec != rList.end(); ++pRec)
    {
        if (pRec->meState != E_APT_SAVED_INPUT_LINKED &&
            pRec->meState != E_APT_SAVED_INPUT_LOADED)
        {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Checkpoint @0x82AFD3D0 -- register a checkpoint for `name`.
//
// X360 flow:
//   1. Scan the existing records for one whose mInputName equals `name`
//      (Burnout_X360_Artist_0040_0 @0x82AD85D8 = the AptFileSavedInputState name
//      equality, which only compares the embedded EAStringC). On a hit: if that
//      record is LOADED(2), promote it to LINKED(3); either way, return.
//   2. On a miss: ask the current target's loader whether the file is loaded
//      (AptLoader::IsLoaded), releasing the counted reference it hands back, and
//      append a new record -- LINKED(3) if loaded, PENDING(1) if not.
// The trailing DecreaseInternalRefCount the asm performs is the temporary record's
// ~EAStringC after PushBack copies it in; the temp here does that on scope exit.
// ---------------------------------------------------------------------------
void Checkpoint(AptFileSavedInputStateVector& rList, const EAStringC& name)
{
    // 1. Promote an existing record for this name.
    for (AptFileSavedInputState* pRec = rList.begin(); pRec != rList.end(); ++pRec)
    {
        if (pRec->mInputName == name)
        {
            if (pRec->meState == E_APT_SAVED_INPUT_LOADED)
                pRec->meState = E_APT_SAVED_INPUT_LINKED;
            return;
        }
    }

    // 2. Not present -- is the file already loaded?
    AptLoader*  pLoader = gpAptTarget->GetLoader();
    AptFilePtr  loaded  = pLoader->IsLoaded(name);
    const bool  bLoaded = (loaded.pData != nullptr);

    // Release the reference IsLoaded handed back (we only needed the yes/no).
    AptFile* pTmp = loaded.pData;
    loaded.pData = nullptr;
    if (pTmp && AptSharedPtrDecRef(pTmp) == 0)
        AptSharedPtrDelete(pTmp);

    // Append a fresh record (its mInputName copy-ctors from `name`).
    AptFileSavedInputState newRecord;
    newRecord.mInputName = name;
    newRecord.meState = bLoaded ? E_APT_SAVED_INPUT_LINKED : E_APT_SAVED_INPUT_PENDING;
    rList.PushBack(newRecord);
    // newRecord goes out of scope here -> ~EAStringC releases its name (the asm's
    // trailing EAStringC::DecreaseInternalRefCount on the temp's first word).
}

}   // namespace AptSavedInputCheckpoints
