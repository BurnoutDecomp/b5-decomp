#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // BaseEventQueue<T>::AddEvent/AddEventSafe/Append (inline generic)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h" // BrnTraffic::BrnTrafficIO::ActivateHullEvent (12-byte element)

// CgsModule::BaseEventQueue<BrnTraffic::BrnTrafficIO::ActivateHullEvent>::AddEvent     @ 0x8271A128
// CgsModule::BaseEventQueue<BrnTraffic::BrnTrafficIO::ActivateHullEvent>::AddEventSafe @ 0x82558358
// CgsModule::BaseEventQueue<BrnTraffic::BrnTrafficIO::ActivateHullEvent>::Append       @ 0x823C3E88
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent /
// ::AddEventSafe / ::Append bodies are already inline in CgsBaseEventQueue.h; these are the
// thin explicit instantiations. The X360 element stride is 12 bytes -- now matched exactly:
//   AddEvent/AddEventSafe write 3 dwords per element (*v = *src; v[1] = src[1]; v[2] = src[2])
//   Append's XMemCpy copies 12 * srcLen bytes
// matching sizeof(ActivateHullEvent) == EActiveRaceCarIndex(4) + u16 + pad(2) + u32 == 12.
template bool
CgsModule::BaseEventQueue<BrnTraffic::BrnTrafficIO::ActivateHullEvent>::AddEvent(const BrnTraffic::BrnTrafficIO::ActivateHullEvent&);

template bool
CgsModule::BaseEventQueue<BrnTraffic::BrnTrafficIO::ActivateHullEvent>::AddEventSafe(const BrnTraffic::BrnTrafficIO::ActivateHullEvent&);

template bool
CgsModule::BaseEventQueue<BrnTraffic::BrnTrafficIO::ActivateHullEvent>::Append(const CgsModule::BaseEventQueue<BrnTraffic::BrnTrafficIO::ActivateHullEvent>&);
