// Per-instantiation .cpp for Array<BrnDirector::MomentController::MomentHandle, 10>.
// The generic Array<T,N>::Append body is fully inline in CgsArray.h; this TU is the thin
// explicit instantiation only (the X360 emits one out-of-line Append per using-TU). Do NOT
// re-define the generic.
//
//   Array<MomentController::MomentHandle,10>::Append @0x821FD990
//       (caller: BrnDirector::MomentSelector::AddMoment)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The X360 body matches the generic store-for-store:
//   * asserts the array was Construct/Clear'd (miCount @ +0xF0 != the -1 sentinel,
//     CgsArray.h:225 -> "Array used before Construct/Clear was called"),
//   * asserts there is room (unsigned miCount >= 0xA, CgsArray.h:226 -> the streamed
//     "Array container out of space, Length/Capacity" message, kept here as the generic's
//     static CGS_ASSERT string),
//   * copies the 24-byte MomentHandle into &maElements[miCount] as six 4-byte words
//     (slwi/add/slwi yields a *0x18 stride; loop count 6), then increments miCount.
// The count word at byte 0xF0 == 10 * sizeof(MomentHandle) confirms the inline maElements[10]
// buffer end and sizeof(MomentController::MomentHandle) == 0x18.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Director/MomentController/BrnMomentController.h"

template void Array<BrnDirector::MomentController::MomentHandle, 10>::Append(
    const BrnDirector::MomentController::MomentHandle&);
