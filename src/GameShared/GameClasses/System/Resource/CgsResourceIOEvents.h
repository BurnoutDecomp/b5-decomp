#pragma once

// Resource bundle-loader IO event payloads. Reconstructed from the DecFIGS DWARF.
// Events derive from an empty per-module Event base (CgsModule event convention).
#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"        // CgsResource::ID (64-bit hash)
#include "GameShared/GameClasses/System/Resource/CgsSmallResource.h"     // SmallResourceDescriptor (Entry::ResourceDescriptor form)
#include "GameShared/GameClasses/System/Resource/CgsResourceBundle2.h"   // BundleV2 (struct) + nested ResourceEntry

namespace CgsModule { class BaseEventReceiverQueue; }   // referenced by pointer only
namespace CgsResource { struct ResourceHandle; }        // AcquireResourceListRequest::mpHandles (by pointer)

namespace CgsResource
{
// [merge fix] BundleV2 is a struct (CgsResourceBundle2.h) with nested ResourceEntry; the prior wave-side
// `namespace BundleV2 { struct ResourceEntry; }` forward-decl clashed (C2869) whenever a TU also pulled in
// struct BundleV2 (e.g. via CgsResourcePool.h -> CgsResourceBundle2.h). Include the real header instead.

struct Entry;   // AcquireResourceResponse carries the found pool Entry* (by pointer only)

namespace Events
{
    struct Event {};

    struct BundleLoaderEvent : public Event
    {
        CgsModule::BaseEventReceiverQueue* mpUser;
        s32  miEventId;
        bool mbLiveUpdateReplace;
        char macFileName[128];
        s32  miPoolId;

        // Copies a NUL-terminated path into macFileName, asserting the source fits
        // (length < 128, the macFileName buffer size); a null source clears the
        // buffer. Returns *this so callers can chain. Recovered from
        // CgsResource::Events::BundleLoaderEvent::SetFileName @ 0x822770F8; the X360
        // build inlines the CgsStringUtils.h:65 bounded-copy helper at the call site.
        BundleLoaderEvent& SetFileName(const char* pcFileName);
    };

    struct LoadBundleRequest : public BundleLoaderEvent
    {
        bool mbAllowFailiure;
        bool mbUseHDCache;
    };

    // CgsResourceIOEvents.h -- the request to register a live-update patch for an already-loaded
    // bundle. Recovered from CgsResource::Events::AddPatchRequest::Construct @ 0x82661200 (called
    // by BrnResource::GameDataModule::Prepare). The ctor stores two leading 32-bit fields, copies
    // a NUL-terminated path into an inline 256-byte buffer (asserting length < 256 via the inlined
    // CgsStringUtils.h:65 bounded-copy helper -- "String <name> is too long. Buffer size = 256"),
    // then stores a trailing 32-bit field. Field offsets are X360-store-confirmed:
    //   +0   miField0     (Construct arg a2)            stw r4, 0(r27)
    //   +4   miField4     (Construct arg a3)            stw r5, 4(r27)
    //   +8   macFileName[256] (NUL-terminated path)     stbx loop into r27+8, buffer size 256
    //   +264 miField264   (Construct arg a5)            stw r7, 0x108(r27)
    // The two leading fields and the trailing field are typed s32 (the only attestation is their
    // 32-bit store width + register-passed int args); they are named by offset pending a richer
    // DWARF/caller signature.
    struct AddPatchRequest : public Event
    {
        s32  miField0;            // +0    (arg a2)
        s32  miField4;            // +4    (arg a3)
        char macFileName[256];    // +8    NUL-terminated patch bundle path
        s32  miField264;          // +264  (arg a5)

        // X360 0x82661200. Initialise the request in place: store the two leading fields, copy the
        // NUL-terminated path into macFileName (bounded by the 256-byte buffer), then store the
        // trailing field. Returns this (the X360 ctor-style Construct returns its `result`).
        AddPatchRequest* Construct(s32 liField0, s32 liField4, const char* lpcFileName, s32 liField264);
    };

    struct LoadBundleResponse : public BundleLoaderEvent
    {
        enum EResult
        {
            E_RESULT_SUCCESS       = 0,
            E_RESULT_OUT_OF_MEMORY = 1
        };
        EResult meResult;
    };

    struct UnloadBundleResponse : public BundleLoaderEvent
    {
    };

    // CgsResourceIOEvents.h -- the request to unload a previously-loaded bundle. Structurally a
    // BundleLoaderEvent (no extra payload fields): the X360 BaseEventQueue<UnloadBundleRequest>::
    // AddEvent @ 0x828E9F50 memcpys whole elements at stride 0x90 == 144 bytes, exactly the
    // BundleLoaderEvent base size (mpUser 4 + miEventId 4 + mbLiveUpdateReplace 1 [+3 pad] +
    // macFileName[128] + miPoolId 4 == 144 on the 32-bit ARTIST target). It is the queued element
    // of CgsModule::BaseEventQueue<UnloadBundleRequest> (drained by
    // CgsResource::ResourceModule::ProcessResourceRequests).
    struct UnloadBundleRequest : public BundleLoaderEvent
    {
    };

    // CgsResourceIOEvents.h -- the request to open a streaming handle on a file. Recovered from
    // CgsResource::Events::OpenStreamRequest::SetFileName @ 0x82680278 (called by
    // CgsSound::Playback::Module::DoOpenStream and CgsResource::BundleLoaderModule::CheckForLoads).
    // SetFileName copies a NUL-terminated path into the inline macFileName[128] buffer at +8 via
    // the inlined CgsStringUtils.h:65 bounded-copy helper (asserting length < 128, the buffer size:
    // "String <name> is too long. Buffer size = 128"); a null source clears macFileName[0] to '\0'.
    // Field offsets are X360-store-confirmed against SetFileName / the DoOpenStream call site:
    //   +0   miField0   (caller-written leading field)   stw into 0(request)
    //   +4   miField4   (caller-written leading field)   stw into 4(request)
    //   +8   macFileName[128] (NUL-terminated path)       SetFileName stbx loop into request+8
    // The two leading fields are typed s32 (the only attestation is their 32-bit store width at the
    // DoOpenStream call site); they are named by offset pending a richer DWARF/caller signature.
    struct OpenStreamRequest : public Event
    {
        s32  miField0;            // +0    (caller-written)
        s32  miField4;            // +4    (caller-written)
        char macFileName[128];    // +8    NUL-terminated path to stream

        // X360 0x82680278. Copy a NUL-terminated path into macFileName (bounded by the 128-byte
        // buffer); a null source clears the buffer. Returns this (the X360 SetFileName returns its
        // incoming `result`).
        OpenStreamRequest* SetFileName(const char* lpcFileName);
    };

    // ===== ADDITIVE GROW (Wave 49): deprecated write-stream request event types =====
    // CgsResourceIOEvents.h -- the (deprecated) open/close write-stream requests. The X360
    // adders for these (ResourceModule::AddOpenWriteStreamRequest @0x828D8310 /
    // ::AddCloseWriteStreamRequest @0x828D83A0) are bare de-inlined asserts that reject the
    // request ("Write streams deprecated") and return false; they never read any field, so
    // these carry no payload beyond the Event base. Declared so the adder signatures resolve.
    struct OpenWriteStreamRequest : public Event {};   // DWARF CgsResourceModule.h:799 param
    struct CloseWriteStreamRequest : public Event {};  // DWARF CgsResourceModule.h:877 param

    // Base of every pool-targeted event (CgsResourceIOEvents.h:221). Carries the user
    // queue to notify, the per-request event id, and the target pool id. On the X360 the
    // ID members of derived requests are 8-byte aligned, so this base occupies 12 bytes.
    struct PoolEvent : public Event
    {
        CgsModule::BaseEventReceiverQueue* mpUser;   // +0
        s32                                miEventId; // +4
        s32                                miPoolId;  // +8
    };

    // CgsResourceIOEvents.h:266 -- the request to create a resource pool. The queued
    // element type of FifoQueue<CreatePoolRequest,128> (X360 element stride 172). Field
    // order/types from the DecFIGS DWARF; sizes/offsets follow the X360 spine (mDescriptor
    // is the in-memory three-pool descriptor, Entry::ResourceDescriptor == 24 bytes).
    struct CreatePoolRequest : public PoolEvent
    {
        char                    mpcName[32];              // +12
        s32                     miDeletionDelayFrames;    // +44
        u32                     muMaxResources;           // +48
        u32                     muMaxImports;             // +52
        SmallResourceDescriptor mDescriptor;              // +56  (Entry::ResourceDescriptor, 24B)
        u32                     mauMaxResources[3];       // +80
        s32                     miNumDependencies;        // +92
        s32                     maiDependencyIds[16];     // +96
        s32                     miBankId;                 // +160
        s32                     miParentBankId;           // +164
        bool                    mbAllowDefragmentation;   // +168
    };

    // CgsResourceIOEvents.h:495 -- the request to allocate a resource list. The queued
    // element type of FifoQueue<AllocateResourceListRequest,4> (X360 element stride 48).
    // mListId (ID, 8-byte aligned) sits at +16 after the 12-byte PoolEvent base + 4 pad.
    struct AllocateResourceListRequest : public PoolEvent
    {
        ID                                  mListId;             // +16
        const BundleV2::ResourceEntry*      mpEntries;           // +24
        char*                               mpcDebugData;        // +28
        s32                                 miNumEntries;        // +32
        bool*                               mpNeeds;             // +36
        void*                               mpResources;         // +40  (ResourceHandle::Resource*)
        bool                                mbLiveUpdateReplace; // +44
        bool                                mbAllowFailiure;     // +45
        bool                                mbCompressedBundle;  // +46
    };

    // CgsResourceIOEvents.h -- the reply an allocate/live-update pass posts to the pool output queue
    // (queue event id 17) when it finishes. The step state's GenerateResponse fills the working-set body
    // (list id + the caller's bundle-entry array/count/output arrays + the resolved pool id); the driver
    // (PoolModule::UpdateAllocating / UpdateLiveUpdate) then stamps the request's event id (miEventId)
    // and the trailing simple-frag flag before posting. X360 store offsets (from GenerateResponse
    // 0x828E4200): miPoolId@+0x08, mListId@+0x10, mpEntries@+0x18, miNumEntries@+0x1C, mpNeeds@+0x20,
    // mpResources@+0x24, mpListEntry@+0x28, mbSimpleFrag@+0x2C (48-byte record on the 32-bit target;
    // x64-widened here). Field order mirrors AllocateResourceListRequest.
    struct AllocateResourceListResponse : public PoolEvent
    {
        ID                             mListId;       // +0x10
        const BundleV2::ResourceEntry* mpEntries;     // +0x18
        s32                            miNumEntries;  // +0x1C
        bool*                          mpNeeds;       // +0x20
        void*                          mpResources;   // +0x24  (ResourceHandle::Resource*)
        Entry*                         mpListEntry;   // +0x28  the list's own entry-list resource entry
        bool                           mbSimpleFrag;  // +0x2C  driver-stamped (simple-frag / result flag)
    };

    // Acquire a single already-loaded resource by id from a pool (resource request id 4 -> pool input).
    // PoolModule::DoAcquireResourceRequest (X360 0x828FCD48) resolves it via Pool::FindResource and replies
    // with an AcquireResourceResponse carrying the resolved handle. [X360 element size 24 on the 32-bit
    // target; x64-widened here.]
    struct AcquireResourceRequest : public PoolEvent
    {
        ID   mResourceId;      // +16 (8-byte aligned) the resource to acquire
        bool mbCheckRefCount;  // FindResource ref-count gate (X360 a2[4])
    };

    // Acquire every resource of a named resource LIST (request id/type 5 -> pool input) into a
    // caller-owned handle array. DWARF CgsResourceIOEvents.h:410 {mListResourceId, mpHandles,
    // miMaxHandles}; X360 32-byte record attested by WorldEntityModule::PrepareZoneCollision
    // @ 0x82302C38 ({mpUser@0, miEventId@4, miPoolId@8, mListResourceId@0x10, mpHandles@0x18,
    // miMaxHandles@0x1C}, AddEvent type 5). The reply on the receiver queue is this same
    // record echoed with the handle array filled.
    struct AcquireResourceListRequest : public PoolEvent
    {
        ID              mListResourceId;   // :451 (X360 +0x10)
        ResourceHandle* mpHandles;         // :452 (X360 +0x18)
        s32             miMaxHandles;      // :453 (X360 +0x1C)
    };

    // Reply to an AcquireResourceRequest (pool output queue, tag 6): the resolved resource handle -- the
    // ResourceHandle pair (mpResourceMemory -> the entry's main-memory SmallResource slot; mpSourceEntry ->
    // the entry), or both null if the resource is absent.
    struct AcquireResourceResponse : public PoolEvent
    {
        void*  mpResourceMemory;   // &Entry.mResource.m_baseResources[E_MEMTYPE_MAINMEMORY] (null if absent)
        Entry* mpSourceEntry;      // the found entry (null if absent)
    };
}
}

// X360 element strides (the FifoQueue instantiations memcpy whole elements at these strides on the
// 32-bit ARTIST target; they are NOT asserted here because this gate compiles x64 -- 8-byte
// pointers + u64 alignment widen each struct, and the generic FifoQueue<T,N> uses sizeof(T)):
//   LoadBundleRequest           == 148  (RunningLoad,4>::Pop @ 0x828DF7C8, stride 148)
//   LoadBundleResponse          == 148  (BaseEventQueue<LoadBundleResponse>::AddEvent @ 0x828EA098, stride 0x94)
//   UnloadBundleRequest         == 144  (BaseEventQueue<UnloadBundleRequest>::AddEvent @ 0x828E9F50, stride 0x90)
//   CreatePoolRequest           == 172  (CreatePoolRequest,128>::Push/Pop @ 0x828DFB28/0x828DFBB8)
//   AllocateResourceListRequest ==  48  (AllocateResourceListRequest,4>::Push/Pop @ 0x828DFC48/0x828DFCC0)
