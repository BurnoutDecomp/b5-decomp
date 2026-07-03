#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"

// CgsModule::EventQueue<BrnPhysics::Props::PropUpdateNotification, 200>::Construct  @ 0x825A80D8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (200) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
//
// ASM (store-for-store):
//   addi r30,r31,0x10   -> lpEventBuffer = &maEvents (base+0x10: PropUpdateNotification
//                          is alignas(16)/64-byte, so the 12-byte BaseEventQueue base is
//                          padded to a 16-byte boundary before the inline maEvents array)
//   cmplwi/bne          -> BaseEventQueue<T>::Construct assert lpEventBuffer != NULL
//                          (CgsBaseEventQueue.h:160 -> CGS_ASSERT "lpEventBuffer != NULL")
//   li r11,0xC8 ; stw r11,4(r31)  -> miMaxLength = KI_LENGTH = 200
//   stw r30,0(r31)                -> mpEvents = lpEventBuffer
//   li r10,0 ; stw r10,8(r31)     -> miLength = 0
template void CgsModule::EventQueue<BrnPhysics::Props::PropUpdateNotification, 200>::Construct();
