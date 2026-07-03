#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                          // BaseEventQueue<T>::AddEvent/Append (inline generic)
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"   // BrnWorld::PropEntityIO::TrafficLightKnockDownEvent (4-byte element)

// CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::TrafficLightKnockDownEvent>::AddEvent @ 0x822C8D78
// CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::TrafficLightKnockDownEvent>::Append   @ 0x827A8650
//
// Thin explicit instantiation forcing the per-element out-of-line emission the X360 build
// produced; the generic AddEvent/Append bodies are already inline in CgsBaseEventQueue.h. The
// 4-byte element stride is asm-attested (AddEvent `slwi r11,miLength,2; stwx` single 4-byte store;
// Append XMemCpy `slwi count,srcLen,2` == 4*count) and matches the committed
// TrafficLightKnockDownEvent struct (sizeof 4). Mirrors
// BaseEventQueue_TrafficLightRestoreEvent_AddEventAppend.cpp exactly.
//   AddEvent callers (X360): BrnWorld::PropEntityIO::PropToTrafficInterface::RequestTrafficLightKnockDown.
//   Append   callers (X360): WorldModule::BridgePropModuleToTrafficModule_PrePhysics.
template bool
CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::TrafficLightKnockDownEvent>::AddEvent(
    const BrnWorld::PropEntityIO::TrafficLightKnockDownEvent&);

template bool
CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::TrafficLightKnockDownEvent>::Append(
    const CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::TrafficLightKnockDownEvent>&);
