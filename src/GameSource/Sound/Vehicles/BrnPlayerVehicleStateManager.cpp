#include "GameSource/Sound/Vehicles/BrnPlayerVehicleStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// =============================================================================
// BrnSound::Vehicles::PlayerVehicleStateManager -- out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// This canonical home brings the PLAYER-car engine/vehicle sound-logic state manager
// up as a CONCRETE, registrable leaf the StateManager factory CreateStateMan
// @ 0x826A5B60 can construct. The deepest vehicle-audio cascade (player engine
// voices, per-component content, world-scene 3D placement) is deferred -- see the
// per-function FLAGs.
//
// Sources:
//   PlayerVehicleStateManager::CreateObject  @ 0x827022D8  (real)
//   PlayerVehicleStateManager::Prepare       @ 0x826EF428  (stub -- deep vehicle cascade)
//   PlayerVehicleStateManager::ctor          @ 0x82700BE8  (minimal -- global-table + sub-obj cascade)
//   PlayerVehicleStateManager::dtor          @ 0x82700D30  (minimal -- content-release cascade)
// GetTypeInfo / GetTypeName / GetStaticTypeInfo / GetResourceRegistrar /
// ResourcesAreReady were NOT individually exported; reconstructed from the
// established in-tree RTTI pattern + the sibling BrnEffectObject::GetResourceRegistrar
// @ 0x82696850. GetTypeName returns the "PlayerVehicleStateManager" literal
// (off_82F2E880, the tag CreateObject's operator new uses).
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::PlayerVehicleStateManager()  ctor @ 0x82700BE8
//
//   result = CgsSound::Logic::StateManager::StateManager();   ; base ctor
//   *(result+144) = off_820AB608;                             ; (transient) +0x90 vtable
//   *result       = off_820B87A0;                             ; primary vtable @ +0
//   *(result+144) = off_820B8798;                             ; IResourceRequester vtable @ +0x90
//   *(result+152) = off_820B1738;                             ; THIRD secondary vtable @ +0x98 (see FLAG)
//   <build seven content sub-objects at +0x42C..+0x47C, each {&off_820B3250,0,0}>
//   <seed scalar tail at +0x41C..+0x428>
//   <THREE global-static table init loops: dword_82FFB350[8]=0; qword_82FFB3C8[8]=self;
//    qword_82FFB380[8]=self; + qword_82FFB370/408/378 = &unk_83000000 sentinel>
//   return result;
//
// The vtable stores at +0/+0x90 are the compiler's devirtualisation of the base/
// derived (primary) + IResourceRequester sub-object vptrs; produced implicitly by
// constructing a polymorphic class deriving from BrnStateManager.
//
// FLAG (minimal -- multi-leg cascade): the X360 ctor does THREE things this minimal
// shell does NOT reproduce faithfully:
//   1. installs a THIRD secondary sub-object vtable @ +0x98 (off_820B1738) -- an
//      ADDITIONAL secondary base beyond IResourceRequester (see header BASE-CHAIN
//      FLAG); not modelled (the shell derives only BrnStateManager), so that base
//      sub-object is absent.
//   2. builds SEVEN refcounted CgsSound::Logic::Content sub-objects (+0x42C..+0x47C,
//      each {&off_820B3250, 0, 0}) -- the player engine/component banks; these are the
//      deferred maDeferredPlayerVehicleState (see header) and are NOT individually
//      modelled.
//   3. initialises THREE PROCESS-GLOBAL static tables (dword_82FFB350 -- 8 zeroed
//      words; qword_82FFB3C8 + qword_82FFB380 -- 8 self-linked nodes each; plus the
//      qword_82FFB370/82FFB408/82FFB378 sentinels = &unk_83000000). These are GLOBAL
//      audio-subsystem registries (NOT `this` members) seeded by the player manager's
//      construction; reproducing them requires those global homes, which are not
//      reconstructed in this slice -- DEFERRED. (They are side-effects on global state,
//      so omitting them leaves those registries un-seeded; only the per-frame player-
//      vehicle path consumes them, which boot does not exercise.)
// This ctor therefore reproduces only the base construction; the deferred pad + global
// tables are left untouched. NOT a faithful body -- the seven content sub-objects, the
// +0x98 base, and the global-table seeding are all deferred with the player vehicle
// audio domain. The content sub-objects are never constructed-into by this slice
// (Prepare stubbed), so there is nothing for the dtor to release.
// ---------------------------------------------------------------------------
PlayerVehicleStateManager::PlayerVehicleStateManager()
    : BrnSound::Logic::BrnStateManager()
{
}

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::~PlayerVehicleStateManager()  @ 0x82700D30  (the X360 `vector deleting destructor`)
//
//   for each of the SEVEN content sub-objects (a1[285],a1[282],...,a1[267]):
//       a1[n] = &off_820B3250; if (a1[n+1]) CgsSound::Playback::Object::Release(...);  ; drop each bank
//   *a1 = off_820B66E4;
//   CgsSound::Logic::StateManager::RegisteredContent_4_int_::~ObjectPool(a1 + 12);     ; base pool teardown
//   *a1 = &off_820AA820;                                                               ; MemBase vtable
//
// FLAG (minimal -- content-release cascade): the real dtor releases the seven
// player-engine/component Content sub-objects (CgsSound::Playback::Object::Release),
// then tears down the base RegisteredContent ObjectPool at +0xC (its ~ObjectPool) and
// re-installs the MemBase vtable. The seven Content sub-objects are the deferred
// maDeferredPlayerVehicleState (see header) and are never constructed by this slice's
// stubbed Prepare, so there is nothing to release; the base pool teardown + vtable
// re-install are re-synthesised by the host toolchain from this virtual destructor +
// the base ~BrnStateManager. NOT a member-for-member faithful body -- the content
// release legs are deferred with the player vehicle audio domain.
// ---------------------------------------------------------------------------
PlayerVehicleStateManager::~PlayerVehicleStateManager()
{
}

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::CreateObject(u32)  @ 0x827022D8   (the factory hook)
//
//   if ( a1 ) { if ( MemBase::operator new(1152, "PlayerVehicleStateManager", 1) ) return new'd ctor; }
//   else      { if ( MemBase::operator new(1152, "PlayerVehicleStateManager", 0) ) return new'd ctor; }
//   return 0;
//
// The X360 allocates a 1152-byte (0x480) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "PlayerVehicleStateManager" (off_82F2E880) and
// placement-constructs a PlayerVehicleStateManager into it. Both arms call the SAME
// size+ctor; the `a1` argument only selects the operator-new flavour (0/1). The
// factory CreateStateMan @ 0x826A5B60 calls this as createObject(0).
//
// FLAG (allocator gate): CgsSound::MemBase (CgsMemBase.h) does NOT model operator
// new(size, tag, flavour) -- the sound allocator (off_82FFB954) is not homed in this
// group -- so a faithful placement-new through that allocator is not yet expressible.
// This reconstruction uses the host `new` (global operator new, NOT the sound
// allocator); the observable result -- a constructed PlayerVehicleStateManager* (or
// null) handed to the factory -- matches. Replace with the sound-allocator placement-
// new once MemBase::operator new is homed. The 1152-byte size is the X360 0x480; on
// the 64-bit host the real object differs in size, so the literal is documentation
// only and is NOT passed to the host new.
// ---------------------------------------------------------------------------
CgsSound::Logic::StateManager* PlayerVehicleStateManager::CreateObject( u32 /*luType*/ )
{
    return new PlayerVehicleStateManager();
}

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::GetStaticTypeInfo()  (RTTI descriptor)
//
// Mirrors the in-tree GetStaticTypeInfo convention (CgsStateManager.cpp:230). A
// function-local static ClassTypeInfo<StateManager> seeded with (ObjectID, typeName,
// baseTypeInfo, createObject) so the factory CreateStateMan can match
// descriptor->ObjectID and call ->createObject.
//
// FLAG (ObjectID UNRESOLVED): the per-leaf registration static-init that calls
// StateManager::AddToClassTypeInfoArray(@0x8268DFE8) with the explicit ObjectID was
// NOT exported (CreateObject @ 0x827022D8 has no xrefs_to) and no map-state enum names
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
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* PlayerVehicleStateManager::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> sTypeInfo(
        1,                              // ObjectID (PS3 DecFIGS static-init 0x85FA1C: PlayerVehicleStateManager=1)
        "PlayerVehicleStateManager",    // typeName
        0,                              // baseTypeInfo     -- StateManager base descriptor (deferred)
        &PlayerVehicleStateManager::CreateObject // createObject
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
// (CreateObject @ 0x827022D8 has no xrefs_to). Assigned 7 here as a unique-among-the-9
// placeholder; the real id is this manager's slot in the CreateStateManagers 0..8 loop
// (@ 0x826AFEF8) -- pin at integration. This TU is OUT of the build, so dormant until
// the conductor adds it.
// ---------------------------------------------------------------------------
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* const
    gpPlayerVehicleStateManagerReg =
        CgsSound::Logic::StateManager::AddToClassTypeInfoArray(
            PlayerVehicleStateManager::GetStaticTypeInfo());

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::GetTypeInfo() const  (vtable RTTI hook)
//   Returns this leaf's static descriptor.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* PlayerVehicleStateManager::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::GetTypeName() const
//   The X360 leaf GetTypeName loads the tag string (off_82F2E880) used by
//   CreateObject's operator new -> returns the literal "PlayerVehicleStateManager".
// ---------------------------------------------------------------------------
const char* PlayerVehicleStateManager::GetTypeName() const
{
    return "PlayerVehicleStateManager";
}

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::Prepare()  @ 0x826EF428   (vtable +0x0C)
//
// X360 body: a switch on the +0x24 prepare-state (cases 0/5 -> 0, 1, 2, 3, 4):
//   state 1: Content::Construct + LoadAsset the player engine/component banks
//            (StateManager::GetContent feeds the per-component content);
//            SoundWorldScene::Prepare (the player 3D voice scene);
//   state 2: if (!Content::IsLoaded(...)) return 0;
//            mCpuMonitor = PerfMonCpu::AddMonitor("Player Car", 14, 0, 1.0, ...)
//   state 3: if (!StateManager::PrepareStates(...)) return 0;
//   state 4: return 1;
//
// FLAG (stub -- deep vehicle-audio cascade): the real body is the deepest of the
// state-manager Prepares; it cascades into
//   * CgsSound::Logic::Content::Construct / Content::IsLoaded (the player engine +
//     per-component splicer banks -- the deferred seven Content sub-objects),
//   * CgsSound::Logic::StateManager::GetContent (the base content accessor feeding the
//     per-component content),
//   * BrnSound::Logic::World::SoundWorldScene::Prepare (the player 3D voice scene),
//   * CgsSound::Playback::Name::MakeHash (the content-name hasher),
//   * BrnSound::Logic::IResourceRequester::LoadAsset (the streaming-resource broker),
//   * CgsDev::PerfMonCpu::AddMonitor (the CPU perf monitor), and
//   * CgsSound::Logic::StateManager::PrepareStates @ 0x826EAD30 (the State machine,
//     itself a declared-only stub in the foundation).
// None reconstructed in this slice. PrepareStateManagersOnBoot (0x826837F8) only needs
// Prepare() to return true to advance boot, so this stub returns true (boot-ready)
// WITHOUT the player engine/voice bring-up. NOT an X360-faithful body -- the prepare
// state machine + the player vehicle audio domain are deferred. X360 addr above.
// ---------------------------------------------------------------------------
bool PlayerVehicleStateManager::Prepare()
{
    return true;
}

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::ResourcesAreReady()  (IResourceRequester completion callback)
//
// FLAG (stub -- deep vehicle-audio cascade): the IResourceRequester completion
// callback (invoked by the resource broker once the player engine/component banks
// resolve) seeds the player voice/content state -- the deferred player vehicle audio
// domain. Not reconstructed in this slice and not needed for boot: it is invoked only
// AFTER LoadAsset resolves, which this slice's Prepare stub never issues. Bodied as a
// no-op so the leaf is concrete; NOT X360-faithful. Deferred with the player vehicle
// audio domain.
// ---------------------------------------------------------------------------
void PlayerVehicleStateManager::ResourcesAreReady()
{
}

// ---------------------------------------------------------------------------
// PlayerVehicleStateManager::GetResourceRegistrar()  (IResourceRequester slot 1)
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
BrnSound::Logic::ResourceRegistrar& PlayerVehicleStateManager::GetResourceRegistrar()
{
    CGS_ASSERT( false,
                "PlayerVehicleStateManager::GetResourceRegistrar reached without a homed "
                "SoundLogicModule (boot path does not call this)" );
    static BrnSound::Logic::ResourceRegistrar sUnhomedRegistrar;
    return sUnhomedRegistrar;
}

} // namespace Vehicles
} // namespace BrnSound
