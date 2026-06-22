#ifndef BRN_SOUND_LOGIC_PASSBY_BRN_PASSBY_EFFECT_H
#define BRN_SOUND_LOGIC_PASSBY_BRN_PASSBY_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/Brn3DEffectControl.h"

// =============================================================================
// BrnSound::Logic::Passby::Passby3DControl
//   GameSource/Sound/Passby/BrnPassbyEffect.h (DWARF home) +
//   GameSource/Sound/Passby/BrnPassbyEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Passby3DControl is the 3D-positional
// sound-logic control for vehicle pass-bys. DWARF (BrnPassbyEffect.h:33):
//   struct BrnSound::Logic::Passby::Passby3DControl
//       : public BrnSound::Logic::Brn3DEffectControl
// so it inherits the (minimally-homed) Brn3DEffectControl base, which owns the
// Attrib::Instance member (mEngineDataAtrib) the destructor tears down.
//
// This TU's recon'd function set is exactly ONE entry:
//   `scalar deleting destructor'  @ 0x826E8ED0
// whose X360 teardown is:
//   Attrib::Instance::~Instance(this + 176);  ; destroy mEngineDataAtrib
//   *(this+0x24) = 3                          ; meDetachState = E_DETACH_STATE_FINISHED
//   *(this+0)    = &off_820AA820              ; primary vptr settle
//   *(this+0x2D) = 0                          ; (control bookkeeping flag) = false
//   *(this+0x20) = 0                          ; mfDeltaTime = 0
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
// The Attrib::Instance member teardown is produced by the inherited
// Brn3DEffectControl destructor chain (which destroys mEngineDataAtrib via the
// committed Attrib::Instance::~Instance); the remaining stores settle base members.
//
// FLAG (shape vs full surface): this is a MINIMAL home for the boot-trace
// Passby3DControl TU. The full DWARF surface (the RTTI GetTypeInfo/GetTypeName/
// CreateObject/GetStaticTypeInfo hooks, the sibling PassbyEffect : BrnEffectObject
// and its mpPassby3DControl link) is DEFERRED to its own TU(s); only the destructor
// is materialised here. The (a2 & 1) deallocation tail dispatches the global sound
// allocator (off_82FFB954); that allocator vtable is not homed here, so the
// `delete` half of the X360 scalar deleting destructor is left to the host
// toolchain (same treatment as the BrnEffectControl / BrnEffectObject siblings).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 offsets (mEngineDataAtrib @
// +176, meDetachState @ +0x24, mfDeltaTime @ +0x20, +0x2D bookkeeping flag) assume
// 4-byte pointers/vptr; members are pinned BY NAME via the inherited bases and
// absolute offsets are NOT static_asserted across pointer members on the 64-bit
// host.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Passby
{

// BrnPassbyEffect.h:33 (DWARF). Reuses the Brn3DEffectControl base by name; the
// virtual (scalar/vector deleting) destructor @ 0x826E8ED0 runs the inherited
// teardown (incl. the Attrib::Instance member via Attrib::Instance::~Instance).
struct Passby3DControl : public BrnSound::Logic::Brn3DEffectControl
{
    Passby3DControl() {}
    virtual ~Passby3DControl();
};

} // namespace Passby
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_PASSBY_BRN_PASSBY_EFFECT_H
