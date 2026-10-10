#include "GameShared/GameClasses/System/Resource/CgsResourceDebugComponent.h"

#include "GameShared/GameClasses/Development/Log/CgsLogFileBuffered.h"   // CgsDev::Log::LogFileBuffered
#include "GameShared/GameClasses/System/Resource/CgsResourceModule.h"      // ResourceModule::GetPoolModule
#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"  // PoolModule::GetPoolIndex / GetPoolByIndex
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"        // Pool (heaps, entries, FindResource)
#include "GameShared/GameClasses/System/Resource/CgsDebugPoolTextures.h"   // EDebugSortMode / EDebugTextureRenderMode + their name tables

// CgsResource::DebugComponent -- Construct, GetPath ("Core"), DumpPoolStatistics, UpdateShareTest.

namespace CgsResource
{
    namespace
    {
        // A pool entry whose status byte is exactly this is loaded (the share test skips the rest).
        const u8 KU8_ENTRY_STATUS_LOADED = 2;
    }

    // The option tables are filled from the shared mode-name tables: the type-list modes in the
    // order Detailed, Summary, Raw; the sort and render modes in enum order, each closed by a
    // {0, null} row. The console then points the window table at its four sub-windows and has the
    // texture browser allocate its entry array from mParams.mpDebugAllocator; neither step has a
    // counterpart here because those sub-windows are not members of this class (see the header).
    void DebugComponent::Construct(ResourceModule* lpResourceModule, const DebugComponentParams* lpParams)
    {
        CgsDev::DebugComponent::Construct();

        mpResourceModule = lpResourceModule;
        mParams = *lpParams;

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
    }

    const char* DebugComponent::GetPath() const
    {
        return "Core";
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
