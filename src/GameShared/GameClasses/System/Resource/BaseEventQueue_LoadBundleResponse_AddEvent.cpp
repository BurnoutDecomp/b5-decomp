// Per-instantiation .cpp for CgsModule::BaseEventQueue<CgsResource::Events::LoadBundleResponse>::AddEvent.
// The generic BaseEventQueue<T> body (AddEvent/AddEventSafe/GetEvent/Append/...) is fully inline in
// CgsBaseEventQueue.h, so this TU is just the explicit member instantiation (the X360 emits one
// out-of-line copy per using-TU).
//
// Byte-parity check against the X360 AddEvent pseudocode for this instantiation:
//   AddEvent @ 0x828EA098, called by CgsResource::BundleLoaderModule::PostLoadFinishedEvent and
//   CgsResource::BundleLoaderModule::CheckForLoads. The assert message literally names this type:
//     "CgsModule::BaseEventQueue<class CgsResource::Events::LoadBundleResponse>::AddEvent".
//   Layout a1[0]=mpEvents, a1[1]=miMaxLength, a1[2]=miLength (element stride 0x94 == 148). It asserts
//   `mpEvents != NULL` (CgsBaseEventQueue.h:312), then on `miLength >= miMaxLength` asserts
//   "Reached Max length <max>" (line 313); both are non-gating tripwires. It then appends
//   UNCONDITIONALLY: `memcpy(148 * miLength++ + mpEvents, item, 148)` == `mpEvents[miLength++] =
//   *lpEvent`, returning 1. This matches the generic BaseEventQueue<T>::AddEvent exactly.
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // LoadBundleResponse (148B element)

template bool CgsModule::BaseEventQueue<CgsResource::Events::LoadBundleResponse>::AddEvent(
    const CgsResource::Events::LoadBundleResponse&);
