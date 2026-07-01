#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// =============================================================================
// BrnSound::Logic::Streaming::StreamingStateManager -- out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// This canonical home (CONFIRMED by the Prepare FireAssert source path) brings the
// streamed-audio sound-logic state manager up as a CONCRETE, registrable leaf the
// StateManager factory CreateStateMan @ 0x826A5B60 can construct. The streaming
// audio domain (per-stream voices, language stream-header banks) is deferred -- see
// the per-function FLAGs.
//
// Sources:
//   StreamingStateManager::CreateObject  @ 0x82700B68  (real)
//   StreamingStateManager::Prepare       @ 0x826EE680  (stub -- domain cascade)
//   StreamingStateManager::ctor          @ 0x826FBFE0  (real -- self-contained shell)
// GetTypeInfo / GetTypeName / GetStaticTypeInfo / GetResourceRegistrar /
// ResourcesAreReady were NOT individually exported; reconstructed from the
// established in-tree RTTI pattern + the sibling BrnEffectObject::GetResourceRegistrar
// @ 0x82696850. GetTypeName returns the "StreamingStateManager" literal (off_82F2E850,
// the tag CreateObject's operator new uses).
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Streaming
{

// ---------------------------------------------------------------------------
// StreamingStateManager::StreamingStateManager()  ctor @ 0x826FBFE0
//
//   result = CgsSound::Logic::StateManager::StateManager();   ; base ctor
//   *(result+144) = off_820AB608;                             ; (transient) +0x90 vtable
//   *result       = off_820B7F80;                             ; primary vtable @ +0
//   *(result+144) = off_820B7F78;                             ; IResourceRequester vtable @ +0x90
//   <zero-init the dense f32 + int table at +0x98 .. +0x214>
//   return result;
//
// The two vtable stores are the compiler's devirtualisation of the base/derived
// (primary @ +0) and IResourceRequester sub-object (@ +0x90) vptrs; in reconstructed
// C++ they are produced implicitly by constructing a polymorphic class deriving from
// BrnStateManager, so the only observable work to express here is the base
// construction (implicit base ctor) and the zero-init of the streaming members.
//
// FLAG (deferred member init -- self-contained shell): the X360 ctor zero-inits a
// dense block of per-stream f32 + int fields (+0x98 through +0x214 -- gains, fades,
// voice handles, the streaming bookkeeping). Those members are the deferred ~360
// bytes of streaming-voice state (maDeferredStreamingState, see header FLAG) and are
// NOT individually modelled, so this ctor reproduces only the base construction; the
// deferred pad is left default-initialised. NOT a member-for-member faithful body --
// the streaming-voice table is deferred with the streaming audio domain. (The ctor
// builds NO sub-objects with their own vtables / refcounts beyond the inherited
// IResourceRequester, so there is nothing for the dtor to release.)
// ---------------------------------------------------------------------------
StreamingStateManager::StreamingStateManager()
    : BrnSound::Logic::BrnStateManager()
{
}

// ---------------------------------------------------------------------------
// StreamingStateManager::~StreamingStateManager()  (the X360 `vector deleting destructor`)
//
// FLAG (minimal): the matching dtor was not individually exported. The base
// BrnStateManager / CgsSound::Logic::StateManager virtual destructor tears down the
// base content pool, re-installs the MemBase vtable and routes storage back to the
// sound allocator (off_82FFB954) -- all re-synthesised by the host toolchain from
// this virtual destructor. No leaf member teardown is modelled (the deferred
// streaming-voice state owns no recovered resources in this slice).
// ---------------------------------------------------------------------------
StreamingStateManager::~StreamingStateManager()
{
}

// ---------------------------------------------------------------------------
// StreamingStateManager::CreateObject(u32)  @ 0x82700B68   (the factory hook)
//
//   if ( a1 ) { if ( MemBase::operator new(552, "StreamingStateManager", 1) ) return new'd ctor; }
//   else      { if ( MemBase::operator new(552, "StreamingStateManager", 0) ) return new'd ctor; }
//   return 0;
//
// The X360 allocates a 552-byte (0x228) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "StreamingStateManager" (off_82F2E850) and
// placement-constructs a StreamingStateManager into it. Both arms call the SAME
// size+ctor; the `a1` argument only selects the operator-new flavour (0/1). The
// factory CreateStateMan @ 0x826A5B60 calls this as createObject(0).
//
// FLAG (allocator gate): CgsSound::MemBase (CgsMemBase.h) does NOT model operator
// new(size, tag, flavour) -- the sound allocator (off_82FFB954) is not homed in this
// group -- so a faithful placement-new through that allocator is not yet expressible.
// This reconstruction uses the host `new` (global operator new, NOT the sound
// allocator); the observable result -- a constructed StreamingStateManager* (or null)
// handed to the factory -- matches. Replace with the sound-allocator placement-new
// once MemBase::operator new is homed. The 552-byte size is the X360 0x228; on the
// 64-bit host the real object differs in size, so the literal is documentation only
// and is NOT passed to the host new.
// ---------------------------------------------------------------------------
CgsSound::Logic::StateManager* StreamingStateManager::CreateObject( u32 /*luType*/ )
{
    return new StreamingStateManager();
}

// ---------------------------------------------------------------------------
// StreamingStateManager::GetStaticTypeInfo()  (RTTI descriptor)
//
// Mirrors the in-tree GetStaticTypeInfo convention (CgsStateManager.cpp:230). A
// function-local static ClassTypeInfo<StateManager> seeded with (ObjectID, typeName,
// baseTypeInfo, createObject) so the factory CreateStateMan can match
// descriptor->ObjectID and call ->createObject.
//
// FLAG (ObjectID UNRESOLVED): the per-leaf registration static-init that calls
// StateManager::AddToClassTypeInfoArray(@0x8268DFE8) with the explicit ObjectID was
// NOT exported (CreateObject @ 0x82700B68 has no xrefs_to) and no map-state enum
// names the id in-tree. Per the established in-tree placeholder convention (every
// committed GetStaticTypeInfo uses 0), the ObjectID is seeded 0 here and MUST be
// replaced with the real id at integration -- the id is this manager's slot in the
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
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* StreamingStateManager::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> sTypeInfo(
        6,                          // ObjectID (PS3 DecFIGS static-init 0x85FA1C: StreamingStateManager=6)
        "StreamingStateManager",    // typeName
        CgsSound::Logic::StateManager::GetStaticTypeInfo(), // baseTypeInfo (PS3 0x85FA1C: =StateManager::GetStaticTypeInfo())
        &StreamingStateManager::CreateObject // createObject
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
// ObjectID RESOLVED (PS3 DecFIGS static-init 0x85FA1C): StreamingStateManager::sTypeInfo
// .ObjectID = 6. The descriptor comes from GetStaticTypeInfo() (seeded with that id and
// baseTypeInfo = StateManager::GetStaticTypeInfo()), so this registration lands the real
// id. This TU is OUT of the build, so dormant until the conductor adds it.
// ---------------------------------------------------------------------------
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* const
    gpStreamingStateManagerReg =
        CgsSound::Logic::StateManager::AddToClassTypeInfoArray(
            StreamingStateManager::GetStaticTypeInfo());

// ---------------------------------------------------------------------------
// StreamingStateManager::GetTypeInfo() const  (vtable RTTI hook)
//   Returns this leaf's static descriptor.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* StreamingStateManager::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

// ---------------------------------------------------------------------------
// StreamingStateManager::GetTypeName() const
//   The X360 leaf GetTypeName loads the tag string (off_82F2E850) used by
//   CreateObject's operator new -> returns the literal "StreamingStateManager".
// ---------------------------------------------------------------------------
const char* StreamingStateManager::GetTypeName() const
{
    return "StreamingStateManager";
}

// ---------------------------------------------------------------------------
// StreamingStateManager::Prepare()  @ 0x826EE680   (vtable +0x0C)
//
// X360 body: a switch on the +0x24 prepare-state (a1[9]; cases 0/5 -> 0, 1, 2, 3, 4):
//   state 1: switch (HardwareSku::FindLanguage(this)) -> LoadAsset(this+0x90, one of
//              "sound\\streams\\StreamHeaders[_XX].bundle", 0, 0)  (assert on unhandled
//              language at BrnStreamingStateManager.cpp:102);
//            mCpuMonitor = PerfMonCpu::AddMonitor("Streams", 14, 0, 1.0, ...)
//   state 3: if (!StateManager::PrepareStates(this, 1, 3, 0)) return 0;
//   state 4: clear the +0x218.. tail (a1[134..137] = 0); return 1;
//
// FLAG (stub -- domain cascade): the real body cascades into
//   * CgsSystem::HardwareSku::FindLanguage (the SKU/language selector),
//   * BrnSound::Logic::IResourceRequester::LoadAsset (the streaming-resource broker),
//   * CgsDev::PerfMonCpu::AddMonitor (the CPU perf monitor), and
//   * CgsSound::Logic::StateManager::PrepareStates @ 0x826EAD30 (the State machine,
//     itself a declared-only stub in the foundation).
// None reconstructed in this slice. PrepareStateManagersOnBoot (0x826837F8) only needs
// Prepare() to return true to advance boot, so this stub returns true (boot-ready)
// WITHOUT loading the language stream-header bundle / state bring-up. NOT an X360-
// faithful body -- the prepare state machine is deferred. X360 addr above.
// ---------------------------------------------------------------------------
bool StreamingStateManager::Prepare()
{
    return true;
}

// ---------------------------------------------------------------------------
// StreamingStateManager::ResourcesAreReady()  (IResourceRequester completion callback)
//
// FLAG (stub -- domain cascade): the IResourceRequester completion callback (invoked
// by the resource broker once the StreamHeaders bundle resolves) seeds the per-stream
// voice/header state -- the deferred streaming audio domain. Not reconstructed in this
// slice and not needed for boot: it is invoked only AFTER LoadAsset resolves, which
// this slice's Prepare stub never issues. Bodied as a no-op so the leaf is concrete;
// NOT X360-faithful. Deferred with the streaming audio domain.
// ---------------------------------------------------------------------------
void StreamingStateManager::ResourcesAreReady()
{
}

// ---------------------------------------------------------------------------
// StreamingStateManager::GetResourceRegistrar()  (IResourceRequester slot 1)
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
BrnSound::Logic::ResourceRegistrar& StreamingStateManager::GetResourceRegistrar()
{
    CGS_ASSERT( false,
                "StreamingStateManager::GetResourceRegistrar reached without a homed "
                "SoundLogicModule (boot path does not call this)" );
    static BrnSound::Logic::ResourceRegistrar sUnhomedRegistrar;
    return sUnhomedRegistrar;
}

// ---------------------------------------------------------------------------
// StreamingStateManager::PostStreamRequest(const StreamRequest&)  @ 0x826834F0
//   DWARF (BrnStreamingStateManager.h:255): void PostStreamRequest(const StreamRequest&).
//   (Hex-Rays `float*(float*,float*)` mislabels `this` as `result`; return is void.)
//
// Guard that the 6-deep play-request ring isn't full, append the request at
// maPlayRequests[muPlayRequestCount], stamp its timestamp from the manager's current
// time (base CgsStateManager::mfCurrentTime) and its id from the running unique-id
// counter, then bump both counters. Per-slot stride == sizeof(StreamRequest) (24 B).
// The X360 6-word straight-line copy is expressed as a struct assignment. Assert
// message is X360 rodata reproduced VERBATIM (no trailing \n; path/line dropped).
// ---------------------------------------------------------------------------
void StreamingStateManager::PostStreamRequest( const StreamRequest& lStreamRequest )
{
    CGS_ASSERT( muPlayRequestCount < E_MAX_STREAM_REQUESTS,
                "muPlayRequestCount < E_MAX_STREAM_REQUESTS" );

    maPlayRequests[muPlayRequestCount] = lStreamRequest;

    maPlayRequests[muPlayRequestCount].mfTimeStamp  = mfCurrentTime;
    maPlayRequests[muPlayRequestCount].mu32UniqueId = muUniqueId;
    ++muUniqueId;

    ++muPlayRequestCount;
}

// ---------------------------------------------------------------------------
// StreamingStateManager::PostStreamRequestInternal(const StreamRequest&)  @ 0x826836A8
//   DWARF (BrnStreamingStateManager.h:305): private void (const StreamRequest&).
//
// Appends a pending play request to the maPlayRequests ring then bumps the count. The
// X360 BeginAssert/FireAssert/EndAssert triple is a NON-GATING tripwire (both branches
// of cmplwi>=6 join at the copy+increment), so it collapses to a single non-gating
// CGS_ASSERT. The raw 6-word copy is expressed as a struct assignment. Message VERBATIM
// (no trailing \n).
// ---------------------------------------------------------------------------
void StreamingStateManager::PostStreamRequestInternal( const StreamRequest& arRequest )
{
    CGS_ASSERT( muPlayRequestCount < E_MAX_STREAM_REQUESTS,
                "muPlayRequestCount < E_MAX_STREAM_REQUESTS" );

    maPlayRequests[muPlayRequestCount] = arRequest;
    ++muPlayRequestCount;
}

// ---------------------------------------------------------------------------
// StreamingStateManager::RePostStreamRequest(const StreamRequest&)  @ 0x82683750
//   DWARF (BrnStreamingStateManager.h:347): private void (const StreamRequest&).
//
//   if ( muNumRePostRequests >= E_MAX_REQUEST_RE_POSTS ) CGS_ASSERT(false, "...");
//   maRePostRequests[muNumRePostRequests] = request;   // 24-byte (6 DWORD) copy
//   ++muNumRePostRequests;
//
// Appends a re-post request to the maRePostRequests ring (24-byte stride) then bumps
// muNumRePostRequests. Assert collapses the BeginAssert/FireAssert/EndAssert triple ->
// one CGS_ASSERT; message reproduced VERBATIM from FireAssert rodata (no trailing \n).
// ---------------------------------------------------------------------------
void StreamingStateManager::RePostStreamRequest( const StreamRequest& request )
{
    CGS_ASSERT( muNumRePostRequests < E_MAX_REQUEST_RE_POSTS,
                "muNumRePostRequests < E_MAX_REQUEST_RE_POSTS" );

    maRePostRequests[muNumRePostRequests] = request;
    ++muNumRePostRequests;
}

} // namespace Streaming
} // namespace Logic
} // namespace BrnSound
