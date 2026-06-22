#ifndef CGS_SOUND_LOGIC_CGSEFFECTBASE_H
#define CGS_SOUND_LOGIC_CGSEFFECTBASE_H

#include "types.hpp"
#include "GameShared/GameClasses/Sound/CgsMemBase.h"

// =============================================================================
// CgsSound::Logic effect-base family
//   GameShared/GameClasses/Sound/Logic/CgsEffectBase.h (DWARF home) +
//   GameShared/GameClasses/Sound/Logic/CgsEffectBase.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. This is the CANONICAL engine home
// for the sound-logic effect hierarchy:
//   EffectBase   : public CgsSound::MemBase   (DWARF CgsEffectBase.h:379)
//   EffectControl: public EffectBase          (DWARF CgsEffectBase.h:763)
//   EffectObject : public EffectBase          (DWARF CgsEffectBase.h:772)
// plus the per-class RTTI descriptor ClassTypeInfo<T> (DWARF CgsEffectBase.h:313).
//
// The minimal BrnSound EffectBase/EffectControl shapes in
// GameSource/Sound/Module/LogicModule/BrnEffectControl.h (and BrnEffectObject.h /
// BrnState.h) deliberately DEFERRED to this home; per their ODR notes those minimal
// shapes are never included in the SAME TU as this header, so there is no
// redefinition clash.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 ASM accesses members by
// absolute byte offset. The member SEQUENCE below mirrors the DWARF declaration
// order and the X360 access pattern, but because the leading CgsSound::MemBase
// sub-object carries a vptr (4 bytes on X360, 8 on a 64-bit host) and there are
// pointer members, absolute member offsets are NOT static_asserted across the
// 32/64 boundary. Members are pinned BY NAME + SEQUENCE only.
//
// X360 offset map proven by this TU's functions (over the 32-bit layout):
//   +0x08 -> mpState         (Prepare: stw r4, 8(this))
//   +0x0E -> mu16AttachCount  (Attach: lhz/sth 0xE(this))
//   +0x24 -> meDetachState   (Attach: stw 0, 0x24(this) -> E_DETACH_STATE_NONE)
//   +0x28 -> mpLogicModule   (Prepare: stw r10, 0x28(this))
// =============================================================================

namespace CgsSound
{
namespace Logic
{

// Forward declarations of the logic types the RTTI descriptors are templated on.
struct State;
struct StateManager;
struct EffectControl;
struct EffectObject;
struct Module;

const s32 KI_MAX_IO_CONNECTIONS = 16; // CgsEffectBase.h:57 (DWARF)

// CgsEffectBase.h:313 (DWARF). Per-class RTTI descriptor. Templated on the leaf
// class; holds the object id, type name, base descriptor and a factory hook.
template <typename T>
struct ClassTypeInfo
{
    ClassTypeInfo(s32 aiObjectID,
                  const char* apcTypeName,
                  ClassTypeInfo<T>* apBaseTypeInfo,
                  T* (*apfnCreateObject)(u32))
        : ObjectID(aiObjectID)
        , typeName(apcTypeName)
        , baseTypeInfo(apBaseTypeInfo)
        , createObject(apfnCreateObject)
    {
    }

    T* CreateObject(u32 au32Param) { return createObject ? createObject(au32Param) : nullptr; }

    s32               ObjectID;     // CgsEffectBase.h:316
    const char*       typeName;     // CgsEffectBase.h:317
    ClassTypeInfo<T>* baseTypeInfo; // CgsEffectBase.h:318
    T* (*createObject)(u32);        // CgsEffectBase.h:319
};

// CgsEffectBase.h:379 (DWARF): CgsSound::Logic::EffectBase : public CgsSound::MemBase.
// Engine effect base: owns the attach/detach state machine, the per-effect timing,
// and the back-link to its owning logic module. Member ORDER mirrors the DWARF.
struct EffectBase : public CgsSound::MemBase
{
    static const s32 KI_MAX_SFX_CTLS = 32; // CgsEffectBase.h:398 (DWARF)

    enum EAttachState // CgsEffectBase.h:382 (DWARF)
    {
        E_ATTACH_STATE_NONE             = 0,
        E_ATTACH_STATE_WAITING_FOR_DATA = 1,
        E_ATTACH_STATE_PREPARING        = 2,
        E_ATTACH_STATE_FINISHED         = 3,
    };

    enum EDetachState // CgsEffectBase.h:390 (DWARF)
    {
        E_DETACH_STATE_NONE     = 0,
        E_DETACH_STATE_BEGIN    = 1,
        E_DETACH_STATE_UPDATING = 2,
        E_DETACH_STATE_FINISHED = 3,
    };

    EffectBase()
        : mpNextEffectBase(nullptr)
        , mpState(nullptr)
        , mu16RefCount(0)
        , mu16AttachCount(0)
        , mu16AllocatorIndex(0)
        , miObjectId(0)
        , mfRunningTime(0.0f)
        , mfDeltaTime(0.0f)
        , meAttachState(E_ATTACH_STATE_NONE)
        , meDetachState(E_DETACH_STATE_NONE)
        , mpLogicModule(nullptr)
        , mbEnabled(true)
        , mbHasLoadedData(false)
        , mpDynamicMixIo(nullptr)
    {
    }

    virtual ~EffectBase() {} // CgsEffectBase.cpp:80

    // CgsEffectBase.cpp:157 @ 0x8268CEC8. Latch the controlling State and pull the
    // owning logic module from it. The X360 reads State+0x24 (the state's owner
    // back-link) and then +0x2C off that to recover the Module*; reconstructed by
    // name via the State accessor below.
    virtual bool Prepare(State* apState);

    // CgsEffectBase.cpp:174 @ 0x826A1138. Reset the detach state and bump the
    // attach count. Returns true.
    virtual bool Attach();

    EAttachState GetAttachState() const { return meAttachState; }
    void         SetAttachState(EAttachState aeState) { meAttachState = aeState; }
    State*       GetStateBase() const { return mpState; }
    void         SetStateBase(State* apState) { mpState = apState; }
    Module*      GetLogicModule() const { return mpLogicModule; }
    u16          GetAttachCount() const { return mu16AttachCount; }

    // --- members (DWARF order; offsets are X360 32-bit, not asserted on host) ---
    EffectBase*  mpNextEffectBase;   // CgsEffectBase.h:681
protected:
    State*       mpState;            // CgsEffectBase.h:735  (+0x08 X360)
    u16          mu16RefCount;       // CgsEffectBase.h:736
    u16          mu16AttachCount;    // CgsEffectBase.h:737  (+0x0E X360)
    u16          mu16AllocatorIndex; // CgsEffectBase.h:738
    s32          miObjectId;         // CgsEffectBase.h:739
    f32          mfRunningTime;      // CgsEffectBase.h:741
    f32          mfDeltaTime;        // CgsEffectBase.h:742
    EAttachState meAttachState;      // CgsEffectBase.h:746
    EDetachState meDetachState;      // CgsEffectBase.h:747  (+0x24 X360)
private:
    Module*      mpLogicModule;      // CgsEffectBase.h:752  (+0x28 X360)
    bool         mbEnabled;          // CgsEffectBase.h:753
    bool         mbHasLoadedData;    // CgsEffectBase.h:754
    void*        mpDynamicMixIo;     // CgsEffectBase.h:757  (Nicotine::DMixIO*, opaque here)
};

// CgsEffectBase.h:736 (DWARF): the controlling State carries a back-link to its
// owning Module. EffectBase::Prepare reads it; modelled minimally here so the
// canonical home is self-contained. State's full surface is reconstructed in its
// own home (CgsState.{h,cpp}); this is the shape Prepare touches BY NAME.
// FLAG: minimal — only the owner/module back-link Prepare reads is modelled.
struct State
{
    State() : mpOwner(nullptr) {}
    virtual ~State() {}

    // The X360 Prepare reads State+0x24 then +0x2C off it for the Module*. Modelled
    // as an owner object that exposes its logic module.
    struct Owner
    {
        Owner() : mpLogicModule(nullptr) {}
        Module* mpLogicModule; // +0x2C off the owner
    };

    Owner* GetOwner() const { return mpOwner; } // +0x24 off the State

protected:
    Owner* mpOwner;
};

// CgsEffectBase.h:763 (DWARF): EffectControl : public EffectBase.
struct EffectControl : public EffectBase
{
    EffectControl() {}
    virtual ~EffectControl() {}

    // CgsEffectBase.cpp @ 0x8268CEB8. Returns the static type name "EffectControl".
    virtual const char* GetTypeName() const;

    ClassTypeInfo<EffectControl>* GetTypeInfo() const { return GetStaticTypeInfo(); }
    static ClassTypeInfo<EffectControl>* GetStaticTypeInfo();

    static const u16 KU_SIZEOF_CLASS_ARRAY = 64; // CgsEffectBase.h:765 (DWARF)
};

// CgsEffectBase.h:772 (DWARF): EffectObject : public EffectBase.
struct EffectObject : public EffectBase
{
    EffectObject() {}
    virtual ~EffectObject() {}

    // CgsEffectBase.cpp @ 0x8268CE98. Returns the static type name "EffectObject".
    virtual const char* GetTypeName() const;

    ClassTypeInfo<EffectObject>* GetTypeInfo() const { return GetStaticTypeInfo(); }
    static ClassTypeInfo<EffectObject>* GetStaticTypeInfo();

    static const u16 KU_SIZEOF_CLASS_ARRAY = 64; // CgsEffectBase.h:774 (DWARF)
};

} // namespace Logic
} // namespace CgsSound

#endif // CGS_SOUND_LOGIC_CGSEFFECTBASE_H
