#include "GameSource/Sound/Module/LogicModule/BrnEmitterStateManager.h"

// =============================================================================
// BrnSound::Logic::World::EmitterStateManager — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnEmitterStateManager.h for the
// CgsSound::Logic::StateManager-descendant shape and the offset/ODR notes.
//
// This TU's recon'd function set is exactly two entries:
//   EmitterStateManager()    (the ctor)                       @ 0x826FE290
//   ~EmitterStateManager()   (the `vector deleting destructor`) @ 0x826FE2E0
//
// dep_flags: depends on two DONE TUs, called BY NAME / structurally:
//   * CgsSound::Logic::StateManager::StateManager @ 0x826FAA18 — the base ctor the
//     X360 ctor `bl`s into; reproduced here as the implicit base-class construction.
//   * ObjectPool<StateManager::RegisteredContent,4,int>::~ObjectPool @ 0x826EAC90 —
//     the member-pool destructor the X360 dtor calls on the +0x30 sub-object;
//     reproduced here as the implicit member destruction of maContentPool.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace World
{

// ---------------------------------------------------------------------------
// EmitterStateManager  @ 0x826FE290
//
//   bl   CgsSound::Logic::StateManager::StateManager   ; base ctor
//   stw  off_820AB608, 0x90(this)                      ; (transient) secondary vtable
//   stw  off_820B81C0, 0(this)                         ; primary vtable
//   stw  off_820B81B8, 0x90(this)                      ; secondary vtable (final)
//   stw  off_820B1738, 0x98(this)                      ; secondary vtable (final)
//   return this
//
// The four vtable stores are the compiler's devirtualisation of the base/derived and
// secondary sub-object vptrs; in reconstructed C++ they are produced implicitly by the
// construction of a polymorphic class deriving from CgsSound::Logic::StateManager, so
// the BODY here is empty — the only observable work is the base construction (handled
// by the member-init list / implicit base ctor) and the member-pool default
// construction (maContentPool).
// ---------------------------------------------------------------------------
EmitterStateManager::EmitterStateManager()
    : CgsSound::Logic::StateManager()
{
}

// ---------------------------------------------------------------------------
// ~EmitterStateManager  @ 0x826FE2E0  (the X360 `vector deleting destructor`)
//
//   stw  off_820B66E4, 0(this)                                  ; (transient) vtable
//   bl   ObjectPool<RegisteredContent,4,int>::~ObjectPool(this+0x30) ; destruct pool
//   stw  off_820AA820, 0(this)                                  ; MemBase vtable
//   if (a2 & 1) { ... deallocate via off_82FFB954 (the MemBase allocator) }
//   return this
//
// The pool teardown at +0x30 and the vtable re-installs are produced implicitly by the
// destructor chain: destructing maContentPool runs its ~ObjectPool (and therefore each
// RegisteredContent reference drop), which is exactly the X360 sub-object destructor
// call. The BODY here is therefore empty.
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free the
// object; that allocator is not homed here, so operator-delete dispatch is left to the
// host toolchain (the `delete` half of the X360 vector deleting destructor).
// ---------------------------------------------------------------------------
EmitterStateManager::~EmitterStateManager()
{
}

// =============================================================================
// RTTI/factory + boot virtual grown in this slice (so EmitterStateManager is a
// CONCRETE, registrable leaf the StateManager factory CreateStateMan @ 0x826A5B60
// can construct). Sources:
//   EmitterStateManager::CreateObject  @ 0x82701518  (real)
//   EmitterStateManager::Prepare       @ 0x826F5AC0  (stub -- domain cascade)
//   EmitterStateManager::GetTypeName   @ 0x826867B8  (real)
// GetTypeInfo / GetStaticTypeInfo were NOT individually exported; reconstructed
// from the established in-tree RTTI pattern (CgsStateManager.cpp GetStaticTypeInfo).
//
// FLAG (IResourceRequester sub-object deferred -- committed-home modelling choice):
// the X360 EmitterStateManager ALSO carries a BrnSound::Logic::IResourceRequester
// sub-object (ctor @ 0x826FE290 installs secondary vtables @ +0x90/+0x98; it has an
// IResourceRequester completion callback ResourcesAreReady @ 0x826867C8 setting the
// prepare-state, and Prepare's real body calls IResourceRequester::LoadAsset(this+
// 0x90, ...)). The committed home (see header) derives ONLY CgsSound::Logic::
// StateManager (the RegisteredContent partial view, which has no IResourceRequester
// and no pure virtuals -- so this leaf is already concrete). Adding IResourceRequester
// here would force the full-vs-minimal StateManager ODR fork into this TU; it is left
// deferred to preserve the committed base/ctor/dtor. Consequence: ResourcesAreReady /
// LoadAsset are NOT modelled in this slice (boot only needs Prepare()==true).
// =============================================================================

// ---------------------------------------------------------------------------
// EmitterStateManager::CreateObject(u32)  @ 0x82701518   (the factory hook)
//
//   if ( a1 ) { if ( MemBase::operator new(1088, "EmitterStateManager", 1) ) return new'd ctor; }
//   else      { if ( MemBase::operator new(1088, "EmitterStateManager", 0) ) return new'd ctor; }
//   return 0;
//
// The X360 allocates a 1088-byte (0x440) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "EmitterStateManager" (off_82F2F850) and placement-
// constructs an EmitterStateManager. Both arms call the SAME size+ctor; the `a1`
// argument only selects the operator-new flavour (0/1). The factory CreateStateMan
// @ 0x826A5B60 calls this as createObject(0).
//
// FLAG (allocator gate): CgsSound::MemBase (CgsMemBase.h) does NOT model operator
// new(size, tag, flavour) -- the sound allocator (off_82FFB954) is not homed in this
// group -- so a faithful placement-new through it is not yet expressible. This uses
// the host `new` (global operator new, NOT the sound allocator); the observable
// result -- a constructed EmitterStateManager* (or null) -- matches. Replace with the
// sound-allocator placement-new once MemBase::operator new is homed. The 1088-byte
// size is the X360 0x440; the host object differs in size, so the literal is
// documentation only and is NOT passed to the host new.
// ---------------------------------------------------------------------------
CgsSound::Logic::StateManager* EmitterStateManager::CreateObject( u32 /*luType*/ )
{
    return new EmitterStateManager();
}

// ---------------------------------------------------------------------------
// EmitterStateManager::GetStaticTypeInfo()  (RTTI descriptor)
//
// Mirrors the in-tree GetStaticTypeInfo convention (CgsStateManager.cpp:230): a
// function-local static ClassTypeInfo<StateManager> seeded with (ObjectID, typeName,
// baseTypeInfo, createObject) so CreateStateMan matches descriptor->ObjectID and
// calls ->createObject.
//
// FLAG (ObjectID UNRESOLVED): the per-leaf registration static-init that calls
// StateManager::AddToClassTypeInfoArray(@0x8268DFE8) with the explicit ObjectID was
// NOT exported (CreateObject @ 0x82701518 has no xrefs_to) and no map-state enum
// names the id in-tree. Per the established in-tree placeholder convention (every
// committed GetStaticTypeInfo uses 0), the ObjectID is seeded 0 here and MUST be
// replaced with the real id at integration -- the id is this manager's slot in the
// CreateStateManagers 0..8 sequence (@ 0x826AFEF8).
//
// FLAG (registry hookup deferred): this TU's RegisteredContent StateManager view does
// NOT declare AddToClassTypeInfoArray (that lives in the full CgsStateManager.h view,
// ODR-incompatible with the RegisteredContent view used here for the content pool, so
// not co-includable). The descriptor is produced here but its insertion into the
// static registry (dword_82FFBC58) must be done by a registration site using the full
// StateManager view (the conductor-owned CreateStateMan TU). &CreateObject is an
// ABI-compatible StateManager*(*)(u32) across both views.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* EmitterStateManager::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> sTypeInfo =
    {
        0,                       // ObjectID         -- FLAG UNRESOLVED (placeholder 0)
        "EmitterStateManager",   // mpcTypeName
        0,                       // mpBaseTypeInfo   -- StateManager base descriptor (deferred)
        &EmitterStateManager::CreateObject // mpfnCreateObject
    };
    return &sTypeInfo;
}

// ---------------------------------------------------------------------------
// EmitterStateManager::GetTypeInfo() const  (vtable RTTI hook)
//   Returns this leaf's static descriptor.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* EmitterStateManager::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

// ---------------------------------------------------------------------------
// EmitterStateManager::GetTypeName() const  @ 0x826867B8
//   X360: `lwz r3, off_82F2F850` -> returns the literal "EmitterStateManager".
// ---------------------------------------------------------------------------
const char* EmitterStateManager::GetTypeName() const
{
    return "EmitterStateManager";
}

// ---------------------------------------------------------------------------
// EmitterStateManager::Prepare()  @ 0x826F5AC0   (vtable +0x0C in the real layout)
//
// X360 body: a switch on the +0x24 prepare-state (cases 0/5 -> 0, 1, 2, 3, 4):
//   state 1: assert(mpLogicModule != 0); clear the world-scene fields; SoundWorldScene
//            ::Prepare(); LoadAsset(this+0x90, off_82F2CE28[0], 0, 0);
//            mCpuMonitor = PerfMonCpu::AddMonitor("Emitters", 14, 0, 1.0, ...)
//   state 3: if (!StateManager::PrepareStates(this, 1, 4, 0)) stay;
//   state 4: return 1;
//
// FLAG (stub -- domain cascade): the real body cascades into
//   * BrnSound::Logic::World::SoundWorldScene::Prepare (the world emitter scene),
//   * BrnSound::Logic::IResourceRequester::LoadAsset (the streaming-resource broker;
//     the IResourceRequester sub-object is itself deferred -- see the TU-level FLAG),
//   * CgsDev::PerfMonCpu::AddMonitor (the CPU perf monitor), and
//   * CgsSound::Logic::StateManager::PrepareStates @ 0x826EAD30 (the State machine,
//     a declared-only stub in the foundation).
// None reconstructed in this slice. PrepareStateManagersOnBoot (0x826837F8) only
// needs Prepare() to return true to advance boot, so this stub returns true WITHOUT
// the world-scene prep / asset load / state bring-up. NOT an X360-faithful body --
// the prepare state machine + SoundWorldScene are deferred. X360 addr above.
// ---------------------------------------------------------------------------
bool EmitterStateManager::Prepare()
{
    return true;
}

} // namespace World
} // namespace Logic
} // namespace BrnSound
