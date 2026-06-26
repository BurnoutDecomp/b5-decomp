#include "GameSource/Sound/Module/LogicModule/BrnEmitterStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

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
    : BrnSound::Logic::BrnStateManager()
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
// ODR FOLD (2026-06-25): Emitter now derives the canonical BrnSound::Logic::
// BrnStateManager (-> the FULL CgsSound::Logic::StateManager + IResourceRequester),
// the same base the eight sibling managers use, instead of the RegisteredContent
// partial view of CgsSound::Logic::StateManager. The X360 EmitterStateManager DOES
// carry a BrnSound::Logic::IResourceRequester sub-object (ctor @ 0x826FE290 installs
// secondary vtables @ +0x90/+0x98; its completion callback ResourcesAreReady
// @ 0x826867C8 sets the prepare-state, and Prepare's real body calls
// IResourceRequester::LoadAsset(this+0x90, ...)), so this leaf now overrides the two
// IResourceRequester pure virtuals (ResourcesAreReady / GetResourceRegistrar) to stay
// CONCRETE -- consistent with the siblings. Their deep bodies (the world-emitter
// scene + the resolved Content) remain deferred (boot only needs Prepare()==true).
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
// ObjectID RESOLVED = 7: the PS3 DecFIGS __static_initialization_and_destruction_0
// (0x85FA1C) sets BrnSound::Logic::World::EmitterStateManager::sTypeInfo.ObjectID = 7
// (its slot in the CreateStateManagers 0..8 sequence; the other manager ids there are
// PlayerVehicle=1, AIVehicle=2, Traffic=3, Passby=4, Collision=5, Streaming=6,
// Emitter=7, GlobalStateManager=0). The X360 CreateObject @ 0x82701518 has no
// xrefs_to so the id was not directly exported from the X360, but the PS3 (same source)
// names it explicitly; used here.
//
// FLAG (registry hookup deferred): the full CgsSound::Logic::StateManager view (now
// the base via BrnStateManager.h) DOES declare AddToClassTypeInfoArray, but the
// per-leaf static-init that calls it with this leaf's ObjectID is not wired here (it
// is the conductor-owned registration step). The descriptor is produced here; its
// insertion into the static registry (StateManager::AddToClassTypeInfoArray) is done
// at the wiring step. &CreateObject is the StateManager*(*)(u32) the canonical
// ClassTypeInfo::createObject stores.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* EmitterStateManager::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> sTypeInfo(
        7,                       // ObjectID (PS3 DecFIGS __static_init 0x85FA1C: EmitterStateManager::sTypeInfo.ObjectID = 7)
        "EmitterStateManager",   // typeName
        0,                       // baseTypeInfo     -- StateManager base descriptor (deferred)
        &EmitterStateManager::CreateObject // createObject
    );
    return &sTypeInfo;
}

// ---------------------------------------------------------------------------
// File-scope registration (Part D): land this leaf's descriptor in the shared
// StateManager RTTI registry (CgsStateManager.cpp gapClassTypeInfoArray, X360
// dword_82FFBC58) at load time, so the factory StateManager::CreateStateMan
// (0x826A5B60) can find it by ObjectID and CreateStateManagers (0x826AFEF8) can
// construct it. AddToClassTypeInfoArray is the canonical StateManager registration
// entry (@ 0x8268DFE8), reached through the BrnStateManager base.
//
// ObjectID = 7 (RESOLVED, see GetStaticTypeInfo above): the PS3 DecFIGS __static_init
// (0x85FA1C) sets EmitterStateManager::sTypeInfo.ObjectID = 7 and appends &sTypeInfo to
// CgsSound::Logic::StateManager::ClassTypeInfoArray -- exactly the registration this
// static-init reproduces (the descriptor is seeded 7 by GetStaticTypeInfo, then inserted
// here). This TU is OUT of the build, so the registration is dormant until the conductor
// adds it.
// ---------------------------------------------------------------------------
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* const
    gpEmitterStateManagerReg =
        CgsSound::Logic::StateManager::AddToClassTypeInfoArray(
            EmitterStateManager::GetStaticTypeInfo());

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
//   * BrnSound::Logic::IResourceRequester::LoadAsset (the streaming-resource broker,
//     reached through the now-inherited IResourceRequester sub-object),
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

// ---------------------------------------------------------------------------
// EmitterStateManager::ResourcesAreReady()  @ 0x826867C8  (IResourceRequester completion callback)
//
// X360 body: once the emitter splicer-bank bundle resolves, it advances the +0x24
// prepare-state and seeds the world-emitter Content. It is invoked by the resource
// broker only AFTER Prepare's LoadAsset resolves.
//
// FLAG (stub -- world/Content cascade): the real body cascades into the world-emitter
// scene + the resolved CgsSound::Logic::Content, neither reconstructed in this slice;
// and it is never reached on boot (this slice's Prepare stub issues no LoadAsset).
// Bodied as a no-op so the leaf is concrete; NOT X360-faithful. Deferred with the
// world emitter audio domain.
// ---------------------------------------------------------------------------
void EmitterStateManager::ResourcesAreReady()
{
}

// ---------------------------------------------------------------------------
// EmitterStateManager::GetResourceRegistrar()  (IResourceRequester slot 1)
//
// Recovered semantically from the sibling BrnEffectObject::GetResourceRegistrar
// @ 0x82696850: load this->mpLogicModule (+0x2C), tail-call the IResourceRequester
// slot-1 of the module's embedded ResourceRegistrar. The state-manager leaves share
// the +0x2C module back-pointer (stamped by CreateStateMan).
//
// FLAG (module opaque): the canonical CgsSound::Logic::StateManager exposes
// mpLogicModule by name, but the SoundLogicModule home is not reconstructed in this
// slice, so this cannot be bodied faithfully here. Provided as a non-cascading stub
// that abort-asserts if ever reached on boot (PrepareStateManagersOnBoot does NOT call
// it; it is only used on the per-frame attach/detach path this slice never exercises).
// Returns a TU-local empty registrar purely to satisfy the non-void signature; NOT a
// faithful body. Body via the module once the SoundLogicModule is available.
// ---------------------------------------------------------------------------
BrnSound::Logic::ResourceRegistrar& EmitterStateManager::GetResourceRegistrar()
{
    CGS_ASSERT( false,
                "EmitterStateManager::GetResourceRegistrar reached without a homed "
                "SoundLogicModule (boot path does not call this)" );
    static BrnSound::Logic::ResourceRegistrar sUnhomedRegistrar;
    return sUnhomedRegistrar;
}

} // namespace World
} // namespace Logic
} // namespace BrnSound
