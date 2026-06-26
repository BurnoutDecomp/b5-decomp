#include "GameSource/Sound/Collision/BrnCollisionStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// =============================================================================
// BrnSound::Logic::Collision::CollisionStateManager -- out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// This canonical home brings the collision / crash sound-logic state manager up as a
// CONCRETE, registrable leaf the StateManager factory CreateStateMan @ 0x826A5B60
// can construct. The collision audio domain (per-material collision generators, crash
// attribute tables, crash voices) is the LARGEST of the managers and is deferred WHOLE
// -- see the per-function FLAGs.
//
// Sources:
//   CollisionStateManager::CreateObject  @ 0x82701FA8  (real)
//   CollisionStateManager::Prepare       @ 0x826F8B78  (stub -- collision-domain cascade)
//   CollisionStateManager::ctor          @ 0x826FFAC0  (MINIMAL -- heavy construction deferred)
//   CollisionStateManager::dtor          @ 0x826FFD48  (minimal -- collision-domain cascade)
// GetTypeInfo / GetTypeName / GetStaticTypeInfo / GetResourceRegistrar /
// ResourcesAreReady were NOT individually exported; reconstructed from the
// established in-tree RTTI pattern + the sibling BrnEffectObject::GetResourceRegistrar
// @ 0x82696850. GetTypeName returns the "CollisionStateManager" literal (off_82F2F950,
// the tag CreateObject's operator new uses).
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// ---------------------------------------------------------------------------
// CollisionStateManager::CollisionStateManager()  ctor @ 0x826FFAC0  (HEAVY)
//
//   CgsSound::Logic::StateManager::StateManager();           ; base ctor
//   *(a1+144) = off_820AB608;                                ; (transient) +0x90 vtable
//   *a1       = off_820B844C;                                ; primary vtable @ +0
//   *(a1+144) = off_820B8444;                                ; IResourceRequester vtable @ +0x90
//   short_65536_::SelectionHistory_512(a1 + 0x8B0);          ; a SelectionHistory<512>
//   <zero a 32-entry table @ +0x1300 (stride 28)>
//   <zero a 500-entry table @ +0x1670 (stride 16)>
//   <zero a 16-entry table @ +0x1E69 (stride 48)>
//   <64x: vector-construct BaseCollisionGenerator arrays @ +0x2170 (16+4 each, stride 160)>
//   <64x: vector-construct BaseCollisionGenerator arrays @ +0x4990 (4+16 each, stride 224)>
//   <seed scalar block @ +0x81A0..>
//   <build crash Content sub-objects @ +0x8210.. ({&off_820B3250,0,0})>
//   Attrib::Gen::crashbinlist::crashbinlist(a1 + 0x8234, 0, 0);
//   Attrib::Gen::propscrashbinlist::propscrashbinlist(a1 + 0x8244, 0, 0);
//   Attrib::Gen::proptomaterialmappings::proptomaterialmappings(a1 + 0x8254, 0, 0);
//   <seed tail @ +0x8264..>
//   return a1;
//
// The two vtable stores at +0/+0x90 are produced implicitly by constructing a
// polymorphic class deriving from BrnStateManager.
//
// FLAG (MINIMAL ctor -- heavy construction DEFERRED, do NOT pull the collision domain):
// per the task constraint, this shell does a MINIMAL ctor that only forwards to the
// BrnStateManager base (which value-inits the modelled base members). The X360 ctor's
// heavy construction -- the SelectionHistory<512>, the four zeroed tables, the TWO
// 64-entry arrays of CgsSceneManager::CgsCollision::BaseCollisionGenerator (vector-
// constructed via _vector_constructor_iterator_), the three Attrib::Gen attribute
// tables (crashbinlist / propscrashbinlist / proptomaterialmappings), and the crash
// Content sub-objects -- is NOT reproduced here: doing so would require pulling in the
// entire collision domain (CgsSceneManager::CgsCollision + the AttribSys-generated
// crash tables), which this slice deliberately does NOT do. Those members are the
// deferred maDeferredCollisionState (see header) and are left default-initialised. NOT
// a member-for-member faithful body -- the whole collision construction is deferred.
// The crash Content sub-objects are never constructed-into by this slice (Prepare
// stubbed), so there is nothing for the dtor to release.
// ---------------------------------------------------------------------------
CollisionStateManager::CollisionStateManager()
    : BrnSound::Logic::BrnStateManager()
{
}

// ---------------------------------------------------------------------------
// CollisionStateManager::~CollisionStateManager()  @ 0x826FFD48  (the X360 `vector deleting destructor`)
//
//   Attrib::Instance::~Instance(a1 + 0x8234);   ; proptomaterialmappings / crash tables
//   Attrib::Instance::~Instance(a1 + 0x8224);
//   Attrib::Instance::~Instance(a1 + 0x8214);
//   a1[8330] = &off_820B3250; <drop a refcounted CgsSound/Playback/CgsObject (refcount @ +4)>
//   a1[8327] = &off_820B3250; if (a1[8328]) CgsSound::Playback::Object::Release(...);
//   a1[8324] = &off_820B3250; if (a1[8325]) CgsSound::Playback::Object::Release(...);
//   *a1 = off_820B66E4;
//   CgsSound::Logic::StateManager::RegisteredContent_4_int_::~ObjectPool(a1 + 12);   ; base pool teardown
//   *a1 = &off_820AA820;                                                             ; MemBase vtable
//
// FLAG (minimal -- collision-domain cascade): the real dtor destructs the three
// Attrib::Instance crash tables, drops one refcounted CgsSound::Playback object (the
// CgsObject.h refcount drop at line 117) and releases two CgsSound::Playback::Object
// Content sub-objects, then tears down the base RegisteredContent ObjectPool at +0xC
// (its ~ObjectPool) and re-installs the MemBase vtable. The Attrib tables + Content
// sub-objects are the deferred maDeferredCollisionState (see header) and are never
// constructed by this slice's MINIMAL ctor / stubbed Prepare, so there is nothing to
// destruct or release; the base pool teardown + vtable re-install are re-synthesised
// by the host toolchain from this virtual destructor + the base ~BrnStateManager. NOT
// a member-for-member faithful body -- the collision teardown legs are deferred with
// the collision audio domain.
// ---------------------------------------------------------------------------
CollisionStateManager::~CollisionStateManager()
{
}

// ---------------------------------------------------------------------------
// CollisionStateManager::CreateObject(u32)  @ 0x82701FA8   (the factory hook)
//
//   if ( a1 ) { if ( MemBase::operator new(33408, "CollisionStateManager", 1) ) return new'd ctor; }
//   else      { if ( MemBase::operator new(33408, "CollisionStateManager", 0) ) return new'd ctor; }
//   return 0;
//
// The X360 allocates a 33408-byte (0x8280) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "CollisionStateManager" (off_82F2F950) and placement-
// constructs a CollisionStateManager into it. Both arms call the SAME size+ctor; the
// `a1` argument only selects the operator-new flavour (0/1). The factory CreateStateMan
// @ 0x826A5B60 calls this as createObject(0).
//
// FLAG (allocator gate): CgsSound::MemBase (CgsMemBase.h) does NOT model operator
// new(size, tag, flavour) -- the sound allocator (off_82FFB954) is not homed in this
// group -- so a faithful placement-new through that allocator is not yet expressible.
// This reconstruction uses the host `new` (global operator new, NOT the sound
// allocator); the observable result -- a constructed CollisionStateManager* (or null)
// handed to the factory -- matches. Replace with the sound-allocator placement-new
// once MemBase::operator new is homed. The 33408-byte size is the X360 0x8280; on the
// 64-bit host the real object differs in size (and is FAR smaller here -- the ~33KB of
// collision state is the deferred pad), so the literal is documentation only and is
// NOT passed to the host new.
// ---------------------------------------------------------------------------
CgsSound::Logic::StateManager* CollisionStateManager::CreateObject( u32 /*luType*/ )
{
    return new CollisionStateManager();
}

// ---------------------------------------------------------------------------
// CollisionStateManager::GetStaticTypeInfo()  (RTTI descriptor)
//
// Mirrors the in-tree GetStaticTypeInfo convention (CgsStateManager.cpp:230). A
// function-local static ClassTypeInfo<StateManager> seeded with (ObjectID, typeName,
// baseTypeInfo, createObject) so the factory CreateStateMan can match
// descriptor->ObjectID and call ->createObject.
//
// FLAG (ObjectID UNRESOLVED): the per-leaf registration static-init that calls
// StateManager::AddToClassTypeInfoArray(@0x8268DFE8) with the explicit ObjectID was
// NOT exported (CreateObject @ 0x82701FA8 has no xrefs_to) and no map-state enum names
// the id in-tree. Per the established in-tree placeholder convention (every committed
// GetStaticTypeInfo uses 0), the ObjectID is seeded 0 here and MUST be replaced with
// the real id at integration -- the id is this manager's slot in the
// CreateStateManagers 0..8 sequence (@ 0x826AFEF8).
//
// FLAG (registry hookup deferred): the minimal CgsSound::Logic::StateManager view
// pulled via BrnStateManager.h (this TU's base) does NOT declare
// AddToClassTypeInfoArray (full CgsStateManager.h view only, ODR-incompatible with
// BrnStateManager.h, not co-includable here). The descriptor is produced here but its
// insertion into the static registry (dword_82FFBC58) must be done by a registration
// site using the full StateManager view (the conductor-owned CreateStateMan TU).
// &CreateObject is an ABI-compatible StateManager*(*)(u32) across both views.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* CollisionStateManager::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> sTypeInfo(
        5,                          // ObjectID         -- FLAG: arbitrary-unique (slot 5)
        "CollisionStateManager",    // typeName
        0,                          // baseTypeInfo     -- StateManager base descriptor (deferred)
        &CollisionStateManager::CreateObject // createObject
    );
    return &sTypeInfo;
}

// ---------------------------------------------------------------------------
// File-scope registration (Part D): land this leaf's descriptor in the shared
// StateManager RTTI registry (CgsStateManager.cpp gapClassTypeInfoArray, X360
// dword_82FFBC58) at load time, so StateManager::CreateStateMan (0x826A5B60) can
// find it by ObjectID. AddToClassTypeInfoArray is the canonical StateManager
// registration entry (@ 0x8268DFE8), reached through the BrnStateManager base.
//
// FLAG (ObjectID arbitrary-unique): the exact X360 ObjectID was not exported
// (CreateObject @ 0x82701FA8 has no xrefs_to). Assigned 5 here as a unique-among-the-9
// placeholder; the real id is this manager's slot in the CreateStateManagers 0..8 loop
// (@ 0x826AFEF8) -- pin at integration. This TU is OUT of the build, so dormant until
// the conductor adds it.
// ---------------------------------------------------------------------------
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* const
    gpCollisionStateManagerReg =
        CgsSound::Logic::StateManager::AddToClassTypeInfoArray(
            CollisionStateManager::GetStaticTypeInfo());

// ---------------------------------------------------------------------------
// CollisionStateManager::GetTypeInfo() const  (vtable RTTI hook)
//   Returns this leaf's static descriptor.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* CollisionStateManager::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

// ---------------------------------------------------------------------------
// CollisionStateManager::GetTypeName() const
//   The X360 leaf GetTypeName loads the tag string (off_82F2F950) used by
//   CreateObject's operator new -> returns the literal "CollisionStateManager".
// ---------------------------------------------------------------------------
const char* CollisionStateManager::GetTypeName() const
{
    return "CollisionStateManager";
}

// ---------------------------------------------------------------------------
// CollisionStateManager::Prepare()  @ 0x826F8B78   (vtable +0x0C)
//
// X360 body: a switch on the +0x24 prepare-state (cases 0/5 -> 0, 1, 2, 3, 4):
//   state 1: SetCollisionBinList(this) (seed the per-material crash-bin lists);
//            Content::Construct + LoadAsset the crash splicer banks
//            (MakeHash the crash content names);
//   state 2: if (!Content::IsLoaded(...)) return 0;
//            mCpuMonitor = PerfMonCpu::AddMonitor("Collisions", 14, 0, 1.0, ...)
//   state 3: if (!StateManager::PrepareStates(...)) return 0;
//   state 4: return 1;
//
// FLAG (stub -- collision-domain cascade): the real body cascades into
//   * BrnSound::Logic::Collision::CollisionStateManager::SetCollisionBinList (seeds the
//     per-material collision-generator/crash-bin lists -- the deferred collision domain),
//   * CgsSound::Logic::Content::Construct / Content::IsLoaded (the crash splicer banks),
//   * CgsSound::Playback::Name::MakeHash (the content-name hasher),
//   * BrnSound::Logic::IResourceRequester::LoadAsset (the streaming-resource broker),
//   * CgsDev::PerfMonCpu::AddMonitor (the CPU perf monitor), and
//   * CgsSound::Logic::StateManager::PrepareStates @ 0x826EAD30 (the State machine,
//     itself a declared-only stub in the foundation).
// None reconstructed in this slice. PrepareStateManagersOnBoot (0x826837F8) only needs
// Prepare() to return true to advance boot, so this stub returns true (boot-ready)
// WITHOUT the crash-bin / content bring-up. NOT an X360-faithful body -- the prepare
// state machine + the collision audio domain are deferred. X360 addr above.
// ---------------------------------------------------------------------------
bool CollisionStateManager::Prepare()
{
    return true;
}

// ---------------------------------------------------------------------------
// CollisionStateManager::ResourcesAreReady()  (IResourceRequester completion callback)
//
// FLAG (stub -- collision-domain cascade): the IResourceRequester completion callback
// (invoked by the resource broker once the crash splicer banks resolve) seeds the
// crash voice/content state -- the deferred collision audio domain. Not reconstructed
// in this slice and not needed for boot: it is invoked only AFTER LoadAsset resolves,
// which this slice's Prepare stub never issues. Bodied as a no-op so the leaf is
// concrete; NOT X360-faithful. Deferred with the collision audio domain.
// ---------------------------------------------------------------------------
void CollisionStateManager::ResourcesAreReady()
{
}

// ---------------------------------------------------------------------------
// CollisionStateManager::GetResourceRegistrar()  (IResourceRequester slot 1)
//
// Recovered semantically from the sibling BrnEffectObject::GetResourceRegistrar
// @ 0x82696850: load this->mpLogicModule (+0x2C), tail-call the IResourceRequester
// slot-1 of the module's embedded ResourceRegistrar. The state-manager leaves share
// the +0x2C module back-pointer (stamped by CreateStateMan).
//
// FLAG (module opaque): the minimal CgsSound::Logic::StateManager view in this TU
// (via BrnStateManager.h) does not expose mpLogicModule (+0x2C), and the
// SoundLogicModule home is not reconstructed in this slice -- so this cannot be bodied
// faithfully here. Provided as a non-cascading stub that abort-asserts if ever reached
// on boot (PrepareStateManagersOnBoot does NOT call it; it is only used on the per-
// frame attach/detach path this slice never exercises). Returns a TU-local empty
// registrar purely to satisfy the non-void signature; NOT a faithful body. Body via
// the module once the full StateManager view (mpLogicModule) + SoundLogicModule are
// available.
// ---------------------------------------------------------------------------
BrnSound::Logic::ResourceRegistrar& CollisionStateManager::GetResourceRegistrar()
{
    CGS_ASSERT( false,
                "CollisionStateManager::GetResourceRegistrar reached without a homed "
                "SoundLogicModule (boot path does not call this)" );
    static BrnSound::Logic::ResourceRegistrar sUnhomedRegistrar;
    return sUnhomedRegistrar;
}

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
