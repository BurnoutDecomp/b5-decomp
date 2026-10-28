#pragma once

#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsWindow.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceBasePool.h"

namespace CgsResource
{
    const s32 KI_NUMHISTOGRAMCOUNTS = 15;
    extern const s32 KAI_HISTOGRAMCOUNTS[KI_NUMHISTOGRAMCOUNTS];

    class Pool;

    // ARTIST component+0x3F8..0x47F; mSaved's real descriptor
    // constructors account for the observed three {size=0,alignment=1} pairs.
    class DebugPoolHistogram : public CgsDev::DebugUI::Window
    {
    public:
        void Reset(const Pool* lpPool);
        void Render(CgsDev::Debug2DImmediateRender* lpRender) ;
        void Update(f32 lfTimeStep, CgsDev::DebugUI::InputEvent leEvent) ;

    private:
        void AddToCount(s32 liSize);
        void DrawBar(CgsDev::Debug2DImmediateRender* lpRender, char* lpcText,
                     s32 liCount, f32 lfY);

        const Pool* mpPool;
        s32 miMaxResources;
        s32 miLoadedResources;
        s32 mauValues[KI_NUMHISTOGRAMCOUNTS + 1];
        Entry::ResourceDescriptor mSaved;
    };
}
