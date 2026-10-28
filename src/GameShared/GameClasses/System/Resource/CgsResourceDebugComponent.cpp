#include "GameShared/GameClasses/System/Resource/CgsResourceDebugComponent.h"

#include "GameShared/GameClasses/Development/Log/CgsLogFileBuffered.h"   // CgsDev::Log::LogFileBuffered
#include "GameShared/GameClasses/System/Resource/CgsResourceModule.h"      // ResourceModule::GetPoolModule
#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"  // PoolModule::GetPoolIndex / GetPoolByIndex
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"        // Pool (heaps, entries, FindResource)
#include "GameShared/GameClasses/System/Resource/CgsDebugPoolTextures.h"   // EDebugSortMode / EDebugTextureRenderMode + their name tables
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include <cmath>

// ARTIST82824048 always disables screen checking; the last native compatibility
// parameter is ignored by its existing PC wrapper.
int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCheckScreen);

// CgsResource::DebugComponent -- Construct, GetPath ("Core"), DumpPoolStatistics, UpdateShareTest.

namespace CgsResource
{
    namespace
    {
        // A pool entry whose status byte is exactly this is loaded (the share test skips the rest).
        const u8 KU8_ENTRY_STATUS_LOADED = 2;

        // Unnamed ARTIST storage: selector82F33FF0 and latch830EA3EE.
        // The original callback writes the latch; a complete consumer
        // disposition is still open, so no defrag action is synthesized here.
        s32 siDefragPoolId = -1;
        bool sbTriggerDefrag = false;
        const s32 KAI_TEXTURE_PAGE_SIZES[5] = {1, 1, 8, 16, 32}; //820F75F4

        // Source homes for mutable ARTIST image words82F34728/2C/30. The
        // direct reference scan finds only this HUD's reads; indirect writers
        // remain unclassified. These initial values are present in the image.
        f32 sfBundleQueuePriorityOffset = 400.0f;
        f32 sfBundleQueueY = 150.0f;
        f32 sfBundleQueueX = 300.0f;
    }

    // The option tables are filled from the shared mode-name tables: the type-list modes in the
    // order Detailed, Summary, Raw; the sort and render modes in enum order and
    // closed by a {0, null} row. ARTIST's three type-mode rows terminate through
    // the adjacent zero at +0x5260; the widened native table owns an end row.
    // The original window table holds the four embedded pool
    // windows; the texture browser allocates its entries from the debug allocator.
    void DebugComponent::Construct(ResourceModule* lpResourceModule, const DebugComponentParams* lpParams)
    {
        // ARTIST 828F6E40 calls the positively empty folded helper 8284CB38.
        // Preserve its lack of active/link stores in this two-phase operation.

        mpResourceModule = lpResourceModule;
        mParams = *lpParams;

        mapWindows[0] = &mPoolWindow;
        mapWindows[1] = &mPoolHistogram;
        mapWindows[2] = &mPoolTypeList;
        mapWindows[3] = &mPoolTextures;

        miTypeListMode = E_POOLTYPELISTMODE_ARTIST;
        maTypeStringList[0].miValue = E_POOLTYPELISTMODE_PROGRAMMER;
        maTypeStringList[0].mpcName = KAPC_POOLTYPELISTMODENAMES[E_POOLTYPELISTMODE_PROGRAMMER];
        maTypeStringList[1].miValue = E_POOLTYPELISTMODE_ARTIST;
        maTypeStringList[1].mpcName = KAPC_POOLTYPELISTMODENAMES[E_POOLTYPELISTMODE_ARTIST];
        maTypeStringList[2].miValue = E_POOLTYPELISTMODE_RAW;
        maTypeStringList[2].mpcName = KAPC_POOLTYPELISTMODENAMES[E_POOLTYPELISTMODE_RAW];
        maTypeStringList[E_POOLTYPELISTMODE_COUNT] = {0, 0};

        miSortMode = E_DEBUGSORTMODE_NONE;
        for (s32 liMode = 0; liMode < E_DEBUGSORTMODE_COUNT; ++liMode)
        {
            maSortModeStringList[liMode].miValue = liMode;
            maSortModeStringList[liMode].mpcName = KAPC_DEBUGSORTMODENAMES[liMode];
        }
        maSortModeStringList[E_DEBUGSORTMODE_COUNT].miValue = 0;
        maSortModeStringList[E_DEBUGSORTMODE_COUNT].mpcName = 0;

        miRenderMode = E_DEBUGTEXTURERENDERMODE_STRETCH;
        for (s32 liMode = 0; liMode < E_DEBUGTEXTURERENDERMODE_COUNT; ++liMode)
        {
            maRenderModeStringList[liMode].miValue = liMode;
            maRenderModeStringList[liMode].mpcName = KAPC_DEBUGTEXTURERENDERMODE[liMode];
        }
        maRenderModeStringList[E_DEBUGTEXTURERENDERMODE_COUNT].miValue = 0;
        maRenderModeStringList[E_DEBUGTEXTURERENDERMODE_COUNT].mpcName = 0;

        mbTriggerStatsDump      = false;
        miStatsDumpCount        = 0;
        mbPoolShareTestVisible  = false;
        mbShowBundleLoaderQueue = false;
        mPoolTextures.AllocateDebugMemory(mParams.mpDebugAllocator);
    }

    const char* DebugComponent::GetPath() const
    {
        return "Core";
    }

    // ARTIST828D9778: walk all128 original pool slots in index order.
    void DebugComponent::GenerateStringList()
    {
        maStringList[0] = {-1, "<none>"};
        s32 liNext = 1;
        PoolModule& lrPools = mpResourceModule->GetPoolModule();
        for (s32 liPool = 0; liPool < PoolModule::KI_MAX_POOLS; ++liPool)
        {
            const Pool& lrPool = lrPools.GetPoolByIndex(liPool);
            if (lrPool.GetId() != -1)
                maStringList[liNext++] = {lrPool.GetId(), lrPool.GetName()};
        }
        maStringList[liNext] = {-1, 0};
    }

    // ARTIST828EE200. These are the original18 registration sites.
    void DebugComponent::InitWindows()
    {
        GenerateStringList();
        RegisterVariable(&siDefragPoolId, "Defrag Pool");
        SetOptions(&siDefragPoolId, maStringList);
        RegisterFunction(&TriggerDefragCallback, this, "Trigger Defrag");
        for (s32 liWindow = 0; liWindow < KI_MAX_VISIBLE_POOLS; ++liWindow)
        {
            maiPoolIds[liWindow] = -1;
            maiPrevPoolIds[liWindow] = -1;
        }
        RegisterVariable(&maiPoolIds[0], "Heap Window");
        SetOptions(&maiPoolIds[0], maStringList);
        RegisterVariable(&maiPoolIds[1], "Histogram");
        SetOptions(&maiPoolIds[1], maStringList);
        RegisterVariable(&maiPoolIds[2], "Type Stats");
        SetOptions(&maiPoolIds[2], maStringList);
        RegisterVariable(&miTypeListMode, "Type Stats Mode");
        SetOptions(&miTypeListMode, maTypeStringList);
        RegisterVariable(&maiPoolIds[3], "Texture Window");
        SetOptions(&maiPoolIds[3], maStringList);
        RegisterVariable(&miSortMode, "Sort By");
        SetOptions(&miSortMode, maSortModeStringList);
        RegisterVariable(&miRenderMode, "Draw");
        SetOptions(&miRenderMode, maRenderModeStringList);
        RegisterVariable(&mbShowBundleWindow, "Bundle Window");
        SetChangeCallback(&mbShowBundleWindow, &ShowBundleWindowChangeCallback, this);
        RegisterFunction(&IncTextureCallback, this, "Next Texture");
        RegisterFunction(&DecTextureCallback, this, "Prev Texture");
        RegisterFunction(&IncPageCallback, this, "Next Texture Page");
        RegisterFunction(&DecPageCallback, this, "Prev Texture Page");
        RegisterFunction(&TriggerStatsCallback, this, "Dump Stats");
        RegisterVariable(&miPoolShareTest0, "ShareTest0");
        SetOptions(&miPoolShareTest0, maStringList);
        RegisterVariable(&miPoolShareTest1, "ShareTest1");
        SetOptions(&miPoolShareTest1, maStringList);
        RegisterVariable(&mbPoolShareTestVisible, "Show Share Test");
    }

    // ARTIST828F6FA8. The base activation is an empty ICF body.
    void DebugComponent::OnActivate()
    {
        InitWindows();
        s32* const lpiMaxFixups = &mpResourceModule->mBundleLoaderModule.miMaxPartialFixups;
        RegisterVariable(lpiMaxFixups, "Max Partial Fixups");
        SetRange(lpiMaxFixups, 0, 10000);
        SetStep(lpiMaxFixups, 10);
        RegisterVariable(&mbShowBundleLoaderQueue, "Show bundle loader queue");
    }

    // ARTIST828E38D0..828E3930, including its initial option-table refresh.
    void DebugComponent::UpdatePoolMenuItems()
    {
        GenerateStringList();
        PoolModule& lrPools = mpResourceModule->GetPoolModule();
        for (s32 liWindow = 0; liWindow < KI_MAX_VISIBLE_POOLS; ++liWindow)
        {
            if (maiPoolIds[liWindow] != -1 && lrPools.GetPoolIndex(maiPoolIds[liWindow]) < 0)
                maiPoolIds[liWindow] = -1;
        }
    }

    // ARTIST828EE4C0: an out-of-range window selector returns without a reset.
    void DebugComponent::ResetWindow(s32 liWindow, const Pool* lpPool)
    {
        switch (liWindow)
        {
        case 0:
            mPoolWindow.Reset(lpPool);
            break;
        case 1:
            mPoolHistogram.Reset(lpPool);
            break;
        case 2:
            mPoolTypeList.Reset(lpPool, &mpResourceModule->GetPoolModule(),
                                static_cast<EPoolTypeListMode>(miTypeListMode));
            break;
        case 3:
            mPoolTextures.Reset(lpPool, mParams.mpTextureRenderCallback,
                                mParams.mpTextureRenderUserData,
                                static_cast<EDebugSortMode>(miSortMode),
                                static_cast<EDebugTextureRenderMode>(miRenderMode));
            break;
        }
    }

    // ARTIST828F7178. Add a newly selected window before resetting it, and
    // copy each current selector to its previous slot after handling changes.
    void DebugComponent::UpdateWindows()
    {
        mPoolTypeList.SetMode(static_cast<EPoolTypeListMode>(miTypeListMode));
        mPoolTextures.meSortMode = static_cast<EDebugSortMode>(miSortMode);
        mPoolTextures.meRenderMode = static_cast<EDebugTextureRenderMode>(miRenderMode);
        for (s32 liWindow = 0; liWindow < KI_MAX_VISIBLE_POOLS; ++liWindow)
        {
            if (maiPoolIds[liWindow] != maiPrevPoolIds[liWindow])
            {
                if (maiPoolIds[liWindow] == -1)
                    GetUI().RemoveWindow(mapWindows[liWindow]);
                else
                {
                    if (maiPrevPoolIds[liWindow] == -1)
                        GetUI().AddWindow(mapWindows[liWindow]);
                    PoolModule& lrPools = mpResourceModule->GetPoolModule();
                    const s32 liPoolIndex = lrPools.GetPoolIndex(maiPoolIds[liWindow]);
                    ResetWindow(liWindow, &lrPools.GetPoolByIndex(liPoolIndex));
                }
            }
            maiPrevPoolIds[liWindow] = maiPoolIds[liWindow];
        }
    }

    void DebugComponent::Update()
    {
        // ARTIST828FF00C calls the positively empty ICF body8284CB38.
        UpdatePoolMenuItems();
        UpdateWindows();
        if (mbTriggerStatsDump)
        {
            mbTriggerStatsDump = false;
            DumpStats();
        }
        if (mbPoolShareTestVisible)
            UpdateShareTest();
    }

    // ARTIST828E36C0. Banner, optional three share-test values, then the live
    // load-request queue in priority order. Both queue getters retain their
    // original bounds assertions; callbacks/draws may change the live length.
    void DebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay)
    {
        MaybeDrawText(lpDisplay, "Resource Module Debug Component Active",
                      30.0f, 30.0f, 10.0f, 0xFFFFFFFFu, false);
        if (mbPoolShareTestVisible)
        {
            MaybeDrawText(lpDisplay, "Share Test Results",
                          200.0f, 60.0f, 14.0f, 0xFFFFFFFFu, false);
            for (s32 liHeap = 0; liHeap < 3; ++liHeap)
            {
                const f32 lfY = std::fma(static_cast<f32>(liHeap), 16.0f, 76.0f);
                lpDisplay->DrawValue(maiPoolShareTestResults[liHeap],
                                     220.0f, lfY, 14.0f, 0xFFFFFFFFu);
            }
        }
        if (mbShowBundleLoaderQueue)
        {
            const f32 lfHeaderY = sfBundleQueueY;
            MaybeDrawText(lpDisplay, "FileName", sfBundleQueueX,
                          lfHeaderY, 18.0f, 0xFFFFFFFFu, false);
            MaybeDrawText(lpDisplay, "Priority",
                          sfBundleQueuePriorityOffset + sfBundleQueueX,
                          lfHeaderY, 18.0f, 0xFFFFFFFFu, false);
            f32 lfY = lfHeaderY + 20.0f;
            for (s32 liRequest = 0;
                 liRequest < mpResourceModule->mBundleLoaderModule.mLoadRequestQueue.GetLength();
                 ++liRequest)
            {
                const Events::LoadBundleRequest& lrRequest =
                    mpResourceModule->mBundleLoaderModule.mLoadRequestQueue.GetEntry(liRequest);
                const s32 liPriority =
                    mpResourceModule->mBundleLoaderModule.mLoadRequestQueue.GetEntryPriority(liRequest);
                MaybeDrawText(lpDisplay, lrRequest.macFileName,
                              sfBundleQueueX, lfY, 16.0f, 0xFFFFFFFFu, false);
                lpDisplay->DrawValue(liPriority,
                                     sfBundleQueuePriorityOffset + sfBundleQueueX,
                                     lfY, 16.0f, 0xFFFFFFFFu);
                lfY += 18.0f;
            }
        }
    }

    void DebugComponent::TriggerDefragCallback(void* /*lpData*/)
    {
        sbTriggerDefrag = true; //ARTIST828D9918; original latch, consumer unresolved
    }

    void DebugComponent::IncTextureCallback(void* lpData)
    {
        s32& liIndex = static_cast<DebugComponent*>(lpData)->mPoolTextures.miTextureIndex;
        liIndex = static_cast<s32>(static_cast<u32>(liIndex) + 1u);
    }

    void DebugComponent::DecTextureCallback(void* lpData)
    {
        s32& liIndex = static_cast<DebugComponent*>(lpData)->mPoolTextures.miTextureIndex;
        liIndex = static_cast<s32>(static_cast<u32>(liIndex) - 1u);
    }

    void DebugComponent::IncPageCallback(void* lpData)
    {
        DebugPoolTextures& lrTextures = static_cast<DebugComponent*>(lpData)->mPoolTextures;
        const s32 liSide = KAI_TEXTURE_PAGE_SIZES[lrTextures.meRenderMode];
        const u32 luNext = static_cast<u32>(lrTextures.miTextureIndex) + static_cast<u32>(liSide * liSide);
        lrTextures.miTextureIndex = static_cast<s32>(luNext);
        if (lrTextures.miTextureIndex > lrTextures.miTotalTextures)
            lrTextures.miTextureIndex = static_cast<s32>(static_cast<u32>(lrTextures.miTotalTextures) + 1u);
    }

    void DebugComponent::DecPageCallback(void* lpData)
    {
        DebugPoolTextures& lrTextures = static_cast<DebugComponent*>(lpData)->mPoolTextures;
        const s32 liSide = KAI_TEXTURE_PAGE_SIZES[lrTextures.meRenderMode];
        lrTextures.miTextureIndex = static_cast<s32>(static_cast<u32>(lrTextures.miTextureIndex)
            - static_cast<u32>(liSide * liSide));
        if (lrTextures.miTextureIndex < 0)
            lrTextures.miTextureIndex = 0;
    }

    void DebugComponent::TriggerStatsCallback(void* lpData)
    {
        static_cast<DebugComponent*>(lpData)->mbTriggerStatsDump = true;
    }

    void DebugComponent::ShowBundleWindowChangeCallback(void* lpValue, void* lpData)
    {
        DebugComponent* const lpThis = static_cast<DebugComponent*>(lpData);
        if (*static_cast<const bool*>(lpValue))
        {
            lpThis->mBundleFilesWindow.Reset();
            lpThis->GetUI().AddWindow(&lpThis->mBundleFilesWindow);
        }
        else
            lpThis->GetUI().RemoveWindow(&lpThis->mBundleFilesWindow);
    }

    // Heap 0 is main memory, heap 1 video memory.
    void DebugComponent::DumpPoolStatistics(CgsDev::Log::LogFileBuffered& lrLogFile, const Pool* lpPool)
    {
        const s32 liMainMemSize  = static_cast<s32>(lpPool->maHeaps[0].GetTotalSizeBytes());
        const s32 liMainMemFree  = static_cast<s32>(lpPool->maHeaps[0].GetAmountFreeBytes());
        const s32 liVideoMemSize = static_cast<s32>(lpPool->maHeaps[1].GetTotalSizeBytes());
        const s32 liVideoMemFree = static_cast<s32>(lpPool->maHeaps[1].GetAmountFreeBytes());
        const s32 liMainMemUsed  = liMainMemSize - liMainMemFree;
        const s32 liVideoMemUsed = liVideoMemSize - liVideoMemFree;

        CgsDev::StrStreamBase& lrStream = lrLogFile;
        lrStream << lpPool->GetName();
        lrStream << "\t" << liMainMemSize << "\t" << liVideoMemSize << "\t";
        lrStream << liMainMemUsed << "\t" << liVideoMemUsed << "\t";
        lrStream << liMainMemFree << "\t" << liVideoMemFree << "\n";
    }

    // ARTIST828EE038. Allocate only the main-memory lane for a64KiB
    // scratch buffer, and free that same single-lane resource after closing.
    void DebugComponent::DumpStats()
    {
        rw::ResourceDescriptor lDescriptor;
        for (u32 luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            lDescriptor.m_baseResourceDescriptors[luLane].m_size = 0;
            lDescriptor.m_baseResourceDescriptors[luLane].m_alignment = 1;
        }
        lDescriptor.m_baseResourceDescriptors[0].m_size = 65536;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
        const rw::Resource lResource = mParams.mpDebugAllocator->DoAllocate(lDescriptor, 0);
        char* const lpcBuffer = static_cast<char*>(lResource.m_baseResources[0]);
        if (lpcBuffer != 0)
        {
            char lacFileName[192];
            CgsCore::SPrintf(lacFileName, 128, "d:\\cgsresourcedump_%d.txt", miStatsDumpCount);
            {
                CgsDev::Log::LogFileBuffered lLog;
                miStatsDumpCount = static_cast<s32>(static_cast<u32>(miStatsDumpCount) + 1u);
                lLog.Open(lacFileName, lpcBuffer, 65536);
                lLog << "Pool Stats\n";
                lLog << "Name\tMainMemSize(B)\tVideoMemSize(B)\tMainMemUsed(B)\tVideoMemUsed(B)\tMainMemFree(B)\tVideoMemFree(B)\n";
                PoolModule& lrPools = mpResourceModule->GetPoolModule();
                for (s32 liPool = 0; liPool < PoolModule::KI_MAX_POOLS; ++liPool)
                {
                    const Pool& lrPool = lrPools.GetPoolByIndex(liPool);
                    if (lrPool.GetId() >= 0)
                        DumpPoolStatistics(lLog, &lrPool);
                }
                lLog.Close();
            }
            rw::Resource lFreeResource;
            lFreeResource.m_baseResources[0] = lpcBuffer;
            mParams.mpDebugAllocator->DoFree(lFreeResource);
        }
        else if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "Failed to allocate buffer for stats dump\n";
    }

    // Clear the three totals, resolve both share-test pool ids to pool slots, and when both pools
    // are valid walk every slot of pool 0: for each loaded entry, look its id up in pool 1 (no
    // refcount check, loaded entries only) and add that entry's main / graphics-system /
    // graphics-local sizes to the totals.
    void DebugComponent::UpdateShareTest()
    {
        maiPoolShareTestResults[0] = 0;
        maiPoolShareTestResults[1] = 0;
        maiPoolShareTestResults[2] = 0;

        PoolModule& lrPoolModule = mpResourceModule->GetPoolModule();
        const s32 liPoolIndex0 = lrPoolModule.GetPoolIndex(miPoolShareTest0);
        const s32 liPoolIndex1 = lrPoolModule.GetPoolIndex(miPoolShareTest1);
        if (liPoolIndex0 < 0 || liPoolIndex1 < 0)
            return;

        Pool& lrPool0 = lrPoolModule.GetPoolByIndex(liPoolIndex0);
        Pool& lrPool1 = lrPoolModule.GetPoolByIndex(liPoolIndex1);
        if (!lrPool0.IsValid() || !lrPool1.IsValid())
            return;

        for (u16 luIndex = 0; luIndex < lrPool0.GetMaxResources(); ++luIndex)
        {
            if (lrPool0.GetEntryStatusDirect(luIndex) != KU8_ENTRY_STATUS_LOADED)
                continue;

            const Entry* lpEntry = lrPool1.FindResource(lrPool0.GetEntryDirect(luIndex)->mID, false,
                                                        KU8_ENTRY_STATUS_LOADED, 0);
            if (lpEntry)
            {
                for (s32 liMemType = 0; liMemType < 3; ++liMemType)
                {
                    maiPoolShareTestResults[liMemType] += static_cast<s32>(
                        lpEntry->mResourceDescriptor.m_baseResourceDescriptors[liMemType].m_size);
                }
            }
        }
    }
}
