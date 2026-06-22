#ifndef BRN_SOUND_LOGIC_BRN_STATE_MANAGER_H
#define BRN_SOUND_LOGIC_BRN_STATE_MANAGER_H

#include "types.hpp"

// =============================================================================
// BrnSound::Logic::BrnStateManager
//   GameSource/Sound/Module/LogicModule/BrnStateManager.h (DWARF home) +
//   GameSource/Sound/Module/LogicModule/BrnStateManager.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnStateManager owns and drives
// the sound-logic state machine. It multiply-inherits the engine
// CgsSound::Logic::StateManager (the primary base carrying the per-class RTTI
// hooks GetTypeInfo/GetTypeName, returning ClassTypeInfo<CgsSound::Logic::
// StateManager>) AND BrnSound::Logic::IResourceRequester (the streaming-resource
// interface, sub-object vptr after the primary). This TU bodies only
// GetTypeName().
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): no member offsets are touched by
// this TU's function (GetTypeName loads a static string literal and ignores
// `this`), so no absolute offsets are asserted here.
// =============================================================================

namespace CgsSound
{
namespace Logic
{

// Per-class RTTI descriptor. CgsStateManager.h (DWARF). Templated on the leaf
// class; only the shape (id/name/base/factory) is load-bearing here.
// FLAG: minimal — the owning CgsStateManager RTTI home is not yet
// reconstructed; this is the shape needed to declare BrnStateManager's hooks.
template <typename T>
struct ClassTypeInfo
{
    s32               ObjectID;
    const char*       mpcTypeName;
    ClassTypeInfo<T>* mpBaseTypeInfo;
    T* (*mpfnCreateObject)(u32);
};

// CgsStateManager.h:88 (DWARF): CgsSound::Logic::StateManager : public
// CgsSound::MemBase. Engine sound-logic state-manager base; carries the
// per-class RTTI virtuals overridden by BrnStateManager.
// FLAG: minimal reconstruction of an un-homed base (MemBase base elided; only
// the polymorphic RTTI surface needed by this leaf is modelled).
struct StateManager
{
    StateManager() {}
    virtual ~StateManager() {}

    virtual ClassTypeInfo<StateManager>* GetTypeInfo() const;
    virtual const char*                  GetTypeName() const;
};

} // namespace Logic
} // namespace CgsSound

namespace BrnSound
{
namespace Logic
{

class ResourceRegistrar;

// BrnResourceRegistrar.h:337 (DWARF). The resource-requester interface that
// BrnStateManager implements as its second base. Declared minimally here; the
// full type is reconstructed in its own home.
// FLAG: minimal forward-declared surface — the GetResourceRegistrar return type
// (ResourceRegistrar) is only forward-declared; this TU does not body either
// interface method.
struct IResourceRequester
{
    virtual ~IResourceRequester() {}
    virtual void               ResourcesAreReady()      = 0;
    virtual ResourceRegistrar& GetResourceRegistrar()   = 0;
};

// BrnStateManager.h:59 (DWARF): BrnStateManager : public StateManager (primary,
// RTTI hooks) + public IResourceRequester (sub-object). GetTypeName() is bodied
// in this TU's .cpp. The IResourceRequester pure virtuals and the CPU-monitor
// bookkeeping are declared/elided for home completeness but not bodied here.
struct BrnStateManager : public CgsSound::Logic::StateManager,
                         public IResourceRequester
{
    BrnStateManager() : miCpuMonitor(0) {}
    virtual ~BrnStateManager() {}

    // BrnStateManager.cpp:34 — per-class RTTI.
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetTypeInfo() const;
    virtual const char*                                                   GetTypeName() const;

    // IResourceRequester overrides — declared for home completeness; not bodied
    // by this group (outside this TU's func set).
    virtual void               ResourcesAreReady();
    virtual ResourceRegistrar& GetResourceRegistrar();

private:
    // BrnStateManager.h:94 (DWARF): the CPU-monitor handle used by the
    // performance-monitor bookkeeping. Pinned by name.
    s32 miCpuMonitor;
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_STATE_MANAGER_H
