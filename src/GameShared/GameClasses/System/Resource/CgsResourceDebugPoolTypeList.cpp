#include "GameShared/GameClasses/System/Resource/CgsResourceDebugPoolTypeList.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace CgsResource
{
    const char* KAPC_POOLTYPELISTMODENAMES[E_POOLTYPELISTMODE_COUNT] =
    {
        "Raw", "Detailed", "Summary",
    };

    // ARTIST827DF8E8. Real BaseResourceDescriptor constructors initialize
    // every descriptor to {0,1} (821F05C8). Retain the additional first-pair
    // stores and the pool descriptor stores. Counts, names, mode and owner
    // pointers are not initialized by this original constructor.
    DebugPoolTypeList::DebugPoolTypeList()
    {
        for (s32 liIndex = 0; liIndex < KI_NUMTYPES; ++liIndex)
        {
            maTypes[liIndex].mSize.m_baseResourceDescriptors[0].m_size = 0;
            maTypes[liIndex].mSize.m_baseResourceDescriptors[0].m_alignment = 1;
        }
        for (s32 liType = 0; liType < 3; ++liType)
        {
            mPoolDescriptor.m_baseResourceDescriptors[liType].m_size = 0;
            mPoolDescriptor.m_baseResourceDescriptors[liType].m_alignment = 1;
        }
        for (s32 liIndex = 0; liIndex <= E_DEBUGRESOURCECATEGORY_COUNT; ++liIndex)
        {
            maCategories[liIndex].mSize.m_baseResourceDescriptors[0].m_size = 0;
            maCategories[liIndex].mSize.m_baseResourceDescriptors[0].m_alignment = 1;
        }
    }

    // ARTIST828DABC8: title is the pool's name; seven category entries,
    // including the COUNT slot, are cleared byte-for-byte after construction.
    void DebugPoolTypeList::Reset(const Pool* lpPool, const PoolModule* lpPoolModule,
                                  EPoolTypeListMode leMode)
    {
        CgsDev::DebugUI::Window::Prepare(100.0f, 100.0f, lpPool->GetName(), KX_FLAGNORMAL);
        mpPool = lpPool;
        mpPoolModule = lpPoolModule;
        meMode = leMode;
        mfTransparency = 0.75f;
        miMaxResources = 0;
        miNumTypes = 0;
        std::memset(maCategories, 0, sizeof(maCategories));
    }

    void DebugPoolTypeList::SetMode(EPoolTypeListMode leMode)
    {
        meMode = leMode; //inlined store828F719C, console Window+0x484C
    }
    // ARTIST828EE9A8: round each loaded slot to its heap alignment before
    // accumulating descriptors; the category call is the original Type virtual.
    void DebugPoolTypeList::Update(f32 lfTimeStep, CgsDev::DebugUI::InputEvent leEvent)
    {
        CgsDev::DebugUI::Window::Update(lfTimeStep, leEvent);
        if (mpPool)
        {
            miMaxResources = static_cast<s32>(mpPool->GetMaxResources());
            miNumTypes = 0;
            std::memset(maCategories, 0, sizeof(maCategories));
            for (s32 liIndex = 0; liIndex < miMaxResources; ++liIndex)
            {
                if ((mpPool->GetEntryStatusDirect(liIndex) & 2u) != 0)
                {
                    const Entry* lpEntry = mpPool->GetEntryDirect(liIndex);
                    Entry::ResourceDescriptor lSize = lpEntry->mResourceDescriptor;
                    for (u32 luType = 0; luType < BasePool::KI_NUM_TYPES; ++luType)
                    {
                        u32& luSize = lSize.m_baseResourceDescriptors[luType].m_size;
                        if (luSize != 0)
                        {
                            const u32 luAlignment = mpPool->GetHeapAlignment(static_cast<s32>(luType));
                            luSize = (luSize + luAlignment - 1u) & ~(luAlignment - 1u);
                        }
                    }
                    AddReferenceToType(lpEntry->mpResourceType->GetCachedId(), lSize);
                    AddReferenceToCategory(lpEntry->mpResourceType->GetDebugResourceCategory(), lSize);
                }
            }
            std::qsort(maTypes, static_cast<size_t>(miNumTypes), sizeof(TypeEntry), SortTypesByName);
            for (u32 luType = 0; luType < BasePool::KI_NUM_TYPES; ++luType)
            {
                mPoolDescriptor.m_baseResourceDescriptors[luType].m_size =
                    mpPool->maHeaps[luType].GetTotalSizeBytes();
            }
        }
    }

    // ARTIST828E4260; the registry is searched by id and null/missing names
    // fall back to NoName. Existing types still accumulate after the table fills.
    void DebugPoolTypeList::AddReferenceToType(u32 luTypeId, const Entry::ResourceDescriptor& lrSize)
    {
        for (s32 liIndex = 0; liIndex < miNumTypes; ++liIndex)
        {
            TypeEntry& lrEntry = maTypes[liIndex];
            if (lrEntry.muTypeId == luTypeId)
            {
                ++lrEntry.miCount;
                lrEntry.mSize += lrSize;
                return;
            }
        }
        if (miNumTypes < KI_NUMTYPES)
        {
            TypeEntry& lrEntry = maTypes[miNumTypes];
            lrEntry.muTypeId = luTypeId;
            lrEntry.miCount = 1;
            const char* lpcTypeName = nullptr;
            for (s32 liIndex = 0; liIndex < mpPoolModule->mNumTypes; ++liIndex)
            {
                const PoolModule::ResourceTypeEntry& lrRegistered = mpPoolModule->maTypes[liIndex];
                if (lrRegistered.muTypeId == luTypeId)
                {
                    lpcTypeName = lrRegistered.mpcName;
                    break;
                }
            }
            lrEntry.mpcTypeName = lpcTypeName ? lpcTypeName : "NoName";
            lrEntry.mSize = lrSize;
            ++miNumTypes;
        }
        else if ((CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
        {
            *CgsDev::Log::gpDebugPrint <<
                "Warning: Too many types found to display accurate stats in TypeList debug component\n";
        }
    }

    // Inlined at ARTIST828EEADC..828EEAF8; DecFIGS names the category helper.
    void DebugPoolTypeList::AddReferenceToCategory(EDebugResourceCategory leCategory,
                                                  const Entry::ResourceDescriptor& lrSize)
    {
        CategoryEntry& lrEntry = maCategories[leCategory];
        lrEntry.mSize += lrSize;
        ++lrEntry.miCount;
    }

    // ARTIST828DAC38 compares unsigned bytes and returns their actual difference.
    s32 DebugPoolTypeList::SortTypesByName(const void* lpA, const void* lpB)
    {
        const u8* lpNameA = reinterpret_cast<const u8*>(static_cast<const TypeEntry*>(lpA)->mpcTypeName);
        const u8* lpNameB = reinterpret_cast<const u8*>(static_cast<const TypeEntry*>(lpB)->mpcTypeName);
        s32 liDifference;
        do
        {
            liDifference = static_cast<s32>(*lpNameA) - static_cast<s32>(*lpNameB);
            if (*lpNameA == 0)
                break;
            ++lpNameA;
            ++lpNameB;
        } while (liDifference == 0);
        return liDifference;
    }
    // ARTIST828EE920. Invalid modes still render the base window only.
    void DebugPoolTypeList::Render(CgsDev::Debug2DImmediateRender* lpRender)
    {
        CgsDev::DebugUI::Window::Render(lpRender);
        if (mpPool)
        {
            switch (meMode)
            {
            case E_POOLTYPELISTMODE_RAW:
                RenderRaw(lpRender);
                break;
            case E_POOLTYPELISTMODE_PROGRAMMER:
                RenderProgrammer(lpRender);
                break;
            case E_POOLTYPELISTMODE_ARTIST:
                RenderArtist(lpRender);
                break;
            default:
                break;
            }
        }
    }

    // DecFIGS GetSizeString, inlined at ARTIST828E46C4..828E46EC and
    // the corresponding detailed/summary call sites. The components are unsigned.
    void DebugPoolTypeList::GetSizeString(u32 luBytes, char* lpcBuffer)
    {
        std::sprintf(lpcBuffer, "%uMB %uKB %uB", luBytes >> 20,
                     (luBytes >> 10) & 0x3FFu, luBytes & 0x3FFu);
    }

    // ARTIST828E43E8. Raw uses all three descriptor lanes; detailed mode
    // uses the original {0,1,INVALID} column mapping on X360.
    void DebugPoolTypeList::RenderRaw(CgsDev::Debug2DImmediateRender* lpRender)
    {
        static const f32 KF_NAME_X = 0.0f;
        static const f32 KF_MAIN_X = KF_NAME_X + 250.0f;
        static const f32 KF_MAIN_PERCENT_X = KF_MAIN_X + 110.0f;
        static const f32 KF_SECOND_X = KF_MAIN_PERCENT_X + 50.0f;
        static const f32 KF_SECOND_PERCENT_X = KF_SECOND_X + 110.0f;
        static const f32 KF_THIRD_X = KF_SECOND_PERCENT_X + 50.0f;
        static const f32 KF_THIRD_PERCENT_X = KF_THIRD_X + 110.0f;
        static const f32 KF_WINDOW_WIDTH = KF_THIRD_PERCENT_X + 60.0f;
        const f32 lafSizeX[3] = { KF_MAIN_X, KF_SECOND_X, KF_THIRD_X };
        const f32 lafPercentX[3] = { KF_MAIN_PERCENT_X, KF_SECOND_PERCENT_X, KF_THIRD_PERCENT_X };
        const s32 laiMemTypes[3] = { 0, 1, 2 };
        u32 lauTotals[3] = { 0, 0, 0 };
        const f32 lfX = GetX();
        f32 lfY = GetY() + 5.0f;
        lpRender->DrawText("NAME", KF_NAME_X + lfX, lfY, 14.0f, 0xFF000000u);
        lpRender->DrawText("MAIN MEM", KF_MAIN_X + lfX, lfY, 14.0f, 0xFF000000u);
        lpRender->DrawText("GRSYS(PS3/X360)", KF_SECOND_X + lfX, lfY, 14.0f, 0xFF000000u);
        lpRender->DrawText("GRLOCAL(PS3 ONLY)", KF_THIRD_X + lfX, lfY, 14.0f, 0xFF000000u);
        lfY += 15.0f;
        const f32 lfHeaderLineY = lfY;
        lfY += 4.0f;
        char lacText[512];
        char lacSize[128];
        for (s32 liType = 0; liType < miNumTypes; ++liType)
        {
            const TypeEntry& lrEntry = maTypes[liType];
            CgsCore::SPrintf(lacText, sizeof(lacText), "%s(%d)", lrEntry.mpcTypeName, lrEntry.miCount);
            lpRender->DrawText(lacText, KF_NAME_X + lfX, lfY, 14.0f, 0xFF000000u);
            for (s32 liColumn = 0; liColumn < 3; ++liColumn)
            {
                const s32 liMemType = laiMemTypes[liColumn];
                if (liMemType != E_MEMCAT_INVALID)
                {
                    const u32 luCapacity = mPoolDescriptor.m_baseResourceDescriptors[liMemType].m_size;
                    const u32 luBytes = lrEntry.mSize.m_baseResourceDescriptors[liMemType].m_size;
                    if (luCapacity != 0)
                    {
                        GetSizeString(luBytes, lacSize);
                        CgsCore::SPrintf(lacText, sizeof(lacText), "%s", lacSize);
                        lpRender->DrawText(lacText, lafSizeX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
                        // ARTIST converts unsigned words to f64, performs the
                        // multiply/divide in f64, then rounds once to f32.
                        const f32 lfPercent = static_cast<f32>((static_cast<f64>(luBytes) * 100.0) /
                                                               static_cast<f64>(luCapacity));
                        CgsCore::SPrintf(lacText, sizeof(lacText), "%.2f%%", lfPercent);
                        lpRender->DrawText(lacText, lafPercentX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
                        lauTotals[liMemType] += luBytes;
                    }
                }
            }
            lfY += 15.0f;
        }
        const f32 lfTotalLineY = lfY;
        lfY += 4.0f;
        lpRender->DrawText("TOTAL", KF_NAME_X + lfX, lfY, 14.0f, 0xFF000000u);
        for (s32 liColumn = 0; liColumn < 3; ++liColumn)
        {
            const s32 liMemType = laiMemTypes[liColumn];
            if (liMemType == E_MEMCAT_INVALID ||
                mPoolDescriptor.m_baseResourceDescriptors[liMemType].m_size == 0)
            {
                lpRender->DrawText("0MB", lafSizeX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
                lpRender->DrawText("0%", lafPercentX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
            }
            else
            {
                const u32 luBytes = lauTotals[liMemType];
                const u32 luCapacity = mPoolDescriptor.m_baseResourceDescriptors[liMemType].m_size;
                GetSizeString(luBytes, lacSize);
                CgsCore::SPrintf(lacText, sizeof(lacText), "%s", lacSize);
                lpRender->DrawText(lacText, lafSizeX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
                const f32 lfPercent = static_cast<f32>((static_cast<f64>(luBytes) * 100.0) /
                                                       static_cast<f64>(luCapacity));
                CgsCore::SPrintf(lacText, sizeof(lacText), "%.2f%%", lfPercent);
                lpRender->DrawText(lacText, lafPercentX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
            }
        }
        lfY += 20.0f;
        const Vector2 lvHeaderStart = { KF_NAME_X + lfX, lfHeaderLineY, 0.0f, 0.0f };
        const Vector2 lvHeaderEnd = { (KF_WINDOW_WIDTH + lfX) - 4.0f, lfHeaderLineY, 0.0f, 0.0f };
        lpRender->DrawLine(lvHeaderStart, lvHeaderEnd, 0xFF000000u);
        const Vector2 lvTotalStart = { KF_NAME_X + lfX, lfTotalLineY, 0.0f, 0.0f };
        const Vector2 lvTotalEnd = { (KF_WINDOW_WIDTH + lfX) - 4.0f, lfTotalLineY, 0.0f, 0.0f };
        lpRender->DrawLine(lvTotalStart, lvTotalEnd, 0xFF000000u);
        for (s32 liColumn = 0; liColumn < 3; ++liColumn)
        {
            const f32 lfColumnX = (lafSizeX[liColumn] + lfX) - 3.0f;
            const Vector2 lvStart = { lfColumnX, GetY(), 0.0f, 0.0f };
            const Vector2 lvEnd = { lfColumnX, lfY, 0.0f, 0.0f };
            lpRender->DrawLine(lvStart, lvEnd, 0xFF000000u);
        }
        SetSize(KF_WINDOW_WIDTH, lfY - GetY());
    }

    // ARTIST828E4AF8. Raw uses all three descriptor lanes; detailed mode
    // uses the original {0,1,INVALID} column mapping on X360.
    void DebugPoolTypeList::RenderProgrammer(CgsDev::Debug2DImmediateRender* lpRender)
    {
        static const f32 KF_NAME_X = 0.0f;
        static const f32 KF_MAIN_X = KF_NAME_X + 250.0f;
        static const f32 KF_MAIN_PERCENT_X = KF_MAIN_X + 110.0f;
        static const f32 KF_SECOND_X = KF_MAIN_PERCENT_X + 50.0f;
        static const f32 KF_SECOND_PERCENT_X = KF_SECOND_X + 110.0f;
        static const f32 KF_THIRD_X = KF_SECOND_PERCENT_X + 50.0f;
        static const f32 KF_THIRD_PERCENT_X = KF_THIRD_X + 110.0f;
        static const f32 KF_WINDOW_WIDTH = KF_THIRD_PERCENT_X + 60.0f;
        const f32 lafSizeX[3] = { KF_MAIN_X, KF_SECOND_X, KF_THIRD_X };
        const f32 lafPercentX[3] = { KF_MAIN_PERCENT_X, KF_SECOND_PERCENT_X, KF_THIRD_PERCENT_X };
        const s32 laiMemTypes[3] = { 0, 1, 3 };
        u32 lauTotals[3] = { 0, 0, 0 };
        const f32 lfX = GetX();
        f32 lfY = GetY() + 5.0f;
        lpRender->DrawText("NAME", KF_NAME_X + lfX, lfY, 14.0f, 0xFF000000u);
        lpRender->DrawText("MAIN MEM", KF_MAIN_X + lfX, lfY, 14.0f, 0xFF000000u);
        lpRender->DrawText("VIDEO", KF_SECOND_X + lfX, lfY, 14.0f, 0xFF000000u);
        lpRender->DrawText("GRSYS(PS3 ONLY)", KF_THIRD_X + lfX, lfY, 14.0f, 0xFF000000u);
        lfY += 15.0f;
        const f32 lfHeaderLineY = lfY;
        lfY += 4.0f;
        char lacText[512];
        char lacSize[128];
        for (s32 liType = 0; liType < miNumTypes; ++liType)
        {
            const TypeEntry& lrEntry = maTypes[liType];
            CgsCore::SPrintf(lacText, sizeof(lacText), "%s(%d)", lrEntry.mpcTypeName, lrEntry.miCount);
            lpRender->DrawText(lacText, KF_NAME_X + lfX, lfY, 14.0f, 0xFF000000u);
            for (s32 liColumn = 0; liColumn < 3; ++liColumn)
            {
                const s32 liMemType = laiMemTypes[liColumn];
                if (liMemType != E_MEMCAT_INVALID)
                {
                    const u32 luCapacity = mPoolDescriptor.m_baseResourceDescriptors[liMemType].m_size;
                    const u32 luBytes = lrEntry.mSize.m_baseResourceDescriptors[liMemType].m_size;
                    if (luCapacity != 0)
                    {
                        GetSizeString(luBytes, lacSize);
                        CgsCore::SPrintf(lacText, sizeof(lacText), "%s", lacSize);
                        lpRender->DrawText(lacText, lafSizeX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
                        // ARTIST converts unsigned words to f64, performs the
                        // multiply/divide in f64, then rounds once to f32.
                        const f32 lfPercent = static_cast<f32>((static_cast<f64>(luBytes) * 100.0) /
                                                               static_cast<f64>(luCapacity));
                        CgsCore::SPrintf(lacText, sizeof(lacText), "%.2f%%", lfPercent);
                        lpRender->DrawText(lacText, lafPercentX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
                        lauTotals[liMemType] += luBytes;
                    }
                }
            }
            lfY += 15.0f;
        }
        const f32 lfTotalLineY = lfY;
        lfY += 4.0f;
        lpRender->DrawText("TOTAL", KF_NAME_X + lfX, lfY, 14.0f, 0xFF000000u);
        for (s32 liColumn = 0; liColumn < 3; ++liColumn)
        {
            const s32 liMemType = laiMemTypes[liColumn];
            if (liMemType == E_MEMCAT_INVALID ||
                mPoolDescriptor.m_baseResourceDescriptors[liMemType].m_size == 0)
            {
                lpRender->DrawText("0MB", lafSizeX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
                lpRender->DrawText("0%", lafPercentX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
            }
            else
            {
                const u32 luBytes = lauTotals[liMemType];
                const u32 luCapacity = mPoolDescriptor.m_baseResourceDescriptors[liMemType].m_size;
                GetSizeString(luBytes, lacSize);
                CgsCore::SPrintf(lacText, sizeof(lacText), "%s", lacSize);
                lpRender->DrawText(lacText, lafSizeX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
                const f32 lfPercent = static_cast<f32>((static_cast<f64>(luBytes) * 100.0) /
                                                       static_cast<f64>(luCapacity));
                CgsCore::SPrintf(lacText, sizeof(lacText), "%.2f%%", lfPercent);
                lpRender->DrawText(lacText, lafPercentX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
            }
        }
        lfY += 20.0f;
        const Vector2 lvHeaderStart = { KF_NAME_X + lfX, lfHeaderLineY, 0.0f, 0.0f };
        const Vector2 lvHeaderEnd = { (KF_WINDOW_WIDTH + lfX) - 4.0f, lfHeaderLineY, 0.0f, 0.0f };
        lpRender->DrawLine(lvHeaderStart, lvHeaderEnd, 0xFF000000u);
        const Vector2 lvTotalStart = { KF_NAME_X + lfX, lfTotalLineY, 0.0f, 0.0f };
        const Vector2 lvTotalEnd = { (KF_WINDOW_WIDTH + lfX) - 4.0f, lfTotalLineY, 0.0f, 0.0f };
        lpRender->DrawLine(lvTotalStart, lvTotalEnd, 0xFF000000u);
        for (s32 liColumn = 0; liColumn < 3; ++liColumn)
        {
            const f32 lfColumnX = (lafSizeX[liColumn] + lfX) - 3.0f;
            const Vector2 lvStart = { lfColumnX, GetY(), 0.0f, 0.0f };
            const Vector2 lvEnd = { lfColumnX, lfY, 0.0f, 0.0f };
            lpRender->DrawLine(lvStart, lvEnd, 0xFF000000u);
        }
        SetSize(KF_WINDOW_WIDTH, lfY - GetY());
    }

    // ARTIST828E5280 and raw category-name table82F340CC..82F340E4.
    void DebugPoolTypeList::RenderArtist(CgsDev::Debug2DImmediateRender* lpRender)
    {
        static const f32 KF_NAME_X = 0.0f;
        static const f32 KF_MAIN_X = KF_NAME_X + 250.0f;
        static const f32 KF_MAIN_PERCENT_X = KF_MAIN_X + 110.0f;
        static const f32 KF_SECOND_X = KF_MAIN_PERCENT_X + 50.0f;
        static const f32 KF_SECOND_PERCENT_X = KF_SECOND_X + 110.0f;
        static const f32 KF_THIRD_X = KF_SECOND_PERCENT_X + 50.0f;
        static const f32 KF_THIRD_PERCENT_X = KF_THIRD_X + 110.0f;
        static const f32 KF_WINDOW_WIDTH = KF_THIRD_PERCENT_X + 60.0f;
        static const char* const KAPC_CATEGORY_NAMES[E_DEBUGRESOURCECATEGORY_COUNT] =
        {
            "Overhead", "Materials", "Mesh", "Texture", "Misc", "Debug",
        };
        const f32 lafSizeX[3] = { KF_MAIN_X, KF_SECOND_X, KF_THIRD_X };
        const f32 lafPercentX[3] = { KF_MAIN_PERCENT_X, KF_SECOND_PERCENT_X, KF_THIRD_PERCENT_X };
        const s32 laiMemTypes[3] = { 0, 1, E_MEMCAT_INVALID };
        u32 lauTotals[3] = { 0, 0, 0 };
        const f32 lfX = GetX();
        f32 lfY = GetY() + 5.0f;
        lpRender->DrawText("NAME", KF_NAME_X + lfX, lfY, 14.0f, 0xFF000000u);
        lpRender->DrawText("MAIN MEM", KF_MAIN_X + lfX, lfY, 14.0f, 0xFF000000u);
        lpRender->DrawText("VIDEO", KF_SECOND_X + lfX, lfY, 14.0f, 0xFF000000u);
        lpRender->DrawText("GRSYS(PS3 ONLY)", KF_THIRD_X + lfX, lfY, 14.0f, 0xFF000000u);
        lfY += 15.0f;
        const f32 lfHeaderLineY = lfY;
        lfY += 4.0f;
        char lacText[512];
        char lacSize[128];
        for (s32 liCategory = 0; liCategory < E_DEBUGRESOURCECATEGORY_COUNT; ++liCategory)
        {
            CgsCore::SPrintf(lacText, sizeof(lacText), "%s", KAPC_CATEGORY_NAMES[liCategory]);
            lpRender->DrawText(lacText, KF_NAME_X + lfX, lfY, 14.0f, 0xFF000000u);
            for (s32 liColumn = 0; liColumn < 3; ++liColumn)
            {
                const s32 liMemType = laiMemTypes[liColumn];
                if (liMemType != E_MEMCAT_INVALID)
                {
                    const u32 luCapacity = mPoolDescriptor.m_baseResourceDescriptors[liMemType].m_size;
                    const u32 luBytes = maCategories[liCategory].mSize.m_baseResourceDescriptors[liMemType].m_size;
                    if (luCapacity != 0)
                    {
                        GetSizeString(luBytes, lacSize);
                        CgsCore::SPrintf(lacText, sizeof(lacText), "%s", lacSize);
                        lpRender->DrawText(lacText, lafSizeX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
                        const f32 lfPercent = static_cast<f32>((static_cast<f64>(luBytes) * 100.0) /
                                                               static_cast<f64>(luCapacity));
                        CgsCore::SPrintf(lacText, sizeof(lacText), "%.2f%%", lfPercent);
                        lpRender->DrawText(lacText, lafPercentX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
                        const Vector2 lvMin = { lafSizeX[liColumn] + GetX(), lfY + 15.0f, 0.0f, 0.0f };
                        const Vector2 lvMax = { (lafSizeX[liColumn] + GetX()) + 150.0f, lfY + 25.0f, 0.0f, 0.0f };
                        lpRender->DrawHorizontalBar(lvMin, lvMax, lfPercent, 100.0f, 0x8000B400u, 0x80640064u);
                        lauTotals[liMemType] += luBytes;
                    }
                }
            }
            lfY += 30.0f;
        }
        const f32 lfTotalLineY = lfY;
        lfY += 4.0f;
        lpRender->DrawText("TOTAL", KF_NAME_X + lfX, lfY, 14.0f, 0xFF000000u);
        for (s32 liColumn = 0; liColumn < 3; ++liColumn)
        {
            const s32 liMemType = laiMemTypes[liColumn];
            if (liMemType == E_MEMCAT_INVALID ||
                mPoolDescriptor.m_baseResourceDescriptors[liMemType].m_size == 0)
            {
                lpRender->DrawText("0MB", lafSizeX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
                lpRender->DrawText("0%", lafPercentX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
            }
            else
            {
                const u32 luBytes = lauTotals[liMemType];
                const u32 luCapacity = mPoolDescriptor.m_baseResourceDescriptors[liMemType].m_size;
                GetSizeString(luBytes, lacSize);
                CgsCore::SPrintf(lacText, sizeof(lacText), "%s", lacSize);
                lpRender->DrawText(lacText, lafSizeX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
                const f32 lfPercent = static_cast<f32>((static_cast<f64>(luBytes) * 100.0) /
                                                       static_cast<f64>(luCapacity));
                CgsCore::SPrintf(lacText, sizeof(lacText), "%.2f%%", lfPercent);
                lpRender->DrawText(lacText, lafPercentX[liColumn] + lfX, lfY, 14.0f, 0xFF000000u);
                const Vector2 lvMin = { lafSizeX[liColumn] + GetX(), lfY + 15.0f, 0.0f, 0.0f };
                const Vector2 lvMax = { (lafSizeX[liColumn] + GetX()) + 150.0f, lfY + 25.0f, 0.0f, 0.0f };
                lpRender->DrawHorizontalBar(lvMin, lvMax, lfPercent, 100.0f, 0x8000B400u, 0x80640064u);
            }
        }
        lfY += 35.0f;
        const Vector2 lvHeaderStart = { KF_NAME_X + lfX, lfHeaderLineY, 0.0f, 0.0f };
        const Vector2 lvHeaderEnd = { (KF_WINDOW_WIDTH + lfX) - 4.0f, lfHeaderLineY, 0.0f, 0.0f };
        lpRender->DrawLine(lvHeaderStart, lvHeaderEnd, 0xFF000000u);
        const Vector2 lvTotalStart = { KF_NAME_X + lfX, lfTotalLineY, 0.0f, 0.0f };
        const Vector2 lvTotalEnd = { (KF_WINDOW_WIDTH + lfX) - 4.0f, lfTotalLineY, 0.0f, 0.0f };
        lpRender->DrawLine(lvTotalStart, lvTotalEnd, 0xFF000000u);
        for (s32 liColumn = 0; liColumn < 3; ++liColumn)
        {
            const f32 lfColumnX = (lafSizeX[liColumn] + lfX) - 3.0f;
            const Vector2 lvStart = { lfColumnX, GetY(), 0.0f, 0.0f };
            const Vector2 lvEnd = { lfColumnX, lfY, 0.0f, 0.0f };
            lpRender->DrawLine(lvStart, lvEnd, 0xFF000000u);
        }
        SetSize(KF_WINDOW_WIDTH, lfY - GetY());
    }
}
