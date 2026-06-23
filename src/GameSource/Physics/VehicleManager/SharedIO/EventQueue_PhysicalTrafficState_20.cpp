#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

// CgsModule::EventQueue<BrnPhysics::Vehicle::PhysicalTrafficState, 20>::Construct  @ 0x8228DB90
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (20) event-queue
// instantiation: points the base queue at its inline maEvents buffer (self + 0x10,
// `addi r30, r31, 0x10`), sets the max length (`li r11, 0x14` => 20) and clears the
// live count (`li r10, 0; stw r10, 8(r31)`). The 0x10 buffer offset comes from the
// 12-byte BaseEventQueue<T> base (mpEvents@0, miMaxLength@4, miLength@8) padded out to
// the 16-byte alignment of the PhysicalTrafficState[20] inline storage.
template void CgsModule::EventQueue<BrnPhysics::Vehicle::PhysicalTrafficState, 20>::Construct();
