#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"

// CgsModule::EventQueue<BrnPhysics::Props::AddPhysicalPartEvent, 50>::Construct  @ 0x822E3750
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (50) event-queue
// instantiation: points the base queue at its inline maEvents buffer (base+0x10,
// the asm's `addi r30,r31,0x10`), sets the max length (50 == 0x32) and clears the
// live count. lpEventBuffer!=NULL tripwire (CgsBaseEventQueue.h:160) is intrinsic to
// the base Construct. Element stride 80 attested by the sibling
// BaseEventQueue<AddPhysicalPartEvent>::AddEvent @ 0x822C8498 (slwi r9,r11,2; add
// r11,r11,r9; slwi r11,r11,4 == idx*5*16 == idx*80); field copy Matrix44Affine@0(64B)
// + PropEntityID@0x40(4B) + three s16 @0x44/0x46/0x48 -> AddPhysicalPartEvent sized to
// exactly 80 by its alignas(16). Thin explicit instantiation only.
template void CgsModule::EventQueue<BrnPhysics::Props::AddPhysicalPartEvent, 50>::Construct();
