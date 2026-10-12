#include "GameShared/GameClasses/System/Resource/CgsResourceDebugComponent.h"

#include "GameShared/GameClasses/Development/Log/CgsLogFileBuffered.h"   // CgsDev::Log::LogFileBuffered
#include "GameShared/GameClasses/System/Resource/CgsResourceModule.h"      // ResourceModule::GetPoolModule
#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"  // PoolModule::GetPoolIndex / GetPoolByIndex
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"        // Pool (heaps, entries, FindResource)
#include "GameShared/GameClasses/System/Resource/CgsDebugPoolTextures.h"   // EDebugSortMode / EDebugTextureRenderMode + their name tables
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"

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
    }

    // The option tables are filled from the shared mode-name tables: the type-list modes in the
    // order Detailed, Summary, Raw; the sort and render modes in enum order, each closed by a
    // {0, null} row. The original window table holds the four embedded pool
    // windows; the texture browser allocates its entries from the debug allocator.
    void DebugComponent::Construct(ResourceModule* lpResourceModule, const DebugComponentParams* lpParams)
    {
        CgsDev::DebugComponent::Construct();

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

        miSortMode = E_DEBUGSORTMODE_NONE;
        for (s32 liMode = 0; liMode < E_DEBUGSORTMODE_COUNT; ++liMode)
        {
            maSortModeStringList[liMode].miValue = liMode;
            maSortModeStringList[liMode].mpcName = KAPC_DEBUGSORTMODENAMES[liMode];
        }
        maSortModeStringList[E_DEBUGSORTMODE_COUNT].miValue = 0;
        maSortModeStringList[E_DEBUGSORTMODE_COUNT].mpcName = nullptr;

        miRenderMode = E_DEBUGTEXTURERENDERMODE_STRETCH;
        for (s32 liMode = 0; liMode < E_DEBUGTEXTURERENDERMODE_COUNT; ++liMode)
        {
            maRenderModeStringList[liMode].miValue = liMode;
            maRenderModeStringList[liMode].mpcName = KAPC_DEBUGTEXTURERENDERMODE[liMode];
        }
        maRenderModeStringList[E_DEBUGTEXTURERENDERMODE_COUNT].miValue = 0;
        maRenderModeStringList[E_DEBUGTEXTURERENDERMODE_COUNT].mpcName = nullptr;

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
        maStringList[liNext] = {-1, nullptr};
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
                                                        KU8_ENTRY_STATUS_LOADED, nullptr);
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
