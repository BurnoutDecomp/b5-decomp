#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "SharedClasses/Trigger/BrnTriggerData.h"   // complete BrnTrigger::TriggerData

// Per-instantiation TU for CgsResource::ResourcePtr<BrnTrigger::TriggerData>.
// Bodies are the generic inline accessors in CgsResourcePtr.h; this .cpp only forces
// the out-of-line emission of the three symbols the X360 ARTIST build attested.
//
//   operator->()        @ 0x823685E8  (baked assert line 544, non-const)
//   operator->() const  @ 0x82211DA0  (baked assert line 563)
//   GetMemoryResource() const @ 0x82211E40  (baked assert line 599)
template BrnTrigger::TriggerData*       CgsResource::ResourcePtr<BrnTrigger::TriggerData>::operator->();
template const BrnTrigger::TriggerData* CgsResource::ResourcePtr<BrnTrigger::TriggerData>::operator->() const;
template const BrnTrigger::TriggerData* CgsResource::ResourcePtr<BrnTrigger::TriggerData>::GetMemoryResource() const;
