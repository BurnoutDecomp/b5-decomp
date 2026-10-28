#pragma once

#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsWindow.h"

namespace CgsResource
{
    class Pool;
    class Heap;

    // ARTIST component+0x1C..0x3F7; declarations from DecFIGS.
    class DebugPoolWindow : public CgsDev::DebugUI::Window
    {
    public:
        void Reset(const Pool* lpPool);
        void Render(CgsDev::Debug2DImmediateRender* lpRender) ;
        void Update(f32 lfTimeStep, CgsDev::DebugUI::InputEvent leEvent) ;

    private:
        struct HeapDisplayInfo
        {
            Heap* mpHeap;
            char macName[128];
            char macShortName[16];
            u32 muBytesPerSegment;
            u32 muScroll;
        };

        void RenderHeap(CgsDev::Debug2DImmediateRender* lpRender,
                        HeapDisplayInfo* lpInfo, f32 lfX, f32 lfY,
                        f32 lfWidth, f32 lfHeight);
        CgsDev::RGBA CalculateColour(f32 lfRatio);
        void AddHeap(const Heap* lpHeap, const char* lpcName, const char* lpcShortName);
        void AddTextLine(CgsDev::Debug2DImmediateRender* lpRender, char* lpcText,
                         CgsDev::RGBA lColour);

        const Pool* mpPool;
        HeapDisplayInfo maHeaps[6];
        s32 miNumHeaps;
        s32 miSelectedHeap;
        f32 mrTextDrawPos;
    };
}
