#pragma once

#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsWindow.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceBasePool.h"

namespace CgsResource
{
    class Pool;
    class PoolModule;

    enum EPoolTypeListMode
    {
        E_POOLTYPELISTMODE_RAW = 0,
        E_POOLTYPELISTMODE_PROGRAMMER = 1,
        E_POOLTYPELISTMODE_ARTIST = 2,
        E_POOLTYPELISTMODE_COUNT = 3,
    };
    extern const char* KAPC_POOLTYPELISTMODENAMES[E_POOLTYPELISTMODE_COUNT];

    // DecFIGS declaration shape; ARTIST827DF8E8 constructs the descriptor
    // arrays at +0x3C and +0x4854, not a standalone accounting object.
    class DebugPoolTypeList : public CgsDev::DebugUI::Window
    {
    public:
        static const s32 KI_NUMTYPES = 512;
        DebugPoolTypeList();
        void Reset(const Pool* lpPool, const PoolModule* lpPoolModule, EPoolTypeListMode leMode);
        void Render(CgsDev::Debug2DImmediateRender* lpRender) override;
        void Update(f32 lfTimeStep, CgsDev::DebugUI::InputEvent leEvent) override;
        void SetMode(EPoolTypeListMode leMode);

    private:
        struct TypeEntry
        {
            u32 muTypeId;
            s32 miCount;
            const char* mpcTypeName;
            Entry::ResourceDescriptor mSize;
        };
        struct CategoryEntry
        {
            s32 miCount;
            Entry::ResourceDescriptor mSize;
        };

        void RenderRaw(CgsDev::Debug2DImmediateRender* lpRender);
        void RenderProgrammer(CgsDev::Debug2DImmediateRender* lpRender);
        void RenderArtist(CgsDev::Debug2DImmediateRender* lpRender);
        void AddReferenceToType(u32 luTypeId, const Entry::ResourceDescriptor& lrSize);
        void AddReferenceToCategory(EDebugResourceCategory leCategory, const Entry::ResourceDescriptor& lrSize);
        void GetSizeString(u32 luBytes, char* lpcBuffer);
        static s32 SortTypesByName(const void* lpA, const void* lpB);

        const Pool* mpPool;
        const PoolModule* mpPoolModule;
        s32 miMaxResources;
        TypeEntry maTypes[KI_NUMTYPES];
        s32 miNumTypes;
        Entry::ResourceDescriptor mPoolDescriptor;
        EPoolTypeListMode meMode;
        CategoryEntry maCategories[E_DEBUGRESOURCECATEGORY_COUNT + 1];
    };
}
