#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h"

// Per-instantiation .cpp for the BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent event
// queue. The generic bodies are inline in CgsBaseEventQueue.h; this TU is the thin explicit
// instantiation, mirroring committed siblings (EventQueue_TakedownEvent_8.cpp / EventQueue_
// TrafficTypeResponse_32.cpp). Element stride == 4 (EBO over the empty CgsModule::Event base
// + the 4-byte TriggerId), matching the X360 4*miLength addressing.
//   AddEvent  @ 0x823250E8
//   Append    @ 0x823C3F78
//   Construct @ 0x822E4C70 -- the derived EventQueue<T,256>::Construct: points the base queue at
//             the inline maEvents buffer, sets miMaxLength = 256, clears miLength. The element
//             is 4-byte aligned so maEvents lands at this+0xC (12-byte BaseEventQueue base, no
//             pad), matching `addi r30, r31, 0xC` / `stw 0x100 (=256), 4` / `stw 0, 8`. Callers:
//             InputBuffer_PreScene::Construct, GameStateModuleIO::OutputBuffer::Construct,
//             BrnTrafficIO::OutputBuffer_PreScene::Construct, BrnWorldIO::UpdateInputBuffer::Construct.
template bool CgsModule::BaseEventQueue<BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent>::AddEvent(const BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent&);
template bool CgsModule::BaseEventQueue<BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent>::Append(const CgsModule::BaseEventQueue<BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent>&);
template void CgsModule::EventQueue<BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent, 256>::Construct();
