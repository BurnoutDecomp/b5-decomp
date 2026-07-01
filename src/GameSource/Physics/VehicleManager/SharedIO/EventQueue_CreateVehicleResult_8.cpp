#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

// CgsModule::EventQueue<BrnPhysics::Vehicle::CreateVehicleResult, 8>::Construct @ 0x822E3280
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (8) event-queue instantiation:
// points the base queue at its inline maEvents buffer (buffer at +0x10, the asm's
// addi r30,r31,0x10), sets the max length (8 == li r11,8; stw r11,4(r31)) and clears the live
// count (stw r10(=0),8(r31)). The lpEventBuffer!=NULL tripwire (CgsBaseEventQueue.h:160) is
// reproduced by BaseEventQueue<T>::Construct. Called by
// BrnPhysics::Vehicle::VehicleManagerOutputInterface::Construct. Element stride 16.
template void CgsModule::EventQueue<BrnPhysics::Vehicle::CreateVehicleResult, 8>::Construct();
