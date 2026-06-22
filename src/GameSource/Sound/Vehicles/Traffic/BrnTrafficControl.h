#ifndef BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_CONTROL_H
#define BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"

// =============================================================================
// BrnSound::Logic::Traffic::TrafficControl
//   GameSource/Sound/Vehicles/Traffic/BrnTrafficControl.h (DWARF home) +
//   GameSource/Sound/Vehicles/Traffic/BrnTrafficControl.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. TrafficControl is the sound-logic
// traffic effect *control*. DWARF (BrnTrafficControl.h:36):
//   struct BrnSound::Logic::Traffic::TrafficControl
//       : public BrnSound::Logic::BrnEffectControl
// so it inherits the COMMITTED BrnEffectControl dual base (EffectControl primary
// vptr @ this+0, IResourceRequester sub-object vptr @ this+4) reused BY NAME from
// GameSource/Sound/Module/LogicModule/BrnEffectControl.h.
//
// This TU's recon'd function set is exactly ONE entry:
//   `scalar deleting destructor'  @ 0x826B2580
// whose X360 teardown matches the BrnEffectControl shape EXACTLY:
//   *(this+0)    = off_820AEA6C      ; primary vptr (EffectControl path)
//   *(this+4)    = off_820AEA38      ; (transient) base IResourceRequester vptr
//   *(this+0x28) = 3                 ; meDetachState = E_DETACH_STATE_FINISHED
//   *(this+4)    = &off_820AA820     ; final IResourceRequester sub-object vptr
//   *(this+0x31) = 0                 ; mbResourcesReady = false
//   *(this+0x24) = 0                 ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { deallocate via the global sound allocator off_82FFB954 }
//   return this
// All of those members are owned by the committed BrnEffectControl base; the
// teardown is therefore produced by the inherited destructor chain.
//
// FLAG (shape vs full surface): this is a MINIMAL home for the boot-trace
// TrafficControl TU. The full DWARF surface (mpTrafficEntity, mpTraffic3dControl,
// GetTrafficEntity, the RTTI GetTypeInfo/GetTypeName/CreateObject hooks) is
// DEFERRED to its own TU(s); only the destructor is materialised here. The
// (a2 & 1) deallocation tail dispatches the global sound allocator (off_82FFB954);
// that allocator vtable is not homed here, so the `delete` half of the X360
// scalar deleting destructor is left to the host toolchain (same treatment as the
// BrnEffectControl / BrnEffectObject sibling homes).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the inherited BrnEffectControl
// teardown offsets (meDetachState @ +0x28, mbResourcesReady @ +0x31,
// meAttachState @ +0x24) are X360 byte offsets assuming 4-byte pointers/vptr;
// members are pinned BY NAME via the committed base and absolute offsets are NOT
// static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

// BrnTrafficControl.h:36 (DWARF). Reuses the committed BrnEffectControl base by
// name. The virtual (scalar/vector deleting) destructor @ 0x826B2580 runs the
// inherited BrnEffectControl member teardown.
struct TrafficControl : public BrnSound::Logic::BrnEffectControl
{
    TrafficControl() {}
    virtual ~TrafficControl();
};

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_CONTROL_H
