// Per-instantiation .cpp for Array<BrnDirector::MomentDescription, 10>.
// The generic Array<T,N>::Append body is fully inline in CgsArray.h; this TU is the thin
// explicit instantiation only (the X360 emits one out-of-line Append per using-TU). Do NOT
// re-define the generic.
//
//   Array<MomentDescription,10>::Append @0x821FD858
//       (caller: BrnDirector::MomentSelector::AddMoment)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The X360 body matches the generic store-for-store:
//   * asserts the array was Construct/Clear'd (miCount @ +0xA0 != the -1 sentinel,
//     CgsArray.h:225 -> "Array used before Construct/Clear was called"),
//   * asserts there is room (unsigned miCount >= 0xA, CgsArray.h:226 -> the streamed
//     "Array container out of space, Length/Capacity" message, kept here as the generic's
//     static CGS_ASSERT string),
//   * copies the 16-byte MomentDescription into &maElements[miCount] as four 4-byte words
//     (slwi r11,miCount,4 -> *0x10 stride; four explicit stw), then increments miCount.
// The count word at byte 0xA0 == 10 * sizeof(MomentDescription) confirms the inline
// maElements[10] buffer end and sizeof(MomentDescription) == 0x10.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Director/MomentController/BrnMomentController.h"

template void Array<BrnDirector::MomentDescription, 10>::Append(
    const BrnDirector::MomentDescription&);
