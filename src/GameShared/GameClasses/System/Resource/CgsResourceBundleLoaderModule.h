#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"     // base
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"   // mReceiverQueue
#include "GameShared/GameClasses/Module/CgsEventQueue.h"               // EventQueue<LoadBundleRequest,256> (load intake)
#include "GameShared/GameClasses/Containers/CgsPriorityQueue.h"        // load/unload priority queues
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h" // LoadBundleRequest (RunningLoad element)
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h" // BundleLoader + FTypeResolver (the load leaf)

// CgsResource::BundleLoaderModule - the asynchronous bundle streamer (X360 CgsBundleLoaderModule.cpp).
// It owns a set of in-flight "stream slots", a load and an unload priority queue, and an EA job/
// GeneralAllocator pair, and drives the multi-stage streaming state machine (StreamIdle ->
// StreamHeader -> StreamEntryList -> StreamData -> StreamDone) that reads .BUNDLE files off disk
// (via the FileSystem), decompresses them, and hands the resources to the PoolModule. It is a
// CgsModule::ModuleSingleBuffered, embedded by value inside ResourceModule.
//
// SOURCES (X360 ARTIST): ctor 0x827DD270, Construct 0x828EBAF8, Prepare 0x828E2678, Release
// 0x828E27A0, Destruct 0x828E2850, Update 0x82907638, UpdateStream 0x82906B30, ProcessReceiverQueue
// 0x828E2888, ProcessPoolResponses 0x828EC148, CheckForLoads 0x828FB758, CheckForUnloads 0x828FB308,
// + the StreamXxxFunc state-machine steps and the MoveToFirst/NextResource cursor helpers.
//
// Layout: faithful field order; x64 widths; compiler-laid-out (members identified by X360 offset,
// NOT byte-matched). Populated incrementally - this pass lands only the members the rw/file-
// INDEPENDENT spine touches (the two stage fields, the receiver queue, the load/unload queues and
// the stream-slot array). The embedded EA::Allocator::GeneralAllocator (the deferred PPMalloc), the
// two EA::Jobs::Job, the IO buffers, the RW mutexes and the bundle/stream data are added with the
// Construct + streaming passes that use them.
//
// DEFER STATUS: this pass reconstructs the rw/file-INDEPENDENT spine - the Prepare / Release /
// Destruct stage machines (they only reset the stream slots + queues and chain the base module).
// Construct (embeds the GeneralAllocator + jobs), the streaming state machine (Update / UpdateStream
// / StreamXxxFunc), ProcessReceiverQueue / ProcessPoolResponses, CheckForLoads / CheckForUnloads and
// the bundle parse path are DEFERRED (rw allocator / FileSystem / job system) as inert marked stubs.
namespace CgsResource
{
    class PoolModule;   // ProcessLoadRequests resolves a request's pool id via the PoolModule

    // CgsBundleLoaderModule.h:43 -- one in-flight bundle load record. The DWARF shows it
    // wraps a single LoadBundleRequest; the loader keeps a FifoQueue<RunningLoad,4> of
    // these (BundleLoaderModule::RunningLoadQueue mQueuedLoads). sizeof == 148 (the
    // LoadBundleRequest element stride the X360 RunningLoad,4>::Pop memcpys).
    struct RunningLoad
    {
        Events::LoadBundleRequest mLoadRequest;   // CgsBundleLoaderModule.h:45
    };

    class BundleLoaderModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        enum EStage { E_STAGE_START = 0, E_STAGE_RUNNING = 1, E_STAGE_DONE = 2 };

        // One in-flight bundle stream (X360 32-byte slot; miId == -1 means free).
        struct StreamSlot
        {
            s32 miId;                 // [+0x00] resource/bundle id (-1 = free)
            u32 mauReserved[5];       // [+0x04]
            s32 mField24;             // [+0x18] cleared by Prepare
            u32 muReserved2;          // [+0x1C]
        };

        // CgsResource::BundleLoaderModule::InitOptions (DWARF CgsBundleLoaderModule.h:121) - the
        // bundle-loader bring-up options (Construct consumes them): the stream/header/debug buffer
        // sizes, the bundle/per-bundle-resource limits, and the filename-case flags.
        struct InitOptions
        {
            s32  miBundleHeaderBufferSize;         // :127
            s32  miDebugDataBufferSize;            // :128
            s32  miStreamBufferSize;               // :129
            s32  miMaxBundles;                     // :130
            s32  miMaxResourcesPerBundle;          // :131
            bool mbForceUpperCaseFileNames;        // :132
            bool mbForcePS3HardDriveUpperCaseGame; // :134
        };

        // "New module": skip ModuleSingleBuffered's old DataStructure IO path (X360 *(this+4)=1).
        // Construct the load-request queue here (embedded-member ctor) so it is ready even though the
        // full BundleLoaderModule::Construct -- the allocator/job bring-up -- is still deferred.
        BundleLoaderModule() { mbIsNewModule = true; mLoadRequests.Construct(); mUnloadRequests.Construct(); }

        // ---- lifecycle ----------------------------------------------------------------
        void Construct();   // deferred (embeds the GeneralAllocator + jobs)
        bool Prepare();     // 0x828E2678
        bool Release();     // 0x828E27A0
        void Destruct();    // 0x828E2850

        // ---- dispatch / streaming (deferred) ------------------------------------------
        bool Update(void* lpInputBuffer, void* lpOutputBuffer);
        void ProcessReceiverQueue();

        // ---- focused synchronous load path (the resource-streaming bring-up) -----------
        // Queue a LoadBundleRequest the ResourceModule shuttle routed here (id 2). The X360 lands it on
        // the bundle loader's input-buffer LoadBundleRequest queue via the IOBufferStack; modelled here as
        // a module-member EventQueue for the focused path.
        void EnqueueLoadRequest(const Events::LoadBundleRequest& lrRequest);

        // Drain the queued LoadBundleRequests: resolve each request's pool (by id, via lpPoolModule) and
        // load its bundle into that pool through the PC synchronous BundleLoader (the load leaf), then emit
        // a LoadBundleResponse to the request's reply target. [The X360 async streaming FSM -- FileSystem +
        // EA Jobs + chunked StreamHeader/EntryList/Data reads -- is collapsed to the sync BundleLoader load
        // at the leaf; the faithful request/response + per-resource pool create/FixUp are preserved.]
        void ProcessLoadRequests(PoolModule* lpPoolModule, FTypeResolver lpfnResolveType);

        // The unload counterparts (resource request id 3): queue an UnloadBundleRequest, then drain them --
        // resolve each pool + BundleLoader::UnloadBundle (ref-count-release the bundle's resources) + emit
        // an UnloadBundleResponse.
        void EnqueueUnloadRequest(const Events::UnloadBundleRequest& lrRequest);
        void ProcessUnloadRequests(PoolModule* lpPoolModule);

    private:
        // ---- Layout (faithful order; x64 widths; compiler-laid-out; incremental) ------
        EStage            mePrepareStage;     // +0x228 (a1[138])
        EStage            meReleaseStage;     // +0x22C (a1[139])
        s32               mField140;          // +0x230 (a1[140])
        s32               mField426;          // +0x6A8 (a1[426])
        s32               mField427;          // +0x6AC (a1[427])
        CgsModule::BaseEventReceiverQueue mReceiverQueue;   // +0x6B4 (a1+429)
        CgsContainers::BasePriorityQueue  mLoadQueue;       // +0x74C (a1+467)
        CgsContainers::BasePriorityQueue  mUnloadQueue;     // +0x5564 (a1+5465)
        StreamSlot*       mpStreamSlots;      // +0xA17C (a1[10335])
        u32               muStreamSlotCursor; // +0xA180 (a1[10336])
        u32               muNumStreamSlots;   // +0xA184 (a1[10337])
        // (the EA GeneralAllocator / jobs / IO buffers / RW mutexes / bundle+stream data
        //  are added with the Construct + streaming passes that use them.)

        // The load-request intake (the X360 keeps an EventQueue<LoadBundleRequest,256> in its input
        // buffer; modelled as a module member for the focused synchronous load path).
        CgsModule::EventQueue<Events::LoadBundleRequest, 256>   mLoadRequests;
        CgsModule::EventQueue<Events::UnloadBundleRequest, 256> mUnloadRequests;
    };
}
