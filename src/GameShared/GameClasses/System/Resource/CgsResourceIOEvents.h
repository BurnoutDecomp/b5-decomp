#pragma once

// Resource bundle-loader IO event payloads. Reconstructed from the DecFIGS DWARF.
// Events derive from an empty per-module Event base (CgsModule event convention).
#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"        // CgsResource::ID (64-bit hash)
#include "GameShared/GameClasses/System/Resource/CgsSmallResource.h"     // SmallResourceDescriptor (Entry::ResourceDescriptor form)

namespace CgsModule { class BaseEventReceiverQueue; }   // referenced by pointer only

namespace CgsResource
{
namespace BundleV2 { struct ResourceEntry; }   // referenced by pointer only

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
}
}

// X360 element strides (the FifoQueue instantiations memcpy whole elements at these strides on the
// 32-bit ARTIST target; they are NOT asserted here because this gate compiles x64 -- 8-byte
// pointers + u64 alignment widen each struct, and the generic FifoQueue<T,N> uses sizeof(T)):
//   LoadBundleRequest           == 148  (RunningLoad,4>::Pop @ 0x828DF7C8, stride 148)
//   CreatePoolRequest           == 172  (CreatePoolRequest,128>::Push/Pop @ 0x828DFB28/0x828DFBB8)
//   AllocateResourceListRequest ==  48  (AllocateResourceListRequest,4>::Push/Pop @ 0x828DFC48/0x828DFCC0)
