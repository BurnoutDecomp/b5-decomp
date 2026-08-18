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
        5,                          // ObjectID (PS3 DecFIGS static-init 0x85FA1C: CollisionStateManager=5)
        "CollisionStateManager",    // typeName
        CgsSound::Logic::StateManager::GetStaticTypeInfo(), // baseTypeInfo (PS3 0x85FA1C: =StateManager::GetStaticTypeInfo())
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
// ObjectID RESOLVED (PS3 DecFIGS static-init 0x85FA1C): CollisionStateManager::sTypeInfo
// .ObjectID = 5. The descriptor comes from GetStaticTypeInfo() (seeded with that id and
// baseTypeInfo = StateManager::GetStaticTypeInfo()), so this registration lands the real
// id. This TU is OUT of the build, so dormant until the conductor adds it.
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

// ---------------------------------------------------------------------------
// CollisionStateManager::FindInScrapeHistory(const ScrapeInfo&)  @ 0x826889E0
//   DWARF (BrnCollisionStateManager.h:880): non-const member returning ScrapeInfo*.
//
// Linear scan of the 16-slot scrape history (maScrapeHistory, DWARF h:639): for each
// slot, the per-element ScrapeInfo::mbValid flag byte (+0x29) gates the comparison --
// if the flag is clear, the slot is skipped WITHOUT calling operator== (mirrors the asm
// short-circuit `!*(v5+41) || !operator==(...)`). The comparison is ScrapeInfo::
// operator== (committed BrnCollisionDataStructures.cpp). First slot where mbValid is set
// AND operator== returns true is returned; 16 misses returns nullptr.
//
// FLAG: maScrapeHistory is modelled with the COMMITTED ScrapeInfo (which carries mbValid
// + operator==); the DWARF's full 48-byte ScrapeInfo shape (EntityId pair, CollisionTag,
// eOrientation, etc.) is a deferred richer form -- this is a semantic-parity match on the
// scan gate + equality, not a byte-exact element layout.
// ---------------------------------------------------------------------------
BrnSound::Logic::Collision::ScrapeInfo*
CollisionStateManager::FindInScrapeHistory( const BrnSound::Logic::Collision::ScrapeInfo& rScrapeInfo )
{
    for ( u32 luIndex = 0; luIndex < 16u; ++luIndex )
    {
        BrnSound::Logic::Collision::ScrapeInfo& rSlot = maScrapeHistory[luIndex];

        // asm: `!*(v5+41)` (lbz r11,0x29(r30)) short-circuits operator== when the slot's
        // mbValid flag byte (+0x29) is clear.
        if ( rSlot.mbValid && ( rSlot == rScrapeInfo ) )
        {
            return &rSlot;
        }
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// CollisionStateManager::PlayCollision(OutputCollision*)  @ 0x82704028
//
// FLAG (STUB -- deferred collision-audio domain; NOT X360-faithful): this is the
// runtime "play a collision sound" entry point of the SAME collision-audio domain the
// class ctor / dtor / Prepare / ResourcesAreReady / GetResourceRegistrar already defer
// wholesale. A byte-faithful body needs types/globals NOT homed anywhere in the
// committed tree and with no recovered host layout: OutputCollision (fields), the
// GetRandomSampleID<Attrib::Gen::crashbin/propscrashbin> template methods over the
// deferred crash-bin tables, the crash-splicer "voice" type (slot getters +0x14/+0xC
// and the +0x54/+0x58 priority pair), the SoundLogicModule back-pointer (+0x2C) linked
// list, and the collision-audio debug globals (dword_82FFB91C / off_82F2F9BC). Bodied
// as a safe stub matching this class's established deferred-domain convention: it
// asserts + returns 0 (the X360 abort-safe value; the boot path never calls this -- only
// UpdateParams does, which this slice does not exercise). Revisit once the whole
// collision-audio domain is homed together.
// ---------------------------------------------------------------------------
int CollisionStateManager::PlayCollision( OutputCollision* /*lpCollision*/ )
{
    CGS_ASSERT( false,
                "CollisionStateManager::PlayCollision reached without the homed collision-audio "
                "domain (OutputCollision / GetRandomSampleID<Attrib::Gen::*> / crash-voice type / "
                "SoundLogicModule back-pointer are all deferred -- see header FLAG)" );
    return 0;
}

// ---------------------------------------------------------------------------
// BrnSound::Logic::Collision  SelectBin name->bin-index helper  @ 0x826A0598
//
// Shared non-template body both CollisionStateManager::SelectBin<> instantiations
// (SelectBin<crashbinlist,crashbin> and SelectBin<propscrashbinlist,propscrashbin>,
// DWARF h:733) tail-call. Hashes the requested crash-bin content name (a2) with
// CgsSound::Playback::Name::MakeHash and looks it up in a small interned name-hash
// table starting at dword_83005F24.
//
//   Hash = MakeHash(a2);
//   v6 = 0;
//   for ( i = &dword_83005F24; Hash != *i; ++i )
//       if ( ++v6 ) return 1;
//   return v6;
//
// PPC control flow (0x826A05B8..0x826A05D4): the loop-exit `cmplwi v6,1 / blt` can never
// re-enter once v6 has been bumped to 1, so the body runs AT MOST ONCE -- a hit on entry 0
// returns bin index 0 (default); any miss returns bin index 1 (fallback). a1 (`this`, r3)
// and a3/a4/a5 are DEAD in this leaf (the asm forwards ONLY a2 to MakeHash and never
// dereferences `this`); kept in the signature for ABI documentation.
//
// FLAG (dword_83005F24 table UNRESOLVED): the interned crash-bin name-hash list this
// indexes is NOT homed in this slice; modelled as a single attested slot (index 0, the
// only load the asm performs) with a placeholder-zero sentinel rather than fabricating
// the rest of the table.
// ---------------------------------------------------------------------------
namespace
{
    // Single attested table slot (dword_83005F24): the interned hash of the default /
    // first crash-bin content name. Real value depends on the (unresolved) crash-bin
    // name string table; 0 is a placeholder sentinel.
    uintptr_t gauCollisionBinNameHashes[1] = { 0 };
}

int SelectBin( int /*a1*/, const char* lkpacName, int /*a3*/, int /*a4*/, int /*a5*/ )
{
    uintptr_t luHash = CgsSound::Playback::Name::MakeHash( lkpacName );

    int luBinIndex = 0;
    for ( uintptr_t* lpuEntry = gauCollisionBinNameHashes; luHash != *lpuEntry; ++lpuEntry )
    {
        if ( ++luBinIndex )
            return 1;
    }
    return luBinIndex;
}

// ---------------------------------------------------------------------------
// CrashBinUtils<CrashBin>::GetSampleIds -- copy an AttribSys crash-bin's
// collision-sample-id array into a caller u16 buffer.
//
//   crashbin      @ 0x8268DC18  (DWARF BrnCollisionStateManager.h:538)
//   propscrashbin @ 0x8268FE90  (DWARF BrnCollisionStateManager.h:604)
//
// Stateless utility MEMBER (not a free function): the container is the explicit
// first arg lpCrashBin; because CrashBinUtils holds no data the method never
// touches its own `this`. lpfnGetArraySize / lpfnGetArrayItem are the bin's
// generated AttribSys array accessors as POINTERS TO MEMBER (DWARF :529/:530
// `{ __pfn, __delta }`), and the X360 leaf invokes them THROUGH lpCrashBin
// (`mr r3,r28 ; mtctr r29 ; bctrl` @0x8268FF24, `mr r3,r28 ; mtctr r27 ; bctrl`
// @0x8268FF68) -- the bin IS dereferenced, as `this` of each accessor; the
// Int32 layout field is a plain 32-bit int living in the crash-bin attribute data
// area, so the accessors return a reference to it. Copies luNumCollisions =
// *lpfnGetArraySize() indices into lpauArray (truncating each to u16), bounded by
// luMaxSize, and returns the count. Asserts collapse to CGS_ASSERT; message
// strings verbatim from X360 rodata, file-path + line args dropped.
//
// The single generic template body below is shared by both instantiations (the
// crashbin/propscrashbin bins differ only in type); the two explicit instantiations
// emit the linker symbols at their X360 addresses.
// ---------------------------------------------------------------------------
template< typename CrashBin >
unsigned int CrashBinUtils< CrashBin >::GetSampleIds(
    const CrashBin*                lpCrashBin,   // r3 (r28) -- `this` of both accessor calls
    const int&        (CrashBin::*lpfnGetArraySize)() const,
    const int&        (CrashBin::*lpfnGetArrayItem)( unsigned int ) const,
    u16*                           lpauArray,
    u16                            luMaxSize )
{
    CGS_ASSERT( lpfnGetArrayItem != 0, "lpGetArrayItem" );
    CGS_ASSERT( lpfnGetArraySize != 0, "lpGetArraySize" );
    CGS_ASSERT( lpauArray        != 0, "lpauArray" );

    unsigned int luNumCollisions = ( lpCrashBin->*lpfnGetArraySize )();

    CGS_ASSERT( luNumCollisions < luMaxSize, "luNumCollisions < luMaxSize" );

    for ( unsigned int i = 0; i < luNumCollisions; ++lpauArray )
    {
        *lpauArray = static_cast<u16>( ( lpCrashBin->*lpfnGetArrayItem )( i++ ) );
    }

    return luNumCollisions;
}

// Explicit instantiations (the two crash-bin specialisations the X360 build emits).
template struct CrashBinUtils< Attrib::Gen::crashbin >;
template struct CrashBinUtils< Attrib::Gen::propscrashbin >;

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
