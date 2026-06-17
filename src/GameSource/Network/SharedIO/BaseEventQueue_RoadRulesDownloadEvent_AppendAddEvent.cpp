#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"   // BaseEventQueue<T>::Append/AddEvent (inline generic)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"     // BrnNetwork::RoadRulesDownloadEvent (56-byte element)

// CgsModule::BaseEventQueue<BrnNetwork::RoadRulesDownloadEvent>::Append   @ 0x823C44D8
// CgsModule::BaseEventQueue<BrnNetwork::RoadRulesDownloadEvent>::AddEvent @ 0x82557BE0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::Append /
// ::AddEvent bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit
// instantiations. The X360 element stride is 56 bytes (Append XMemCpy's 56*srcLen; AddEvent's
// element assignment is the 7-qword copy) -- now matched exactly: sizeof(RoadRulesDownloadEvent)
// == 2*sizeof(PlayerName 16) + sizeof(RoadRulesMessageData 24) == 56.
template bool
CgsModule::BaseEventQueue<BrnNetwork::RoadRulesDownloadEvent>::AddEvent(const BrnNetwork::RoadRulesDownloadEvent&);

template bool
CgsModule::BaseEventQueue<BrnNetwork::RoadRulesDownloadEvent>::Append(const CgsModule::BaseEventQueue<BrnNetwork::RoadRulesDownloadEvent>&);
