#ifndef BRN_SOUND_LOGIC_BRN_EFFECT_OBJECT_H
#define BRN_SOUND_LOGIC_BRN_EFFECT_OBJECT_H

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Logic/CgsClassTypeInfo.h"  // ClassTypeInfo<T> (canonical)
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/BrnResourceRegistrar.h"   // BrnSound::Logic::IResourceRequester + ResourceRegistrar (canonical home)

// =============================================================================
// BrnSound::Logic::BrnEffectObject
//   GameSource/Sound/Module/LogicModule/BrnEffectObject.h (DWARF home) +
//   GameSource/Sound/Module/LogicModule/BrnEffectObject.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnEffectObject is a sound-logic
// effect object: it multiply-inherits CgsSound::Logic::EffectObject (the engine
// effect-base path: attach/detach state machine + owning SoundLogicModule
// pointer) AND BrnSound::Logic::IResourceRequester (the streaming-resource
// interface). The X360 vtable layout proves the dual base: the vector deleting
// destructor writes a primary vptr at this+0 and the IResourceRequester
// sub-object vptr at this+4, and the IResourceRequester thunks adjust by 4
// (e.g. the `vector deleting destructor adjustor{4}` does `this - 4` to recover
// the primary object before forwarding to the real destructor @ 0x826AF4C8).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 ASM accesses members by
// absolute byte offset (meAttachState @ +0x24, meDetachState @ +0x28,
// mpLogicModule @ +0x2C, mbResourceRequestActive @ +0x2D, mbResourcesReady @
// +0x31). Those offsets assume 4-byte pointers and a 4-byte vptr; on a 64-bit
// host pointer/vptr widths differ, so members are pinned BY NAME and SEQUENCE
// only and absolute offsets are NOT static_asserted across pointer members.
// =============================================================================

namespace CgsSound
{
namespace Logic
{

// SHARED minimal engine-effect types (ClassTypeInfo + EffectBase). These are also
// declared by the sibling BrnEffectControl.h; both were independent minimal homes of
// the same un-homed CgsSound::Logic engine types and were never co-included in one TU
// until TrafficEngine (BrnEffectObject base) landed alongside Traffic3DControl
// (BrnEffectControl base). To let the two headers coexist without an ODR clash, both
// gate these definitions on the SAME guard macro -- whichever header is included first
// defines them; the second skips. BrnEffectObject.h's EffectBase is a superset of
// BrnEffectControl.h's (adds miObjectId/GetObjectId), so it satisfies both consumers.
#ifndef CGS_SOUND_LOGIC_EFFECT_ENGINE_TYPES_DEFINED
#define CGS_SOUND_LOGIC_EFFECT_ENGINE_TYPES_DEFINED

// Per-class RTTI descriptor. CgsEffectBase.h:313 (DWARF). CANONICAL definition
// folded into GameShared/.../Sound/Logic/CgsClassTypeInfo.h (2026-08-25,
// audio-faithfulness wave 1 -- the per-header aggregate copies were an ODR
// violation; canonical member names: typeName / baseTypeInfo / createObject).

// CgsEffectBase.h:379 (DWARF). Engine effect base: owns the attach/detach state
// machine + back-pointer to the owning logic module. Only the members this TU
// touches (by name) are reconstructed; member ORDER mirrors the DWARF and the
// X360 access sequence.
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
        : miObjectId(0)
        , mfDeltaTime(0.0f)
        , meAttachState(E_ATTACH_STATE_NONE)
        , meDetachState(E_DETACH_STATE_NONE)
        , mpLogicModule(nullptr)
    {
    }

    virtual ~EffectBase() {}

    EAttachState GetAttachState() const { return meAttachState; }

    // CgsEffectBase.h:739 (DWARF) — the per-instance class/object id. The X360
    // reads it at +0x14 (`*(controller+20)`) when a controller-attach hook tests a
    // supplied controller's class; exposed BY NAME for those tests (e.g.
    // SingleGinsuEffect::AttachController). FLAG: ADDITIVE home-grow of this minimal
    // base for the controller-attach TUs; the canonical full home is CgsEffectBase.h.
    s32 GetObjectId() const { return miObjectId; }

    // ORDER mirrors the X360 access pattern:
    //   +0x14 -> miObjectId     (controller-attach hooks read this on a controller)
    //   +0x20 -> mfDeltaTime    (Detach stores 0 here)
    //   +0x24 -> meAttachState  (Detach/ResourcesAreReady/dtor read+write here)
    //   +0x28 -> meDetachState  (dtor stores E_DETACH_STATE_FINISHED here)
    //   +0x2C -> mpLogicModule  (GetResourceRegistr reads here)
    // FLAG: X360 byte offsets; not asserted on the 64-bit host.
    s32          miObjectId;
    f32          mfDeltaTime;
    EAttachState meAttachState;
    EDetachState meDetachState;
    void*        mpLogicModule; // CgsSound::Logic::Module* (opaque here)
};

#endif // CGS_SOUND_LOGIC_EFFECT_ENGINE_TYPES_DEFINED

// CgsEffectBase.h:772 (DWARF). EffectObject : public EffectBase. Carries the
// per-class RTTI hook used by BrnEffectObject's GetTypeInfo/CreateObject.
struct EffectObject : public EffectBase
{
    EffectObject() {}
    virtual ~EffectObject() {}

    virtual ClassTypeInfo<EffectObject>* GetTypeInfo() const;
    virtual const char*                  GetTypeName() const;
};

} // namespace Logic
} // namespace CgsSound

namespace BrnSound
{
namespace Logic
{

// IResourceRequester + ResourceRegistrar now live in their canonical home
// (GameSource/Sound/BrnResourceRegistrar.h, included above) -- folded out of here to resolve the
// cross-header ODR (they were duplicated in BrnEffectControl.h + BrnStateManager.h too).

// BrnEffectObject.h:51 (DWARF). Multiply inherits the engine effect-object base
// (primary vptr @ this+0) and IResourceRequester (sub-object vptr @ this+4).
struct BrnEffectObject : public CgsSound::Logic::EffectObject,
                         public IResourceRequester
{
    // BrnEffectObject.h:87 (DWARF). A resolved sample reference.
    struct SampleTag
    {
        f32 mfVolume;
        s16 miSampleIndex;

        void Construct();
    };

    BrnEffectObject();
    virtual ~BrnEffectObject();

    // — per-class RTTI.
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const;
    virtual const char*                                                    GetTypeName() const;

    // BrnEffectObject.h:139 — IResourceRequester override, bodied inline in this
    // DWARF home. ASM @ 0x82696778: mark resources-ready, assert the effect is
    // still waiting for its data, then advance the attach state to PREPARING.
    virtual void ResourcesAreReady()
    {
        // X360 STORE ORDER (0x82696778):
        //   stb 1, +0x31   (mbResourcesReady = true)   -- precedes the assert
        //   lwz    +0x24   (read meAttachState)
        //   assert meAttachState == E_ATTACH_STATE_WAITING_FOR_DATA
        //   stw 2, +0x24   (meAttachState = E_ATTACH_STATE_PREPARING)
        mbResourcesReady = true;
        CGS_ASSERT(GetAttachState() == CgsSound::Logic::EffectBase::E_ATTACH_STATE_WAITING_FOR_DATA,
                   "GetAttachState() == E_ATTACH_STATE_WAITING_FOR_DATA");
        meAttachState = CgsSound::Logic::EffectBase::E_ATTACH_STATE_PREPARING;
    }

    // BrnEffectObject.h:157 — IResourceRequester override; bodied in the .cpp
    // (ASM @ 0x82696850 forwards to the owning module's embedded registrar).
    virtual ResourceRegistrar& GetResourceRegistrar();

    // BrnEffectObject.h:171 — override of EffectBase::Detach, bodied inline in
    // this DWARF home. ASM @ 0x826EBF88: if a resource request is outstanding,
    // pull this object's requests out of its registrar, then reset bookkeeping.
    virtual bool Detach()
    {
        // X360 STORE ORDER (0x826EBF88):
        //   if (this+0x2D /* mbResourceRequestActive */) {
        //       reg = (this as IResourceRequester)->GetResourceRegistrar();
        //       reg.RemoveRequests(this as IResourceRequester);
        //   }
        //   stb 0, +0x2D   (mbResourceRequestActive = false)
        //   stw 3, +0x24   (meAttachState = E_ATTACH_STATE_FINISHED)
        //   stw 0, +0x20   (mfDeltaTime = 0)
        //   return true
        if (mbResourceRequestActive)
        {
            GetResourceRegistrar().RemoveRequests(static_cast<IResourceRequester*>(this));
        }
        mbResourceRequestActive = false;
        meAttachState = CgsSound::Logic::EffectBase::E_ATTACH_STATE_FINISHED;
        mfDeltaTime   = 0.0f;
        return true;
    }

    // BrnEffectObject.h:207 — resolve a sample tag. Declared for home
    // completeness; not bodied by this group (outside this TU's func set).
    bool GetSampleTag(u32 eTag, u32 uIndex, u32 uCount, SampleTag& rTag) const;

    // Members observed in the X360 layout (by name; ORDER per access pattern).
    //   +0x2D -> mbResourceRequestActive (Detach gate; cleared in the dtor path)
    //   +0x31 -> mbResourcesReady        (set by ResourcesAreReady; cleared in dtor)
    // FLAG: X360 byte offsets (+0x2D, +0x31) not asserted on the 64-bit host.
    bool mbResourceRequestActive;
    bool mbResourcesReady;
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_EFFECT_OBJECT_H
