#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"

// CgsModule::EventQueue<BrnPhysics::ContactSpy::HingedPartContact, 50>::Construct  @ 0x825A8DD0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (50) event queue
// instantiation: points the base queue at the inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnPhysics::ContactSpy::HingedPartContact, 50>::Construct();
