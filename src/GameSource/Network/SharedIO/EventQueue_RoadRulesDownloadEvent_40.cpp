#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"

// CgsModule::EventQueue<BrnNetwork::RoadRulesDownloadEvent, 40>::Construct  @ 0x82373A70
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (40) event queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnNetwork::RoadRulesDownloadEvent, 40>::Construct();

// CgsModule::BaseEventQueue<BrnNetwork::RoadRulesDownloadEvent>::GetEvent(s32) const  @ 0x82318840
//   (called by BrnGameState::StreetManager::UpdateFriendHighScores). The generic const GetEvent
//   body is inline in CgsBaseEventQueue.h (asserts mpEvents != NULL / index bounds, returns
//   mpEvents[liIndex]). The X360 return `mulli r11,r29,0x38` == index*sizeof(T) with
//   sizeof(RoadRulesDownloadEvent) == 56. Thin out-of-line instantiation.
template const BrnNetwork::RoadRulesDownloadEvent&
CgsModule::BaseEventQueue<BrnNetwork::RoadRulesDownloadEvent>::GetEvent(s32) const;
