#ifndef BRN_SOUND_LOGIC_BRN_3D_EFFECT_CONTROL_H
#define BRN_SOUND_LOGIC_BRN_3D_EFFECT_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

// =============================================================================
// BrnSound::Logic::Brn3DEffectControl  (+ its Cgs3dEffectControl base)
//   GameSource/Sound/Module/LogicModule/Brn3DEffectControl.h (minimal OWNING home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF (BrnEffectControl.h:100):
//   struct BrnSound::Logic::Brn3DEffectControl : public Cgs3dEffectControl
// with private members:
//   globalenginedata                          mEngineDataAtrib;  // BrnEffectControl.h:149
//   BrnSound::Logic::Brn3DEffectControl::DrawSphere mDrawSphere;  // BrnEffectControl.h:152
// and the RTTI/Prepare/UpdateParams/Notify control surface.
//
// This is a MINIMAL owning slice introduced because no committed Brn3DEffectControl
// home exists yet; it is needed only so the Passby::Passby3DControl leaf (which
// derives from Brn3DEffectControl) can structurally tear down its inherited
// members. The ONLY member modelled BY NAME is mEngineDataAtrib, because the
// Passby3DControl `scalar deleting destructor' (@ 0x826E8ED0) tears it down via the
// committed Attrib::Instance::~Instance:
//   Attrib::Instance::~Instance(this + 176);   // mEngineDataAtrib teardown
// In the X360 build mEngineDataAtrib is the generated class Attrib::Gen::
// globalenginedata, which derives from Attrib::Instance; only the Attrib::Instance
// sub-object slice the destructor touches is load-bearing here, so we model the
// member as a plain Attrib::Instance (reused BY NAME from the committed AttribSys
// home). The DrawSphere member, the Cgs3dEffectControl transform/emitter state, and
// the RTTI/Prepare/UpdateParams/Notify surface are DEFERRED.
//
// FLAG: minimal reconstruction of an un-homed base (Cgs3dEffectControl) and an
// un-homed leaf (Brn3DEffectControl). Only the inheritance spine and the single
// destructor-touched member are present. The X360 +176 byte offset of
// mEngineDataAtrib is NOT reproduced as a host offset (32-bit-vs-64-bit pointer
// widths differ); the member is pinned BY NAME only.
// =============================================================================

namespace CgsSound
{
namespace Logic
{

// Cgs3dEffectControl : public EffectControl (DWARF: Brn3DEffectControl's base is
// Cgs3dEffectControl, the engine 3D-positional effect control). Modelled as the
// committed EffectControl shape; the 3D transform/emitter state is DEFERRED.
// FLAG: minimal reconstruction of an un-homed engine base.
struct Cgs3dEffectControl : public CgsSound::Logic::EffectControl
{
    Cgs3dEffectControl() {}
    virtual ~Cgs3dEffectControl() {}
};

} // namespace Logic
} // namespace CgsSound

namespace BrnSound
{
namespace Logic
{

// BrnEffectControl.h:100 (DWARF). Minimal owning slice: inherits the engine 3D
// effect-control base and owns the single member the Passby3DControl destructor
// tears down (mEngineDataAtrib, an Attrib::Instance).
struct Brn3DEffectControl : public CgsSound::Logic::Cgs3dEffectControl
{
    // Constructor @ 0x826AF7F0. The X360 ctor zero-inits the Cgs3dEffectControl
    // transform/emitter state and the +0xA0/+0xA4/+0xA8 control fields, then
    // constructs the generated Attrib::Gen::globalenginedata at this+0xB0 with
    // (collection=0, owner=0). Only the single HOMED member (mEngineDataAtrib) is
    // modelled, so the ctor stays inline here: it builds mEngineDataAtrib from a
    // null collection (matching globalenginedata(this+0xB0, 0, 0)).
    // FLAG: the un-homed transform/emitter member zero-inits (X360 +0x18..+0x58)
    // and the +0xA0(=2.0)/+0xA4(=0.5)/+0xA8(=0) control fields are declaration-only
    // / DEFERRED with the rest of the Cgs3dEffectControl surface — not fabricated.
    Brn3DEffectControl()
        : mEngineDataAtrib(nullptr, nullptr)  // asm: globalenginedata(this+0xB0, owner=0, 0)
    {
    }

    // Nested DrawSphere payload. DWARF (BrnEffectControl.h:152) declares the owning
    // member `BrnSound::Logic::Brn3DEffectControl::DrawSphere mDrawSphere`. It is the
    // element type of the sound-command message the 3-D effect controls post through
    // the logic queue as CgsSound::Io::Message<Brn3DEffectControl::DrawSphere>: the
    // X360 typed-AddEvent thunk @0x826DF9B8 (VariableEventQueue<8192,16>::AddEvent<
    // Message<DrawSphere>>) bakes the record size `li r6,0x20` (=32). Because
    // sizeof(Message<T>) == 16-byte MessageHeader + sizeof(T), this pins
    // sizeof(DrawSphere) == 16. Posted by BrnSound::Logic::Traffic::TrafficEngine,
    // Collision::CollisionControl, Passby::PassbyEffect, Vehicles::Engines::DualGinsuEffect.
    //
    // FLAG: only the SIZE (16 bytes) is X360-attested (from the AddEvent record-size
    // immediate). The internal field breakdown is NOT grounded by asm in this TU, so
    // the payload is modelled as honest opaque 4-byte words (align 4, consistent with a
    // float/vector payload) rather than fabricated centre/radius fields. Pinned by the
    // static_assert below; full field semantics DEFERRED to the Brn3DEffectControl TU.
    // Defining the nested type is additive and does not alter Brn3DEffectControl's own
    // size/vtable (the mDrawSphere member itself stays DEFERRED, as noted above).
    struct DrawSphere
    {
        u32 mauOpaque[4];  // 16 bytes = X360-attested payload size (Message == li r6,0x20 == 32)
    };

    // Scalar deleting destructor @ 0x826C85F8 — out-of-line; see Brn3DEffectControl.cpp.
    virtual ~Brn3DEffectControl();

    // BrnEffectControl.h:149 (DWARF) — generated globalenginedata attribute handle.
    // Modelled as the Attrib::Instance sub-object slice the destructor tears down.
    // FLAG: full Attrib::Gen::globalenginedata generated class is DEFERRED.
    Attrib::Instance mEngineDataAtrib;
};

// X360-attested payload size (see the DrawSphere FLAG above): the AddEvent<Message<
// DrawSphere>> thunk @0x826DF9B8 bakes li r6,0x20 (=32) == sizeof(MessageHeader)+16.
static_assert(sizeof(Brn3DEffectControl::DrawSphere) == 16,
              "Brn3DEffectControl::DrawSphere payload must be 16 bytes (Message == 32 == li r6,0x20)");

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_3D_EFFECT_CONTROL_H
