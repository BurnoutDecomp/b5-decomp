#ifndef BRN_SOUND_VEHICLES_ENVIRONMENT_BRN_SPEED_STREAM_CONTROL_H
#define BRN_SOUND_VEHICLES_ENVIRONMENT_BRN_SPEED_STREAM_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"

// =============================================================================
// BrnSound::Vehicles::Environment::SpeedStreamControl
//   GameSource/Sound/Vehicles/Environment/BrnSpeedStreamControl.h (DWARF home) +
//   GameSource/Sound/Vehicles/Environment/BrnSpeedStreamControl.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. SpeedStreamControl is a vehicle
// environment (speed-driven streaming) sound-logic effect object. The X360 vector
// deleting destructor @ 0x826BA0A0 proves it multiply-inherits (via BrnEffectObject)
// the engine effect-object base (primary vptr @ this+0) and IResourceRequester
// (sub-object vptr @ this+4): the teardown writes the same dual-vptr settle and the
// same attach/detach/resources-ready member clears as the committed BrnEffectObject
// destructor @ 0x826AF4C8 and the sibling LoopModelEffect @ 0x826AFBA0 — only the
// leaf vtable symbols differ (off_820AEA6C / off_820AEA38 here vs off_820AE9BC /
// off_820AE988 there; both settle +4 to the shared base off_820AA820).
//
// This TU's recon'd function set is exactly ONE entry:
//   `vector deleting destructor'  @ 0x826BA0A0
// whose X360 teardown (verified store-for-store against the assembly) is:
//   stw  &off_820AEA6C, 0(this)        ; primary vptr settle (SpeedStreamControl vtable)
//   stw  &off_820AEA38, 4(this)        ; IResourceRequester sub-object vptr (intermediate)
//   stw  3,            0x28(this)      ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  &off_820AA820, 4(this)        ; IResourceRequester sub-object vptr (final settle,
//                                        the shared base vtable used by BrnEffectObject)
//   stb  0,            0x31(this)      ; mbResourcesReady = false
//   stw  0,            0x24(this)      ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
// The two vptr stores at +0/+4 and the member clears at +0x28/+0x31/+0x24 are the
// BrnEffectObject dual-base teardown the compiler emits, so the leaf destructor body
// adds nothing of its own. The asm writes NO data-member store beyond the inherited
// base set, so no SpeedStreamControl-specific field teardown is modelled here.
//
// FLAG (shape vs full surface): MINIMAL home for the boot-trace deleting-destructor
// TU. The full DWARF surface (RTTI GetTypeInfo/GetTypeName/CreateObject/
// GetStaticTypeInfo; the speed-stream control's own params/update hooks and any data
// members) is DEFERRED to its own TU(s). Only the inheritance spine needed to settle
// the destructor is materialised here. The (a2 & 1) deallocation tail dispatches the
// global sound allocator (off_82FFB954), not homed here, so the `delete` half is left
// to the host toolchain (same treatment as the committed BrnEffectObject /
// LoopModelEffect siblings).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 offsets (meDetachState @ +0x28,
// mbResourcesReady @ +0x31, meAttachState @ +0x24, IResourceRequester sub-object vptr
// @ +4) assume 4-byte pointers/vptr; members are pinned BY NAME via the inherited
// BrnEffectObject bases and absolute offsets are NOT static_asserted across pointer
// members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

// Reuses the BrnEffectObject dual base by name; the virtual `vector deleting
// destructor' @ 0x826BA0A0 runs the inherited BrnEffectObject teardown (both vptr
// settles + the attach/detach/resources-ready member clears).
struct SpeedStreamControl : public BrnSound::Logic::BrnEffectControl
{
    SpeedStreamControl() {}
    virtual ~SpeedStreamControl();
};

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENVIRONMENT_BRN_SPEED_STREAM_CONTROL_H
