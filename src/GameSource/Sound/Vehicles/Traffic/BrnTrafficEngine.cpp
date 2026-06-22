#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficEngine.h"

// =============================================================================
// BrnSound::Logic::Traffic::Traffic3DControl — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnTrafficEngine.h for the
// inheritance rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// This TU's recon'd function set is exactly ONE entry:
//   `vector deleting destructor'  @ 0x826E3000
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

// ---------------------------------------------------------------------------
// ~Traffic3DControl  @ 0x826E3000  (the X360 `vector deleting destructor')
//
//   bl   Attrib::Instance::~Instance(this + 0xB0)  ; destroy mEngineDataAtrib
//   li   r9, 3 ; stw r9, 0x24(this)                ; meDetachState = FINISHED
//   stw  &off_820AA820, 0(this)                    ; primary vptr settle
//   stb  0, 0x2D(this)                             ; control bookkeeping flag = false
//   stw  0, 0x20(this)                             ; mfDeltaTime = 0
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
//
// The Attrib::Instance member teardown (mEngineDataAtrib) is produced by the
// inherited ~Brn3DEffectControl destructor chain, which destroys the member via
// the committed Attrib::Instance::~Instance. The remaining stores settle members
// owned by the inherited bases, so this leaf destructor body adds nothing of its
// own (identical in shape to the committed Passby3DControl destructor).
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to
// free the object; that allocator is not homed here, so operator-delete dispatch
// is left to the host toolchain (the `delete` half of the X360 vector deleting
// destructor) rather than reproducing the raw allocator vtable call.
// ---------------------------------------------------------------------------
Traffic3DControl::~Traffic3DControl()
{
}

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound
