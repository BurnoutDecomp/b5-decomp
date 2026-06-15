#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"

// CgsModule::EventQueue<BrnPhysics::Props::UpdatePropEvent, 15>::Construct  @ 0x82614E68
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (15) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnPhysics::Props::UpdatePropEvent, 15>::Construct();
