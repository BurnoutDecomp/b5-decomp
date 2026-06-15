#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"

// CgsModule::EventQueue<BrnPhysics::Deformation::DetachedPartNotificationEvent, 50>::Construct  @ 0x8228DC70
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (50) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnPhysics::Deformation::DetachedPartNotificationEvent, 50>::Construct();
