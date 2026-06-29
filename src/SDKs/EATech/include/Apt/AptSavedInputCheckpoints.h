#pragma once

// ===========================================================================
// EATech Apt -- AptSavedInputCheckpoints: the saved-input replay/debug helpers.
//
// AptSavedInputCheckpoints is a NAMESPACE of free functions operating on the Apt
// saved-input (input replay / debug record-and-playback) list -- a single global
// vector of AptFileSavedInputState records (ARTIST dword_8324D810). Each record is
// {EAStringC mInputName; E_APT_SAVED_INPUT_STATE meState}. The AptLinker
// (AptLinker::Update @0x82B0CC68) and the saved-input replay tick (sub_82B0D7E8)
// drive the list through these:
//   - Checkpoint(list, name)        register/promote a file as it streams in.
//   - CanLinkPendingFiles(list)     gate AptLinker::Update (every record 2 or 3).
//   - CanContinueSavedInputs(list)  gate the replay tick (every record 2 or 4).
//   - AllLinked(list)               clear the list once all files are linked.
// (On the X360 each takes the list as the implicit `this` / first __fastcall arg
// r3 -- the saved-input vector itself.)
//
// THE CONTAINER: on the X360 the list is the EA "string-as-vector" idiom --
//   BasicString<StringAsVectorEncoding<AptFileSavedInputState>, StringAsVectorPolicy>
// i.e. the EAStringC/BasicString template reused as a generic vector (the same
// StringAsVectorPolicy<AptFileSavedInputState>::ReAlloc that AptFileSavedInputState.h
// names; ReAlloc @0x82AEFC00 / Insert @0x82AF8460 / the swap-assign @0x82AEC4C8).
// The console header is three 32-bit words {size, capacity, begin} with a
// small-buffer optimisation and a count-prefixed heap block, allocated through the
// global operator new. There is no DWARF / Feb-2007 / wiki entry for it, and the
// BasicString<...> instantiation is not its own ledger TU (Insert/ReAlloc/Move/the
// swap helper are compiler-emitted template bodies). It is therefore reconstructed
// here as the equivalent typed vector AptFileSavedInputStateVector, accessed by
// NAMED members (semantic parity, not the console's literal 4-byte offsets / SBO)
// -- the pervasive x64 PC-port FLAG.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptFileSavedInputState.h"   // element type + E_APT_SAVED_INPUT_STATE
#include "SDKs/EATech/include/Apt/AptString/EAString.h"        // EAStringC (the Checkpoint key)

// ---------------------------------------------------------------------------
// AptFileSavedInputStateVector -- the reconstructed saved-input list container
// (the X360 BasicString<StringAsVectorEncoding<AptFileSavedInputState>>). Modelled
// as a clean typed vector: the console header is {size, capacity, begin} (three
// 32-bit words); the inline-buffer / count-prefix SBO is a template storage detail
// de-optimised away for the x64 reconstruction. Elements are owned (each holds a
// ref-counted EAStringC), so growth/teardown manage element lifetimes explicitly.
// ---------------------------------------------------------------------------
struct AptFileSavedInputStateVector
{
    int32_t                 mnSize;        // [0] logical element count
    int32_t                 mnCapacity;    // [1] allocated slots
    AptFileSavedInputState* mpData;        // [2] element storage

    AptFileSavedInputStateVector() : mnSize(0), mnCapacity(0), mpData(nullptr) {}

    AptFileSavedInputState*       begin()       { return mpData; }
    AptFileSavedInputState*       end()         { return mpData + mnSize; }
    const AptFileSavedInputState* begin() const { return mpData; }
    const AptFileSavedInputState* end()   const { return mpData + mnSize; }
    int32_t                       size()  const { return mnSize; }

    // The StringAsVectorPolicy growth/teardown ops, reconstructed as the typed
    // vector equivalents (Insert @0x82AF8460 / ReAlloc @0x82AEFC00 grow through the
    // global operator new; the AllLinked swap-destruct frees the block).
    void PushBack(const AptFileSavedInputState& rState);   // append (grow if needed)
    void Clear();                                          // release all + free storage

private:
    void Reserve(int32_t nCount);                          // grow capacity
};

// ---------------------------------------------------------------------------
// The saved-input checkpoint operations. A namespace (not a class) -- the IDA
// `AptSavedInputCheckpoints::` qualifier is a namespace; the container's real type
// is BasicString<StringAsVectorEncoding<AptFileSavedInputState>> (above).
// ---------------------------------------------------------------------------
namespace AptSavedInputCheckpoints
{
    // @0x82AFADB0 -- clear the checkpoint list (the X360 swaps the vector with an
    // empty temporary and destroys the temporary, freeing the elements + storage;
    // the leftover return value is unused). Symbol name kept verbatim.
    void AllLinked(AptFileSavedInputStateVector& rList);

    // @0x82AEE980 -- true iff every record is LOADED(2) or PLAYED(4) (the replay
    // may continue: nothing is still PENDING / being-linked).
    bool CanContinueSavedInputs(const AptFileSavedInputStateVector& rList);

    // @0x82AEE938 -- true iff every record is LOADED(2) or LINKED(3) (all pending
    // files are ready for the linker).
    bool CanLinkPendingFiles(const AptFileSavedInputStateVector& rList);

    // @0x82AFD3D0 -- register a checkpoint for `name`. If a record for the name
    // already exists, promote it from LOADED(2) to LINKED(3); otherwise append a
    // new record, LINKED(3) when the file is already loaded or PENDING(1) when it
    // is not (queried through the current AptTarget's loader).
    void Checkpoint(AptFileSavedInputStateVector& rList, const EAStringC& name);
}

// ---------------------------------------------------------------------------
// FLAG (homed by the saved-input system boot, not yet reconstructed): the single
// global checkpoint list (ARTIST dword_8324D810). AptLinker::Update and the replay
// tick operate on this instance. Null until the saved-input system is initialised.
// ---------------------------------------------------------------------------
extern AptFileSavedInputStateVector* gpAptSavedInputCheckpoints;   // dword_8324D810
