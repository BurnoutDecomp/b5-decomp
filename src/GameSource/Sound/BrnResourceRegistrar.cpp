#include "GameSource/Sound/BrnResourceRegistrar.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"

#include <cstring>   // strncpy

// BrnSound::Logic::ResourceRegistrar -- the sound-logic streaming-resource broker. Faithfully
// decompiled from BURNOUT_X360_ARTIST.XEX (per-function addresses cited at each body). The
// queue-driven load/unload state machines (QueuedResource::Prepare/Release/PrepareAttribSys/
// ReleaseAttribSys) feed the reconstructed GameDataIO and AttribSys request interfaces. The owner
// SoundLogicModule drives Update each bridge tick and appends both outgoing queues to the root pump.

// =============================================================================
// rw::RwHash32String @ X360 0x82BBC458 -- FNV-1a over the raw bytes (prime 0x01000193); the caller
// passes the offset basis (0x811C9DC5) as the seed. Faithful to the X360 body:
//   while (*p) { v = (16777619 * seed) ^ *p; ++p; seed = v; } return seed;
// (NOT lowercased -- distinct from BrnParticle's case-insensitive Entry::HashString.)
// FLAG: home-of-convenience (see header) -- migrate to a vendor rwcore string-hash home when one lands.
// =============================================================================
namespace rw
{
    u32 RwHash32String(const char* lpacString, u32 luSeed)
    {
        const u8* lpcCursor = reinterpret_cast<const u8*>(lpacString);
        while (*lpcCursor)
        {
            luSeed = (0x01000193u * luSeed) ^ static_cast<u32>(*lpcCursor);
            ++lpcCursor;
        }
        return luSeed;
    }
}

namespace BrnSound
{
namespace Logic
{
    // The verbatim X360-baked source path for this file's asserts (preserved exactly).
    static const char* const KAC_RR_FILE =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Sound/BrnResourceRegistrar.cpp";

    // The FNV offset basis the registrar seeds every name hash with (0x811C9DC5 == -2128831035).
    static const u32 KU_FNV_OFFSET_BASIS = 0x811C9DC5u;

    // =========================================================================
    // RequestedResource
    // =========================================================================

    // 0x826ADAE8 -- build a RequestedResource from the QueuedResource the load was driven from.
    ResourceRegistrar::RequestedResource::RequestedResource(const QueuedResource& lrSource)
    {
        // miResourceType (+0x40) <- src miResourceType (+0x128)
        miResourceType = static_cast<s32>(lrSource.muResourceNameHash);

        // muBundleNameHash (+0x44) <- RwHash32String(src bundle name, basis); 0 when the name is empty.
        if (lrSource.macBundleName[0] != 0)
        {
            muBundleNameHash = rw::RwHash32String(lrSource.macBundleName, KU_FNV_OFFSET_BASIS);
        }
        else
        {
            muBundleNameHash = 0;
        }

        // mResourceHandle (+0x48/+0x4C) <- src handle (+0x118/+0x11C).
        mResourceHandle = lrSource.mResourceHandle;

        // mRequesterList (+0x50, N=16) -- seed the pool (free seeded, live empty). The X360 stores the
        // capacity 16 to +0x50 then InternalInits the two sublists; LinkedListHelper::Construct does both.
        mRequesterList.Construct();

        // ids (+0x12C / +0x130) <- src mResourceClass (+0x134) / src mBundleId (+0x13C)
        miId12C = lrSource.mResourceClass;
        miId130 = lrSource.mBundleId;

        // macBundleName (+0x00) <- strncpy(src bundle name, 64), NUL-terminate at [63] (X360 stores 0 @+0x3F).
        strncpy(macBundleName, lrSource.macBundleName, 64);
        macBundleName[63] = 0;

        // If the source carried a requester (src+0x138, mpRequester), record it in the requester list.
        if (lrSource.mpRequester != 0)
        {
            mRequesterList.AddTail(lrSource.mpRequester);
        }
    }

    ResourceRegistrar::RequestedResource&
    ResourceRegistrar::RequestedResource::operator=(const RequestedResource& lrSource)
    {
        if (this == &lrSource)
            return *this;

        strncpy(macBundleName, lrSource.macBundleName, sizeof(macBundleName));
        macBundleName[sizeof(macBundleName) - 1] = 0;
        miResourceType = lrSource.miResourceType;
        muBundleNameHash = lrSource.muBundleNameHash;
        mResourceHandle = lrSource.mResourceHandle;
        miId12C = lrSource.miId12C;
        miId130 = lrSource.miId130;

        // The X360 copy destination owns its own fixed requester-node pool. Rebuild that
        // pool instead of copying the helper's self-referential list pointers.
        mRequesterList.Construct();
        const CgsContainers::LinkedListNode<IResourceRequester*>* lpRequester =
            lrSource.mRequesterList.GetHead();
        while (lpRequester)
        {
            mRequesterList.AddTail(lpRequester->mData);
            lpRequester = static_cast<const CgsContainers::LinkedListNode<IResourceRequester*>*>(
                lpRequester->GetNextNode());
        }
        return *this;
    }

    // 0x826ADCA8 -- move every requester-list node whose payload == lpRequester from live -> free;
    // return true iff at least one was removed. The X360 finds-then-removes in a loop
    // (sub_826A5F48 == LinkedListHelper find-by-value); modelled here as a head-restart walk.
    bool ResourceRegistrar::RequestedResource::ClearReferencesToResource(IResourceRequester* lpRequester)
    {
        bool lbRemovedAny = false;
        CgsContainers::LinkedListNode<IResourceRequester*>* lpNode = mRequesterList.GetHead();
        while (lpNode)
        {
            CgsContainers::LinkedListNode<IResourceRequester*>* lpNext =
                static_cast<CgsContainers::LinkedListNode<IResourceRequester*>*>(lpNode->GetNextNode());
            if (lpNode->mData == lpRequester)
            {
                mRequesterList.RecycleNode(lpNode);
                lbRemovedAny = true;
            }
            lpNode = lpNext;
        }
        return lbRemovedAny;
    }

    // =========================================================================
    // QueuedResource
    // =========================================================================

    // 0x82695710 -- build a queued (unload) item from a RequestedResource node payload.
    ResourceRegistrar::QueuedResource::QueuedResource(const RequestedResource& lrSource)
    {
        // handle (+0x118/+0x11C) <- src handle (+0x48/+0x4C)
        mResourceHandle = lrSource.mResourceHandle;

        // Resource identity/hash (+0x120/+0x128) <- the requested resource.
        mResourceId = lrSource.mResourceHandle.GetResourceId();
        muResourceNameHash = static_cast<u32>(lrSource.miResourceType);

        // FAITHFUL: the ctor really sets mState (+0x130) = 5; +0x12C = 0; +0x138 (mpRequester) = 0.
        miPad12C = 0;
        mState   = 5;

        // FLAG (fix): the X360 ctor zeroes +0x140 (`stb r26,0x140(r28)`, r26=0) -- the
        // mbRelinquished flag UpdateRequests later reads. Previously omitted, leaving
        // the flag uninitialised. ARTIST 0x82695768 confirms the zero-init. (Byte store on X360;
        // zeroing the field is the faithful intent.)
        mbRelinquished = false;
        std::memset(maPad141, 0, sizeof(maPad141));

        // mResourceClass (+0x134) <- src miId12C (+0x12C); mpRequester (+0x138) = 0
        mResourceClass = lrSource.miId12C;
        mpRequester = 0;

        // mBundleId (+0x13C) <- src miId130 (+0x130)
        mBundleId = lrSource.miId130;

        // macBundleName (+0xD8) <- strncpy(src bundle name, 64).
        strncpy(macBundleName, lrSource.macBundleName, 64);

        // Bind the embedded receiver queue: cap 0xC0 (+0x10), align 16 (+0x14), buffer base = this+0x18,
        // then Clear() it. The X360 writes mReceiverQueue's members directly then calls Clear; Construct
        // does the equivalent (binds buffer/cap/align then Clears).
        mReceiverQueue.Construct(macReceiverBuffer, 0xC0, 16);
    }

    // 0x82697BA0 -- build a fresh load request from the public LoadAsset arguments.
    ResourceRegistrar::QueuedResource::QueuedResource(
        const char* lpcResourceName, const char* lpcFileName,
        IResourceRequester* lpResourceRequester, s32 liBundleId, EType leType)
    {
        mResourceHandle.Clear();
        miPad12C = 0;
        mResourceClass = static_cast<s32>(leType);
        mpRequester = lpResourceRequester;
        mBundleId = liBundleId;
        mbRelinquished = false;
        std::memset(maPad141, 0, sizeof(maPad141));
        miPad144 = 0;

        CGS_ASSERT(lpcResourceName || lpcFileName, "lpcResourceName || lpcFileName");

        if (lpcFileName)
        {
            CGS_ASSERT(strlen(lpcFileName) < sizeof(macBundleName), "String too long");
            strncpy(macBundleName, lpcFileName, sizeof(macBundleName));
            macBundleName[sizeof(macBundleName) - 1] = 0;
            mState = 0;
        }
        else
        {
            macBundleName[0] = 0;
            mState = 2;
        }

        if (lpcResourceName)
        {
            mResourceId.SetHash(static_cast<u64>(static_cast<u32>(
                CgsResource::ID::HashString(reinterpret_cast<const u8*>(lpcResourceName)))));
            muResourceNameHash = rw::RwHash32String(lpcResourceName, KU_FNV_OFFSET_BASIS);
        }
        else
        {
            mResourceId.SetHash(0);
            muResourceNameHash = 0;
        }

        mReceiverQueue.Construct(macReceiverBuffer, 0xC0, 16);
    }

    ResourceRegistrar::QueuedResource&
    ResourceRegistrar::QueuedResource::operator=(const QueuedResource& lrSource)
    {
        if (this == &lrSource)
            return *this;

        strncpy(macBundleName, lrSource.macBundleName, sizeof(macBundleName));
        macBundleName[sizeof(macBundleName) - 1] = 0;
        mResourceHandle = lrSource.mResourceHandle;
        mResourceId = lrSource.mResourceId;
        muResourceNameHash = lrSource.muResourceNameHash;
        miPad12C = lrSource.miPad12C;
        mState = lrSource.mState;
        mResourceClass = lrSource.mResourceClass;
        mpRequester = lrSource.mpRequester;
        mBundleId = lrSource.mBundleId;
        mbRelinquished = lrSource.mbRelinquished;
        std::memcpy(maPad141, lrSource.maPad141, sizeof(maPad141));
        miPad144 = lrSource.miPad144;

        // Preserve any responses while rebinding the receiver to this object's own buffer.
        mReceiverQueue.Construct(macReceiverBuffer, 0xC0, 16);
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventType = lrSource.mReceiverQueue.GetFirstEvent(&lpEvent, &liEventSize);
        while (liEventType >= 0)
        {
            mReceiverQueue.AddEvent(lpEvent, liEventType, liEventSize);
            liEventType = lrSource.mReceiverQueue.GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }
        return *this;
    }

    // 0x827007E8 -- load bundle -> acquire named resource -> optional AttribSys vault.
    bool ResourceRegistrar::QueuedResource::Prepare(ResourceRegistrar& lrOwner)
    {
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;

        switch (mState)
        {
        case 0:
            mState = 0;
            lrOwner.mResourceRequestInterface.LoadBundle(
                &mReceiverQueue, 1, 6, macBundleName, false);
            // fall through: a response cannot normally exist in the same frame.
        case 1:
        {
            mState = 1;
            if (mReceiverQueue.GetLength() <= 0)
                return false;
            const s32 liType = mReceiverQueue.GetFirstEvent(&lpEvent, &liEventSize);
            CGS_ASSERT(liType == 2,
                "mReceiverQueue.GetFirstEvent( &lpEvent, &liEventSize ) == CgsResource::ResourceIO::EVENT_LOADBUNDLE");
            const CgsResource::Events::LoadBundleResponse* lpLoad =
                reinterpret_cast<const CgsResource::Events::LoadBundleResponse*>(lpEvent);
            CGS_ASSERT(lpLoad != 0, "lpEvent");
            CGS_ASSERT(lpLoad->miEventId == 1,
                "lpLoad->GetEventId() == E_USER_BUNDLE_LOAD_REQUEST_ID");
            // fall through to the resource-acquire leg.
        }
        case 2:
            mState = 2;
            if (muResourceNameHash == 0)
            {
                // The console stamps ready here but reports it on the following tick.
                mState = 5;
                return false;
            }
            mReceiverQueue.Clear();
            {
                CgsResource::Events::AcquireResourceRequest lRequest;
                lRequest.mpUser = &mReceiverQueue;
                lRequest.miEventId = 2;
                lRequest.miPoolId = mBundleId;
                lRequest.mResourceId = mResourceId;
                lRequest.mbCheckRefCount = false;
                lrOwner.mResourceRequestInterface.mRequestQueue.AddEvent(&lRequest, 4);
            }
            // fall through: wait for the resource module's response.
        case 3:
        {
            mState = 3;
            if (mReceiverQueue.GetLength() <= 0)
                return false;
            const s32 liType = mReceiverQueue.GetFirstEvent(&lpEvent, &liEventSize);
            CGS_ASSERT(liType == 4,
                "mReceiverQueue.GetFirstEvent( &lpEvent, &liEventSize ) == CgsResource::ResourceIO::EVENT_ACQUIRERESOURCE");
            const CgsResource::Events::AcquireResourceResponse* lpAcquire =
                reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEvent);
            CGS_ASSERT(lpAcquire != 0, "lpAcquire");
            CGS_ASSERT(lpAcquire->miEventId == 2,
                "lpAcquire->GetEventId() == E_USER_RESOURCE_REQUEST_ID");

            CgsResource::ResourceHandle lResponseHandle;
            lResponseHandle.mpResourceMemory = lpAcquire->mpResourceMemory;
            lResponseHandle.mpSourceEntry = lpAcquire->mpSourceEntry;
            CGS_ASSERT(lpAcquire->mResourceId == mResourceId &&
                       lResponseHandle.GetResourceId() == mResourceId,
                "ResourceRegistrar : The resource system gave us the wrong resource.");
            mResourceHandle = lResponseHandle;
            mReceiverQueue.Clear();
            // fall through to the resource-class leg.
        }
        case 4:
            mState = 4;
            if (mResourceClass == E_DATA)
            {
                mState = 5;
                return true;
            }
            if (mResourceClass == E_ATTRIBSYS && PrepareAttribSys(lrOwner))
            {
                mState = 5;
                return true;
            }
            return false;
        case 5:
            mState = 5;
            return true;
        case 9:
            mState = 9;
            return false;
        default:
            CGS_ASSERT(false, "ResourceRegistrar::QueuedResource::Prepare() - Bad State");
            return false;
        }
    }

    // 0x827005D0 -- unregister the vault, clear its handle, then unload its bundle.
    bool ResourceRegistrar::QueuedResource::Release(ResourceRegistrar& lrOwner)
    {
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;

        switch (mState)
        {
        case 0:
            mState = 0;
            return true;
        case 5:
        case 6:
            mState = 6;
            if (mResourceClass == E_ATTRIBSYS)
            {
                if (!ReleaseAttribSys(lrOwner))
                    return false;
            }
            else
            {
                CGS_ASSERT(mResourceClass == E_DATA, "Bad State");
            }
            // fall through.
        case 7:
            mState = 7;
            mResourceHandle.Clear();
            if (macBundleName[0])
            {
                lrOwner.mResourceRequestInterface.UnloadBundle(
                    &mReceiverQueue, 4, mBundleId, macBundleName);
            }
            // fall through.
        case 8:
            mState = 8;
            if (!macBundleName[0])
            {
                mState = 0;
                return true;
            }
            if (mReceiverQueue.GetLength() <= 0)
                return false;
            {
                const s32 liType = mReceiverQueue.GetFirstEvent(&lpEvent, &liEventSize);
                CGS_ASSERT(liType == 3,
                    "mReceiverQueue.GetFirstEvent( &lpEvent, &liEventSize ) == CgsResource::ResourceIO::EVENT_UNLOADBUNDLE");
                const CgsResource::Events::UnloadBundleResponse* lpUnload =
                    reinterpret_cast<const CgsResource::Events::UnloadBundleResponse*>(lpEvent);
                CGS_ASSERT(lpUnload != 0, "lpEvent");
                CGS_ASSERT(lpUnload->miEventId == 4,
                    "lpUnLoad->GetEventId() == E_USER_BUNDLE_UNLOAD_REQUEST_ID");
            }
            mState = 0;
            return true;
        default:
            CGS_ASSERT(false, "Bad State in release machine");
            return false;
        }
    }

    namespace
    {
        struct VaultResponse : public CgsModule::Event
        {
            s32 miEventId;
        };
    }

    // 0x826FBD68 -- register this acquired handle as a resident AttribSys vault.
    bool ResourceRegistrar::QueuedResource::PrepareAttribSys(ResourceRegistrar& lrOwner)
    {
        if (miPad12C == 0)
        {
            mReceiverQueue.Clear();
            lrOwner.mAttribSysRequestInterface.RegisterVault(
                &mReceiverQueue, mResourceHandle, 3,
                CgsAttribSys::AttribSysIO::E_VAULT_TYPE_RESIDENT);
            miPad12C = 1;
        }
        if (miPad12C == 1)
        {
            if (mReceiverQueue.GetLength() <= 0)
                return false;
            const CgsModule::Event* lpEvent = 0;
            s32 liEventSize = 0;
            const s32 liType = mReceiverQueue.GetFirstEvent(&lpEvent, &liEventSize);
            CGS_ASSERT(liType == 3,
                "mReceiverQueue.GetFirstEvent( &lpEvent, &liEventSize ) == CgsAttribSys::AttribSysIO::E_OUT_EVENT_VAULT_REGISTERED");
            const VaultResponse* lpRegistered = reinterpret_cast<const VaultResponse*>(lpEvent);
            CGS_ASSERT(lpRegistered != 0, "lpVaultRegistered");
            CGS_ASSERT(lpRegistered->miEventId == 3,
                "lpVaultRegistered->GetEventId() == E_USER_ATTRIBSYS_REQUEST_ID");
            mReceiverQueue.Clear();
            miPad12C = 2;
        }
        return miPad12C == 2;
    }

    // 0x826FBE98 -- unregister the resident vault before its bundle is released.
    bool ResourceRegistrar::QueuedResource::ReleaseAttribSys(ResourceRegistrar& lrOwner)
    {
        if (miPad12C == 0)
        {
            mReceiverQueue.Clear();
            lrOwner.mAttribSysRequestInterface.UnregisterVault(
                &mReceiverQueue, mResourceHandle, 3);
            miPad12C = 1;
        }
        if (miPad12C == 1)
        {
            if (mReceiverQueue.GetLength() <= 0)
                return false;
            const CgsModule::Event* lpEvent = 0;
            s32 liEventSize = 0;
            const s32 liType = mReceiverQueue.GetFirstEvent(&lpEvent, &liEventSize);
            CGS_ASSERT(liType == 5,
                "mReceiverQueue.GetFirstEvent( &lpEvent, &liEventSize ) == CgsAttribSys::AttribSysIO::E_OUT_EVENT_VAULT_UNREGISTERED");
            const VaultResponse* lpUnregistered = reinterpret_cast<const VaultResponse*>(lpEvent);
            CGS_ASSERT(lpUnregistered != 0, "lpVaultUnRegistered");
            CGS_ASSERT(lpUnregistered->miEventId == 3,
                "lpVaultUnRegistered->GetEventId() == E_USER_ATTRIBSYS_REQUEST_ID");
            mReceiverQueue.Clear();
            miPad12C = 2;
        }
        if (miPad12C == 2)
            return true;
        CGS_ASSERT(false, "Bad State.");
        return false;
    }

    // =========================================================================
    // ResourceRegistrar
    // =========================================================================

    // 0x826B0470
    void ResourceRegistrar::Construct()
    {
        // The two request interfaces: Construct then Clear (the X360 calls both back-to-back).
        mResourceRequestInterface.Construct();
        mResourceRequestInterface.Clear();
        mAttribSysRequestInterface.mRequestQueue.Construct();
        mAttribSysRequestInterface.mRequestQueue.Clear();

        // The three pools: free list seeded with the node pool, live list empty (helper Construct does both).
        mLoadedResourceList.Construct();
        mLoadingQueuedResourceList.Construct();
        mUnLoadingQueuedResourceList.Construct();

        // The removal-candidate map: empty-but-usable (count word -> 0). X360 stores 0 to a1[3164].
        mapRemovalCandidates.Clear();
    }

    // 0x826B0A60 -- is there a loading-queued item for lpRequester (optionally matching liResourceType)?
    // X360 prototype: SearchQueued(this, a2 /*requester*/, a3 /*match-type bool*/, a4 /*resource type*/).
    // The X360 reads the NODE pointer; the QueuedResource data lives at node+8, so Head[76] (node byte
    // 304) == data +0x128 == miResourceType and Head[80] (node byte 320) == data +0x138 == mpRequester.
    // Match: (!lbMatchType || liResourceType == miResourceType) && lpRequester == mpRequester.
    bool ResourceRegistrar::SearchQueued(IResourceRequester* lpRequester, bool lbMatchType, s32 liResourceType)
    {
        QueuedNode* lpNode = mLoadingQueuedResourceList.GetHead();
        while (lpNode)
        {
            // X360 BrnResourceRegistrar.cpp:478 asserts lpIterator->GetData() (the node's payload
            // pointer is non-null). For a live pool node the payload is always present; assert the
            // node pointer itself, which carries the same intent without the always-true &mData!=0.
            CGS_ASSERT(lpNode != 0, "lpIterator->GetData()");

            if ((!lbMatchType || liResourceType == static_cast<s32>(lpNode->mData.muResourceNameHash)) &&
                lpRequester == lpNode->mData.mpRequester)
            {
                return true;
            }
            lpNode = static_cast<QueuedNode*>(lpNode->GetNextNode());
        }
        return false;
    }

    // 0x826B0B08 -- resolve (resource name, bundle name) -> ResourceHandle*, or 0.
    // X360 prototype: GetResource(this, a2 /*bundle name, may be null*/, a3 /*resource name*/).
    CgsResource::ResourceHandle* ResourceRegistrar::GetResource(const char* lpcBundleName,
                                                                const char* lpcResourceName)
    {
        const u32 luResourceHash = rw::RwHash32String(lpcResourceName, KU_FNV_OFFSET_BASIS);
        u32 luBundleHash = 0;
        if (lpcBundleName)
        {
            luBundleHash = rw::RwHash32String(lpcBundleName, KU_FNV_OFFSET_BASIS);
        }

        // Walk the loading requested LIVE list. The X360 reads the NODE (Head); the RequestedResource
        // data lives at node+8, so Head[18] (node byte 72) == data +0x40 == miResourceType (keyed by the
        // resource-name hash) and Head[19] (node byte 76) == data +0x44 == muBundleNameHash. Match both:
        //   while (Head[19] != bundleHash || Head[18] != resourceHash) Head = Head->next;
        RequestedNode* lpNode = mLoadedResourceList.GetHead();
        while (lpNode)
        {
            if (lpNode->mData.miResourceType == static_cast<s32>(luResourceHash) &&
                lpNode->mData.muBundleNameHash == luBundleHash)
            {
                CgsResource::ResourceHandle* lpHandle = &lpNode->mData.mResourceHandle;

                // Debug cross-check: the CRC name hash must equal the stored resource id.
                s32 liDebugId = CgsResource::ID::HashString(reinterpret_cast<const u8*>(lpcResourceName));
                CgsResource::ID lStoredId = lpHandle->GetResourceId();
                if (static_cast<s32>(lStoredId.GetHash()) != liDebugId)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert("lDebugId == lpData->mResourceHandle.GetResourceId()",
                                               KAC_RR_FILE, 559);
                    CgsDev::Assert::EndAssert();
                }
                return lpHandle;
            }
            lpNode = static_cast<RequestedNode*>(lpNode->GetNextNode());
        }
        return 0;
    }

    // 0x826E2220 -- pull a requester's outstanding requests out of the broker.
    void ResourceRegistrar::RemoveRequests(IResourceRequester* lpRequester)
    {
        if (SearchQueued(lpRequester, false, 0))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("!DoesRequesterHaveAnyQueued( lpResourceRequester )",
                                       KAC_RR_FILE, 367);
            CgsDev::Assert::EndAssert();
        }

        for (RequestedNode* lpNode = mLoadedResourceList.GetHead();
             lpNode;
             lpNode = static_cast<RequestedNode*>(lpNode->GetNextNode()))
        {
            if (lpNode->mData.ClearReferencesToResource(lpRequester))
            {
                // If the node now has no live requesters, queue it for removal (guarded by !Contains).
                if (lpNode->mData.GetNumberOfReferences() == 0)
                {
                    if (mapRemovalCandidates.Contains(lpNode))
                    {
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert("!mapRemovalCandidates.Contains(lpNode)",
                                                   "..\\..\\..\\GameSource\\Sound/BrnResourceRegistrar.h", 513);
                        CgsDev::Assert::EndAssert();
                    }
                    mapRemovalCandidates.Append(lpNode);
                }
            }
        }
    }

    // BrnResourceRegistrar.cpp:398.  A zone can leave the streaming set while
    // its resource-only request is still pending.  Mark the matching queued
    // request relinquished so UpdateRequests retires it without notifying the
    // requester, then clear an already-loaded reference to the same resource.
    void ResourceRegistrar::RemoveRefsToResource(IResourceRequester* lpRequester,
                                                  const char* lpcResourceName)
    {
        const u32 lu32NameHash =
            rw::RwHash32String(lpcResourceName, KU_FNV_OFFSET_BASIS);

        for (QueuedNode* lpQueuedIterator = mLoadingQueuedResourceList.GetHead();
             lpQueuedIterator;
             lpQueuedIterator =
                 static_cast<QueuedNode*>(lpQueuedIterator->GetNextNode()))
        {
            QueuedResource& lrData = lpQueuedIterator->mData;
            if (lrData.muResourceNameHash == lu32NameHash &&
                lrData.mpRequester == lpRequester)
            {
                lrData.mpRequester = 0;
                lrData.mbRelinquished = true;
            }
        }

        for (RequestedNode* lpIterator = mLoadedResourceList.GetHead();
             lpIterator;
             lpIterator = static_cast<RequestedNode*>(lpIterator->GetNextNode()))
        {
            RequestedResource& lrData = lpIterator->mData;
            if (lrData.miResourceType == static_cast<s32>(lu32NameHash) &&
                lrData.ClearReferencesToResource(lpRequester))
            {
                AddNodeToRemoveResourceCandidateList(lpIterator);
            }
        }
    }

    // 0x82695860 -- queue a requested-resource node for removal once its reference count hits 0.
    void ResourceRegistrar::AddNodeToRemoveResourceCandidateList(RequestedNode* lpNode)
    {
        if (lpNode->mData.GetNumberOfReferences() != 0)
        {
            return;
        }
        if (mapRemovalCandidates.Contains(lpNode))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("!mapRemovalCandidates.Contains(lpNode)",
                                       "..\\..\\..\\GameSource\\Sound/BrnResourceRegistrar.h", 513);
            CgsDev::Assert::EndAssert();
        }
        mapRemovalCandidates.Append(lpNode);
    }

    // 0x826B0560 -- resolve completed loads. The X360 OUTER walk is over the loading-QUEUED list
    // (a1+6068, byte = node, data at node+8); the INNER walk is over the loading-REQUESTED list
    // (a1+51860). For each queued item it hashes the queued bundle name (data +0xD8) and reads the
    // queued resource type (data +0x128), then finds the requested node whose (miResourceType,
    // muBundleNameHash) match. On a match:
    //   * a per-queued-item state guard fires the debug assert if mState is not in {0,2,5,9};
    //   * if the queued item's mbRelinquished (+0x140) is set, the requested node is queued
    //     for removal once it has no live refs (AddNodeToRemoveResourceCandidateList);
    //   * otherwise the queued item's requester (mpRequester, +0x138) is transferred onto the
    //     requested node's mRequesterList, the requested node is dropped from the removal candidates
    //     if present, and -- when the queued list holds no other duplicate referencing that requester
    //     -- the requester is notified through ResourcesAreReady (the X360 slot-0 indirect call);
    //   * the matched queued node is then recycled (live -> free).
    void ResourceRegistrar::UpdateRequests()
    {
        QueuedNode* lpQueued = mLoadingQueuedResourceList.GetHead();
        while (lpQueued)
        {
            // Hash the queued item's bundle name (data +0xD8); 0 when empty.
            u32 luBundleHash = 0;
            if (lpQueued->mData.macBundleName[0] != 0)
            {
                luBundleHash = rw::RwHash32String(lpQueued->mData.macBundleName, KU_FNV_OFFSET_BASIS);
            }
            const s32 liResType = static_cast<s32>(lpQueued->mData.muResourceNameHash); // data +0x128

            bool lbAdvanceQueued = true;

            // Inner: find the matching requested node (data +0x40 == type, +0x44 == bundle hash).
            RequestedNode* lpRequested = mLoadedResourceList.GetHead();
            while (lpRequested)
            {
                if (lpRequested->mData.miResourceType == liResType &&
                    lpRequested->mData.muBundleNameHash == luBundleHash)
                {
                    // Per-queued-item state guard (X360: mState must be one of 9/5/0/2).
                    const s32 liState = lpQueued->mData.mState; // data +0x130
                    if (liState != 9 && liState != 5 && liState != 0 && liState != 2)
                    {
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert("State", KAC_RR_FILE, 137);
                        CgsDev::Assert::EndAssert();
                    }

                    if (lpQueued->mData.mbRelinquished) // data +0x140
                    {
                        // FLAG (fix): the X360 guard is `FindFirstInstanceOf(lpRequested) == -1`
                        //   (queue for removal only if NOT already a candidate), NOT a refcount
                        //   check. ARTIST 0x826B0560 (`if (ResourceR(a1+12160,&v25) == -1)` with
                        //   v25 = the requested node v8) then AddNodeToRemoveResourceCandidateList.
                        //   The refcount==0 guard is enforced inside AddNode itself; the caller's
                        //   guard is the not-contained test. Previously used GetNumberOfReferences()==0.
                        if (mapRemovalCandidates.FindFirstInstanceOf(lpRequested) == -1)
                        {
                            AddNodeToRemoveResourceCandidateList(lpRequested);
                        }
                    }
                    else if (lpQueued->mData.mpRequester != 0) // data +0x138
                    {
                        // Transfer the queued requester onto the requested node's list.
                        lpRequested->mData.mRequesterList.AddTail(lpQueued->mData.mpRequester);

                        // Drop the requested node from the removal candidates if it was queued for GC.
                        s32 liCandidateIndex = mapRemovalCandidates.FindFirstInstanceOf(lpRequested);
                        if (liCandidateIndex != -1)
                        {
                            mapRemovalCandidates.EraseFast(static_cast<u32>(liCandidateIndex));
                        }
                        bool lbRequesterStillQueued = false;
                        for (QueuedNode* lpOther = mLoadingQueuedResourceList.GetHead();
                             lpOther;
                             lpOther = static_cast<QueuedNode*>(lpOther->GetNextNode()))
                        {
                            if (lpOther != lpQueued &&
                                lpOther->mData.mpRequester == lpQueued->mData.mpRequester)
                            {
                                lbRequesterStillQueued = true;
                                break;
                            }
                        }
                        if (!lbRequesterStillQueued)
                            lpQueued->mData.mpRequester->ResourcesAreReady();
                    }

                    // Recycle the matched queued node (live -> free); resume the inner walk past it and
                    // continue the OUTER walk from the queued node that follows (X360 advances v4 first).
                    QueuedNode* lpNextQueued = static_cast<QueuedNode*>(lpQueued->GetNextNode());
                    mLoadingQueuedResourceList.RecycleNode(lpQueued);
                    lpQueued = lpNextQueued;
                    lbAdvanceQueued = false;
                    break;
                }
                lpRequested = static_cast<RequestedNode*>(lpRequested->GetNextNode());
            }

            if (lbAdvanceQueued)
            {
                lpQueued = static_cast<QueuedNode*>(lpQueued->GetNextNode());
            }
        }
    }

    // 0x82702108 -- step each queued item.
    void ResourceRegistrar::UpdateQueued()
    {
        // Loading queue: Prepare() each; when ready, promote to the requested list (AddTail) and leave
        // the queued node in place (the X360 does NOT recycle here -- the requested-list promotion is
        // the visible effect; the queued node is reclaimed via the requested-match path in UpdateRequests).
        for (QueuedNode* lpNode = mLoadingQueuedResourceList.GetHead();
             lpNode;
             lpNode = static_cast<QueuedNode*>(lpNode->GetNextNode()))
        {
            CGS_ASSERT(lpNode != 0, "lpQueuedResource"); // X360 BrnResourceRegistrar.cpp:244
            if (lpNode->mData.Prepare(*this))
            {
                // X360 (sub_826EB100): grab a free RequestedResource node and placement-construct the
                // RequestedResource directly in the node from the queued source, then AddTail it.
                // AddTail copy-assigns into the destination node; RequestedResource::operator=
                // reconstructs its embedded self-referential requester pool before copying entries.
                RequestedResource lRequested(lpNode->mData);
                mLoadedResourceList.AddTail(lRequested);
            }
        }

        // Unloading queue: Release() each; recycle finished nodes (live -> free).
        QueuedNode* lpUnload = mUnLoadingQueuedResourceList.GetHead();
        while (lpUnload)
        {
            CGS_ASSERT(lpUnload != 0, "lpQueuedResource"); // X360 BrnResourceRegistrar.cpp:271
            QueuedNode* lpNext = static_cast<QueuedNode*>(lpUnload->GetNextNode());
            if (lpUnload->mData.Release(*this))
            {
                mUnLoadingQueuedResourceList.RecycleNode(lpUnload);
            }
            lpUnload = lpNext;
        }
    }

    // 0x826E2080 -- GC removal candidates into the unloading queued list (batch cap 18), then
    // AppendArray the un-GC'd remainder back into the candidate map.
    void ResourceRegistrar::ClearUnreferancedFiles()
    {
        ::Array<RequestedNode*, 124> lRemainder;
        lRemainder.Clear();

        const u32 luCount = mapRemovalCandidates.GetLength();
        u32 luIndex = 0;
        bool lbBatchFull = false;

        for (; luIndex < luCount; ++luIndex)
        {
            RequestedNode* lpNode = mapRemovalCandidates[luIndex];

            // The candidate must have no live requesters left.
            if (lpNode->mData.GetNumberOfReferences() != 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpListNode->GetData()->GetNumberOfReferences() == 0",
                                           KAC_RR_FILE, 311);
                CgsDev::Assert::EndAssert();
            }

            // Build an unloading QueuedResource from the requested node and AddHead it onto the
            // unload list. QueuedResource::operator= rebinds the destination receiver queue to its
            // own embedded buffer while preserving any queued responses.
            QueuedResource lUnloadingItem(lpNode->mData);
            QueuedNode* lpAdded = mUnLoadingQueuedResourceList.AddHead(lUnloadingItem);
            if (lpAdded == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mUnLoadingQueuedResourceList.AddHead( lUnloadingItem )",
                                           KAC_RR_FILE, 320);
                CgsDev::Assert::EndAssert();
            }

            // Retire the requested node (live -> free) now that it is unloading.
            mLoadedResourceList.RecycleNode(lpNode);

            // Batch cap: stop GC'ing once the unloading list has reached 18 in flight.
            if (mUnLoadingQueuedResourceList.CountElements() == 18)
            {
                lbBatchFull = true;
                break;
            }
        }

        if (lbBatchFull)
        {
            // Carry the un-GC'd remainder (candidates after the one that hit the cap) back into the map.
            for (u32 luRem = luIndex + 1; luRem < mapRemovalCandidates.GetLength(); ++luRem)
            {
                lRemainder.Append(mapRemovalCandidates[luRem]);
            }
        }

        // Clear the map then re-append the remainder (X360 a1[3164] = 0 then AppendArray).
        mapRemovalCandidates.Clear();
        mapRemovalCandidates.AppendArray(lRemainder);
    }

    // 0x82702228
    void ResourceRegistrar::Update()
    {
        mResourceRequestInterface.Clear();
        mAttribSysRequestInterface.mRequestQueue.Clear();

        if (mAttribSysRequestInterface.mRequestQueue.GetLength())
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mAttribSysRequestInterface.GetRequestQueue()->GetLength() == 0",
                                       KAC_RR_FILE, 513);
            CgsDev::Assert::EndAssert();
        }
        if (mResourceRequestInterface.mRequestQueue.GetLength())
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mResourceRequestInterface.GetRequestQueue()->GetLength() == 0",
                                       KAC_RR_FILE, 514);
            CgsDev::Assert::EndAssert();
        }

        UpdateRequests();
        UpdateQueued();
        ClearUnreferancedFiles();
    }

    // 0x826E1F88 -- mark a duplicate load as state 9, then append it regardless so
    // UpdateRequests can merge its requester into the already-loaded entry.
    void ResourceRegistrar::AddRequest(const QueuedResource& lrRequest)
    {
        QueuedResource lRequest;
        lRequest = lrRequest;
        const u32 luBundleHash = lRequest.macBundleName[0]
            ? rw::RwHash32String(lRequest.macBundleName, KU_FNV_OFFSET_BASIS)
            : 0;

        for (QueuedNode* lpNode = mLoadingQueuedResourceList.GetHead();
             lpNode;
             lpNode = static_cast<QueuedNode*>(lpNode->GetNextNode()))
        {
            const u32 luExistingBundleHash = lpNode->mData.macBundleName[0]
                ? rw::RwHash32String(lpNode->mData.macBundleName, KU_FNV_OFFSET_BASIS)
                : 0;
            if (luBundleHash == luExistingBundleHash &&
                lRequest.muResourceNameHash == lpNode->mData.muResourceNameHash)
            {
                lRequest.mState = 9;
                break;
            }
        }
        mLoadingQueuedResourceList.AddTail(lRequest);
    }

    // 0x826E2348 -- public requester entry point. Pool 6 is the sound-data pool.
    void IResourceRequester::LoadAsset(const char* lpcBundleName,
                                       const char* lpcResourceName,
                                       ResourceRegistrar::EType leType)
    {
        CGS_ASSERT(lpcBundleName, "lpFilename");
        ResourceRegistrar::QueuedResource lRequest(
            lpcResourceName, lpcBundleName, this, 6, leType);
        GetResourceRegistrar().AddRequest(lRequest);
    }

    void IResourceRequester::LoadAsset(const char* lpcResourceName,
                                       EResourcePool lePool,
                                       ResourceRegistrar::EType leType)
    {
        CGS_ASSERT(lpcResourceName, "lpcResourceName");
        ResourceRegistrar::QueuedResource lRequest(
            lpcResourceName, 0, this, static_cast<s32>(lePool), leType);
        GetResourceRegistrar().AddRequest(lRequest);
    }

    CgsResource::ResourceHandle IResourceRequester::GetAsset(
        const char* lpcFilename, const char* lpcResourceName)
    {
        CGS_ASSERT(lpcResourceName, "lpResourceName");
        CgsResource::ResourceHandle* lpResourceHandle =
            GetResourceRegistrar().GetResource(lpcFilename, lpcResourceName);
        CGS_ASSERT(lpResourceHandle, "lpResourceHandle");
        return lpResourceHandle ? *lpResourceHandle : CgsResource::NULLResourceHandle;
    }
}
}
