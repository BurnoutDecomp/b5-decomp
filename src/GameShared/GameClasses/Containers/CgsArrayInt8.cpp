// Per-instantiation .cpp for Array<s32,8>. The generic Array<T,N> body (Append /
// FindFirstInstanceOf / GetItem + siblings) is fully inline in CgsArray.h, so this TU is
// just the explicit class instantiation (the X360 emits one out-of-line copy per using-TU):
//   Array<int,8>::Append           @ 0x823183D8  (OnlineRoundResults::SetPosition,
//                                                  ModeManager::SetupOnlineStartingGrid,
//                                                  ModeManager::SendModeStopMessages)
//   Array<int,8>::Append           @ 0x82317A10  (ModeManager::SetupPathfinding,
//                                                  Array<int,8>::AppendArray<8>)
//   Array<int,8>::FindFirstInstanceOf @ 0x823184F8 (ModeManager::SetupOnlineStartingGrid)
//   Array<int,8>::GetItem (non-const) @ 0x8254E1D0 (OnlineRoundResults::GetPosition)
//   Array<int,8>::GetItem             @ 0x827699F8 (RouteMapDebugComponent::DrawEventBlockSectionsInMap,
//                                                  RouteRequestManager::GenerateStandardRouteRequest)
// Layout: maElements[8] (32B) + miCount @ +0x20, matching the X360 result[8]/*(a1+32) count word.
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<int,8u>. int == s32 (types.hpp).
//
// Both out-of-line GetItem copies (0x8254E1D0, 0x827699F8) carry BOTH the unconstructed
// (CgsArray.h:556) and out-of-bounds (:557) asserts and route through the checked
// operator[]; the committed generic GetItem/operator[] body reproduces both, so it is
// already faithful (no per-instantiation specialization required).
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s32, 8>;
