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
