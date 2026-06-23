// Per-instantiation .cpp for CgsModule::BaseEventQueue<CgsResource::Events::UnloadBundleResponse>::AddEvent.
// The generic BaseEventQueue<T> body (AddEvent/AddEventSafe/GetEvent/Append/...) is fully inline in
// CgsBaseEventQueue.h, so this TU is just the explicit member instantiation (the X360 emits one
// out-of-line copy per using-TU).
//
// Byte-parity check against the X360 AddEvent pseudocode for this instantiation:
//   AddEvent @ 0x828EA1D8, called by CgsResource::BundleLoaderModule::CheckForUnloads. The assert
//   message literally names this type:
//     "CgsModule::BaseEventQueue<class CgsResource::Events::UnloadBundleResponse>::AddEvent".
//   Layout a1[0]=mpEvents, a1[1]=miMaxLength, a1[2]=miLength (element stride 0x90 == 144 == the
//   BundleLoaderEvent base size: mpUser 4 + miEventId 4 + mbLiveUpdateReplace 1 [+3 pad] +
//   macFileName[128] + miPoolId 4 == 144 on the 32-bit ARTIST target -- UnloadBundleResponse adds
//   no payload fields). It asserts `mpEvents != NULL` (CgsBaseEventQueue.h:312), then on
//   `miLength >= miMaxLength` asserts "Reached Max length <max>" (line 313); both are non-gating
//   tripwires. It then appends UNCONDITIONALLY: `memcpy(144 * miLength++ + mpEvents, item, 144)` ==
//   `mpEvents[miLength++] = *lpEvent`, returning 1. This matches the generic
//   BaseEventQueue<T>::AddEvent exactly.
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // UnloadBundleResponse (144B element)

template bool CgsModule::BaseEventQueue<CgsResource::Events::UnloadBundleResponse>::AddEvent(
    const CgsResource::Events::UnloadBundleResponse&);
