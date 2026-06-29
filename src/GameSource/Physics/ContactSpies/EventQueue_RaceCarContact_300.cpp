#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"

// CgsModule::EventQueue<BrnPhysics::ContactSpy::RaceCarContact, 300>::Construct  @ 0x825A8C80
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (300) event queue
// instantiation: points the base queue at the inline maEvents buffer (this+0x10,
// stw r30,0(r31)), sets the max length (li r11,0x12C=300; stw r11,4(r31)), and
// clears the live count (stw 0,8(r31)).
template void CgsModule::EventQueue<BrnPhysics::ContactSpy::RaceCarContact, 300>::Construct();
