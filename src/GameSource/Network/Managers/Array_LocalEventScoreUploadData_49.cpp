// Per-instantiation .cpp for Array<BrnNetwork::LocalEventScoreUploadData, 49>.
//
//   Array<BrnNetwork::LocalEventScoreUploadData,49>::Append    @ 0x8254E0A8
//     (called by CgsContainers::AppendArray<..>(Array<LocalEventScoreUploadData,49>&) )
//   Array<BrnNetwork::LocalEventScoreUploadData,49>::Erase     @ 0x8254D370
//     (called by BrnNetwork::EventScoresManager::_UploadEventScoreCallback)
//   Array<BrnNetwork::LocalEventScoreUploadData,49>::EraseFast @ 0x8235C378
//     (called by BrnProgression::Profile::RemoveEventScoreToUpload)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N>::Append/Erase/EraseFast
// bodies are fully inline in CgsArray.h, so this TU is just the explicit member
// instantiations (the X360 emits one out-of-line copy per using-TU). The element type is the
// committed BrnNetwork::LocalEventScoreUploadData (16-byte record), reused by name.
//
// Layout / byte-parity check against the X360 bodies:
//   miCount lives at +0x310 (== 49 * 16 == N * sizeof(T)), i.e. immediately after the inline
//   maElements[49] buffer, exactly as the committed generic Array<T,N> places it.
//     lwz r11, 0x310(this); cmpwi -1   -> "Array used before Construct/Clear was called"
//                                          (KI_UNCONSTRUCTED sentinel guard)
//     cmplwi 0x31 / cmplw miCount       -> capacity-49 / index bounds guards (unsigned, so the
//                                          -1 sentinel also fails them)
//     Append:    two `std` of the 16-byte element at 16*miCount + this, then ++miCount
//     Erase:     --miCount, then shift the 16-byte tail down one slot (order-preserving)
//     EraseFast: copy maElements[miCount-1] over maElements[index] (16 bytes), then --miCount
// The X360 Append streams the dynamic "Array container out of space, Length/Capacity" message
// via the StrStream/BasePriorityQueue::Clear path; the committed generic Append keeps that as
// the static CGS_ASSERT string -- the documented generic-body parity gap (fixing the dynamic
// form must GROW the shared CgsArray.h body so every instantiation re-verifies; deliberately
// not specialised in this thin TU).
//
// The element is a plain aggregate (no user-declared copy-assignment), so its element copy is
// the compiler-generated member-wise copy -- matching the X360's 16-byte block move. Only the
// three emitted members are instantiated (NOT a whole-class `template class`, which would
// force the equality-based members FindFirstInstanceOf/Contains that require T::operator==;
// LocalEventScoreUploadData has none and the X360 never emits those for it).
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Network/Managers/BrnEventScoresManager.h"  // BrnNetwork::LocalEventScoreUploadData (16B element)

template void Array<BrnNetwork::LocalEventScoreUploadData, 49>::Append(const BrnNetwork::LocalEventScoreUploadData&);
template void Array<BrnNetwork::LocalEventScoreUploadData, 49>::Erase(u32);
template void Array<BrnNetwork::LocalEventScoreUploadData, 49>::EraseFast(u32);

// non-const operator[]  @ 0x8254E3E8  (checked indexed accessor; reached through
// CgsContainers::AppendArray<..>(Array<LocalEventScoreUploadData,49>&) which indexes the source
// array element-by-element). The X360 body is the generic non-const operator[] (CgsArray.h:538/539):
// asserts miCount != the -1 sentinel ("Array used before Construct/Clear was called"), unsigned
// bounds-checks the index against miCount @ +0x310 (== 49 * 16 == N * sizeof(T)), then returns
// &maElements[i] (element stride 0x10 == 16). The dynamic "Array index out of bounds. Index: <i>,
// length: <n>" StrStream message is the documented generic-body parity gap (kept as the static
// CGS_ASSERT string).
template BrnNetwork::LocalEventScoreUploadData&
    Array<BrnNetwork::LocalEventScoreUploadData, 49>::operator[](u32);

// AddNew  @ 0x8235C268  (reserve-a-slot accessor; called by
// BrnNetwork::EventScoresManager::StoreEventScoreForUpload @0x8255DFB0 via maPendingUploads.AddNew()
// and by BrnProgression::Profile::SetEventScoreToUpload). The X360 body is the generic
// Array<T,N>::AddNew (CgsArray.h:179): asserts miCount != the -1 sentinel + unsigned capacity-49
// guard, then `slwi r10,r11,4` (16*miCount) + add this -> returns &maElements[miCount] and
// post-increments miCount (stw r11+1,0x310). Stride 0x10 == 16 == sizeof(LocalEventScoreUploadData).
template BrnNetwork::LocalEventScoreUploadData*
    Array<BrnNetwork::LocalEventScoreUploadData, 49>::AddNew();

// AppendArray  @ 0x8255E940  (append every live element of a same-instantiation source Array;
// called by BrnNetwork::EventScoresManager::ProcessNetworkEvents, which folds the drained
// per-event upload records into maPendingUploads). The X360 body is the generic
// Array<T,N>::AppendArray<N> (CgsArray.h:272): asserts this + source were Construct/Clear'd
// (both miCount @ +0x310 != the -1 sentinel; source's constructed-guard is the CgsArray.h:336
// GetLength assert, re-issued per loop iteration in the X360 -- source count is loop-invariant so
// the committed cache-once form is observably identical), then asserts the combined live count
// fits N via the unsigned `> 0x31` capacity-49 guard ("Array container out of space appending an
// array", CgsArray.h:246), then Appends source[0..GetLength()-1] in order (checked operator[]
// stride 0x10 == 16 == sizeof(LocalEventScoreUploadData) feeding the generic Append). The dynamic
// StrStream "Length/Capacity" message is the documented generic-body parity gap (kept as the
// static CGS_ASSERT string). Reuses the already-instantiated operator[] and Append above.
template void
    Array<BrnNetwork::LocalEventScoreUploadData, 49>::AppendArray(
        const Array<BrnNetwork::LocalEventScoreUploadData, 49>&);
