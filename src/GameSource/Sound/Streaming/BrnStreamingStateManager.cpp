#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
#include "GameSource/Sound/Streaming/BrnStreamingState.h"
#include "GameShared/GameClasses/System/CgsHardwareSku.h"
#include "GameSource/Sound/Streaming/BrnIStreamUser.h"   // [DIAG] GetCreateParams for the witness
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
// GetTypeName @0x82683A88 reads the name pointer from the typed descriptor at +4.
// The descriptor values and CRT registration thunk are pinned by ARTIST_CRT_ORDER.json.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Streaming
{

namespace
{
    // [DIAG] NOT IN THE X360 BINARY -- the ContentSpec a stream user is asking for.
    // Named for what it reads: the user's own CreateParams content-spec hash, the
    // single value that decides which .SNS the StreamsRegistry resolves.
    u32 DiagSpecOf(const IStreamUser* apUser)
    {
        return apUser ? static_cast<u32>(apUser->GetCreateParams().mContentSpecName) : 0u;
    }

    // [DIAG] A refused request is RE-POSTED and retried every tick, so its witness
    // would storm the log (and starve the harness). Cap it hard and say so.
    u32 guDiagRefusals = 0;
    const u32 KU_DIAG_REFUSAL_CAP = 40;
}

StreamRequest::StreamRequest()
    : mpAttachment(0), mu32Priority(0), mfLagTolerance(0.0f), mfTimeStamp(0.0f),
      mu32UniqueId(0), mbDirty(false)
{
}

StreamRequest::StreamRequest(IStreamUser* apAttachment, u32 auPriority, f32 afLagTolerance)
    : mpAttachment(apAttachment), mu32Priority(auPriority), mfLagTolerance(afLagTolerance),
      mfTimeStamp(0.0f), mu32UniqueId(0), mbDirty(false)
{
}

StreamStopRequest::StreamStopRequest()
    : mpAttachment(0), mfFadeOut(0.0f), mfTimeStamp(0.0f), mu32UniqueId(0)
{
}

StreamStopRequest::StreamStopRequest(const IStreamUser* apAttachment, f32 afFadeOut)
    : mpAttachment(apAttachment), mfFadeOut(afFadeOut), mfTimeStamp(0.0f), mu32UniqueId(0)
{
}

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
    , muPlayRequestCount(0)
    , muStopRequestCount(0)
    , muNumRePostRequests(0)
    , muUniqueId(0)
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
// Tagged allocation now dispatches through the shared sound MemBase allocator.
// ---------------------------------------------------------------------------
CgsSound::Logic::StateManager* StreamingStateManager::CreateObject(u32 luType)
{
    return new (sTypeInfo.typeName, luType != 0) StreamingStateManager();
}

// ARTIST descriptor 0x82F2E84C: ObjectID 6, name "StreamingStateManager", base
// StateManager descriptor 0x82F2FAA0, factory 0x82700B68. The typed static object is
// registered by the direct CRT registrar below.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> StreamingStateManager::sTypeInfo(
    6,
    "StreamingStateManager",
    &CgsSound::Logic::StateManager::sTypeInfo,
    &StreamingStateManager::CreateObject);

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* StreamingStateManager::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

// ---------------------------------------------------------------------------
// ARTIST CRT thunk 0x82C61C10 passes descriptor 0x82F2E84C and tail-branches to
// StateManager::AddToClassTypeInfoArray; the typed registrar below reproduces that call.

namespace
{
    struct StreamingStateManagerTypeRegistration
    {
        StreamingStateManagerTypeRegistration()
        {
            CgsSound::Logic::StateManager::AddToClassTypeInfoArray(
                StreamingStateManager::GetStaticTypeInfo());
        }
    };
    static StreamingStateManagerTypeRegistration gStreamingStateManagerTypeRegistration;
}

// ---------------------------------------------------------------------------
// StreamingStateManager::GetTypeInfo() const  (vtable RTTI hook)
//   Returns this leaf's static descriptor.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* StreamingStateManager::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

// ---------------------------------------------------------------------------
// StreamingStateManager::GetTypeName() const -- ARTIST @0x82683A88 returns the
// descriptor's name pointer from +4; CreateObject uses the same pointer as its tag.
// ---------------------------------------------------------------------------
const char* StreamingStateManager::GetTypeName() const
{
    return sTypeInfo.typeName;
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
    switch (mePrepareState)
    {
    case E_PREPARE_NONE:
    case E_PREPARE_RELEASED:
        mePrepareState = E_PREPARE_NONE;
        // fall through
    case E_PREPARE_BEGIN:
    {
        mePrepareState = E_PREPARE_BEGIN;
        const char* lpacBundle = 0;
        switch (CgsSystem::HardwareSku::FindLanguage())
        {
        case 7:
        case 8:  lpacBundle = "sound\\streams\\StreamHeaders.bundle"; break;
        case 10: lpacBundle = "sound\\streams\\StreamHeaders_FR.bundle"; break;
        case 11: lpacBundle = "sound\\streams\\StreamHeaders_DE.bundle"; break;
        case 15: lpacBundle = "sound\\streams\\StreamHeaders_IT.bundle"; break;
        case 16: lpacBundle = "sound\\streams\\StreamHeaders_JP.bundle"; break;
        case 22: lpacBundle = "sound\\streams\\StreamHeaders_ES.bundle"; break;
        default:
            CGS_ASSERT(false, "Unhandled Language when loading Sound Stream Headers.");
            lpacBundle = "sound\\streams\\StreamHeaders.bundle";
            break;
        }
        LoadAsset(lpacBundle, 0, ResourceRegistrar::E_DATA);
        mePrepareState = E_PREPARE_UPDATING;
        return false;
    }
    case E_PREPARE_UPDATING:
        return false;
    case E_PREPARE_STATES:
        if (!PrepareStates(1, 3, 0))
            return false;
        // fall through
    case E_PREPARE_FINISHED:
        muPlayRequestCount = 0;
        muStopRequestCount = 0;
        muNumRePostRequests = 0;
        muUniqueId = 0;
        mePrepareState = E_PREPARE_FINISHED;
        return true;
    default:
        return false;
    }
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
    if (mePrepareState == E_PREPARE_UPDATING)
        mePrepareState = E_PREPARE_STATES;
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
// ARTIST vtable 0x820B7F80, slot +0x10, points to the shared true leaf.
bool StreamingStateManager::Release()
{
    return true;
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

    // [DIAG] NOT IN THE X360 BINARY (BRN_STREAM_DIAG=1). Witnesses the PLAY request
    // as posted: which user, which ContentSpec, what priority, which ring slot.

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

void StreamingStateManager::PostStreamRequest(const StreamStopRequest& lStopRequest)
{
    CGS_ASSERT(muStopRequestCount < E_MAX_STREAM_REQUESTS,
               "muStopRequestCount < E_MAX_STREAM_REQUESTS");
    StreamStopRequest lStamped = lStopRequest;
    lStamped.mfTimeStamp = mfCurrentTime;
    lStamped.mu32UniqueId = muUniqueId++;
    PostStreamRequestInternal(lStamped);

    // [DIAG] NOT IN THE X360 BINARY (BRN_STREAM_DIAG=1).

}

void StreamingStateManager::PostStreamRequestInternal(const StreamStopRequest& arRequest)
{
    CGS_ASSERT(muStopRequestCount < E_MAX_STREAM_REQUESTS,
               "muStopRequestCount < E_MAX_STREAM_REQUESTS");
    maStopRequests[muStopRequestCount++] = arRequest;
}

void StreamingStateManager::RemoveRequestsForUser(const StreamStopRequest& arRequest)
{
    for (u32 luIndex = 0; luIndex < muPlayRequestCount; ++luIndex)
    {
        StreamRequest& lrRequest = maPlayRequests[luIndex];
        if (lrRequest.mpAttachment == arRequest.mpAttachment &&
            lrRequest.mu32UniqueId < arRequest.mu32UniqueId)
            lrRequest.mbDirty = true;
    }
}

void StreamingStateManager::CheckForDuplicates(u32 auSourceIndex) const
{
    CGS_ASSERT(auSourceIndex < muPlayRequestCount,
               "luSourceIndex < muPlayRequestCount");
    if (auSourceIndex >= muPlayRequestCount)
        return;
    for (u32 luIndex = 0; luIndex < muPlayRequestCount; ++luIndex)
    {
        if (luIndex != auSourceIndex && !maPlayRequests[luIndex].mbDirty &&
            maPlayRequests[luIndex].mpAttachment == maPlayRequests[auSourceIndex].mpAttachment)
            CGS_ASSERT(false, "[Stream] User made multiple requests");
    }
}

void StreamingStateManager::ProcessStopRequests()
{
    StreamStopRequest laReposts[E_MAX_REQUEST_RE_POSTS];
    u32 luRepostCount = 0;

    for (u32 luStop = 0; luStop < muStopRequestCount; ++luStop)
    {
        const StreamStopRequest& lrStop = maStopRequests[luStop];
        for (CgsSound::Logic::State* lpBase = mpHeadState; lpBase;
             lpBase = lpBase->GetNextState())
        {
            StreamingState* lpState = static_cast<StreamingState*>(lpBase);
            if (!lpState->IsAttached())
                continue;
            const StreamRequest& lrActive = lpState->GetRequest();
            if (lrActive.mpAttachment != lrStop.mpAttachment ||
                lrActive.mu32UniqueId >= lrStop.mu32UniqueId)
                continue;

            if (lpState->Detach())
            {
                lpState->SetFadeOut(lrStop.mfFadeOut);
            }
            else
            {
                bool lbAlreadyReposted = false;
                for (u32 luIndex = 0; luIndex < luRepostCount; ++luIndex)
                    lbAlreadyReposted = lbAlreadyReposted ||
                        laReposts[luIndex].mpAttachment == lrStop.mpAttachment;
                if (!lbAlreadyReposted)
                {
                    CGS_ASSERT(luRepostCount < E_MAX_REQUEST_RE_POSTS,
                               "luNumRePosts < E_MAX_REQUEST_RE_POSTS");
                    if (luRepostCount < E_MAX_REQUEST_RE_POSTS)
                        laReposts[luRepostCount++] = lrStop;
                }
            }
        }
        RemoveRequestsForUser(lrStop);
    }

    muStopRequestCount = 0;
    for (u32 luIndex = 0; luIndex < luRepostCount; ++luIndex)
        PostStreamRequestInternal(laReposts[luIndex]);
}

CgsSound::Logic::State* StreamingStateManager::GetFreeState(void* apRequest)
{
    const StreamRequest* lpRequest = static_cast<const StreamRequest*>(apRequest);
    CGS_ASSERT(lpRequest != 0, "lpvObjectPtr");
    if (!lpRequest)
        return 0;

    StreamingState* lpFree = 0;
    for (CgsSound::Logic::State* lpBase = mpHeadState; lpBase;
         lpBase = lpBase->GetNextState())
    {
        StreamingState* lpState = static_cast<StreamingState*>(lpBase);
        if (!lpState->IsAttached() &&
            (!lpFree || lpState->GetUpdateState() == CgsSound::Logic::State::E_UPDATE_UNATTACHED))
            lpFree = lpState;
    }
    if (lpFree)
        return lpFree;

    StreamingState* lpVictim = 0;
    for (CgsSound::Logic::State* lpBase = mpHeadState; lpBase;
         lpBase = lpBase->GetNextState())
    {
        StreamingState* lpState = static_cast<StreamingState*>(lpBase);
        CGS_ASSERT(lpState->IsAttached(), "lpStreamingState->IsAttached()");
        if (lpState->GetRequest().mu32Priority < lpRequest->mu32Priority &&
            (!lpVictim || lpState->GetRequest().mu32Priority < lpVictim->GetRequest().mu32Priority))
            lpVictim = lpState;
    }
    if (!lpVictim)
        return 0;

    const StreamRequest lOldRequest = lpVictim->GetRequest();
    if (!lpVictim->Detach())
        return 0;
    lpVictim->SetFadeOut(lpRequest->mfLagTolerance);
    RePostStreamRequest(lOldRequest);
    return lpVictim;
}

void StreamingStateManager::UpdateParams(f32 af32GameDt)
{
    CgsSound::Logic::StateManager::UpdateParams(af32GameDt);
    CGS_ASSERT(mePrepareState == E_PREPARE_FINISHED,
               "mePrepareState == E_PREPARE_FINISHED");
    if (mePrepareState != E_PREPARE_FINISHED)
        return;

    ProcessStopRequests();
    muNumRePostRequests = 0;
    for (u32 luIndex = 0; luIndex < muPlayRequestCount; ++luIndex)
    {
        StreamRequest& lrRequest = maPlayRequests[luIndex];
        if (lrRequest.mbDirty)
            continue;
        CheckForDuplicates(luIndex);
        StreamingState* lpState = static_cast<StreamingState*>(GetFreeState(&lrRequest));
        if (lpState)
        {
            lpState->Attach(&lrRequest);
            // [DIAG] NOT IN THE X360 BINARY (BRN_STREAM_DIAG=1). The request was
            // ACCEPTED: a StreamingState took it and will bring a voice up.

        }
        else
        {
            // [DIAG] NOT IN THE X360 BINARY (BRN_STREAM_DIAG=1). REFUSED: no free
            // state and no lower-priority victim -- the request is re-posted and
            // will be retried next tick (this is how a stream can queue forever).
            if (guDiagRefusals < KU_DIAG_REFUSAL_CAP)
            {
                ++guDiagRefusals;

            }
            RePostStreamRequest(lrRequest);
        }
    }

    muPlayRequestCount = 0;
    const u32 luRepostCount = muNumRePostRequests;
    for (u32 luIndex = 0; luIndex < luRepostCount; ++luIndex)
        PostStreamRequestInternal(maRePostRequests[luIndex]);
}

} // namespace Streaming
} // namespace Logic
} // namespace BrnSound
