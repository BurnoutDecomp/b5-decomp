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
// interface, sub-object vptr after the primary). This TU bodies GetTypeName(),
// the vector-deleting destructor (X360 0x826FAB58), and GetResourceRegistrar()
// (X360 0x82696510).
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
// RTTI surface reconciliation: the canonical StateManager owns the GetTypeInfo()
// and GetTypeName() slots at its ARTIST primary-vtable offsets +0x04/+0x08.
// BrnStateManager overrides those hooks; each concrete manager overrides them with
// its own descriptor and name.
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
// RTTI hooks) + public IResourceRequester (sub-object). GetTypeName(), the vector-
// deleting destructor, and GetResourceRegistrar() are bodied in this TU's .cpp
// (real, attested X360 addresses); ResourcesAreReady() is bodied as a faithful
// PS3-DecFIGS no-op. Both IResourceRequester pure virtuals are therefore overridden
// with concrete bodies here, so BrnStateManager is concrete (not abstract).
//
// The primary base is now the FULL canonical CgsSound::Logic::StateManager (the
// content-pool/RTTI/ctor keystone in CgsStateManager.h).
struct BrnStateManager : public CgsSound::Logic::StateManager,
                         public IResourceRequester
{
    BrnStateManager() : miCpuMonitor(0) {}

    // `vector deleting destructor'  @ X360 0x826FAB58 -- bodied out-of-line in the
    // .cpp (see there for the asm rationale). Structurally identical to the base
    // CgsSound::Logic::StateManager's own vector-deleting destructor (0x826FAAB8,
    // CgsStateManagerDtor.cpp / CgsStateManager.cpp): BrnStateManager adds no member
    // requiring explicit teardown (miCpuMonitor is a scalar), so the compiler
    // re-derives the SAME `off_820B66E4`-vtable-store + `+0x30 ~ObjectPool` + MemBase
    // vtable re-install + conditional allocator-free shape the base already has --
    // the observable work (destroying the inherited mContentPool) happens via the
    // implicit base-destructor chain, matching the established base-class precedent.
    virtual ~BrnStateManager();

    // ARTIST BrnStateManager primary vtable off_820B7C6C +0x04/+0x08.
    // Returns the canonical CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>
    // descriptor; each concrete leaf supplies its own descriptor and name.
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetTypeInfo() const override;
    virtual const char*                                                   GetTypeName() const override;

    // BrnStateManager.h:61 (DWARF) -- the static RTTI descriptor + factory the
    // sTypeInfo descriptor stores as its createObject hook. CreateObject @ X360
    // 0x826FB5F0 allocates a 152-byte StateManager via CgsSound::MemBase::operator
    // new and patches the BrnStateManager vptrs; its int arg is the operator-new
    // flavour selector, NOT `this` (the function never reads an instance), so it is
    // a STATIC class function -- required for &CreateObject to be storable as the
    // descriptor's free-function pointer (a pointer-to-member is not one).
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetStaticTypeInfo();
    static CgsSound::Logic::StateManager*                                 CreateObject( u32 luType );

private:
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> sTypeInfo;

public:

    // IResourceRequester overrides.
    // ResourcesAreReady  @ PS3 DecFIGS 0x8E8D24 -- empty no-op (faithful to PS3;
    // not individually X360-attested in this TU's dossier). The leaves override
    // with their real resource-ready callbacks.
    virtual void ResourcesAreReady();

    // GetResourceRegistrar  @ X360 0x82696510 (Hex-Rays name-truncated to
    // "GetResourceRegistr" in the dossier -- this IS the declared override).
    // Bodied out-of-line in the .cpp: forwards to the owning module's embedded
    // registrar via mpLogicModule, mirroring the sibling BrnEffectObject::
    // GetResourceRegistrar @ 0x82696850 (same tail-call-through-IResourceRequester
    // shape, same mpLogicModule field the base CgsSound::Logic::StateManager already
    // declares protected/by-name).
    virtual ResourceRegistrar& GetResourceRegistrar();

protected:
    // BrnStateManager.h:94 (DWARF): the CPU-monitor handle used by the
    // performance-monitor bookkeeping. Pinned by name.
    s32 miCpuMonitor;
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_STATE_MANAGER_H
