#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h"

// Per-instantiation .cpp for the BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent event
// queue. The generic bodies are inline in CgsBaseEventQueue.h; this TU is the thin explicit
// instantiation, mirroring committed siblings (EventQueue_TakedownEvent_8.cpp / EventQueue_
// TrafficTypeResponse_32.cpp). Element stride == 4 (EBO over the empty CgsModule::Event base
// + the 4-byte TriggerId), matching the X360 4*miLength addressing.
//   AddEvent  @ 0x823250E8
//   Append    @ 0x823C3F78
template bool CgsModule::BaseEventQueue<BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent>::AddEvent(const BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent&);
template bool CgsModule::BaseEventQueue<BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent>::Append(const CgsModule::BaseEventQueue<BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent>&);
