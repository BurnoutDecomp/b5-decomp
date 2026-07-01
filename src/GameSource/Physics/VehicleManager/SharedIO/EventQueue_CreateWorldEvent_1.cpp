#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

// CgsModule::EventQueue<BrnPhysics::Vehicle::CreateWorldEvent, 1>::Construct @ 0x825A8148
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (1) event-queue instantiation:
// points the base queue at its inline maEvents buffer (buffer at +0x10, the asm's
// addi r30,r31,0x10), sets the max length (1 == li r11,1; stw r11,4(r31)) and clears the live
// count (stw r10(=0),8(r31)). The lpEventBuffer!=NULL tripwire (CgsBaseEventQueue.h:160) is
// reproduced by BaseEventQueue<T>::Construct. Called by
// BrnPhysics::PhysicsModuleIO::InputBuffer::Construct. Element stride 80.
template void CgsModule::EventQueue<BrnPhysics::Vehicle::CreateWorldEvent, 1>::Construct();
