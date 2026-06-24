// Per-instantiation .cpp for Array<BrnAI::RaceBalancingRoute, 7>. The generic
// Array<T,N>::GetItem / operator[] bodies are fully inline in CgsArray.h; this
// TU is the thin explicit member instantiation only (the X360 emits one
// out-of-line indexed accessor per using-TU). Do NOT re-define the generic.
//
//   Array<RaceBalancingRoute,7>::GetItem (non-const) @0x8276A900
//       (callers: RaceBalancingManager::CalculateScheduleOffset / ComputeParSpeed /
//        ComputeTargetSpeed, sub_82794AA0)
//   Array<RaceBalancingRoute,7>::GetItem (const)     @0x8276A7F8
//       (callers: RaceBalancingManager::OnRaceStart / UpdateOpponentRoute /
//        OnOpponentReachedCheckpoint, AIModule::OnPlayerTakedown,
//        RaceBalancingDebugComponent::RenderHUD)
//
// Both bodies match the generic store-for-store: assert the array was
// Construct/Clear'd (miCount @ +0x46A8 != the -1 sentinel, CgsArray.h:538/556 ->
// "Array used before Construct/Clear was called"), bounds-check the index
// (unsigned luIndex < miCount, CgsArray.h:539/557 -> the streamed "Array index
// out of bounds. Index: <i>, length: <n>"), then return &maElements[luIndex].
// The X360 reads the element stride as 0xA18 (2584) and the count word at
// +0x46A8 == 7*0xA18, attesting sizeof(RaceBalancingRoute)==0xA18 and N==7.
//
// Spelled unqualified to match the committed Array<T,N> container convention
// (CgsArray.h); the DWARF spells the type CgsContainers::Array<...,7u>.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingRoute.h"

template BrnAI::RaceBalancingRoute&
Array<BrnAI::RaceBalancingRoute, 7>::GetItem(u32);
template const BrnAI::RaceBalancingRoute&
Array<BrnAI::RaceBalancingRoute, 7>::GetItem(u32) const;
