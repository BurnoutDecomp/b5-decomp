#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"   // BaseEventQueue<T>::Append/AddEvent (inline generic)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"

// CgsModule::EventQueue<BrnNetwork::RoadRulesRecvData, 14>::Construct  @ 0x82373A00
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (14) event queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnNetwork::RoadRulesRecvData, 14>::Construct();

// BaseEventQueue<RoadRulesRecvData>::AddEvent @0x8254D6C8 / ::Append @0x823C43F8.
// Generic bodies are inline in CgsBaseEventQueue.h; thin explicit instantiations
// (264-byte element stride; the symbols live on BaseEventQueue<T>).
template bool CgsModule::BaseEventQueue<BrnNetwork::RoadRulesRecvData>::AddEvent(const BrnNetwork::RoadRulesRecvData&);
template bool CgsModule::BaseEventQueue<BrnNetwork::RoadRulesRecvData>::Append(const CgsModule::BaseEventQueue<BrnNetwork::RoadRulesRecvData>&);
