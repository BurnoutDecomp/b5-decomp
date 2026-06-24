#include "GameSource/Sound/Vehicles/Environment/BrnSpeedStreamControl.h"

// =============================================================================
// BrnSound::Vehicles::Environment::SpeedStreamControl — out-of-line deleting-
// destructor body.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnSpeedStreamControl.h for the
// inheritance rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// ~SpeedStreamControl  @ 0x826BA0A0  (the X360 `vector deleting destructor')
//   stw  &off_820AEA6C, 0(this)        ; primary vptr settle
//   stw  &off_820AEA38, 4(this)        ; IResourceRequester sub-object vptr (intermediate)
//   stw  3,            0x28(this)      ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  &off_820AA820, 4(this)        ; IResourceRequester sub-object vptr (final settle)
//   stb  0,            0x31(this)      ; mbResourcesReady = false
//   stw  0,            0x24(this)      ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
//
// The dual-vptr settle and the attach/detach/resources-ready member clears are the
// inherited BrnEffectObject teardown the compiler emits; this leaf destructor body
// adds nothing of its own.
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left
// to the host toolchain (the `delete` half of the X360 deleting destructor) rather
// than reproducing the raw allocator vtable call.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

SpeedStreamControl::~SpeedStreamControl()
{
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound
