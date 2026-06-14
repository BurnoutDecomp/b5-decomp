#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyRunList.h"

// CgsModule::EventQueue<BrnPhysics::ContactSpy::ContactSpyRunData, 64>::Construct  @ 0x825A8F20
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (64) event queue
// instantiation: points the base queue at the inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnPhysics::ContactSpy::ContactSpyRunData, 64>::Construct();
