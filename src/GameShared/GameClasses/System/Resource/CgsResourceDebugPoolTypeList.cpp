#include "GameShared/GameClasses/System/Resource/CgsResourceDebugPoolTypeList.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"
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
}
