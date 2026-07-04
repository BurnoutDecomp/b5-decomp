#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"   // BaseEventQueue<T>::Append/AddEvent (inline generic)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"

// CgsModule::EventQueue<BrnNetwork::RoadRulesMessageData, 40>::Construct  @ 0x82373AE0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (40) event queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnNetwork::RoadRulesMessageData, 40>::Construct();

// BaseEventQueue<RoadRulesMessageData>::AddEvent @0x82557D38 / ::Append @0x823C45B8.
// Generic bodies are inline in CgsBaseEventQueue.h; these are the thin explicit
// instantiations (24-byte element stride; the symbols live on BaseEventQueue<T>).
template bool CgsModule::BaseEventQueue<BrnNetwork::RoadRulesMessageData>::AddEvent(const BrnNetwork::RoadRulesMessageData&);
template bool CgsModule::BaseEventQueue<BrnNetwork::RoadRulesMessageData>::Append(const CgsModule::BaseEventQueue<BrnNetwork::RoadRulesMessageData>&);

// BaseEventQueue<RoadRulesMessageData>::GetEvent(s32) const  @ 0x823188E8
//   (called by BrnGameState::StreetManager::UpdateUserScoresFromServerRecords). Generic const
//   GetEvent inline in CgsBaseEventQueue.h; the X360 return `slwi ,1; add; slwi ,3` == index*24 ==
//   index*sizeof(RoadRulesMessageData). Thin out-of-line instantiation.
template const BrnNetwork::RoadRulesMessageData&
CgsModule::BaseEventQueue<BrnNetwork::RoadRulesMessageData>::GetEvent(s32) const;
