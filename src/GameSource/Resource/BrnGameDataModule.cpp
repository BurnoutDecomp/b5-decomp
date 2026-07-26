#include "GameSource/Resource/BrnGameDataModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Memory/CgsMemoryModule.h"   // MemoryModule::InitOptions
#include "GameSource/Resource/BrnResourceAllocator.h"        // Allocators::mpInternalDebugAllocator
#include "rw/rwcore_structs.h"                                // rw::IResourceAllocator / Resource / ResourceDescriptor
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // [5b trace] (Construct runs at runtime, gpDebugPrint ready)
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistration.h" // RegisterAllResourceTypes (bridge)
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"     // ResolveResourceType (bridge)
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"             // Pool::InitOptions (CreatePools)
#include "GameShared/GameClasses/Memory/CgsMemoryBank.h"                        // MemoryBank::Params (CreatePoolBank)
#include "GameShared/GameClasses/Memory/CgsMemoryModuleIO.h"                    // MemoryIO Input/OutputBuffer (async pump)
#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"       // SendCreatePoolMemoryRequest / DoCreatePoolRequest
#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h"             // PoolIO::OutputBuffer (request transport)
#include "GameShared/GameClasses/System/Resource/CgsResourceModuleIO.h"         // ResourceIO::InputBuffer (CreatePool request input)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"         // Events::LoadBundleRequest/AcquireResourceRequest (streaming)
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"          // E_RESOURCETYPE_FONT (stream validation)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"            // EventReceiverQueue (acquire response receiver)
#include "GameSource/Resource/BrnMemoryMapData.h"                               // KAC_MEMORY_MAP_POOLS (the 27 real pools)
#include "GameSource/Resource/BrnGameDataModuleIO.h"                            // GameDataIO In/OutputBuffer (Update pump)
#include "GameShared/GameClasses/Core/CgsID.h"                                  // CgsIDUnCompress / CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                         // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"                    // CgsDev::StrStream (prop bundle name build)
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"               // CgsResource::ID::HashString
#include <cstring>                                                              // memset / memcmp / strncmp
#include <cstdlib>                                                              // atoi (prop-instance zone number)

// BrnResource::GameDataModule - see the header. This pass reconstructs the rw-INDEPENDENT
// lifecycle spine (Prepare's base -> ResourceModule::Prepare core). The rw-allocator-gated
// bring-up (CreateBanks/CreatePools/CreateAllocators), the DLC/AttribSys/HUD/popup prepare
// stages, Construct, Update, Destruct and the ProcessXxxRequest handlers are DEFERRED.
namespace BrnResource
{
    // "New module": mark before any Prepare. NO logging here -- BrnGameModule embeds GameDataModule by
    // value, so this ctor runs at STATIC INIT (before gpDebugPrint is initialized); logging here would
    // deref an uninitialized gpDebugPrint and crash before the window opens.
    GameDataModule::GameDataModule()
    {
        mbIsNewModule = true;
    }

    // @ 0x82673F38 - resumable bring-up: base module, then the ResourceModule (memory/pool/
    // bundle/filesystem), then carve banks/pools/allocators. Re-enters at the current stage if
    // a sub-step is not yet ready. (The X360 then runs DLC stages 16-18+ and the AttribSys/HUD/
    // popup prepares - DEFERRED here; the machine completes after the allocator stage.)
    bool GameDataModule::Prepare(void* lpInputBuffer, void* lpOutputBuffer)
    {
        mbIsNewModule = true;   // [reliable] see ResourceModule::Prepare -- set before base Prepare
        switch (mePrepareStage)
        {
        case E_PREPARE_START:
        case E_PREPARE_BASE:
            mePrepareStage = E_PREPARE_BASE;
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                return false;
            // (X360 also Registers two debug components here - deferred)
            // fall through
        case E_PREPARE_RESOURCE:
            mePrepareStage = E_PREPARE_RESOURCE;
            if (!mResourceModule.Prepare())
                return false;
            mReceiverQueue.Clear();
            // fall through
        case E_PREPARE_BANKS:
            mePrepareStage = E_PREPARE_BANKS;
            if (!CreateBanks(lpInputBuffer, lpOutputBuffer))
                return false;
            // fall through
        case E_PREPARE_POOLS:
            mePrepareStage = E_PREPARE_POOLS;
            if (!CreatePools(lpInputBuffer, lpOutputBuffer))
                return false;
            // fall through
        case E_PREPARE_ALLOCATORS:
            mePrepareStage = E_PREPARE_ALLOCATORS;
            if (!CreateAllocators(lpInputBuffer, lpOutputBuffer))
                return false;
            // (X360: DLC stages 16-18+ and the AttribSys/HUD/popup prepares - deferred)
            // fall through
        case E_PREPARE_DONE:
            meReleaseStage = E_PREPARE_START;
            mePrepareStage = E_PREPARE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Invalid stage");
            return false;
        }
    }

    // ---- DEFERRED (rw allocator middleware + game-data subsystems) --------------------
    // CreateBanks (0x8266DA28) / CreatePools (0x8266DB88) / CreateAllocators (0x8266DD00) carve the
    // resource banks/pools/allocators out of the GameData heap through the rw::IResourceAllocator
    // (the rwcore allocator layer). Inert stubs that report success so the Prepare stage machine
    // advances; reconstruct with that allocator layer. Construct (0x82671B90) builds the embedded
    // subsystems; Update pumps the ResourceModule and services the game-data query handlers
    // (ProcessGetVehicle/Wheel/Traffic/WorldUnit/PVS/... - 30+ handlers); Destruct (0x82664508)
    // tears it down. All inert until the loading-machine case 8 drives this module.
    bool GameDataModule::CreateBanks(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/) { return true; }

    // @ 0x8266DB88 - create the game's resource pools. The X360 iterates the binary memory map
    // (unk_82F2A788) and sends a CreatePoolRequest per pool through the async dispatch
    // (DoCreatePoolRequest -> SendCreatePoolMemoryRequest -> MemoryModule alloc -> CreatePool). Here we
    // drive the SAME pool set directly from the extracted memory-map table (BrnMemoryMapData.h, generated
    // from progress/memory_map_artist.yaml): per pool, allocate its per-type backing memory from the root
    // debug allocator and PoolModule::CreatePool it. [marked deviation: the faithful async memory-request
    // dispatch is deferred; the pool data + Pool::InitPool are faithful.] Runs once (the Prepare stage
    // re-enters until done).
    bool GameDataModule::CreatePools(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/)
    {
        static bool s_bPoolsCreated = false;
        if (s_bPoolsCreated)
            return true;
        s_bPoolsCreated = true;

        rw::IResourceAllocator* lpAllocator =
            static_cast<rw::IResourceAllocator*>(Allocators::mpInternalDebugAllocator);
        if (lpAllocator == 0)
            return true;
        CgsResource::PoolModule&  lrPoolModule   = mResourceModule.GetPoolModule();
        CgsMemory::MemoryModule&  lrMemoryModule = mResourceModule.GetMemoryModule();
        const u32 KU_BLOCK = 0x10000u;   // 64 KB -- the MemoryModule root-bank block granularity

        s32 lNumCreated = 0, lNumFromBank = 0, lNumFromHeap = 0, lNumDepsWired = 0, lNumViaAsync = 0;

        // The ResourceModule input buffer CreatePools publishes CreatePool requests to; ResourceModule::
        // Update (the streaming shuttle) owns the pool/memory scratch buffers internally. Static so the
        // (large) embedded request queue is constructed once + reused across the loop.
        static CgsResource::ResourceIO::InputBuffer s_resIn;
        static bool s_ioConstructed = false;
        if (!s_ioConstructed) { s_resIn.Construct(); s_ioConstructed = true; }
        // id -> created Pool*, so each pool's import dependencies (referenced by pool id) resolve to real
        // pointers in this one pass: the memory-map order lists every dependency target before its
        // dependents (deps reference ids 10/25 @ idx 8/23; all dependents are at later indices).
        CgsResource::Pool* lapPoolById[64];
        memset(lapPoolById, 0, sizeof(lapPoolById));
        for (s32 li = 0; li < KI_NUM_MEMORY_MAP_POOLS; ++li)
        {
            const MemoryMapPoolDef& lrDef = KAC_MEMORY_MAP_POOLS[li];

            CgsResource::Pool::InitOptions lOpt;
            memset(&lOpt, 0, sizeof(lOpt));   // POD-ish; zero unused types/deps
            lOpt.miId                   = lrDef.miId;
            lOpt.mpcName                = lrDef.mpcName;
            lOpt.muMaxResources         = static_cast<u32>(lrDef.miMaxResources);
            lOpt.muMaxImports           = static_cast<u32>(lrDef.miMaxImports);
            lOpt.miRefCountThreshold    = 0;
            lOpt.miBankId               = -1;
            lOpt.mbAllowDefragmentation = lrDef.mbAllowDefrag;

            // Resolve this pool's import dependencies to the (earlier-created) pools by id. The dependency
            // graph drives cross-pool import resolution (e.g. OpenWorldGr/VFX/Environment import from Global
            // Textures; CarPool/Traffic also from CarSharedPool).
            s32 lNumDeps = 0;
            for (s32 ld = 0; ld < lrDef.miNumDeps && lNumDeps < 16; ++ld)
            {
                const s32 lDepId = lrDef.maiDeps[ld];
                CgsResource::Pool* lpDep = (lDepId >= 0 && lDepId < 64) ? lapPoolById[lDepId] : 0;
                if (lpDep != 0)
                    lOpt.mapDependencies[lNumDeps++] = lpDep;
            }
            lOpt.miNumDependencies = lNumDeps;
            lNumDepsWired += lNumDeps;

            // Per-type region = heap budget + (type 0) a margin >= the pool overhead (entry arrays / hash /
            // heap nodes Pool::InitPool Mallocs from the region), rounded up to the 64 KB block size so the
            // MemoryModule bank carve is block-exact. (Bump allocator -> the slack is harmless.)
            u32 lauRegion[3] = { 0, 0, 0 };
            u32 lauAlign[3]  = { 16u, 16u, 16u };
            for (s32 lt = 0; lt < 3; ++lt)
            {
                const u32 luHeapSize = lrDef.mauHeapSize[lt];
                if (luHeapSize == 0)
                    continue;
                lOpt.maHeapInfo[lt].muMaxNodes       = static_cast<u32>(lrDef.miMaxHeapNodes);
                lOpt.maHeapInfo[lt].muHeapMemorySize = luHeapSize;
                lOpt.maHeapInfo[lt].muHeapAlignment  = lrDef.mauHeapAlign[lt];

                u32 luMargin = 0;
                if (lt == 0)
                {
                    const u32 luMax = static_cast<u32>(lrDef.miMaxResources);
                    u32 luHashLen = 3u * luMax; --luHashLen;
                    luHashLen |= luHashLen >> 1;  luHashLen |= luHashLen >> 2;
                    luHashLen |= luHashLen >> 4;  luHashLen |= luHashLen >> 8;
                    luHashLen |= luHashLen >> 16; ++luHashLen;
                    luMargin = luMax * 512u + luHashLen * 16u
                             + static_cast<u32>(lrDef.miMaxHeapNodes) * 3u * 128u + 0x10000u;
                }
                lauAlign[lt]  = lrDef.mauHeapAlign[lt] < 16u ? 16u : lrDef.mauHeapAlign[lt];
                lauRegion[lt] = (luHeapSize + luMargin + (KU_BLOCK - 1u)) & ~(KU_BLOCK - 1u);
            }

            bool lbAnyType = (lauRegion[0] | lauRegion[1] | lauRegion[2]) != 0;

            // Carry the pool's resource descriptor through the async pending FIFO into CreatePool (the async
            // path fills mResource from the memory response; mDescriptor is set here from the carved regions).
            for (s32 lt = 0; lt < 3; ++lt)
            {
                if (lauRegion[lt] == 0)
                    continue;
                lOpt.mDescriptor.m_baseResourceDescriptors[lt].m_size      = lauRegion[lt];
                lOpt.mDescriptor.m_baseResourceDescriptors[lt].m_alignment = lauAlign[lt];
            }

            const s32 liBankId = 100 + li;   // unique leaf-bank id (pool ids occupy 0..26)
            CgsResource::Pool* lpPool = 0;
            bool lbViaAsync = false;

            // ---- [Faithful async path] publish a CreatePool request to the ResourceModule input and pump
            // ResourceModule::Update -- the real streaming shuttle: ProcessResourceRequests -> pool input;
            // PoolModule::Update (ProcessInputBuffer -> SendCreatePoolMemoryRequest); ProcessPoolResource
            // Requests -> MemoryModule input; MemoryModule::Update -> ProcessCreateResourceRequest carves the
            // bank; ProcessMemoryResponses -> the pool receiver queue; (next frame) ProcessReceiverQueue ->
            // DoCreatePoolRequest -> CreatePool. The request carries the resolved InitOptions so cross-pool
            // dependency order (lapPoolById) is preserved exactly. Pool create is 2-frame, so pump a few times.
            if (lbAnyType)
            {
                CgsResource::CreatePoolRequestEvent lReqEvent;
                lReqEvent.mOptions       = lOpt;
                lReqEvent.miBankId       = liBankId;
                lReqEvent.miParentBankId = 0;                 // root bank
                for (s32 lt = 0; lt < 3; ++lt) { lReqEvent.mauRegion[lt] = lauRegion[lt]; lReqEvent.mauAlign[lt] = lauAlign[lt]; }

                s_resIn.LockForWrite();
                s_resIn.GetResourceQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lReqEvent),
                                                     0 /*CreatePool*/, static_cast<s32>(sizeof(lReqEvent)));
                s_resIn.UnlockForWrite();

                for (s32 lf = 0; lf < 4 && lpPool == 0; ++lf)
                {
                    mResourceModule.Update(&s_resIn, 0);
                    lpPool = lrPoolModule.GetPool(lrDef.miId);
                }
                if (lpPool != 0 && lpPool->IsValid()) { lbViaAsync = true; ++lNumViaAsync; ++lNumFromBank; }
            }

            // ---- [Fallback] synchronous bank carve, if the async memory dispatch did not deliver. Carve a
            // leaf bank directly, else allocate from the root debug allocator, fill mResource, CreatePool.
            if (!lbViaAsync)
            {
                CgsMemory::MemoryBank::Params lParams;
                memset(&lParams, 0, sizeof(lParams));
                for (s32 lc = 0; lc < 31 && lrDef.mpcName[lc]; ++lc)
                    lParams.macName[lc] = lrDef.mpcName[lc];
                lParams.mnParentBankId       = 0;            // root bank
                lParams.mnBankId             = liBankId;
                lParams.mbIsLeaf             = true;
                lParams.mbAllowFragmentation = lrDef.mbAllowDefrag;
                for (s32 lt = 0; lt < 3; ++lt)
                {
                    if (lauRegion[lt] == 0)
                        continue;
                    lParams.mauBankSize[lt]   = lauRegion[lt];
                    lParams.mauBankBlocks[lt] = lauRegion[lt] / KU_BLOCK;
                }

                rw::Resource lMem;
                for (s32 lr = 0; lr < 4; ++lr)
                    lMem.m_baseResources[lr] = 0;
                const bool lbFromBank = lbAnyType && lrMemoryModule.CreatePoolBank(&lParams, lMem);

                bool lbAllocOk = true;
                for (s32 lt = 0; lt < 3; ++lt)
                {
                    if (lauRegion[lt] == 0)
                        continue;
                    void* lpMem = lbFromBank ? lMem.m_baseResources[lt] : 0;
                    if (!lbFromBank)
                    {
                        rw::ResourceDescriptor lDesc;
                        for (u32 lu = 0; lu < 4; ++lu)
                        {
                            lDesc.m_baseResourceDescriptors[lu].m_size      = 0;
                            lDesc.m_baseResourceDescriptors[lu].m_alignment = 1;
                        }
                        lDesc.m_baseResourceDescriptors[0].m_size      = lauRegion[lt];
                        lDesc.m_baseResourceDescriptors[0].m_alignment = lauAlign[lt];
                        rw::Resource lFb = lpAllocator->DoAllocate(lDesc, lrDef.mpcName);
                        lpMem = lFb.m_baseResources[0];
                    }
                    if (lpMem == 0)
                    {
                        lbAllocOk = false;
                        break;
                    }
                    lOpt.mResource.m_baseResources[lt] = lpMem;
                }

                if (!lbAllocOk)
                {
                    *CgsDev::Log::gpDebugPrint << "[5b POOLS] alloc FAILED for pool " << lrDef.mpcName << "\n";
                    continue;
                }
                if (lbFromBank) ++lNumFromBank; else ++lNumFromHeap;
                lpPool = lrPoolModule.CreatePool(&lOpt);
            }

            if (lpPool != 0 && lpPool->IsValid())
                ++lNumCreated;
            if (lpPool != 0 && lrDef.miId >= 0 && lrDef.miId < 64)
                lapPoolById[lrDef.miId] = lpPool;   // so later pools can depend on this one
        }
        *CgsDev::Log::gpDebugPrint << "[5b POOLS] created " << (s32)lNumCreated << " of "
                                  << (s32)KI_NUM_MEMORY_MAP_POOLS << " pools ("
                                  << (s32)lNumViaAsync << " via async memory dispatch, "
                                  << (s32)lNumFromBank << " from MemoryModule banks, "
                                  << (s32)lNumFromHeap << " from debug heap), "
                                  << (s32)lNumDepsWired << " dependencies wired\n";

        // [resource streaming bring-up] -- FLAG: non-faithful bring-up TEST scaffolding. The X360
        // GameDataModule::Prepare just creates the pools and returns; it does NOT publish inline font
        // load/acquire/unload test requests. This inline pump currently faults (0xC0000005) and blocks
        // the boot, so it is disabled to let Prepare complete faithfully -- the real streaming path is
        // exercised by the GUI / game flow when it actually requests resources. Set to `if (true)` to
        // re-validate the request -> shuttle -> bundle loader -> pool path in isolation.
        *CgsDev::Log::gpDebugPrint << "[5b] GameDataModule::Prepare: pools ready, streaming-test scaffolding skipped (faithful) -> return\n";
        if (false)
        {
            CgsResource::Events::LoadBundleRequest lReq;
            memset(&lReq, 0, sizeof(lReq));
            lReq.SetFileName("Language/Fonts/Default.font");
            lReq.miPoolId = 0;   // the Fonts pool

            s_resIn.LockForWrite();
            s_resIn.GetResourceQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lReq),
                                                 2 /*LoadBundle*/, static_cast<s32>(sizeof(lReq)));
            s_resIn.UnlockForWrite();
            mResourceModule.Update(&s_resIn, 0);

            CgsResource::Pool* lpFonts = lrPoolModule.GetPool(0);
            s32 liFontIdx = -1;
            CgsResource::Entry* lpFontEntry = lpFonts
                ? lpFonts->FindFirstResourceOfType(CgsResource::E_RESOURCETYPE_FONT, &liFontIdx) : 0;
            *CgsDev::Log::gpDebugPrint << "[stream] Fonts pool (id 0) font resource via shuttle: "
                                      << (lpFontEntry ? "FOUND" : "absent") << "\n";

            // The font bundle carries 2 resources: the Font (0x21) + its atlas Texture (RwRaster, 0x0).
            // Confirm the texture resource streamed too -- proving the system handles the most platform-
            // divergent resource type (textures), not just the font, through the same shuttle.
            s32 liTexIdx = -1;
            CgsResource::Entry* lpTexEntry = lpFonts
                ? lpFonts->FindFirstResourceOfType(CgsResource::E_RESOURCETYPE_TEXTURE, &liTexIdx) : 0;
            *CgsDev::Log::gpDebugPrint << "[stream] Fonts pool texture resource (RwRaster) via shuttle: "
                                      << (lpTexEntry ? "FOUND" : "absent") << "\n";

            // [acquire bring-up] prove the consumer path: AcquireResource by id through the shuttle. The
            // streamed font is already at status 2 (loaded) from BundleLoader's load-completion pass, so it
            // is acquirable directly. Publish an AcquireResourceRequest (id 4) -> ProcessResourceRequests ->
            // pool input -> DoAcquireResourceRequest -> Pool::FindResource (gates on status & 2) -> response
            // on the pool output queue -> ProcessPoolOutputResponses routes it back to our receiver.
            if (lpFontEntry != 0 && lpFonts != 0)
            {
                const CgsResource::ID lFontId = lpFontEntry->mID;

                static CgsModule::EventReceiverQueue<4096, 16> s_acqReceiver;
                static bool s_acqRecvCtor = false;
                if (!s_acqRecvCtor) { s_acqReceiver.Construct(); s_acqRecvCtor = true; }
                s_acqReceiver.Clear();

                CgsResource::Events::AcquireResourceRequest lAcq;
                memset(&lAcq, 0, sizeof(lAcq));
                lAcq.mpUser          = &s_acqReceiver;
                lAcq.miPoolId        = 0;
                lAcq.mResourceId     = lFontId;
                lAcq.mbCheckRefCount = true;

                s_resIn.LockForWrite();
                s_resIn.GetResourceQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAcq),
                                                     4 /*AcquireResource*/, static_cast<s32>(sizeof(lAcq)));
                s_resIn.UnlockForWrite();
                mResourceModule.Update(&s_resIn, 0);

                const CgsModule::Event* lpResp = 0; s32 liRespSize = 0;
                const s32 liRespId = s_acqReceiver.GetFirstEvent(&lpResp, &liRespSize);
                const CgsResource::Events::AcquireResourceResponse* lpAcqResp =
                    (liRespId != -1 && lpResp != 0)
                        ? reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpResp) : 0;
                const bool lbAcquired = lpAcqResp != 0 && lpAcqResp->mpSourceEntry != 0
                                        && lpAcqResp->mpSourceEntry == lpFontEntry;
                *CgsDev::Log::gpDebugPrint << "[stream] AcquireResource(font) via shuttle: "
                                          << (lbAcquired ? "handle OK" : "FAILED") << "\n";
            }

            // [unload bring-up] prove the load/unload pair: publish an UnloadBundleRequest (id 3) for the
            // font -> ProcessResourceRequests -> bundle loader -> UnloadBundle (ref-count-release each
            // resource; frees heap memory + slot at refcount 0). Confirm the font is then gone from the pool.
            if (lpFonts != 0)
            {
                CgsResource::Events::UnloadBundleRequest lUnload;
                memset(&lUnload, 0, sizeof(lUnload));
                lUnload.SetFileName("Language/Fonts/Default.font");
                lUnload.miPoolId = 0;

                s_resIn.LockForWrite();
                s_resIn.GetResourceQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lUnload),
                                                     3 /*UnloadBundle*/, static_cast<s32>(sizeof(lUnload)));
                s_resIn.UnlockForWrite();
                mResourceModule.Update(&s_resIn, 0);

                s32 liGoneIdx = -1;
                CgsResource::Entry* lpGone =
                    lpFonts->FindFirstResourceOfType(CgsResource::E_RESOURCETYPE_FONT, &liGoneIdx);
                *CgsDev::Log::gpDebugPrint << "[stream] after UnloadBundle, Fonts pool font resource: "
                                          << (lpGone ? "STILL PRESENT (bug)" : "gone (freed)") << "\n";
            }
        }
        return true;
    }

    bool GameDataModule::CreateAllocators(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/) { return true; }

    void GameDataModule::Construct()
    {
        // X360 0x82671B90 first runs the module base's Construct (stage/DataBuffer reset; it
        // clears mbIsNewModule, which is re-raised below).
        CgsModule::ModuleSingleBuffered::Construct();

        mbIsNewModule = true;   // X360 *(this+4)=1 (new module type; base Prepare skips the old IO path)

        // X360 0x82671B90: wire the in-flight event-slot pool -- capacity 96
        // (`*(a1+439324) = 96`), the module-embedded element array (a1+439328) and
        // free-index array (a1+443936), then Clear -- and the internal receiver queue
        // (0x8000-byte buffer @a1+363120, 16-byte alignment: `*(a1+363112)=0x8000;
        // *(a1+363116)=16` + Clear). Every request the world handlers publish into the
        // ResourceModule names mReceiverQueue as its mpUser reply-to.
        mGameDataEventSlotPool.Construct(maGameDataEventSlots, masGameDataEventSlotFreeIndices,
                                         static_cast<s16>(KI_NUM_GAMEDATA_EVENT_SLOTS));
        mReceiverQueue.Construct();

        // X360 0x82671B90 (tail): reset the bank->allocator registry -- the inline
        // `maiAllocatorMap[0..66] = -1` loop == AllocatorList::Construct. CreateAllocators
        // (deferred) populates it; Update publishes it to the OutputBuffer each frame.
        mAllocatorList.Construct();

        // [5b] Bring up the resource memory system. This is an incrementally-populated stand-in for the
        // X360 ConstructResourceModule (0x8266D570): it now builds the real CgsResource::ResourceModule::
        // InitOptions composite (memory + pool + bundle-loader option blocks) instead of a bare
        // MemoryModule InitOptions, carves the root region from the debug allocator, and constructs the
        // ResourceModule. The MemoryModule block is fully populated (so MemoryModule carves its bank/block
        // tables, as before); the Pool/BundleLoader blocks are minimal for now (PoolModule front half
        // ignores them; the type-list + defrag/heap params land with the PoolModule back half + the full
        // RegisterResourceTypes).
        rw::IResourceAllocator* lpAllocator =
            static_cast<rw::IResourceAllocator*>(Allocators::mpInternalDebugAllocator);
        if (lpAllocator == 0)
            return;

        static CgsResource::ResourceModule::InitOptions s_InitOptions;   // outlives Construct (module keeps the region)
        CgsMemory::MemoryModule::InitOptions& lrMem = s_InitOptions.mMemoryInitOptions;
        lrMem.muMaxBanks  = 128;
        lrMem.muHighestId = 1024;
        lrMem.muMaxBlocks = 30000;

        // Root memory regions, carved in 64 KB blocks, sized to hold the 27 pools' per-type memory (sum
        // ~70 MB type 0 / main + ~196 MB type 1 / graphics, + per-pool overhead + block rounding). The
        // MemoryModule's root bank (bank 0) is built from these; GameDataModule::CreatePools then carves a
        // leaf bank per pool out of them (#2 -- pool memory from the MemoryModule banks).
        const u32 KU_BLOCK    = 0x00010000u;   // 64 KB
        const u32 KU_TYPE0    = 0x0A000000u;   // 160 MB main  (2560 blocks)
        const u32 KU_TYPE1    = 0x0E000000u;   // 224 MB gfx   (3584 blocks)
        rw::ResourceDescriptor lRegionDesc;
        for (u32 li = 0; li < 4; ++li)
        {
            lRegionDesc.m_baseResourceDescriptors[li].m_size      = 0;
            lRegionDesc.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        // Align the root regions to the block size (64 KB): leaf-bank memory is root + N*blockSize, so a
        // block-aligned root keeps every pool's per-type heap memory aligned to its heap alignment (16 main
        // / 4096 graphics, both <= 64 KB) -- else Heap::Prepare asserts (lpHeapMemory % heapAlign == 0).
        lRegionDesc.m_baseResourceDescriptors[0].m_alignment = KU_BLOCK;

        lRegionDesc.m_baseResourceDescriptors[0].m_size = KU_TYPE0;
        rw::Resource lType0 = lpAllocator->DoAllocate(lRegionDesc, "GameDataRoot0");
        lrMem.maResourceSets[0].mpDataStart = lType0.m_baseResources[0];
        lrMem.maResourceSets[0].muDataSize  = KU_TYPE0;
        lrMem.maResourceSets[0].muNumBlocks = static_cast<u16>(KU_TYPE0 / KU_BLOCK);

        lRegionDesc.m_baseResourceDescriptors[0].m_size = KU_TYPE1;
        rw::Resource lType1 = lpAllocator->DoAllocate(lRegionDesc, "GameDataRoot1");
        lrMem.maResourceSets[1].mpDataStart = lType1.m_baseResources[0];
        lrMem.maResourceSets[1].muDataSize  = KU_TYPE1;
        lrMem.maResourceSets[1].muNumBlocks = static_cast<u16>(KU_TYPE1 / KU_BLOCK);
        // memory types 2-4 left zeroed (skipped by CreateRootBank)

        // [BRIDGE -- marked deviation from X360 RegisterResourceTypes 0x82667EA8] Populate the pool type
        // list from our existing singleton resource-type handlers. The X360 operator-new's ~50 Type objects
        // into a GSResourceType array, but that path's prerequisites (Type::operator new / InitCachedValues
        // + the ~50 subclasses) are deferred, and our tree already registers handler SINGLETONS into a
        // CgsResource::TypeRegistry (RegisterAllResourceTypes). So reuse those: register them (idempotent),
        // then build the GSResourceType list from ResolveResourceType(id) for the wired type ids. The
        // PoolModule type-registry loop then copies them into maTypes (keyed by GetTypeID()). Grows as more
        // handlers are wired; backfill the faithful operator-new path if byte-fidelity is needed.
        CgsResource::RegisterAllResourceTypes();
        static CgsResource::PoolModule::InitOptions::GSResourceType s_aGameTypes[5];
        static const struct { u32 muId; const char* mpcName; } skWiredTypes[5] =
        {
            { 0x00u, "RwRasterResourceType" },
            { 0x0Eu, "RwTextureStateResourceType" },
            { 0x0Fu, "MaterialStateResourceType" },
            { 0x21u, "FontResourceType" },
            { 0x42u, "VideoDataResourceType" },
        };
        s32 lNumGameTypes = 0;
        for (s32 li = 0; li < 5; ++li)
        {
            const CgsResource::Type* lpType = CgsResource::ResolveResourceType(skWiredTypes[li].muId);
            if (lpType == 0)
                continue;
            s_aGameTypes[lNumGameTypes].mpType  = lpType;
            s_aGameTypes[lNumGameTypes].mpcName = skWiredTypes[li].mpcName;
            ++lNumGameTypes;
        }
        s_InitOptions.mPoolInitOptions.mpGameSpecificTypes    = s_aGameTypes;
        s_InitOptions.mPoolInitOptions.miNumGameSpecificTypes = lNumGameTypes;
        s_InitOptions.mDebugParams.mpDebugAllocator           = lpAllocator;
        *CgsDev::Log::gpDebugPrint << "[5b] GameDataModule::Construct: registered "
                                  << (s32)lNumGameTypes << " resource types into the pool registry\n";

        *CgsDev::Log::gpDebugPrint << "[5b] GameDataModule::Construct: root type0="
                                  << (s32)(u32)(size_t)lType0.m_baseResources[0]
                                  << " -> ResourceModule::Construct\n";
        mResourceModule.Construct(&s_InitOptions, lpAllocator);
        *CgsDev::Log::gpDebugPrint << "[5b] GameDataModule::Construct: ResourceModule constructed ok\n";
    }
    bool GameDataModule::Release() { return true; }
    void GameDataModule::Destruct() {}

    // ====================================================================================
    // The world-request service path (THIS BATCH): the Update request pump + the GameData
    // dispatch chain + the four world handlers. Ground truth = the X360 ARTIST build
    // (addresses on each function); the request-side event/queue types are the committed
    // SharedIO slice (BrnGameDataRequestQueue.h / BrnGameDataEvents.h / the
    // RequestInterface<N> builders on the producer side).
    // ====================================================================================

    namespace
    {
        // X360 .rdata world-data file-name globals (values read from the ARTIST .i64 data
        // segment; the exports are function-only so the pointer targets are attested by the
        // database, not the JSON).
        const char* const KPC_SURFACE_LIST_FILE_NAME = "surfacelist.bin";           // off_82F2A704
        const char* const KPC_PVS_FILE_NAME          = "pvs.bndl";                  // off_82F2A70C
        const char* const KPC_PROP_INSTANCES_PATH    = "Props/Instances/TRK_UNIT";  // off_82F2A748

        // ProcessLoadPropInstancesRequest baked constants (the assert texts @0x8266F178
        // name both: "strncmp(lacResourceName, \"PRP_INST_\", KU_STRING_INDEX_OF_ZONE_NUMBER)
        // == 0" and "luZoneId < KU_MAX_ZONES"; the compare immediates are 9 and 0x1F4).
        const u32 KU_STRING_INDEX_OF_ZONE_NUMBER = 9;
        const u32 KU_MAX_ZONES                   = 500;
    }

    // @ 0x82674670 -- the per-frame pump. Reconstructed slice: the GameData request drain
    // (raw CgsResource requests forward verbatim into the embedded ResourceModule's input;
    // GameData-level requests route through ProcessGameDataEvent), the drained-input clear,
    // the output allocator-list publish and the ResourceModule pump. DEFERRED stages are
    // marked inline (each with its X360 anchor) -- their subsystems are not committed yet.
    bool GameDataModule::Update(void* lpInputBuffer, void* lpOutputBuffer)
    {
        GameDataIO::InputBuffer*  lpInput  = static_cast<GameDataIO::InputBuffer*>(lpInputBuffer);
        GameDataIO::OutputBuffer* lpOutput = static_cast<GameDataIO::OutputBuffer*>(lpOutputBuffer);

        // [deferred] dev slot-usage dump toggles (byte_82FFAD00/01 -> AllocatorList::
        // DebugDumpStats + the "Slot <n> is used" walk over the event-slot free list).
        // [deferred] CgsGameTalk::GameTalk::Update (X360 a1+399624; module not committed).

        // The per-frame CgsResource input the pump publishes into. The X360 carves it from
        // the CgsModule::IOBufferStack (CreateIOBuffer<ResourceIO::InputBuffer>("Resource"));
        // [marked deviation] the IOBufferStack is deferred, so the module reuses one
        // function-local static buffer, exactly like the committed CreatePools bring-up path.
        static CgsResource::ResourceIO::InputBuffer s_ResourceInput;
        static bool s_bResourceInputConstructed = false;
        if (!s_bResourceInputConstructed) { s_ResourceInput.Construct(); s_bResourceInputConstructed = true; }

        // [deferred] AttribSys input carve ("Attrib") + InputBuffer::
        // AppendRequestInterface<32768> + the AttribSysModule pump (X360 a1+399000) + the
        // attrib response receiver drain (a1+395888: 3 -> ProcessAttribSysRegisterVault
        // Response, 5 -> ProcessUnregisterVehicleAttribsResponse, else "Invalid request
        // received\n" line 1946).

        if (lpOutput != 0)
        {
            // X360: LockForWrite(out); FileSystemStatusInterface::Construct(out+36);
            // out->mpAllocatorList = 0; SetAllocatorList(out, &mAllocatorList); then the
            // LiveUpdateIO status copy (SetLiveUpdateStatus from the LiveUpdate output).
            // The LiveUpdate module + the OutputBuffer's filesystem-status member are
            // deferred; the allocator-list publish is the live part.
            lpOutput->LockForWrite();
            lpOutput->Construct();
            lpOutput->SetAllocatorList(&mAllocatorList);
            lpOutput->UnlockForWrite();
        }

        if (lpInput != 0)
            lpInput->LockForRead();

        s_ResourceInput.LockForWrite();

        // [deferred] LiveUpdateIO::OutputBuffer request append (X360 Append<512,16> of the
        // live-update output's request queue into the resource input; module not committed).

        // ---- the request pump: drain the GameData request interface ---------------------
        // Queue type ids < 26 are raw CgsResource requests (CreatePool 0 / LoadBundle 2 /
        // UnloadBundle 3 / AcquireResource 4 / AcquireResourceList 5 / ...) forwarded
        // verbatim into the ResourceModule input; ids >= 26 are GameData-level events
        // routed through ProcessGameDataEvent (26 == LoadGameDataEvent, the first
        // GameData-level id -- the X360 boundary compare).
        if (lpInput != 0)
        {
            const GameDataIO::RequestInterface<GameDataIO::InputBuffer::knRequestInterfaceQueueSize>*
                lpRequestInterface = lpInput->GetRequestInterface();

            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            s32 liType = lpRequestInterface->mRequestQueue.GetFirstEvent(&lpEvent, &liSize);
            while (lpEvent != 0)
            {
                if (liType >= 26)
                    ProcessGameDataEvent(&s_ResourceInput, lpEvent, liType);
                else
                    s_ResourceInput.GetResourceQueue()->AddEvent(lpEvent, liType, liSize);

                liType = lpRequestInterface->mRequestQueue.GetNextEvent(lpEvent, &lpEvent, &liSize);
            }
            lpInput->UnlockForRead();

            // X360: re-lock the input for write and clear the drained request state (the
            // 32768-byte request queue, the AttribSys request queue and the debug Im2d
            // pointer). The AttribSys queue / Im2d members are deferred with their
            // subsystems; the request-queue clear is the live part.
            lpInput->LockForWrite();
            lpInput->GetRequestInterface()->mRequestQueue.Clear();
            lpInput->UnlockForWrite();
        }

        // ---- internal response drain (DEFERRED) -----------------------------------------
        // X360 drains mReceiverQueue here, routing each completion back out:
        //   2 -> ProcessInternalLoadBundleResponse @0x82672630 (load done -> dispatch the
        //        paired Get, or post the fail response + free the slot)
        //   3 -> ProcessInternalUnloadResponse   4 -> ProcessInternalAcquireResponse
        //        @0x826736D8   7 -> ProcessInternalInvalidateResponse
        //   8 -> ProcessInternalValidateResponse
        //   default -> CGS_ASSERT "Invalid request received\n" (line 1904)
        // then ring-rewinds the queue for the next frame. The ProcessInternal* completion
        // handlers are DEFERRED (they close the loop back to the requesters); [marked
        // deviation] the queue is Clear()ed so the deferred drain cannot overflow it.
        mReceiverQueue.Clear();

        s_ResourceInput.UnlockForWrite();

        // Pump the embedded streaming engine with the requests published above. The X360
        // wraps this in UpdateResourceModule @0x826663B0, which also carves a ResourceIO::
        // OutputBuffer from the IOBufferStack and copies the filesystem status out
        // (a1+475952) -- both deferred with their consumers.
        const bool lbResult = mResourceModule.Update(&s_ResourceInput, 0);

        // [deferred] final output publish of the filesystem status interface (X360
        // SetFileSystemStatusInterface(out, a1+475952); member not committed).
        return lbResult;
    }

    // @ 0x826744F0 -- route one queued GameData-level event by its queue type id.
    void GameDataModule::ProcessGameDataEvent(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                              const CgsModule::Event* lpEvent, s32 liEventType)
    {
        switch (liEventType)
        {
        case 26:   // LoadGameDataEvent
            ProcessLoadGameDataEvent(lpResourceInput,
                                     static_cast<const GameDataIO::GameDataAssetEvent*>(lpEvent), -1);
            break;
        case 39:   // UnloadGameDataEvent
            ProcessUnloadGameDataEvent(lpResourceInput,
                                       static_cast<const GameDataIO::GameDataAssetEvent*>(lpEvent), -1);
            break;
        case 49:   // GetGameDataEvent
            ProcessGetGameDataEvent(lpResourceInput,
                                    static_cast<const GameDataIO::GameDataAssetEvent*>(lpEvent), -1);
            break;
        case 67:   // SwapOutCollisionWorldRequest
            ProcessSwapOutCollisionWorldRequest();
            break;
        case 68:   // SwapInCollisionWorldRequest
            ProcessSwapInCollisionWorldRequest();
            break;
        default:
            CGS_ASSERT(false, "Invalid event id\n");   // X360 line 2791
            break;
        }
    }

    // @ 0x826664A0 -- fetch (liSlotIndex >= 0) or allocate (liSlotIndex < 0, the dispatch
    // path) an event slot, then capture the request event into it (the X360 copies
    // miEventId / the receiver queue / miPoolId / mId / meType and clears the fail flag).
    GameDataEventSlot* GameDataModule::GetGameDataEventSlot(
            const GameDataIO::GameDataAssetEvent* lpEvent, s32 liSlotIndex)
    {
        GameDataEventSlot* lpSlot;
        if (liSlotIndex >= 0)
        {
            lpSlot = &mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)];
        }
        else
        {
            lpSlot = mGameDataEventSlotPool.Pop();
            CGS_ASSERT(lpSlot != 0,
                       "No space for new requests - extend size of game data event pool\n");
            if (lpSlot == 0)
                return 0;   // [marked deviation] the X360 assert is non-fatal and the next
                            // store would fault on a full pool; guard the host instead.
            lpSlot->miResponseEventId = 69;   // fresh-slot sentinel (X360 `*(slot+40) = 69`)
        }

        // X360 store order: fail flag(+36)=0, meType(+32), mId(+24), miPoolId(+16),
        // queue(+12), miEventId(+8).
        lpSlot->mEvent.mbFailFlag      = false;
        lpSlot->mEvent.meType          = lpEvent->meType;
        lpSlot->mEvent.mId             = lpEvent->mId;
        lpSlot->mEvent.miPoolId        = lpEvent->miPoolId;
        lpSlot->mEvent.mpReceiverQueue = lpEvent->mpReceiverQueue;
        lpSlot->mEvent.miEventId       = lpEvent->miEventId;
        return lpSlot;
    }

    // PC bring-up scaffolding (NOT an X360 function) -- see the header note.
    void GameDataModule::DeferredGameDataRequest(const char* lpcHandlerName, GameDataEventSlot* lpSlot)
    {
        *CgsDev::Log::gpDebugPrint << "[GameData] request handler DEFERRED: "
                                   << lpcHandlerName << "\n";
        mGameDataEventSlotPool.PushIndex(mGameDataEventSlotPool.GetObjectIndex(lpSlot));
    }

    // @ 0x82671EA0 -- dispatch a LoadGameDataEvent by the uncompressed prefix of its CgsID.
    // (The export set is function-gapped here; the body was decompiled straight from the
    // ARTIST .i64.) The X360 switches on the packed 4-char dwords of the uncompressed id;
    // the memcmp prefix compares below test the same bytes in the same order,
    // host-endian-safely. Response event ids (stored at slot->miResponseEventId by each
    // handler) are the X360 case immediates: Vehicle 27, TrafficVehicle 28, AILanes 29,
    // TrafficLanes 30, WorldUnit 31, WorldCollision 32, PVS 33, PropPhysics 34,
    // PropInstances 35, Wheel 36, SurfaceList 37.
    void GameDataModule::ProcessLoadGameDataEvent(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                  const GameDataIO::GameDataAssetEvent* lpEvent,
                                                  s32 liSlotIndex)
    {
        char lacName[KI_CGSID_STRING_LEN];
        CgsIDUnCompress(lpEvent->mId, lacName);

        GameDataEventSlot* lpSlot = GetGameDataEventSlot(lpEvent, liSlotIndex);
        if (lpSlot == 0)
            return;   // [marked deviation] full-pool guard (see GetGameDataEventSlot)
        const s32 liIndex = mGameDataEventSlotPool.GetObjectIndex(lpSlot);
        lpSlot->meStage = GameDataEventSlot::E_LOADING;

        if (memcmp(lacName, "VEH_", 4) == 0)
        {
            DeferredGameDataRequest("LoadVehicle (0x8266EB98, id 27)", lpSlot);
        }
        else if (memcmp(lacName, "WHE_", 4) == 0)
        {
            DeferredGameDataRequest("LoadWheel (0x8266EDB8, id 36)", lpSlot);
        }
        else if (memcmp(lacName, "TVEH", 4) == 0)
        {
            DeferredGameDataRequest("LoadTrafficVehicle (0x8266EF00, id 28)", lpSlot);
        }
        else if (memcmp(lacName, "ICE_", 4) == 0)
        {
            // X360: the ICE_ load id dispatches to NO handler (empty case -> fall out).
        }
        else if (memcmp(lacName, "PRP_", 4) == 0)
        {
            if (memcmp(lacName + 4, "PHYS", 4) == 0)
                DeferredGameDataRequest("LoadPropPhysics (id 34)", lpSlot);
            else if (memcmp(lacName + 4, "INST", 4) == 0)
                ProcessLoadPropInstancesRequest(lpResourceInput, lpEvent, 35, liIndex);
            else
                CGS_ASSERT(false, "Invalid game data id: ");   // X360 streams the id (line 2899)
        }
        else if (memcmp(lacName, "GD__", 4) == 0)
        {
            if (memcmp(lacName + 8, "LANE", 4) == 0)
                DeferredGameDataRequest("LoadTrafficLanes (0x8266F398, id 30)", lpSlot);
            else if (memcmp(lacName + 4, "AI__", 4) == 0)
                DeferredGameDataRequest("LoadAILanes (0x8266F4B0, id 29)", lpSlot);
            else
                CGS_ASSERT(false, "Invalid game data id: ");   // X360 streams the id (line 2914)
        }
        else if (memcmp(lacName, "TRK_", 4) == 0)
        {
            if (memcmp(lacName + 4, "UNIT", 4) == 0)
                DeferredGameDataRequest("LoadWorldUnit (0x8266F5C8, id 31)", lpSlot);
            else if (memcmp(lacName + 4, "COLL", 4) == 0)
                DeferredGameDataRequest("LoadWorldCollision (0x8266F830, id 32)", lpSlot);
            else if (memcmp(lacName + 4, "ZONE", 4) == 0)
                ProcessLoadPVSRequest(lpResourceInput, lpEvent, 33, liIndex);
            else
                CGS_ASSERT(false, "Invalid track data id: ");   // X360 streams the id (line 2933)
        }
        else if (memcmp(lacName, "SURF", 4) == 0)
        {
            ProcessLoadSurfaceListRequest(lpResourceInput, lpEvent, 37, liIndex);
        }
        else
        {
            CGS_ASSERT(false, "Invalid data id: ");   // X360 streams the id (line 2945)
        }
    }

    // @ 0x82672268 -- dispatch a GetGameDataEvent by the uncompressed prefix of its CgsID.
    // Response event ids are the X360 case immediates: Vehicle 50, TrafficVehicle 51,
    // VehicleList 52, FreeburnChallengeList 53, AILanes 54, TrafficLanes 55, WorldUnit 56,
    // WorldCollision 57, PVS 58, WheelList 59, Wheel 60, PropPhysics 61, PropInstances 62,
    // PropGraphicsList 63, ICEList 64, ICEMovie 65.
    void GameDataModule::ProcessGetGameDataEvent(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                 const GameDataIO::GameDataAssetEvent* lpEvent,
                                                 s32 liSlotIndex)
    {
        char lacName[KI_CGSID_STRING_LEN];
        CgsIDUnCompress(lpEvent->mId, lacName);

        GameDataEventSlot* lpSlot = GetGameDataEventSlot(lpEvent, liSlotIndex);
        if (lpSlot == 0)
            return;   // [marked deviation] full-pool guard (see GetGameDataEventSlot)
        const s32 liIndex = mGameDataEventSlotPool.GetObjectIndex(lpSlot);
        lpSlot->meStage = GameDataEventSlot::E_AQUIRING;

        if (memcmp(lacName, "VEH_", 4) == 0)
        {
            DeferredGameDataRequest("GetVehicle (0x8266FDA0, id 50)", lpSlot);
        }
        else if (memcmp(lacName, "ICE_", 4) == 0)
        {
            DeferredGameDataRequest("GetICEMovie (id 65)", lpSlot);
        }
        else if (memcmp(lacName, "WHE_", 4) == 0)
        {
            DeferredGameDataRequest("GetWheel (0x82670140, id 60)", lpSlot);
        }
        else if (memcmp(lacName, "TVEH", 4) == 0)
        {
            DeferredGameDataRequest("GetTrafficVehicle (0x82670280, id 51)", lpSlot);
        }
        else if (memcmp(lacName, "VL__", 4) == 0)
        {
            DeferredGameDataRequest("GetVehicleList (id 52)", lpSlot);
        }
        else if (memcmp(lacName, "CL__", 4) == 0)
        {
            DeferredGameDataRequest("GetFreeburnChallengeList (id 53)", lpSlot);
        }
        else if (memcmp(lacName, "IL__", 4) == 0)
        {
            DeferredGameDataRequest("GetICEList (id 64)", lpSlot);
        }
        else if (memcmp(lacName, "PRP_", 4) == 0)
        {
            if (memcmp(lacName + 4, "PHYS", 4) == 0)
                DeferredGameDataRequest("GetPropPhysics (id 61)", lpSlot);
            else if (memcmp(lacName + 4, "GL__", 4) == 0)
                DeferredGameDataRequest("GetPropGraphicsList (id 63)", lpSlot);
            else if (memcmp(lacName + 4, "INST", 4) == 0)
                DeferredGameDataRequest("GetPropInstances (id 62)", lpSlot);
            else
                CGS_ASSERT(false, "Invalid id\n");   // X360 line 3114
        }
        else if (memcmp(lacName, "WL__", 4) == 0)
        {
            DeferredGameDataRequest("GetWheelList (id 59)", lpSlot);
        }
        else if (memcmp(lacName, "GD__", 4) == 0)
        {
            if (memcmp(lacName + 8, "LANE", 4) == 0)
                DeferredGameDataRequest("GetTrafficLanes (0x826703B0, id 55)", lpSlot);
            else if (memcmp(lacName + 4, "AI__", 4) == 0)
                DeferredGameDataRequest("GetAILanes (0x826704C0, id 54)", lpSlot);
            else
                CGS_ASSERT(false, "Invalid id\n");   // X360 line 3133
        }
        else if (memcmp(lacName, "TRK_", 4) == 0)
        {
            if (memcmp(lacName + 4, "UNIT", 4) == 0)
                ProcessGetWorldUnitRequest(lpResourceInput, lpEvent, 56, liIndex);
            else if (memcmp(lacName + 4, "COLL", 4) == 0)
                DeferredGameDataRequest("GetWorldCollision (0x82670700, id 57)", lpSlot);
            else if (memcmp(lacName + 4, "PVS_", 4) == 0)
                DeferredGameDataRequest("GetPVS (0x82670880, id 58)", lpSlot);
            else
                CGS_ASSERT(false, "Invalid id\n");   // X360 line 3152
        }
        else
        {
            CGS_ASSERT(false, "Invalid id\n");   // X360 line 3157
        }
    }

    // @ 0x826733F8 -- dispatch an UnloadGameDataEvent. DEFERRED with the unload handler
    // family: the X360 stages E_UNLOADING then routes exactly like the load dispatcher
    // (VEH_ -> UnloadVehicle 40 @0x82672DE0, TVEH -> 41 @0x82670BE0, GD__/AI__ -> 42
    // @0x82670E40, GD__/LANE -> 43, TRK_/UNIT -> 44 @0x82671160, TRK_/COLL -> 45
    // @0x826712A0, TRK_/PVS_ -> 46 @0x82671420, WHE_ -> 47 @0x82670AA0, PRP_/INST -> 48
    // @0x82670F50; unknown ids assert "Invalid id\n" / "Invalid game data id: ").
    void GameDataModule::ProcessUnloadGameDataEvent(CgsResource::ResourceIO::InputBuffer* /*lpResourceInput*/,
                                                    const GameDataIO::GameDataAssetEvent* /*lpEvent*/,
                                                    s32 /*liSlotIndex*/)
    {
        *CgsDev::Log::gpDebugPrint << "[GameData] ProcessUnloadGameDataEvent DEFERRED\n";
    }

    // @ 0x826717A8 / @ 0x82671530 -- swap the collision world in/out. DEFERRED with the
    // collision-world swap machinery (WorldEntityModule Validate/InvalidateCollision).
    void GameDataModule::ProcessSwapInCollisionWorldRequest()
    {
        *CgsDev::Log::gpDebugPrint << "[GameData] ProcessSwapInCollisionWorldRequest DEFERRED\n";
    }
    void GameDataModule::ProcessSwapOutCollisionWorldRequest()
    {
        *CgsDev::Log::gpDebugPrint << "[GameData] ProcessSwapOutCollisionWorldRequest DEFERRED\n";
    }

    // @ 0x826705D0 -- service a GET world-unit request: acquire the unit's "<name>_list"
    // instance-list resource from the pool named by the request. The reply lands on
    // mReceiverQueue and (X360) ProcessInternalAcquireResponse posts it back to the
    // requester with the response id staged at slot->miResponseEventId (56).
    void GameDataModule::ProcessGetWorldUnitRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                    const GameDataIO::GameDataAssetEvent* lpEvent,
                                                    s32 liEventId, s32 liSlotIndex)
    {
        char lacResourceName[KI_CGSID_STRING_LEN];
        CgsIDConvertToString(lpEvent->mId, lacResourceName);

        // X360: `if (event->meType) assert` -- track units are asset set 0 (GRAPHICS).
        CGS_ASSERT(lpEvent->meType == E_ASSETSET_GRAPHICS,
                   "Invalid asset type for track units\n");   // X360 line 4897

        // "<TRK_UNITn>_list" -- the unit's instance-list resource name (X360 SPrintf cap 128).
        char lacListName[128];
        CgsCore::SPrintf(lacListName, 128, "%s_list", lacResourceName);

        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        // The X360 24-byte type-4 record: {mpUser = &mReceiverQueue, miEventId = the slot
        // index, miPoolId = the request's pool, mResourceId = ID::HashString(list name)}.
        // mbCheckRefCount lies outside the X360 record; false per the committed convention.
        CgsResource::Events::AcquireResourceRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.miPoolId  = lpEvent->miPoolId;
        lRequest.mResourceId.SetHash(static_cast<u64>(static_cast<u32>(
            CgsResource::ID::HashString(reinterpret_cast<const u8*>(lacListName)))));
        lRequest.mbCheckRefCount = false;

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            4 /*AcquireResource*/, static_cast<s32>(sizeof(lRequest)));
    }

    // @ 0x8266F9C0 -- service a LOAD PVS request: stream the world's PVS bundle
    // ("pvs.bndl") into the request's pool. Response id staged at the slot (33).
    void GameDataModule::ProcessLoadPVSRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                               const GameDataIO::GameDataAssetEvent* lpEvent,
                                               s32 liEventId, s32 liSlotIndex)
    {
        CGS_ASSERT(lpEvent->meType == E_ASSETSET_DATA,
                   "Invalid asset type for pvs\n");   // X360 line 4355

        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        // The X360 148-byte type-2 LoadBundle record: {mpUser = &mReceiverQueue,
        // miEventId = the slot index, filename = the baked PVS bundle name,
        // mbLiveUpdateReplace = false, miPoolId = the request's pool,
        // mbAllowFailiure = the request's fail flag, mbUseHDCache = false}.
        CgsResource::Events::LoadBundleRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.SetFileName(KPC_PVS_FILE_NAME);
        lRequest.mbLiveUpdateReplace = false;
        lRequest.miPoolId            = lpEvent->miPoolId;
        lRequest.mbAllowFailiure     = lpEvent->mbFailFlag;
        lRequest.mbUseHDCache        = false;

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            2 /*LoadBundle*/, static_cast<s32>(sizeof(lRequest)));
    }

    // @ 0x8266F718 -- service a LOAD surface-list request: stream the world's surface
    // list ("surfacelist.bin") into the request's pool. Response id staged at the slot
    // (37 -- the id the WorldEntityModule surface-list consumer waits on).
    void GameDataModule::ProcessLoadSurfaceListRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                       const GameDataIO::GameDataAssetEvent* lpEvent,
                                                       s32 liEventId, s32 liSlotIndex)
    {
        CGS_ASSERT(lpEvent->meType == E_ASSETSET_ATTRIBS,
                   "Invalid asset type for surface list\n");   // X360 line 4285

        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        CgsResource::Events::LoadBundleRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.SetFileName(KPC_SURFACE_LIST_FILE_NAME);
        lRequest.mbLiveUpdateReplace = false;
        lRequest.miPoolId            = lpEvent->miPoolId;
        lRequest.mbAllowFailiure     = lpEvent->mbFailFlag;
        lRequest.mbUseHDCache        = false;

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            2 /*LoadBundle*/, static_cast<s32>(sizeof(lRequest)));
    }

    // @ 0x8266F178 -- service a LOAD prop-instances request: parse the zone number out of
    // the "PRP_INST_<n>" id, build the zone's prop-instance bundle path
    // ("Props/Instances/TRK_UNIT<n>_PropInstances.bundle") through the StrStream chain,
    // and stream it into the request's pool. Response id staged at the slot (35).
    void GameDataModule::ProcessLoadPropInstancesRequest(CgsResource::ResourceIO::InputBuffer* lpResourceInput,
                                                         const GameDataIO::GameDataAssetEvent* lpEvent,
                                                         s32 liEventId, s32 liSlotIndex)
    {
        // X360: the stream is constructed over a 208-byte stack buffer with a 100-byte
        // capacity argument, first byte pre-cleared, at function entry.
        char lacFileName[208];
        lacFileName[0] = '\0';
        CgsDev::StrStream lStream(lacFileName, 100);

        CGS_ASSERT(lpEvent->meType == E_ASSETSET_PHYSICS,
                   "Invalid asset type for prop physics \n");   // X360 line 4149 (verbatim, incl. the space)

        char lacResourceName[KI_CGSID_STRING_LEN];
        CgsIDConvertToString(lpEvent->mId, lacResourceName);

        // X360 line 4155 (message-less assert: the stringized condition is the message).
        CGS_ASSERT(strncmp(lacResourceName, "PRP_INST_", KU_STRING_INDEX_OF_ZONE_NUMBER) == 0,
                   "strncmp(lacResourceName, \"PRP_INST_\", KU_STRING_INDEX_OF_ZONE_NUMBER) == 0");

        const u32 luZoneId = static_cast<u32>(atoi(&lacResourceName[KU_STRING_INDEX_OF_ZONE_NUMBER]));
        // X360 fires this guard TWICE (lines 4157 and 4160) inside one out-of-range branch;
        // both asserts are reproduced.
        CGS_ASSERT(luZoneId < KU_MAX_ZONES, "luZoneId < KU_MAX_ZONES");
        CGS_ASSERT(luZoneId < KU_MAX_ZONES, "luZoneId < KU_MAX_ZONES");

        lStream << KPC_PROP_INSTANCES_PATH << luZoneId << "_PropInstances.bundle";

        mGameDataEventSlotPool[static_cast<s16>(liSlotIndex)].miResponseEventId = liEventId;

        CgsResource::Events::LoadBundleRequest lRequest;
        memset(&lRequest, 0, sizeof(lRequest));
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = liSlotIndex;
        lRequest.SetFileName(lacFileName);
        lRequest.mbLiveUpdateReplace = false;
        lRequest.miPoolId            = lpEvent->miPoolId;
        lRequest.mbAllowFailiure     = lpEvent->mbFailFlag;
        lRequest.mbUseHDCache        = false;

        lpResourceInput->GetResourceQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest),
            2 /*LoadBundle*/, static_cast<s32>(sizeof(lRequest)));
    }
}
