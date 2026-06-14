#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"

// CgsModule::EventQueue<BrnPhysics::ContactSpy::DiscardedContact, 20>::Construct  @ 0x825A8068
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (20) event queue
// instantiation: points the base queue at the inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnPhysics::ContactSpy::DiscardedContact, 20>::Construct();
