#include "GameShared/GameClasses/System/Resource/CgsResourceDebugPoolTypeList.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePoolModule.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h"
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
}
