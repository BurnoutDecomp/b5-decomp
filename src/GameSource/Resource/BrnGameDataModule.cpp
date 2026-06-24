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
#include <cstring>                                                              // memset

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

        // [resource streaming bring-up] prove the full faithful streaming path now that the pools exist:
        // publish a LoadBundle request (event id 2) for the debug font to the ResourceModule input and pump
        // Update -> ProcessResourceRequests routes it to the bundle loader -> ProcessLoadRequests loads it
        // into the real Fonts pool (id 0) via BundleLoader -> resources created + fixed up. Confirms
        // request -> shuttle -> bundle loader -> pool end-to-end.
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

    void GameDataModule::Construct(const void* /*lpInitOptions*/)
    {
        mbIsNewModule = true;   // X360 *(this+4)=1 (new module type; base Prepare skips the old IO path)

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
    bool GameDataModule::Update(void* /*lpInputBuffer*/, void* /*lpOutputBuffer*/) { return false; }
}
