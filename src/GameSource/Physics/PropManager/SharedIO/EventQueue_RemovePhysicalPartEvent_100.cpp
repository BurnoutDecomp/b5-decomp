#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"

// CgsModule::EventQueue<BrnPhysics::Props::RemovePhysicalPartEvent, 100>::Construct  @ 0x822E3830
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (100) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
//
// ASM (store-for-store):
//   addi r30,r31,0xC    -> lpEventBuffer = &maEvents (base+0xC: RemovePhysicalPartEvent
//                          is an 8-byte / 4-aligned record -- PropEntityID(4)+int32(4) --
//                          so the inline maEvents array directly follows the 12-byte
//                          BaseEventQueue base with no alignment padding)
//   cmplwi/bne          -> BaseEventQueue<T>::Construct assert lpEventBuffer != NULL
//                          (CgsBaseEventQueue.h:160 -> CGS_ASSERT "lpEventBuffer != NULL")
//   li r11,0x64 ; stw r11,4(r31)  -> miMaxLength = KI_LENGTH = 100
//   stw r30,0(r31)                -> mpEvents = lpEventBuffer
//   li r10,0 ; stw r10,8(r31)     -> miLength = 0
template void CgsModule::EventQueue<BrnPhysics::Props::RemovePhysicalPartEvent, 100>::Construct();
