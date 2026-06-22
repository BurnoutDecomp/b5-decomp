#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficControl.h"

// =============================================================================
// BrnSound::Logic::Traffic::TrafficControl — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnTrafficControl.h for the
// inheritance rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// This TU's recon'd function set is exactly ONE entry:
//   `scalar deleting destructor'  @ 0x826B2580
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

// ---------------------------------------------------------------------------
// ~TrafficControl  @ 0x826B2580  (the X360 `scalar deleting destructor')
//
//   stw  off_820AEA6C, 0(this)     ; primary vptr (EffectControl path)
//   stw  off_820AEA38, 4(this)     ; (transient) base IResourceRequester vptr
//   li   r7, 3 ; stw r7, 0x28(this) ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  off_820AA820, 4(this)     ; final IResourceRequester sub-object vptr
//   stb  0, 0x31(this)             ; mbResourcesReady = false
//   stw  0, 0x24(this)             ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
//
// Every stored member (meDetachState @ +0x28, mbResourcesReady @ +0x31,
// meAttachState @ +0x24) is owned by the committed BrnEffectControl base, so the
// observable teardown is produced by the inherited ~BrnEffectControl destructor
// chain; this leaf destructor body adds nothing of its own.
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to
// free the object; that allocator is not homed here, so operator-delete dispatch
// is left to the host toolchain (the `delete` half of the X360 scalar deleting
// destructor) rather than reproducing the raw allocator vtable call.
// ---------------------------------------------------------------------------
TrafficControl::~TrafficControl()
{
}

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound
