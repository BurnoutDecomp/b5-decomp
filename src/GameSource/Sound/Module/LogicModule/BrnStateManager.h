#ifndef BRN_SOUND_LOGIC_BRN_STATE_MANAGER_H
#define BRN_SOUND_LOGIC_BRN_STATE_MANAGER_H

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h"  // CgsSound::Logic::StateManager (canonical) + CgsSound::Logic::ClassTypeInfo
#include "GameSource/Sound/BrnResourceRegistrar.h"   // BrnSound::Logic::IResourceRequester + ResourceRegistrar (canonical home)

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
// ODR FOLD (2026-06-25): this header previously carried its OWN minimal
// `struct StateManager` + `template ClassTypeInfo` slice (a placeholder used only
// by the manager TUs, none of which were in the build). Those duplicate
// definitions are now DELETED; the SINGLE canonical CgsSound::Logic::StateManager
// + CgsSound::Logic::ClassTypeInfo from CgsStateManager.h (included above, real
// ctor 0x826FAA18, ObjectPool<RegisteredContent,4> content pool, the
// AddToClassTypeInfoArray/GetStaticTypeInfo RTTI hooks) is now the base for
// BrnStateManager and therefore for every concrete leaf manager that includes
// this header. The full StateManager is LARGER than the old minimal slice, so the
// IResourceRequester sub-object now lands at a different (compiler-chosen) offset
// than the X360's +0x90 -- it is relied on as a real SECOND BASE sub-object
// (BrnStateManager : public StateManager, public IResourceRequester), never a
// hand-encoded offset.
//
// RTTI surface reconciliation: the full StateManager base does NOT itself declare
// GetTypeInfo()/GetTypeName() (those were the old minimal slice's two virtuals).
// They are introduced HERE as BrnStateManager's own virtuals (the leaves override
// them), exactly as the old minimal base introduced them -- so the override chain
// the 8 concrete managers rely on (GetTypeInfo/GetTypeName returning
// ClassTypeInfo<CgsSound::Logic::StateManager>*) is preserved against the
// canonical ClassTypeInfo.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): no member offsets are touched by
// this TU's function (GetTypeName loads a static string literal and ignores
// `this`), so no absolute offsets are asserted here.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// IResourceRequester + ResourceRegistrar now live in their canonical home
// (GameSource/Sound/BrnResourceRegistrar.h, included above) -- folded out of here to resolve the
// cross-header ODR.

// BrnStateManager.h:59 (DWARF): BrnStateManager : public StateManager (primary,
// RTTI hooks) + public IResourceRequester (sub-object). GetTypeName() is bodied
// in this TU's .cpp. The IResourceRequester pure virtuals and the CPU-monitor
// bookkeeping are declared/elided for home completeness but not bodied here.
//
// The primary base is now the FULL canonical CgsSound::Logic::StateManager (the
// content-pool/RTTI/ctor keystone in CgsStateManager.h). BrnStateManager remains
// abstract (the two IResourceRequester pure virtuals are declared but not bodied
// here); the concrete leaf managers override+body them.
struct BrnStateManager : public CgsSound::Logic::StateManager,
                         public IResourceRequester
{
    BrnStateManager() : miCpuMonitor(0) {}
    virtual ~BrnStateManager() {}

    // BrnStateManager.cpp:34 — per-class RTTI. Introduced here (the full base does
    // not declare them); the leaves override these. Returns the canonical
    // CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> descriptor.
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
