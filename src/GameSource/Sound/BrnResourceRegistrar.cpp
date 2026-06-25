#include "GameSource/Sound/BrnResourceRegistrar.h"

#include <cstring>   // strncpy

// BrnSound::Logic::ResourceRegistrar -- the sound-logic streaming-resource broker. Faithfully
// decompiled from BURNOUT_X360_ARTIST.XEX (per-function addresses cited at each body). The
// queue-driven load/unload state machines (QueuedResource::Prepare/Release/PrepareAttribSys/
// ReleaseAttribSys) bottom out in BrnResource::GameDataIO::RequestInterface<4096> and
// CgsAttribSys::AttribSysIO::AttribSysRequestInterface<2048>, neither of which is reconstructed at
// the shape the X360 calls here; those four are stubbed (see each FLAG). Everything else is the real
// X360 logic. The broker is not driven at runtime yet (its owner SoundLogicModule isn't prepared),
// so this is faithful-and-compiling code, not a live path.

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
        miResourceType = lrSource.miResourceType;

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

        // miResourceType (+0x128) <- src miResourceType (+0x40)
        miResourceType = lrSource.miResourceType;

        // FAITHFUL: the ctor really sets mState (+0x130) = 5; +0x12C = 0; +0x138 (mpRequester) = 0.
        miPad12C = 0;
        mState   = 5;

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

    // 0x827007E8 -- load state machine.
    // FLAG: stubbed -- the load legs call BrnResource::GameDataIO::RequestInterface<4096>::LoadBundle
    //   (X360 0x827007E8) and the AttribSys leg, neither reconstructed at the shape used here; the
    //   real machine returns false until the (not-yet-reconstructed) request interfaces report ready,
    //   so returning false (not-ready) is the faithful no-progress result.
    bool ResourceRegistrar::QueuedResource::Prepare(ResourceRegistrar& /*lrOwner*/)
    {
        return false;
    }

    // 0x827005D0 -- release state machine.
    // FLAG: stubbed -- the unload legs call RequestInterface<4096>::UnloadBundle (X360 0x827005D0),
    //   not reconstructed; the real machine completes (returns true) once the unload event arrives,
    //   so returning true (done) lets UpdateQueued recycle the node without leaking.
    bool ResourceRegistrar::QueuedResource::Release(ResourceRegistrar& /*lrOwner*/)
    {
        return true;
    }

    // 0x826FBD68 -- AttribSys vault register leg.
    // FLAG: stubbed -- calls CgsAttribSys::AttribSysIO::AttribSysRequestInterface<2048>::RegisterVault
    //   (not reconstructed); returns true (leg complete).
    bool ResourceRegistrar::QueuedResource::PrepareAttribSys(ResourceRegistrar& /*lrOwner*/)
    {
        return true;
    }

    // 0x826FBE98 -- AttribSys vault unregister leg.
    // FLAG: stubbed -- calls AttribSysRequestInterface<2048>::UnregisterVault (not reconstructed);
    //   returns true (leg complete).
    bool ResourceRegistrar::QueuedResource::ReleaseAttribSys(ResourceRegistrar& /*lrOwner*/)
    {
        return true;
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
        mAttribSysRequestInterface.Construct();
        mAttribSysRequestInterface.Clear();

        // The three pools: free list seeded with the node pool, live list empty (helper Construct does both).
        mLoadingRequestedResourceList.Construct();
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

            if ((!lbMatchType || liResourceType == lpNode->mData.miResourceType) &&
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
        RequestedNode* lpNode = mLoadingRequestedResourceList.GetHead();
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

        for (RequestedNode* lpNode = mLoadingRequestedResourceList.GetHead();
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
    //   * if the queued item's mbResourceStillRequested (+0x140) is set, the requested node is queued
    //     for removal once it has no live refs (AddNodeToRemoveResourceCandidateList);
    //   * otherwise the queued item's requester (mpRequester, +0x138) is transferred onto the
    //     requested node's mRequesterList, the requested node is dropped from the removal candidates
    //     if present, and -- when the queued list holds no other duplicate referencing that requester
    //     -- the requester is released through its vtable (the X360 `(***v20)(*v20)` indirect call);
    //   * the matched queued node is then recycled (live -> free).
    // FLAG: the requester-release indirect call `(***v20)(*v20)` (X360) routes through an
    //   IResourceRequester vtable slot that is not modelled on the trimmed 3-virtual interface used
    //   here; it is omitted (the transfer leg never runs because the stubbed load path leaves
    //   mpRequester null), and noted rather than invented. The structural loop is otherwise faithful.
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
            const s32 liResType = lpQueued->mData.miResourceType; // data +0x128

            bool lbAdvanceQueued = true;

            // Inner: find the matching requested node (data +0x40 == type, +0x44 == bundle hash).
            RequestedNode* lpRequested = mLoadingRequestedResourceList.GetHead();
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

                    if (lpQueued->mData.mbResourceStillRequested) // data +0x140
                    {
                        // Retire the requested node once it has no live refs.
                        if (lpRequested->mData.GetNumberOfReferences() == 0)
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
                        // FLAG: the X360 then releases the requester via an IResourceRequester vtable
                        //   slot not present on the trimmed interface; omitted (dead under the stub path).
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
                // FLAG: PC-port divergence. The generic LinkedListHelper::AddTail copy-ASSIGNS the value
                //   into the node (mData = value); RequestedResource embeds a self-referential
                //   LinkedListHelper<...,16> whose internal free/live list pointers reference its own
                //   maNodePool, so a copy carries stale pointers. The X360 builds in place (no copy).
                //   This path is dead (broker not driven at runtime + Prepare is stubbed to false), so
                //   the copy never executes; kept as the faithful structural shape. A future in-place
                //   "AddTailConstruct(src)" helper would remove the divergence.
                RequestedResource lRequested(lpNode->mData);
                mLoadingRequestedResourceList.AddTail(lRequested);
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

            // Build an unloading QueuedResource from the requested node and AddHead it onto the unload list.
            // FLAG: PC-port divergence (same as the UpdateQueued promotion): AddHead copy-ASSIGNS the
            //   QueuedResource, whose embedded BaseEventReceiverQueue mpBuffer points at its own
            //   macReceiverBuffer; a copy carries a stale buffer pointer. The X360 builds in place.
            //   Dead path (broker not driven at runtime); kept as the faithful structural shape.
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
            mLoadingRequestedResourceList.RecycleNode(lpNode);

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
        mAttribSysRequestInterface.Clear();

        if (mAttribSysRequestInterface.GetLength())
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mAttribSysRequestInterface.GetRequestQueue()->GetLength() == 0",
                                       KAC_RR_FILE, 513);
            CgsDev::Assert::EndAssert();
        }
        if (mResourceRequestInterface.GetLength())
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
}
}
