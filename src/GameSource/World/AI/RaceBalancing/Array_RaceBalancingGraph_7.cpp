// Per-instantiation .cpp for Array<BrnAI::RaceBalancingGraph, 7>. The generic
// Array<T,N>::Append / GetItem bodies are fully inline in CgsArray.h; this TU is the
// thin explicit member instantiation only (the X360 emits one out-of-line copy of each
// accessor per using-TU). Do NOT re-define the generic. The methods the X360 emitted
// for this instance:
//   Array<RaceBalancingGraph,7>::Append              @0x8276A238
//       (callers: Array<RaceBalancingGraph,7>::AppendArray<7>,
//        BrnAI::AIModule::SetupRaceBalancingManager)
//   Array<RaceBalancingGraph,7>::GetItem (non-const) @0x8276A6F0
//       (callers: AppendArray<7>, BrnAI::RaceBalancingManager::ComputeParSpeed)
//   Array<RaceBalancingGraph,7>::GetItem (const)     @0x8276A5E8
//       (callers: BrnAI::RaceBalancingManager::UpdateOpponentRoute,
//        BrnAI::RaceBalancingDebugComponent::RenderHUD)
//
// All three match the generic store-for-store. Append (CgsArray.h:167/225/226) asserts the
// array was Construct/Clear'd (count word @ +0x1C0 != -1 sentinel), asserts room (count < 7;
// capacity literal li r4,7), memcpy's the 0x40-byte element into &maElements[count], then
// post-increments count. Both GetItem bodies forward to the checked operator[]
// (CgsArray.h:538/539 non-const, 556/557 const). The element stride is 0x40 (li r5,0x40 memcpy,
// slwi <<6 every accessor) and the count word sits at +0x1C0 == 7*0x40, attesting
// sizeof(RaceBalancingGraph)==0x40 and N==7.
//
// Spelled unqualified to match the committed Array<T,N> convention (the DWARF spells the type
// CgsContainers::Array<...,7u>). Per-method instantiation (RaceBalancingGraph has no operator==,
// so the equality-based generic members are deliberately left un-instantiated) mirrors the
// CgsArrayBankingScore6.cpp / Array_OpponentData_7.cpp precedent.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingGraph.h"

template void                             Array<BrnAI::RaceBalancingGraph, 7>::Append(const BrnAI::RaceBalancingGraph&);
template BrnAI::RaceBalancingGraph&       Array<BrnAI::RaceBalancingGraph, 7>::GetItem(u32);
template const BrnAI::RaceBalancingGraph& Array<BrnAI::RaceBalancingGraph, 7>::GetItem(u32) const;
