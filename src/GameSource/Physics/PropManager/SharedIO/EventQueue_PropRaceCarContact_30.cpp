#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"

// CgsModule::EventQueue<BrnPhysics::Props::PropRaceCarContact, 30>::Construct  @ 0x825A81B8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (30) event-queue
// instantiation: points the base queue at its inline maEvents buffer (base+0x10,
// the asm's `addi r30,r31,0x10`), sets the max length (30 == 0x1E) and clears the
// live count. Capacity 30 == KI_MAX_PROP_RACE_CAR_CONTACTS (BrnPropManager.h:32) and
// the DWARF typedef EventQueue<PropRaceCarContact,30> (BrnPropManager.h:62). This queue
// is the sole member of PropRaceCarContactBuffer (an IOBuffer); the only X360 caller is
// IOBufferStack::CreateIOBuffer<PropRaceCarContactBuffer>.
//
// Element PropRaceCarContact is DWARF-attested { Vector3 mForce; PropEntityID
// mPropEntityId; } (BrnPropManager.h:56-59). Vector3 is alignas(16), so the record is
// 16-aligned: 16B + 4B == 20 used, padded to a 32-byte stride. NOTE: no AddEvent/Append
// TU was available to attest the memcpy stride directly; 32 is derived from the DWARF
// layout + alignment. If a later AddEvent/Append TU attests a different stride, resize.
// The element PropRaceCarContact is homed alongside its prop-event siblings in
// BrnPropEvents.h (its DWARF home BrnPropManager.h is a locked BLOCKED stub). Thin
// explicit instantiation only.
template void CgsModule::EventQueue<BrnPhysics::Props::PropRaceCarContact, 30>::Construct();
