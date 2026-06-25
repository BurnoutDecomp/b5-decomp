#ifndef BRN_SOUND_LOGIC_BRN_EFFECT_CONTROL_H
#define BRN_SOUND_LOGIC_BRN_EFFECT_CONTROL_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/BrnResourceRegistrar.h"   // BrnSound::Logic::IResourceRequester + ResourceRegistrar (canonical home)

// =============================================================================
// BrnSound::Logic::BrnEffectControl
//   GameSource/Sound/Module/LogicModule/BrnEffectControl.h (DWARF home) +
//   GameSource/Sound/Module/LogicModule/BrnEffectControl.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnEffectControl is the sound-logic
// effect *control* sibling of BrnEffectObject: same dual-base shape. The DWARF
// shows `BrnEffectControl : public EffectControl`, but the X360 vector deleting
// destructor proves a SECOND base (IResourceRequester) exactly like BrnEffectObject:
//   - primary vptr written at this+0, IResourceRequester sub-object vptr at this+4
//   - the same final IResourceRequester sub-object vtable (off_820AA820, shared with
//     BrnEffectObject's dtor @ 0x826AF4C8) is stored at this+4
//   - the `vector deleting destructor adjustor{4}` does `this - 4` to recover the
//     primary object before forwarding to the real destructor
//   - identical member teardown offsets: meAttachState @ +0x24, meDetachState @
//     +0x28, mbResourcesReady @ +0x31
// So BrnEffectControl multiply-inherits the engine effect-control base (primary vptr
// @ this+0) and IResourceRequester (sub-object vptr @ this+4).
//
// FLAG (shape vs full surface): this is a MINIMAL home for the boot-trace
// BrnEffectControl TU, whose only two recon'd functions are the vector deleting
// destructor (@ 0x826AEF68) and its adjustor{4} thunk (@ 0x82696868). The control
// hierarchy's full surface (Prepare/UpdateParams/Notify/GetBrnLogicModule/RTTI
// CreateObject, and the Brn3DEffectControl / Brn3DUserSpaceEffectControl subclasses
// with their transform/emitter members) is DEFERRED to its own TU(s). Only the
// members the destructor tears down are modelled BY NAME here.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 ASM accesses members by
// absolute byte offset (meAttachState @ +0x24, meDetachState @ +0x28,
// mbResourcesReady @ +0x31). Those offsets assume 4-byte pointers and a 4-byte
// vptr; on a 64-bit host pointer/vptr widths differ, so members are pinned BY NAME
// and SEQUENCE only and absolute offsets are NOT static_asserted across pointers.
//
// ODR: this home models its own minimal EffectControl/EffectBase/IResourceRequester
// shape (mirroring the BrnEffectObject home for the object hierarchy). The two homes
// are never included in the same TU, so there is no redefinition clash. The full
// CgsEffectBase.h home (DWARF CgsEffectBase.h:379) is DEFERRED.
// =============================================================================

namespace CgsSound
{
namespace Logic
{

// Per-class RTTI descriptor. CgsEffectBase.h:313 (DWARF). Templated on the leaf
// class; only the shape (id/name/base/factory) is load-bearing here.
// FLAG: minimal — the owning CgsEffectBase home is not yet reconstructed; this is
// the shape needed to declare BrnEffectControl's RTTI hooks.
template <typename T>
struct ClassTypeInfo
{
    s32               ObjectID;
    const char*       mpcTypeName;
    ClassTypeInfo<T>* mpBaseTypeInfo;
    T* (*mpfnCreateObject)(u32);
};

// CgsEffectBase.h:379 (DWARF). Engine effect base: owns the attach/detach state
// machine. Only the members this TU's destructor tears down are reconstructed (by
// name); member ORDER mirrors the DWARF and the X360 access sequence.
// FLAG: minimal reconstruction of an un-homed base; absolute offsets not asserted.
struct EffectBase
{
    enum EAttachState
    {
        E_ATTACH_STATE_NONE             = 0,
        E_ATTACH_STATE_WAITING_FOR_DATA = 1,
        E_ATTACH_STATE_PREPARING        = 2,
        E_ATTACH_STATE_FINISHED         = 3,
    };

    enum EDetachState
    {
        E_DETACH_STATE_NONE     = 0,
        E_DETACH_STATE_BEGIN    = 1,
        E_DETACH_STATE_UPDATING = 2,
        E_DETACH_STATE_FINISHED = 3,
    };

    EffectBase()
        : mfDeltaTime(0.0f)
        , meAttachState(E_ATTACH_STATE_NONE)
        , meDetachState(E_DETACH_STATE_NONE)
        , mpLogicModule(nullptr)
    {
    }

    virtual ~EffectBase() {}

    EAttachState GetAttachState() const { return meAttachState; }

    // ORDER mirrors the X360 access pattern (dtor teardown):
    //   +0x20 -> mfDeltaTime
    //   +0x24 -> meAttachState  (dtor stores E_ATTACH_STATE_NONE here)
    //   +0x28 -> meDetachState  (dtor stores E_DETACH_STATE_FINISHED here)
    //   +0x2C -> mpLogicModule
    // FLAG: X360 byte offsets; not asserted on the 64-bit host.
    f32          mfDeltaTime;
    EAttachState meAttachState;
    EDetachState meDetachState;
    void*        mpLogicModule; // CgsSound::Logic::Module* (opaque here)
};

// CgsEffectBase.h:765 (DWARF). EffectControl : public EffectBase. The control-side
// counterpart of EffectObject; carries the per-class RTTI hook used by
// BrnEffectControl's GetTypeInfo/CreateObject.
struct EffectControl : public EffectBase
{
    EffectControl() {}
    virtual ~EffectControl() {}

    virtual ClassTypeInfo<EffectControl>* GetTypeInfo() const;
    virtual const char*                   GetTypeName() const;
};

} // namespace Logic
} // namespace CgsSound

namespace BrnSound
{
namespace Logic
{

// IResourceRequester + ResourceRegistrar now live in their canonical home
// (GameSource/Sound/BrnResourceRegistrar.h, included above) -- folded out of here to resolve the
// cross-header ODR.

// BrnEffectControl.h:56 (DWARF). Multiply inherits the engine effect-control base
// (primary vptr @ this+0) and IResourceRequester (sub-object vptr @ this+4).
struct BrnEffectControl : public CgsSound::Logic::EffectControl,
                          public IResourceRequester
{
    BrnEffectControl();
    virtual ~BrnEffectControl();

    // BrnEffectControl.cpp:38 — per-class RTTI (DEFERRED bodies; declared for the
    // control vtable shape).
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetTypeInfo() const;
    virtual const char*                                                     GetTypeName() const;

    // BrnEffectControl.h:220 — IResourceRequester override. DEFERRED (outside this
    // TU's recon'd function set); declared for the second-base vtable shape.
    virtual void ResourcesAreReady();

    // BrnEffectControl.h:231 — IResourceRequester override. DEFERRED; declared for
    // the second-base vtable shape.
    virtual ResourceRegistrar& GetResourceRegistrar();

    // BrnEffectControl.h:239 — override of the effect Detach. DEFERRED; declared
    // for the control vtable shape.
    virtual bool Detach();

    // Member observed in the X360 dtor teardown (by name).
    //   +0x31 -> mbResourcesReady (dtor stores false)
    // FLAG: X360 byte offset (+0x31) not asserted on the 64-bit host.
    bool mbResourcesReady;
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_EFFECT_CONTROL_H
