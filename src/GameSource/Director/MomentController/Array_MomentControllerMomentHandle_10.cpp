// Per-instantiation .cpp for Array<BrnDirector::MomentController::MomentHandle, 10>.
// The generic Array<T,N>::Append body is fully inline in CgsArray.h; this TU is the thin
// explicit instantiation only (the console build emits one out-of-line Append per using-TU). Do NOT
// re-define the generic.
//
//   Array<MomentController::MomentHandle,10>::Append
//       (caller: BrnDirector::MomentSelector::AddMoment)
//
// Reconstructed from the console executable. The console body matches the generic store-for-store:
//   * asserts the array was Construct/Clear'd (miCount @ +0xF0 != the -1 sentinel,
//     -> "Array used before Construct/Clear was called"),
//   * asserts there is room (unsigned miCount >= 0xA) -> the streamed
//     "Array container out of space, Length/Capacity" message, kept here as the generic's
//     static CGS_ASSERT string),
//   * copies the 24-byte MomentHandle into &maElements[miCount] as six 4-byte words
//     (the index scales by a 0x18 stride; six word copies), then increments miCount.
// The count word at byte 0xF0 == 10 * sizeof(MomentHandle) confirms the inline maElements[10]
// buffer end and sizeof(MomentController::MomentHandle) == 0x18.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Director/MomentController/BrnMomentController.h"

template void Array<BrnDirector::MomentController::MomentHandle, 10>::Append(
    const BrnDirector::MomentController::MomentHandle&);

// Array<BrnDirector::MomentController::MomentHandle,10>::operator[](u32)
// Non-const checked accessor (generic body inline in CgsArray.h; asserts 0x21A/0x21B).
// count @+0xF0, stride 0x18 == sizeof(MomentHandle). Caller MomentSelector::ActualDebugRender.
template BrnDirector::MomentController::MomentHandle&
Array<BrnDirector::MomentController::MomentHandle, 10>::operator[](u32);

// Array<BrnDirector::MomentController::MomentHandle,10>::operator[](u32) const
// Const checked accessor (generic body inline in CgsArray.h; asserts 0x22C/0x22D).
// count @+0xF0, stride 0x18. 9 read-side callers (GetSelectedMoment/Release/...).
template const BrnDirector::MomentController::MomentHandle&
Array<BrnDirector::MomentController::MomentHandle, 10>::operator[](u32) const;
