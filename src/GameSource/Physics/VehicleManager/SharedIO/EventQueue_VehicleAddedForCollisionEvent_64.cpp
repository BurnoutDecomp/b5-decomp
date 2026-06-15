#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

// CgsModule::EventQueue<BrnPhysics::Vehicle::VehicleAddedForCollisionEvent, 64>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (64) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length, and clears the live count.
template void CgsModule::EventQueue<BrnPhysics::Vehicle::VehicleAddedForCollisionEvent, 64>::Construct();
