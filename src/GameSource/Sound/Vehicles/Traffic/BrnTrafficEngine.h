#ifndef BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_ENGINE_H
#define BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_ENGINE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/Brn3DEffectControl.h"

// =============================================================================
// BrnSound::Logic::Traffic::Traffic3DControl
//   GameSource/Sound/Vehicles/Traffic/BrnTrafficEngine.h (DWARF home) +
//   GameSource/Sound/Vehicles/Traffic/BrnTrafficEngine.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Traffic3DControl is the 3D-positional
// sound-logic control for traffic vehicles (the sibling of Passby::Passby3DControl).
// DWARF (BrnTrafficEngine.h:33):
//   struct BrnSound::Logic::Traffic::Traffic3DControl
//       : public BrnSound::Logic::Brn3DEffectControl
// so it inherits the (committed, minimally-homed) Brn3DEffectControl base, which
// owns the Attrib::Instance member (mEngineDataAtrib) the destructor tears down.
//
// This TU's recon'd function set is exactly ONE entry:
//   `vector deleting destructor'  @ 0x826E3000
// whose X360 teardown is IDENTICAL in shape to the committed Passby3DControl
// destructor (@ 0x826E8ED0):
//   bl   Attrib::Instance::~Instance(this + 0xB0)  ; destroy mEngineDataAtrib
//   li   r9, 3 ; stw r9, 0x24(this)                ; meDetachState = FINISHED
//   stw  &off_820AA820, 0(this)                    ; primary vptr settle
//   stb  0, 0x2D(this)                             ; control bookkeeping flag = false
//   stw  0, 0x20(this)                             ; mfDeltaTime = 0
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
// The Attrib::Instance member teardown (mEngineDataAtrib) is produced by the
// inherited ~Brn3DEffectControl destructor chain (which destroys the member via the
// committed Attrib::Instance::~Instance). The remaining stores settle members owned
// by the inherited bases. The matching off_820AA820 vptr settle and identical
// member offsets confirm Traffic3DControl and Passby3DControl share the
// Brn3DEffectControl base teardown EXACTLY.
//
// FLAG (shape vs full surface): this is a MINIMAL home for the boot-trace
// Traffic3DControl TU. The full DWARF surface (the RTTI GetTypeInfo/GetTypeName/
// GetStaticTypeInfo/CreateObject hooks, the sibling TrafficControl::mpTraffic3dControl
// link) is DEFERRED to its own TU(s); only the destructor is materialised here. The
// (a2 & 1) deallocation tail dispatches the global sound allocator (off_82FFB954);
// that allocator vtable is not homed here, so the `delete` half of the X360 vector
// deleting destructor is left to the host toolchain (same treatment as the
// Passby3DControl / BrnEffectControl / BrnEffectObject sibling homes).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 offsets (mEngineDataAtrib @
// +0xB0, meDetachState @ +0x24, mfDeltaTime @ +0x20, +0x2D bookkeeping flag) assume
// 4-byte pointers/vptr; members are pinned BY NAME via the inherited bases and
// absolute offsets are NOT static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

// BrnTrafficEngine.h:33 (DWARF). Reuses the Brn3DEffectControl base by name; the
// virtual (vector deleting) destructor @ 0x826E3000 runs the inherited teardown
// (incl. the Attrib::Instance member via Attrib::Instance::~Instance).
struct Traffic3DControl : public BrnSound::Logic::Brn3DEffectControl
{
    Traffic3DControl() {}
    virtual ~Traffic3DControl();
};

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_ENGINE_H
