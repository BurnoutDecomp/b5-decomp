#include "GameSource/Sound/Global/BrnGlobalStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// =============================================================================
// BrnSound::Logic::GlobalStateManager -- out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// This canonical home brings the game-global sound-logic state manager up as a
// CONCRETE, registrable leaf the StateManager factory CreateStateMan @ 0x826A5B60
// can construct. The global Voice/Content audio domain is deferred -- see the
// per-function FLAGs.
//
// Sources:
//   GlobalStateManager::CreateObject  @ 0x826FE380  (real)
//   GlobalStateManager::Prepare       @ 0x826F5BF8  (stub -- Voice/Content cascade)
//   GlobalStateManager::ctor          @ 0x826FBB50  (real -- self-contained shell)
//   GlobalStateManager::dtor          @ 0x826FBBE8  (minimal -- Voice/Content cascade)
// GetTypeInfo / GetTypeName / GetStaticTypeInfo / GetResourceRegistrar /
// ResourcesAreReady were NOT individually exported; reconstructed from the
// established in-tree RTTI pattern + the sibling BrnEffectObject::GetResourceRegistrar
// @ 0x82696850. GetTypeName returns the "GlobalStateManager" literal (off_82F2F860,
// the tag CreateObject's operator new uses).
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// GlobalStateManager::GlobalStateManager()  ctor @ 0x826FBB50
//
//   result = CgsSound::Logic::StateManager::StateManager();   ; base ctor
//   *(result+144) = off_820AB608;                             ; (transient) +0x90 vtable
//   *result       = off_820B7DA8;                             ; primary vtable @ +0
//   *(result+144) = off_820B7DA0;                             ; IResourceRequester vtable @ +0x90
//   *(result+152) = &off_820B0E20; result[156]=result[160]=0; ; global-Voice sub-obj A (+0x98)
//   *(result+164) = &off_820B0E20; result[168]=result[172]=0; ; global-Voice sub-obj B (+0xA4)
//   *(result+176) = &off_820B3250; result[180]=result[184]=0; ; Content sub-obj A (+0xB0)
//   *(result+188) = &off_820B3250; result[192]=result[196]=0; ; Content sub-obj B (+0xBC)
//   result[200] = 0;                                          ; +0xC8 tail word
//   return result;
//
// The two vtable stores at +0/+0x90 are the compiler's devirtualisation of the
// base/derived (primary) and IResourceRequester sub-object vptrs; produced implicitly
// by constructing a polymorphic class deriving from BrnStateManager. The only
// observable work to express here is the base construction (implicit base ctor) and
// the seed-init of the global members.
//
// FLAG (deferred member init -- self-contained shell): the X360 ctor builds two
// refcounted global-Voice sub-objects (+0x98/+0xA4, each {&off_820B0E20, 0, 0}) and
// two CgsSound::Logic::Content splicer-bank sub-objects (+0xB0/+0xBC, each
// {&off_820B3250, 0, 0}). Those members are the deferred global Voice/Content state
// (maDeferredGlobalState, see header FLAG) and are NOT individually modelled, so this
// ctor reproduces only the base construction; the deferred pad is left default-
// initialised. NOT a member-for-member faithful body -- the global Voice/Content sub-
// objects are deferred with the global audio domain. They are never constructed-into
// by this slice (Prepare stubbed -- no Voice::Construct / Content::Construct), so
// there is nothing for the dtor to release.
// ---------------------------------------------------------------------------
GlobalStateManager::GlobalStateManager()
    : BrnSound::Logic::BrnStateManager()
{
}

// ---------------------------------------------------------------------------
// GlobalStateManager::~GlobalStateManager()  @ 0x826FBBE8  (the X360 `vector deleting destructor`)
//
//   a1[47] = &off_820B3250; if (a1[48]) CgsSound::Playback::Object::Release(...);  ; Content B
//   a1[44] = &off_820B3250; if (a1[45]) CgsSound::Playback::Object::Release(...);  ; Content A
//   for (two global-Voice sub-objs at a1+44 stepping -3 dwords):                   ; Voice B then A
//       *v4 = &off_820B0E20;
//       if (v4[1]) { assert(refcount>0); if (--refcount==0) (*vtbl[1])(obj); }     ; refcount drop
//   *a1 = off_820B66E4;
//   CgsSound::Logic::StateManager::RegisteredContent_4_int_::~ObjectPool(a1 + 12); ; base pool teardown
//   *a1 = &off_820AA820;                                                           ; MemBase vtable
//
// FLAG (minimal -- Voice/Content cascade): the real dtor releases the two splicer-bank
// Content sub-objects (CgsSound::Playback::Object::Release) and decrements the two
// refcounted global-Voice sub-objects (the CgsSound/Playback/CgsObject.h refcount drop
// at line 117), then tears down the base RegisteredContent ObjectPool at +0xC (its
// ~ObjectPool) and re-installs the MemBase vtable. The Content/Voice sub-objects are
// the deferred maDeferredGlobalState (see header) and are never constructed by this
// slice's stubbed Prepare, so there is nothing to release; the base pool teardown +
// vtable re-install are re-synthesised by the host toolchain from this virtual
// destructor + the base ~BrnStateManager. NOT a member-for-member faithful body --
// the Voice/Content release legs are deferred with the global audio domain.
// ---------------------------------------------------------------------------
GlobalStateManager::~GlobalStateManager()
{
}

// ---------------------------------------------------------------------------
// GlobalStateManager::CreateObject(u32)  @ 0x826FE380   (the factory hook)
//
//   if ( a1 ) { if ( MemBase::operator new(208, "GlobalStateManager", 1) ) return new'd ctor; }
//   else      { if ( MemBase::operator new(208, "GlobalStateManager", 0) ) return new'd ctor; }
//   return 0;
//
// The X360 allocates a 208-byte (0xD0) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "GlobalStateManager" (off_82F2F860) and placement-
// constructs a GlobalStateManager into it. Both arms call the SAME size+ctor; the
// `a1` argument only selects the operator-new flavour (0/1). The factory
// CreateStateMan @ 0x826A5B60 calls this as createObject(0).
//
// FLAG (allocator gate): CgsSound::MemBase (CgsMemBase.h) does NOT model operator
// new(size, tag, flavour) -- the sound allocator (off_82FFB954) is not homed in this
// group -- so a faithful placement-new through that allocator is not yet expressible.
// This reconstruction uses the host `new` (global operator new, NOT the sound
// allocator); the observable result -- a constructed GlobalStateManager* (or null)
// handed to the factory -- matches. Replace with the sound-allocator placement-new
// once MemBase::operator new is homed. The 208-byte size is the X360 0xD0; on the
// 64-bit host the real object differs in size, so the literal is documentation only
// and is NOT passed to the host new.
// ---------------------------------------------------------------------------
CgsSound::Logic::StateManager* GlobalStateManager::CreateObject( u32 /*luType*/ )
{
    return new GlobalStateManager();
}

// ---------------------------------------------------------------------------
// GlobalStateManager::GetStaticTypeInfo()  (RTTI descriptor)
//
// Mirrors the in-tree GetStaticTypeInfo convention (CgsStateManager.cpp:230). A
// function-local static ClassTypeInfo<StateManager> seeded with (ObjectID, typeName,
// baseTypeInfo, createObject) so the factory CreateStateMan can match
// descriptor->ObjectID and call ->createObject.
//
// FLAG (ObjectID UNRESOLVED): the per-leaf registration static-init that calls
// StateManager::AddToClassTypeInfoArray(@0x8268DFE8) with the explicit ObjectID was
// NOT exported (CreateObject @ 0x826FE380 has no xrefs_to) and no map-state enum names
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
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GlobalStateManager::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> sTypeInfo(
        0,                          // ObjectID (PS3 DecFIGS static-init 0x85FA1C: GlobalStateManager=0)
        "GlobalStateManager",       // typeName
        0,                          // baseTypeInfo     -- StateManager base descriptor (deferred)
        &GlobalStateManager::CreateObject // createObject
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
// (CreateObject @ 0x826FE380 has no xrefs_to). Assigned 6 here as a unique-among-the-9
// placeholder; the real id is this manager's slot in the CreateStateManagers 0..8 loop
// (@ 0x826AFEF8) -- pin at integration. This TU is OUT of the build, so dormant until
// the conductor adds it.
// ---------------------------------------------------------------------------
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* const
    gpGlobalStateManagerReg =
        CgsSound::Logic::StateManager::AddToClassTypeInfoArray(
            GlobalStateManager::GetStaticTypeInfo());

// ---------------------------------------------------------------------------
// GlobalStateManager::GetTypeInfo() const  (vtable RTTI hook)
//   Returns this leaf's static descriptor.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GlobalStateManager::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

// ---------------------------------------------------------------------------
// GlobalStateManager::GetTypeName() const
//   The X360 leaf GetTypeName loads the tag string (off_82F2F860) used by
//   CreateObject's operator new -> returns the literal "GlobalStateManager".
// ---------------------------------------------------------------------------
const char* GlobalStateManager::GetTypeName() const
{
    return "GlobalStateManager";
}

// ---------------------------------------------------------------------------
// GlobalStateManager::Prepare()  @ 0x826F5BF8   (vtable +0x0C)
//
// X360 body: a switch on the +0x24 prepare-state (cases 0/5 -> 0, 1, 2, 3, 4):
//   state 1: Content::Construct + LoadAsset the global content banks;
//            Voice::Construct + Voice::Connect + Voice::SetGain the global voices;
//   state 2: if (!Content::IsLoaded(...) || !Voice::IsReady(...)) return 0;
//            mCpuMonitor = PerfMonCpu::AddMonitor(...)
//   state 3: if (!StateManager::PrepareStates(...)) return 0;
//   state 4: return 1;
//
// FLAG (stub -- Voice/Content domain cascade): the real body cascades into
//   * CgsSound::Logic::Voice::Construct / Voice::Connect / Voice::SetGain /
//     Voice::IsReady (the global-voice graph -- the deferred Voice sub-objects),
//   * CgsSound::Logic::Content::IsLoaded (the global content banks),
//   * CgsSound::Playback::Name::MakeHash (the content/voice-name hasher),
//   * BrnSound::Logic::IResourceRequester::LoadAsset (the streaming-resource broker),
//   * CgsDev::PerfMonCpu::AddMonitor (the CPU perf monitor), and
//   * CgsSound::Logic::StateManager::PrepareStates @ 0x826EAD30 (the State machine,
//     itself a declared-only stub in the foundation).
// None reconstructed in this slice. PrepareStateManagersOnBoot (0x826837F8) only needs
// Prepare() to return true to advance boot, so this stub returns true (boot-ready)
// WITHOUT the global voice/content bring-up. NOT an X360-faithful body -- the prepare
// state machine + global Voice/Content domain are deferred. X360 addr above.
// ---------------------------------------------------------------------------
bool GlobalStateManager::Prepare()
{
    return true;
}

// ---------------------------------------------------------------------------
// GlobalStateManager::ResourcesAreReady()  (IResourceRequester completion callback)
//
// FLAG (stub -- Voice/Content domain cascade): the IResourceRequester completion
// callback (invoked by the resource broker once the global content banks resolve)
// seeds the global Voice/Content state -- the deferred global audio domain. Not
// reconstructed in this slice and not needed for boot: it is invoked only AFTER
// LoadAsset resolves, which this slice's Prepare stub never issues. Bodied as a no-op
// so the leaf is concrete; NOT X360-faithful. Deferred with the global audio domain.
// ---------------------------------------------------------------------------
void GlobalStateManager::ResourcesAreReady()
{
}

// ---------------------------------------------------------------------------
// GlobalStateManager::GetResourceRegistrar()  (IResourceRequester slot 1)
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
BrnSound::Logic::ResourceRegistrar& GlobalStateManager::GetResourceRegistrar()
{
    CGS_ASSERT( false,
                "GlobalStateManager::GetResourceRegistrar reached without a homed "
                "SoundLogicModule (boot path does not call this)" );
    static BrnSound::Logic::ResourceRegistrar sUnhomedRegistrar;
    return sUnhomedRegistrar;
}

} // namespace Logic
} // namespace BrnSound
