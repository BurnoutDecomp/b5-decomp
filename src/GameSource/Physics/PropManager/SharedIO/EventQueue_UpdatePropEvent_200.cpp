#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"

// CgsModule::EventQueue<BrnPhysics::Props::UpdatePropEvent, 200>::Construct  @ 0x822E38A0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (200) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnPhysics::Props::UpdatePropEvent, 200>::Construct();
