#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"

// CgsModule::EventQueue<BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent, 28>::Construct  @ 0x82336AA0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (28) event queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent, 28>::Construct();

// CgsModule::BaseEventQueue<BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent> methods
// (bodies inline in CgsBaseEventQueue.h).
//   AddEvent  @ 0x823681D8   (16B element stride == sizeof(DirtyTrickEvent), 4x s32 enums)
//   Append    @ 0x82325830
template bool CgsModule::BaseEventQueue<BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent>::AddEvent(const BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent&);
template bool CgsModule::BaseEventQueue<BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent>::Append(const CgsModule::BaseEventQueue<BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent>&);
