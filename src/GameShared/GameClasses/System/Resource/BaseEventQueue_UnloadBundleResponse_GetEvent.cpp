// Per-instantiation .cpp for CgsModule::BaseEventQueue<CgsResource::Events::UnloadBundleResponse>::
// GetEvent(s32) const @ 0x828DF688. The generic const GetEvent body is fully inline in
// CgsBaseEventQueue.h (three non-gating asserts mpEvents!=NULL / liIndex<GetLength() /
// liIndex>=0, then return mpEvents[liIndex]); the X360 emits one out-of-line copy per using-TU,
// so this TU is just the explicit member instantiation. Drained by
// CgsResource::ResourceModule::ProcessResourceResponses.
//
// Element stride 0x90 == 144 (return math slwi3/add/slwi4 == 144*idx + *mpEvents):
// UnloadBundleResponse is a bare BundleLoaderEvent (mpUser 4 + miEventId 4 + mbLiveUpdateReplace 1
// [+3 pad] + macFileName[128] + miPoolId 4 == 144). The generic uses sizeof(T), and the committed
// CgsResourceIOEvents.h UnloadBundleResponse is exactly 144B, so parity holds.
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"   // UnloadBundleResponse (144B element)

template const CgsResource::Events::UnloadBundleResponse&
CgsModule::BaseEventQueue<CgsResource::Events::UnloadBundleResponse>::GetEvent(s32) const;
