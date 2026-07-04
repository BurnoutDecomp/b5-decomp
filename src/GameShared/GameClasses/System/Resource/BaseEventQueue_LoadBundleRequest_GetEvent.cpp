// Per-instantiation .cpp for CgsModule::BaseEventQueue<CgsResource::Events::LoadBundleRequest>::GetEvent.
// The generic BaseEventQueue<T> body is fully inline in CgsBaseEventQueue.h; this TU forces only the
// explicit member instantiation (one out-of-line copy per using-TU), mirroring the committed sibling
// BaseEventQueue_LoadBundleRequest_AddEvent.cpp.
//
// Byte-parity check against the X360 GetEvent(int) const pseudocode:
//   GetEvent @ 0x828DF488, called by CgsResource::BundleLoaderModule::ProcessBundleLoadRequests.
//   Checked const accessor: asserts mpEvents != NULL, liIndex < GetLength(), liIndex >= 0, then
//   returns &mpEvents[liIndex] via `mulli r11,liIndex,0x94; add` == liIndex*148 + mpEvents. The
//   148-byte stride is sizeof(LoadBundleRequest), matching the committed
//   AddEvent<LoadBundleRequest> instantiation.
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // LoadBundleRequest (148B element)

template const CgsResource::Events::LoadBundleRequest&
CgsModule::BaseEventQueue<CgsResource::Events::LoadBundleRequest>::GetEvent(s32) const;
