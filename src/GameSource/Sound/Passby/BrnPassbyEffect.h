#ifndef BRN_SOUND_LOGIC_PASSBY_BRN_PASSBY_EFFECT_H
#define BRN_SOUND_LOGIC_PASSBY_BRN_PASSBY_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/Brn3DEffectControl.h"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // PassbyEffect base (committed, BY NAME)

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

// BrnPassbyEffect.h:42 (DWARF): PassbyEffect : public BrnSound::Logic::BrnEffectObject.
// The sibling PassbyEffect object class the committed header comment above reserved.
// Minimal home for the `scalar deleting destructor' @ 0x826C9200 slice: only the base
// + virtual destructor are materialised; the full DWARF surface (mVoice, mAttribs,
// mPosition, meLifetime, mpPassby3DControl, mePrepareState, muSampleId, mfPitchScale,
// mfVolumeScale, mbPlayerIsBoosting, mbFirstUpdate + the RTTI/Prepare/Attach/
// UpdateParams/ProcessUpdate/Detach methods) is DEFERRED to its own recon slices and is
// NOT fabricated here. The dtor is DWARF-virtual and its body is empty -- all teardown
// forwards to the inherited BrnEffectObject base chain.
struct PassbyEffect : public BrnSound::Logic::BrnEffectObject
{
    PassbyEffect() {}
    virtual ~PassbyEffect();   // @ 0x826C9200 -- see BrnPassbyEffect.cpp
};

} // namespace Passby
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_PASSBY_BRN_PASSBY_EFFECT_H
