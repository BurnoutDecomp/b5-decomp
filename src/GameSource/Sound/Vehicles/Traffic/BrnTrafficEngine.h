#ifndef BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_ENGINE_H
#define BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_ENGINE_H

#include "types.hpp"
// BrnEffectObject.h FIRST: it and Brn3DEffectControl.h's transitive BrnEffectControl.h
// share the guarded engine types (ClassTypeInfo/EffectBase); including the object home
// first makes its superset EffectBase (with miObjectId) the one that wins for this TU.
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // BrnEffectObject dual base (BY NAME, for TrafficEngine)
#include "GameSource/Sound/Module/LogicModule/Brn3DEffectControl.h" // Brn3DEffectControl base (for Traffic3DControl)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"     // CgsSound::Logic::VoiceWrapper value member (complete type)

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

// Forward-declare TrafficControl -- only a pointer to it is held (avoids pulling
// BrnTrafficControl.h transitively; full definition lives in BrnTrafficControl.h).
struct TrafficControl;

// =============================================================================
// BrnSound::Logic::Traffic::TrafficEngine
//   GameSource/Sound/Vehicles/Traffic/BrnTrafficEngine.h (DWARF home, :42) +
//   .cpp
//
// DWARF (references/DecFIGS/dwarfdump/.../BrnTrafficEngine.h:42) is AUTHORITATIVE:
//   struct BrnSound::Logic::Traffic::TrafficEngine : public BrnSound::Logic::BrnEffectObject
// (the dossier's "DWARF none found" is a known false negative). The X360 ctor
// @ 0x826C9160 is MSVC's inlined full-object constructor: it inlines the
// BrnEffectObject dual-base member zero-init, installs the two leaf vptrs directly
// (off_820B37C4 @ this+0, off_820B3790 @ this+4 -- same shape as the committed
// ExplosionEffect sibling), then zero-seeds its own members and constructs the
// embedded VoiceWrapper at +0x44.
//
// This TU bodies the ctor (@ 0x826C9160) and its destructor (the empty out-of-line
// ~TrafficEngine anchoring the vector-deleting-destructor thunk @ 0x826E18B8).
// The remaining virtuals (GetTypeInfo/GetTypeName/GetStaticTypeInfo/CreateObject/
// GetController/AttachController/Attach/UpdateParams/ProcessUpdate/Detach) are
// SEPARATE recon slices and are DEFERRED.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the four own members land at the
// attested ctor-tail offsets (+0x38/+0x3C/+0x40/+0x44) AFTER the BrnEffectObject
// base on the X360; on the 64-bit host pointer/vptr widths differ, so members are
// pinned BY NAME and DWARF declaration ORDER only, and absolute offsets are NOT
// static_asserted. The base's inlined leaf scalar zero-inits (+0x08..+0x34) are
// un-homed BrnEffectObject-base members (identical to the ExplosionEffect sibling),
// produced by the base default ctor -- NOT fabricated as TrafficEngine members.
// =============================================================================
struct TrafficEngine : public BrnSound::Logic::BrnEffectObject
{
    TrafficEngine();                 // DWARF BrnTrafficEngine.h:113; ctor @ 0x826C9160
    virtual ~TrafficEngine();        // DWARF BrnTrafficEngine.cpp:62; vector-deleting-dtor @ 0x826E18B8

    // DWARF-attested members (BrnTrafficEngine.h:94-97), in declaration order.
    uint16_t                                    mu16AemsPatchTrafficIndex; // BrnTrafficEngine.h:94 (X360 sth 0, +0x38)
    BrnSound::Logic::Traffic::TrafficControl*   mpTrafficControl;          // BrnTrafficEngine.h:95 (X360 stw 0, +0x3C)
    BrnSound::Logic::Traffic::Traffic3DControl* mpTraffic3DControl;        // BrnTrafficEngine.h:96 (X360 stw 0, +0x40)
    CgsSound::Logic::VoiceWrapper               mTrafficEngineVoice;       // BrnTrafficEngine.h:97 (X360 bl VoiceWrapper ctor, this+0x44)

    // FLAG: DWARF also lists sTypeInfo static + the RTTI/lifecycle virtuals listed
    // above -- ALL OUT OF SCOPE for this TU and DEFERRED to their own slices.
};

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_ENGINE_H
