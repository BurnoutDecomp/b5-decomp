#include "GameSource/Sound/Vehicles/Brn3dCarPosition.h"

// =============================================================================
// BrnSound::Vehicles::Car3DControl (+ Engine/Exhaust/LeftSide/RightSide3dControl)
// — out-of-line deleting-destructor bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See Brn3dCarPosition.h for the
// inheritance rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// The five recon'd deleting destructors share one X360 teardown shape:
//   bl   Attrib::Instance::~Instance(this + 0xB0)  ; destroy mEngineDataAtrib
//   li   r9, 3 ; stw r9, 0x24(this)                ; meDetachState = FINISHED
//   stw  &off_820AA820, 0(this)                    ; primary vptr settle
//   stb  0, 0x2D(this)                             ; control bookkeeping flag = false
//   stw  0, 0x20(this)                             ; mfDeltaTime = 0
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
//
// The Attrib::Instance member teardown (mEngineDataAtrib) is produced by the
// inherited ~Brn3DEffectControl chain, which destroys the member via the committed
// Attrib::Instance::~Instance. The remaining stores settle members owned by the
// inherited bases, so each leaf destructor body adds nothing of its own.
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left
// to the host toolchain (the `delete` half of the X360 deleting destructor) rather
// than reproducing the raw allocator vtable call.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

// ~Car3DControl  @ 0x826CF838  (the X360 `scalar deleting destructor')
Car3DControl::~Car3DControl()
{
}

// ~Engine3dControl  @ 0x826E4D70  (the X360 `vector deleting destructor')
Engine3dControl::~Engine3dControl()
{
}

// ~Exhaust3dControl  @ 0x826E4E18  (the X360 `scalar deleting destructor')
Exhaust3dControl::~Exhaust3dControl()
{
}

// ~LeftSide3dControl  @ 0x826E4EC0  (the X360 `scalar deleting destructor')
LeftSide3dControl::~LeftSide3dControl()
{
}

// ~RightSide3dControl  @ 0x826E4F68  (the X360 `vector deleting destructor')
RightSide3dControl::~RightSide3dControl()
{
}

} // namespace Vehicles
} // namespace BrnSound
