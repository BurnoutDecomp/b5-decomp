#include "GameSource/Sound/Traffic/BrnTrafficStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// =============================================================================
// BrnSound::Logic::Traffic::TrafficStateManager -- out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// This canonical home brings the ambient-traffic / horn sound-logic state manager
// up as a CONCRETE, registrable leaf the StateManager factory CreateStateMan
// @ 0x826A5B60 can construct. The deep traffic audio domain (splicer banks, per-
// frame voices) is deferred -- see the per-function FLAGs.
//
// Sources:
//   TrafficStateManager::CreateObject  @ 0x82701208  (real)
//   TrafficStateManager::Prepare       @ 0x826F0E78  (stub -- domain cascade)
//   TrafficStateManager::ctor          @ 0x826FC250  (real -- self-contained shell)
// GetTypeInfo / GetTypeName / GetStaticTypeInfo / GetResourceRegistrar /
// ResourcesAreReady were NOT individually exported; reconstructed from the
// established in-tree RTTI pattern + the sibling BrnEffectObject::GetResourceRegistrar
// @ 0x82696850. GetTypeName returns the "TrafficStateManager" literal (off_82F2E904,
// the tag CreateObject's operator new uses).
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

// ---------------------------------------------------------------------------
// TrafficStateManager::TrafficStateManager()  ctor @ 0x826FC250
//
//   result = CgsSound::Logic::StateManager::StateManager();   ; base ctor
//   result[36] = off_820AB608;                                ; (transient) +0x90 vtable
//   *result    = off_820B7FB4;                                ; primary vtable @ +0
//   result[36] = off_820B7FAC;                                ; IResourceRequester vtable @ +0x90
//   <zero-init a 31-entry table at result+61 (stride 24 dwords)>
//   <init four Content sub-objects at result[808..819] -- each {&off_820B3250,0,0}>
//   return result;
//
// The two vtable stores are the compiler's devirtualisation of the base/derived
// (primary @ +0) and IResourceRequester sub-object (@ +0x90) vptrs; in reconstructed
// C++ they are produced implicitly by constructing a polymorphic class deriving from
// BrnStateManager (-> CgsSound::Logic::StateManager + IResourceRequester), so the
// only observable work to express here is the base construction (handled by the
// implicit base ctor) and the zero/seed-init of the traffic members.
//
// FLAG (deferred member init -- self-contained shell): the X360 ctor zero-inits a
// 31-entry traffic-voice table (result+61.., stride 24 dwords) and constructs four
// CgsSound::Logic::Content splicer-bank sub-objects (result[808..819], each seeded
// {&off_820B3250 vtable, 0, 0} -- i.e. an empty Content). Those members are part of
// the deferred ~3.2KB traffic-voice state (maDeferredTrafficState, see header FLAG)
// and are NOT individually modelled, so this ctor reproduces only the base
// construction; the deferred pad is left default-initialised. NOT a member-for-member
// faithful body -- the traffic-voice table + Content sub-objects are deferred with
// the traffic audio domain. The four Content objects are never constructed-into by
// this slice (Prepare is stubbed), so there is nothing for the dtor to release.
// ---------------------------------------------------------------------------
TrafficStateManager::TrafficStateManager()
    : BrnSound::Logic::BrnStateManager()
{
}

// ---------------------------------------------------------------------------
// TrafficStateManager::~TrafficStateManager()  (the X360 `vector deleting destructor`)
//
// FLAG (minimal): the matching dtor was not individually exported. The base
// BrnStateManager / CgsSound::Logic::StateManager virtual destructor tears down the
// base content pool, re-installs the MemBase vtable and routes storage back to the
// sound allocator (off_82FFB954) -- all re-synthesised by the host toolchain from
// this virtual destructor. The four splicer-bank Content sub-objects are released on
// the real teardown path; this slice never constructs them (Prepare stubbed), so no
// leaf member teardown is modelled.
// ---------------------------------------------------------------------------
TrafficStateManager::~TrafficStateManager()
{
}

// ---------------------------------------------------------------------------
// TrafficStateManager::CreateObject(u32)  @ 0x82701208   (the factory hook)
//
//   if ( a1 ) { if ( MemBase::operator new(3280, "TrafficStateManager", 1) ) return new'd ctor; }
//   else      { if ( MemBase::operator new(3280, "TrafficStateManager", 0) ) return new'd ctor; }
//   return 0;
//
// The X360 allocates a 3280-byte (0xCD0) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "TrafficStateManager" (off_82F2E904) and placement-
// constructs a TrafficStateManager into it. Both arms call the SAME size+ctor; the
// `a1` argument only selects the operator-new flavour (0/1). The factory
// CreateStateMan @ 0x826A5B60 calls this as createObject(0).
//
// FLAG (allocator gate): CgsSound::MemBase (CgsMemBase.h) does NOT model operator
// new(size, tag, flavour) -- the sound allocator (off_82FFB954) is not homed in this
// group -- so a faithful placement-new through that allocator is not yet expressible.
// This reconstruction uses the host `new` (global operator new, NOT the sound
// allocator); the observable result -- a constructed TrafficStateManager* (or null)
// handed to the factory -- matches. Replace with the sound-allocator placement-new
// once MemBase::operator new is homed. The 3280-byte size is the X360 0xCD0; on the
// 64-bit host the real object differs in size, so the literal is documentation only
// and is NOT passed to the host new.
// ---------------------------------------------------------------------------
CgsSound::Logic::StateManager* TrafficStateManager::CreateObject( u32 /*luType*/ )
{
    return new TrafficStateManager();
}

// ---------------------------------------------------------------------------
// TrafficStateManager::GetStaticTypeInfo()  (RTTI descriptor)
//
// Mirrors the in-tree GetStaticTypeInfo convention (CgsStateManager.cpp:230,
// CgsEffectBase.cpp:130): a function-local static ClassTypeInfo<StateManager> seeded
// with (ObjectID, typeName, baseTypeInfo, createObject) so the factory CreateStateMan
// can match descriptor->ObjectID and call ->createObject.
//
// FLAG (ObjectID UNRESOLVED): the per-leaf registration static-init that calls
// StateManager::AddToClassTypeInfoArray(@0x8268DFE8) with the explicit ObjectID was
// NOT exported (CreateObject @ 0x82701208 has no xrefs_to) and no map-state enum
// names the id in-tree. Per the established in-tree placeholder convention (every
// committed GetStaticTypeInfo uses 0), the ObjectID is seeded 0 here and MUST be
// replaced with the real id at integration -- the id is this manager's slot in the
// CreateStateManagers 0..8 sequence (@ 0x826AFEF8).
//
// FLAG (registry hookup deferred): the minimal CgsSound::Logic::StateManager view
// pulled via BrnStateManager.h (this TU's base) does NOT declare
// AddToClassTypeInfoArray (that lives in the full CgsStateManager.h view, ODR-
// incompatible with BrnStateManager.h and not co-includable here). So this descriptor
// is produced here but its insertion into the static registry (dword_82FFBC58) must
// be done by a registration site using the full StateManager view (the conductor-
// owned CreateStateMan TU). &CreateObject is an ABI-compatible StateManager*(*)(u32)
// across both views.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* TrafficStateManager::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> sTypeInfo(
        3,                          // ObjectID (PS3 DecFIGS static-init 0x85FA1C: TrafficStateManager=3)
        "TrafficStateManager",      // typeName
        CgsSound::Logic::StateManager::GetStaticTypeInfo(), // baseTypeInfo (PS3 0x85FA1C: =StateManager::GetStaticTypeInfo())
        &TrafficStateManager::CreateObject // createObject
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
// ObjectID RESOLVED (PS3 DecFIGS static-init 0x85FA1C): TrafficStateManager::sTypeInfo
// .ObjectID = 3. The descriptor comes from GetStaticTypeInfo() (seeded with that id and
// baseTypeInfo = StateManager::GetStaticTypeInfo()), so this registration lands the real
// id. This TU is OUT of the build, so dormant until the conductor adds it.
// ---------------------------------------------------------------------------
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* const
    gpTrafficStateManagerReg =
        CgsSound::Logic::StateManager::AddToClassTypeInfoArray(
            TrafficStateManager::GetStaticTypeInfo());

// ---------------------------------------------------------------------------
// TrafficStateManager::GetTypeInfo() const  (vtable RTTI hook)
//   Returns this leaf's static descriptor.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* TrafficStateManager::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

// ---------------------------------------------------------------------------
// TrafficStateManager::GetTypeName() const
//   The X360 leaf GetTypeName loads the tag string (off_82F2E904) used by
//   CreateObject's operator new -> returns the literal "TrafficStateManager".
// ---------------------------------------------------------------------------
const char* TrafficStateManager::GetTypeName() const
{
    return "TrafficStateManager";
}

// ---------------------------------------------------------------------------
// TrafficStateManager::Prepare()  @ 0x826F0E78   (vtable +0x0C)
//
// X360 body: a switch on the +0x24 prepare-state (a1[9]; cases 0/5 -> 0, 1, 2, 3, 4):
//   state 1: Content::Construct(this+0xCB8, ..., MakeHash("TrafficCsis"));
//            Content::Construct(this+0xCC4, ..., MakeHash("HornsCsis"));
//            LoadAsset(this+0x90, "sound\\aems\\Traffic_Bank.bundle", 0, 0);
//            LoadAsset(this+0x90, "sound\\aems\\patch_bank_horns.bundle", 0, 0);
//   state 2: if (!all four Content::IsLoaded(...)) return 0;
//            mCpuMonitor = PerfMonCpu::AddMonitor("Traffic Cars", 14, 0, 1.0, ...)
//   state 3: if (!StateManager::PrepareStates(this, 15, 6, 0)) return 0;
//   state 4: return 1;
//
// FLAG (stub -- domain cascade): the real body cascades into
//   * CgsSound::Playback::Name::MakeHash (the content-name hasher),
//   * CgsSound::Logic::Content::Construct / Content::IsLoaded (the splicer-bank
//     content objects -- the deferred Content sub-objects),
//   * BrnSound::Logic::IResourceRequester::LoadAsset (the streaming-resource broker),
//   * CgsDev::PerfMonCpu::AddMonitor (the CPU perf monitor), and
//   * CgsSound::Logic::StateManager::PrepareStates @ 0x826EAD30 (the State machine,
//     itself a declared-only stub in the foundation).
// None reconstructed in this slice. PrepareStateManagersOnBoot (0x826837F8) only
// needs Prepare() to return true to advance boot, so this stub returns true
// (boot-ready) WITHOUT running the content/asset load / state bring-up. NOT an
// X360-faithful body -- the prepare state machine + splicer banks are deferred.
// X360 addr above.
// ---------------------------------------------------------------------------
bool TrafficStateManager::Prepare()
{
    return true;
}

// ---------------------------------------------------------------------------
// TrafficStateManager::ResourcesAreReady()  (IResourceRequester completion callback)
//
// FLAG (stub -- domain cascade): the IResourceRequester completion callback (invoked
// by the resource broker once the Traffic_Bank / patch_bank_horns bundles resolve)
// seeds the traffic splicer-bank Content + voice table -- the deferred traffic audio
// domain (Content + the per-voice attribute tables). Not reconstructed in this slice
// and not needed for boot: it is invoked only AFTER LoadAsset resolves, which this
// slice's Prepare stub never issues. Bodied as a no-op so the leaf is concrete; NOT
// X360-faithful. Deferred with the traffic audio domain.
// ---------------------------------------------------------------------------
void TrafficStateManager::ResourcesAreReady()
{
}

// ---------------------------------------------------------------------------
// TrafficStateManager::GetResourceRegistrar()  (IResourceRequester slot 1)
//
// Recovered semantically from the sibling BrnEffectObject::GetResourceRegistrar
// @ 0x82696850: load this->mpLogicModule (+0x2C), tail-call the IResourceRequester
// slot-1 of the module's embedded ResourceRegistrar (SoundLogicModule::
// mResourceRegistrar @ module+0x4C90). The state-manager leaves share the +0x2C
// module back-pointer (stamped by CreateStateMan).
//
// FLAG (module opaque): the minimal CgsSound::Logic::StateManager view in this TU
// (via BrnStateManager.h) does not expose mpLogicModule (+0x2C), and the
// SoundLogicModule home is not reconstructed in this slice -- so this cannot be
// bodied faithfully here. Provided as a non-cascading stub that abort-asserts if ever
// reached on boot (PrepareStateManagersOnBoot does NOT call it; it is only used on the
// per-frame attach/detach path this slice never exercises). Returns a TU-local empty
// registrar purely to satisfy the non-void signature; NOT a faithful body. Body via
// the module once the full StateManager view (mpLogicModule) + SoundLogicModule are
// available.
// ---------------------------------------------------------------------------
BrnSound::Logic::ResourceRegistrar& TrafficStateManager::GetResourceRegistrar()
{
    CGS_ASSERT( false,
                "TrafficStateManager::GetResourceRegistrar reached without a homed "
                "SoundLogicModule (boot path does not call this)" );
    static BrnSound::Logic::ResourceRegistrar sUnhomedRegistrar;
    return sUnhomedRegistrar;
}

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound
