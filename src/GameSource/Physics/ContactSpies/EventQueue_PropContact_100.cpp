#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"

// CgsModule::EventQueue<BrnPhysics::ContactSpy::PropContact, 100>::Construct  @ 0x825A8E40
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (100) event queue
// instantiation: points the base queue at the inline maEvents buffer, sets the
// max length (li r11,0x64=100; stw r11,4(this)), and clears the live count.
template void CgsModule::EventQueue<BrnPhysics::ContactSpy::PropContact, 100>::Construct();
