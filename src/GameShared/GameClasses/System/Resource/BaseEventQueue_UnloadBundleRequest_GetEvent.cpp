// Per-instantiation .cpp for CgsModule::BaseEventQueue<CgsResource::Events::UnloadBundleRequest>::GetEvent.
// The generic BaseEventQueue<T> body (AddEvent/GetEvent/Append/...) is fully inline in
// CgsBaseEventQueue.h, so this TU is just the explicit member instantiation (the X360 emits one
// out-of-line copy per using-TU), mirroring the committed sibling
// BaseEventQueue_UnloadBundleRequest_AddEvent.cpp for this same type.
//
// Byte-parity check against the X360 GetEvent(int) const pseudocode for this instantiation:
//   GetEvent @ 0x828DF530, called by CgsResource::BundleLoaderModule::ProcessBundleLoadRequests.
//   Checked const accessor: asserts mpEvents != NULL, liIndex < GetLength(), liIndex >= 0, then
//   returns &mpEvents[liIndex]. The X360 computes the element offset with
//   `slwi r11,liIndex,3; add r11,liIndex,r11; slwi r11,r11,4` == liIndex*(1+8)*16 == liIndex*144,
//   i.e. the 144-byte UnloadBundleRequest element stride (BundleLoaderEvent base, no extra payload),
//   matching the committed BaseEventQueue<UnloadBundleRequest>::AddEvent instantiation (stride 0x90).
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // UnloadBundleRequest (144B element)

template const CgsResource::Events::UnloadBundleRequest&
CgsModule::BaseEventQueue<CgsResource::Events::UnloadBundleRequest>::GetEvent(s32) const;
