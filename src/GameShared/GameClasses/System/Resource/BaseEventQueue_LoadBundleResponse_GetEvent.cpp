// Per-instantiation .cpp for CgsModule::BaseEventQueue<CgsResource::Events::LoadBundleResponse>::GetEvent.
// The generic BaseEventQueue<T> body is fully inline in CgsBaseEventQueue.h; this TU forces only the
// explicit member instantiation (one out-of-line copy per using-TU), mirroring the committed sibling
// BaseEventQueue_LoadBundleResponse_AddEvent.cpp.
//
// Byte-parity check against the X360 GetEvent(int) const pseudocode:
//   GetEvent @ 0x828DF5E0, called by CgsResource::ResourceModule::ProcessResourceResponses. Checked
//   const accessor: asserts mpEvents != NULL, liIndex < GetLength(), liIndex >= 0, then returns
//   &mpEvents[liIndex] via `mulli r11,liIndex,0x94; add` == liIndex*148 + mpEvents. The 148-byte
//   stride is sizeof(LoadBundleResponse) (BundleLoaderEvent base 144 + meResult), matching the
//   committed AddEvent<LoadBundleResponse> instantiation.
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // LoadBundleResponse (148B element)

template const CgsResource::Events::LoadBundleResponse&
CgsModule::BaseEventQueue<CgsResource::Events::LoadBundleResponse>::GetEvent(s32) const;
