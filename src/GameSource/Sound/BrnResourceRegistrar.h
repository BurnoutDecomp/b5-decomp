#ifndef BRN_SOUND_LOGIC_BRN_RESOURCE_REGISTRAR_H
#define BRN_SOUND_LOGIC_BRN_RESOURCE_REGISTRAR_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsLinkedList.h"                // CgsContainers::LinkedListHelper / LinkedListNode
#include "GameShared/GameClasses/Containers/CgsArray.h"                     // CgsContainers::Array (removal-candidate map)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"       // CgsModule::BaseEventReceiverQueue (QueuedResource base)
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"      // CgsResource::ResourceHandle
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"           // BrnResource::GameDataIO::RequestInterface<4096>
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysModuleIO.h"   // CgsAttribSys::AttribSysIO::AttribSysRequestInterface<2048>

// =============================================================================
// BrnSound::Logic::ResourceRegistrar + IResourceRequester
//   GameSource/Sound/BrnResourceRegistrar.{h,cpp}  (DWARF home, BrnResourceRegistrar.h:42/337)
//
// The sound-logic streaming-resource broker. Effects / controls / state managers implement
// IResourceRequester; on attach they enqueue resource requests, on detach they call
// registrar.RemoveRequests(this). Each frame Update() drains the request queues, resolves
// bundle+resource name hashes into CgsResource::ResourceHandles via the AttribSys request
// interfaces, promotes queued->requested, and GCs unreferenced files. Consumers fetch resolved
// handles through IResourceRequester::GetAsset -> GetResource.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct 0x826B0470, Update 0x82702228, UpdateRequests 0x826B0560, UpdateQueued 0x82702108,
//   ClearUnreferancedFiles 0x826E2080, GetResource 0x826B0B08, SearchQueued 0x826B0A60,
//   RemoveRequests 0x826E2220, AddNodeToRemoveResourceCandidateList 0x82695860.
//   Inner classes: RequestedResource ctor 0x826ADAE8 / ClearReferencesToResource 0x826ADCA8;
//   QueuedResource ctor 0x82695710 / Prepare 0x827007E8 / Release 0x827005D0 /
//   PrepareAttribSys 0x826FBD68 / ReleaseAttribSys 0x826FBE98.
//
// This is the CANONICAL HOME: it folds the duplicate minimal IResourceRequester / ResourceRegistrar
// definitions out of BrnEffectObject.h, BrnEffectControl.h and BrnStateManager.h (which now include
// this), resolving the cross-header ODR.
//
// LAYOUT NOTE: the X360 byte offsets quoted throughout assume 4-byte ptr/vptr and 4-byte-aligned
// pools (the QueuedResource node stride is 336, the RequestedResource node stride is 316 there);
// on the 64-bit host pointer widths differ, so members are pinned BY NAME + SEQUENCE only and the
// absolute offsets are NOT static_asserted. The N pool counts are derived from the X360 offset
// spans (see each member).
// =============================================================================

// -----------------------------------------------------------------------------
// rw::RwHash32String -- the RenderWare 32-bit FNV-1a string hasher (X360 0x82BBC458).
// FNV-1a over the raw bytes (NOT lowercased -- distinct from BrnParticle's case-insensitive
// Entry::HashString) with the prime 0x01000193 (16777619); the caller supplies the offset
// basis (0x811C9DC5 == -2128831035) as the seed. The ResourceRegistrar uses it to hash bundle
// and resource names into the QueuedResource/RequestedResource lookup keys.
// FLAG: home-of-convenience. This is a vendor RenderWare routine with no reconstructed rwcore
// home yet; declared here and bodied in BrnResourceRegistrar.cpp so this TU is self-contained.
// When a vendor rwcore string-hash home lands, migrate this definition there (it would
// otherwise multiply-define).
// -----------------------------------------------------------------------------
namespace rw
{
    u32 RwHash32String(const char* lpacString, u32 luSeed);
}

namespace BrnSound
{
namespace Logic
{
    class ResourceRegistrar;
    struct IResourceRequester;   // full definition after ResourceRegistrar (LoadAsset needs EType)

    // BrnResourceRegistrar.h:42 (DWARF). The streaming-resource broker.
    class ResourceRegistrar
    {
    public:
        // BrnResourceRegistrar.h:31 (DWARF) -- the asset class an IResourceRequester::LoadAsset
        // request targets. E_DATA (0) is the streamed bundle-data path; E_ATTRIBSYS (1) the
        // AttribSys vault path. Exercised by the sound effects' SetupLoadData (X360 r6=0 -> E_DATA).
        enum EType
        {
            E_DATA      = 0,
            E_ATTRIBSYS = 1,
        };

        // Inner-class forward decls (RequestedResource's ctor takes a QueuedResource and vice versa).
        class QueuedResource;
        class RequestedResource;

        // ---------------------------------------------------------------------
        // RequestedResource (X360 inner class; payload 0x134 == 308 bytes) -- a resource the broker
        // has resolved to a handle and is keeping alive for one or more requesters. Built FROM a
        // QueuedResource by the ctor (0x826ADAE8) once the queued load completes.
        // ---------------------------------------------------------------------
        class RequestedResource
        {
        public:
            // Host-port default ctor: the X360 RequestedResource pool is raw storage (nodes are only
            // ctor'd in place when handed out). On the typed host the pool array LinkedListNode<
            // RequestedResource> maNodePool[N] must default-construct every element, so a trivial
            // default ctor is required. FLAG: added for the host pool array only; the X360 has no
            // default ctor (it never default-constructs a RequestedResource).
            RequestedResource() {}

            RequestedResource& operator=(const RequestedResource& lrSource);

            // 0x826ADAE8 -- copy the bundle name + handle out of the source QueuedResource, hash the
            // bundle name, seed the per-resource requester list (N=16), and (if the source carried a
            // requester) record it. `lrSource` is the QueuedResource the load was driven from.
            RequestedResource(const QueuedResource& lrSource);

            // 0x826ADCA8 -- move every node in mRequesterList whose payload == lpRequester from the
            // live list back to the free list; return true iff at least one was removed.
            bool ClearReferencesToResource(IResourceRequester* lpRequester);

            // Live requester count (UpdateRequests / RemoveRequests test == 0 to retire the resource).
            s32 GetNumberOfReferences() const { return mRequesterList.CountElements(); }

            // +0x00 macBundleName[64]  (strncpy from src+0xD8, NUL-terminated at [63])
            char macBundleName[64];
            // +0x40 miResourceType     (= src[0x128])
            s32  miResourceType;
            // +0x44 muBundleNameHash   (= RwHash32String(src+0xD8, 0x811C9DC5), 0 when empty)
            u32  muBundleNameHash;
            // +0x48/+0x4C mResourceHandle (= src[0x118]/[0x11C]) -- the resolved handle GetResource hands back
            CgsResource::ResourceHandle mResourceHandle;
            // +0x50 mRequesterList : LinkedListHelper<IResourceRequester*,16> (node stride 12 on X360)
            CgsContainers::LinkedListHelper<IResourceRequester*, 16> mRequesterList;
            // +0x12C / +0x130 ids (= src[0x134] / src[0x13C])
            s32  miId12C;
            s32  miId130;
        };

        // ---------------------------------------------------------------------
        // QueuedResource (X360 inner class; payload 0x148 == 328 bytes) -- a pending load/unload, driven
        // by a per-resource state machine over an embedded receiver queue + the broker's request VEQs.
        // Built either from a bundle-load request or (for unload) from a RequestedResource being retired.
        // ---------------------------------------------------------------------
        class QueuedResource
        {
        public:
            // Host-port default ctor (same rationale as RequestedResource above): the typed host pool
            // array LinkedListNode<QueuedResource> maNodePool[N] must default-construct each element.
            // FLAG: added for the host pool array only; the X360 has no default ctor.
            QueuedResource() {}

            QueuedResource(const char* lpcResourceName, const char* lpcFileName,
                           IResourceRequester* lpRequester, s32 liBundleId, EType leType);

            QueuedResource& operator=(const QueuedResource& lrSource);

            // 0x82695710 -- build the queued item FROM a RequestedResource node payload (`lrSource`):
            // copy the handle/type/ids + bundle name, init the embedded BaseEventReceiverQueue
            // (cap 0xC0, align 16, buffer @ +0x18), and set mState = 5 (FAITHFUL: the ctor really
            // sets 5 -- do NOT "fix" this).
            QueuedResource(const RequestedResource& lrSource);

            // 0x827007E8 -- drive the live load state machine; returns true when the resource is ready.
            bool Prepare(ResourceRegistrar& lrOwner);

            // 0x827005D0 -- drive the release state machine; returns true when the unload is done.
            bool Release(ResourceRegistrar& lrOwner);

            // 0x826FBD68 / 0x826FBE98 -- AttribSys vault register/unregister legs of the state machines.
            bool PrepareAttribSys(ResourceRegistrar& lrOwner);
            bool ReleaseAttribSys(ResourceRegistrar& lrOwner);

            s32  GetNumberOfReferences() const { return 0; } // queued items carry no requester refs

            // +0x00 mReceiverQueue : BaseEventReceiverQueue (cap 0xC0, align 16, buffer @ +0x18)
            CgsModule::BaseEventReceiverQueue mReceiverQueue;
            // +0x18 macReceiverBuffer[0xC0] -- the receiver queue's backing buffer (bound by the ctor)
            u8   macReceiverBuffer[0xC0];
            // +0xD8 macBundleName[64]  (strncpy from src, NUL-terminated at [0xD8+63])
            char macBundleName[64];
            // +0x118 / +0x11C mResourceHandle (= src handle). X360 stored two 32-bit handle words here;
            //   on x64 the faithful equivalent is the committed ResourceHandle (two pointers) -- the
            //   X360 32-bit split is a pointer-width artifact, not two distinct fields.
            CgsResource::ResourceHandle mResourceHandle;
            // +0x120 mResourceId -- the 64-bit CgsResource identity produced by ID::HashString.
            CgsResource::ID mResourceId;
            // +0x128 muResourceNameHash -- the FNV lookup key (zero means bundle-only request).
            u32  muResourceNameHash;
            // +0x12C unused/zeroed in the ctor (set 0)
            s32  miPad12C;
            // +0x130 mState (= 5 in the ctor; the load/release state machines step it 0..9)
            s32  mState;
            // +0x134 mResourceClass (= src[0x12C])
            s32  mResourceClass;
            // +0x138 mpRequester -- the IResourceRequester that enqueued this load (zeroed by the
            //   requested-resource release ctor; set by the public LoadAsset enqueue path).
            //   SearchQueued matches against it (X360 node data +0x138); RequestedResource's ctor reads
            //   it to seed the requested-resource requester list. X360: a 4-byte ptr word; ptr on x64.
            IResourceRequester* mpRequester;
            // +0x13C mBundleId (= src[0x130])
            s32  mBundleId;
            // +0x140 mbRelinquished (DecFIGS name; X360 reads byte node+0x148): when true, a matched
            //   requested resource is retired; otherwise the requester is transferred and notified.
            bool mbRelinquished;
            u8   maPad141[3];
            // +0x144 pad to the X360 payload size 0x148.
            s32  miPad144;
        };

        // 0x826B0470 -- Construct+Clear the two VariableEventQueue request interfaces, Construct the
        // three LinkedListHelper pools (free list seeded with the node pool, live list empty), and
        // Clear the removal-candidate map.
        void Construct();

        // 0x82702228 -- per-frame: Clear both request queues, assert both drained, then
        // UpdateRequests -> UpdateQueued -> ClearUnreferancedFiles.
        void Update();

        // 0x826E2220 -- the BrnEffectObject::Detach entry point: pull a requester's outstanding
        // requests out of the broker. Asserts the requester has nothing still queued, walks the
        // requested list clearing its references, and queues any now-unreferenced node for removal.
        void RemoveRequests(IResourceRequester* lpRequester);

        // BrnResourceRegistrar.cpp:398 -- relinquish one named resource for a
        // requester.  Unlike RemoveRequests(), this preserves the requester's
        // references to every other loaded resource.
        void RemoveRefsToResource(IResourceRequester* lpRequester,
                                  const char* lpcResourceName);

        // 0x826B0B08 -- resolve a (resourceName, bundleName) pair against the loading requested list;
        // returns the resolved ResourceHandle (or 0). a2 == bundle name (may be null), a3 == resource name.
        CgsResource::ResourceHandle* GetResource(const char* lpcBundleName, const char* lpcResourceName);

        // 0x826E1F88 -- duplicate-detect and append a new load request.
        void AddRequest(const QueuedResource& lrRequest);

        BrnResource::GameDataIO::RequestInterface<4096>& GetResourceRequestInterface()
        {
            return mResourceRequestInterface;
        }

        CgsAttribSys::AttribSysIO::AttribSysRequestInterface<2048>& GetAttribSysRequestInterface()
        {
            return mAttribSysRequestInterface;
        }

    private:
        typedef CgsContainers::LinkedListNode<QueuedResource>    QueuedNode;
        typedef CgsContainers::LinkedListNode<RequestedResource> RequestedNode;

        // 0x826B0A60 -- is there a queued item for `lpRequester` (optionally matching resource-type
        // `liResourceType` when `lbMatchType`)? Walks the loading queued list.
        bool SearchQueued(IResourceRequester* lpRequester, bool lbMatchType, s32 liResourceType);

        // 0x826B0560 -- resolve completed loads: walk the requested list, match each against an
        // attrib-sys/loading-queued entry, promote/recycle, and queue retired nodes for removal.
        void UpdateRequests();

        // 0x82702108 -- step each queued item: Prepare() loading ones (promote ready ones to the
        // requested list), Release() unloading ones (recycle finished nodes).
        void UpdateQueued();

        // 0x826E2080 -- GC the removal candidates into the unloading queued list (batch cap 18),
        // then AppendArray the un-GC'd remainder back into the candidate map.
        void ClearUnreferancedFiles();

        // 0x82695860 -- queue a requested-resource node for removal once its reference count hits 0.
        void AddNodeToRemoveResourceCandidateList(RequestedNode* lpNode);

        // =====================================================================
        // MEMBER LAYOUT (X360 ResourceRegistrar::Construct 0x826B0470). Offsets are the X360
        // byte offsets; pinned by name+sequence only on the 64-bit host.
        // =====================================================================

        // @+0x0000 mLoadingQueuedResourceList -- pending loads (free@+0x17A8 / live@+0x17B4, stride 336).
        //   N=18: derived from the X360 pool span -- the free-list head sits at byte 6056 (a1+1514) and
        //   the pool base at byte 8 (a1+2), so the pool spans 6048 bytes / 336-byte stride == 18 nodes.
        CgsContainers::LinkedListHelper<QueuedResource, 18> mLoadingQueuedResourceList;

        // @+0x17C0 mUnLoadingQueuedResourceList -- pending unloads (free@+0x2F68 / live@+0x2F74, 336).
        //   N=18: same 6048/336 span (pool base byte 6088 (a1+1522), free head byte 12136 (a1+3034)).
        CgsContainers::LinkedListHelper<QueuedResource, 18> mUnLoadingQueuedResourceList;

        // @+0x2F80 mapRemovalCandidates -- requested nodes awaiting GC. CORRECTION (blueprint vs JSON):
        //   this is an Array<LinkedListNode<RequestedResource>*, 124>, NOT a LinearHashTable (the X360
        //   uses Contains/Append/operator[]/GetLength/EraseFast and a single count word at +0x1F0).
        //   (Array<T,N> is the global-namespace fixed array from CgsArray.h.)
        ::Array<RequestedNode*, 124> mapRemovalCandidates;

        // @+0x3174 mLoadedResourceList -- resolved/in-use (loaded) resources (free@+0x9914 /
        //   live@+0x9920, stride 316). N=124: pool base byte 12664 (a1+3166), free head byte 51848
        //   (a1+12962) -> 39184 bytes / 316-byte stride == 124 nodes. Name from PS3 DecFIGS DWARF
        //   (BrnResourceRegistrar.h:319); same type/N/position as ours (RequestedResource x124,
        //   the 4th data structure) -- NOT branch-divergent, a pure name reconcile.
        CgsContainers::LinkedListHelper<RequestedResource, 124> mLoadedResourceList;

        // @+0xCAA0 mAttribSysRequestInterface -- VariableEventQueue<2048,16> (the X360 a1+12968).
        CgsAttribSys::AttribSysIO::AttribSysRequestInterface<2048> mAttribSysRequestInterface;

        // @+0xD2B0 mResourceRequestInterface -- VariableEventQueue<4096,16> (the X360 a1+13484).
        BrnResource::GameDataIO::RequestInterface<4096> mResourceRequestInterface;
    };

    // BrnResourceRegistrar.h:337 (DWARF). The resource-requester interface BrnEffectObject and the
    // controls/state-managers implement (X360: the IResourceRequester sub-object vptr @ this+4 on
    // BrnEffectObject; the detach/registrar paths route through it). DecFIGS declares exactly two
    // virtuals and no virtual destructor: {ResourcesAreReady, GetResourceRegistrar}. GetResource is
    // reached through the registrar, not a requester virtual. Defined after ResourceRegistrar so
    // LoadAsset's EType parameter resolves.
    struct IResourceRequester
    {
        enum EResourcePool
        {
            E_SOUND_DATA_POOL = 6,
        };

        virtual void               ResourcesAreReady() = 0;
        virtual ResourceRegistrar& GetResourceRegistrar() = 0;

        // BrnResourceRegistrar.h:272 (DWARF; body ResourceRegistrar.cpp:1084). Enqueue a
        // bundle+resource load request of the given asset class. The sound effects'
        // SetupLoadData tail-forwards here (X360 0x826E4C88: r5=0 -> lpcResourceName null, r6=0 ->
        // E_DATA).
        void LoadAsset(const char* lpcBundleName, const char* lpcResourceName,
                       ResourceRegistrar::EType leType);

        // ARTIST @ 0x826E23D0. Queue a resource by name without an enclosing
        // bundle. Engine loop-model data uses this overload; the explicit pool
        // argument is part of the original request record.
        void LoadAsset(const char* lpcResourceName, EResourcePool lePool,
                       ResourceRegistrar::EType leType);
    };
}
}

#endif // BRN_SOUND_LOGIC_BRN_RESOURCE_REGISTRAR_H
