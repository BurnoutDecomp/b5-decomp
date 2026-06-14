#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"

// CgsModule::EventQueue<BrnNetwork::RoadRulesMessageData, 40>::Construct  @ 0x82373AE0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (40) event queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnNetwork::RoadRulesMessageData, 40>::Construct();
