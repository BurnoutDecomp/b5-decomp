#include "GameShared/GameClasses/System/Resource/CgsDebugPoolWindow.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"

#include <cmath>
#include <cstring>

namespace CgsResource
{
    namespace
    {
        // The two inlined CgsStringUtils.h:65 copies in ARTIST828D9958.
        template<size_t N> void CopyHeapName(char (&destination)[N], const char* source)
        {
            const s32 length = static_cast<s32>(std::strlen(source));
            if (length >= static_cast<s32>(N))
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::SimpleStrStream message;
                message << "String " << (source ? source : "<NULLSTRING>")
                        << " is too long. Buffer size = " << static_cast<s32>(N)
                        << ", string length = " << length << "\n";
                CgsDev::Assert::FireAssert(message.GetBuffer(),
                    "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h", 65);
                CgsDev::Assert::EndAssert();
            }
            std::strcpy(destination, source);
        }

        // ARTIST828E3B88..828E3BB8: round in f32, truncate to s64, take its
        // low byte. Preserve wrapping rather than clamping colour components.
        u32 ColourByte(f32 value)
        {
            const f32 rounded = std::fma(value, 255.0f, 0.5f);
            if (!std::isfinite(rounded) || rounded < -9223372036854775808.0 ||
                rounded >= 9223372036854775808.0)
                return 0; //PPC fctidz indefinite integer has a zero low byte
            return static_cast<u32>(static_cast<u64>(static_cast<s64>(rounded)) & 0xFFu);
        }

        // The scroll path uses fctidz then stfiwx: keep the low32 bits of
        // the64-bit result rather than converting directly to a signed word.
        u32 ScrollSegments(f32 width)
        {
            const f32 segments = width * 0.125f;
            if (!std::isfinite(segments) || segments < -9223372036854775808.0 ||
                segments >= 9223372036854775808.0)
                return 0; //out-of-range instruction/status details remain a hardware model boundary
            return static_cast<u32>(static_cast<u64>(static_cast<s64>(segments)));
        }
    }

    void DebugPoolWindow::Reset(const Pool* lpPool)
    {
        mpPool = lpPool;
        miNumHeaps = 0;
        miSelectedHeap = 0;
        CgsDev::DebugUI::Window::Prepare(100.0f, 100.0f, lpPool->GetName(), KX_FLAGNORMAL);
        AddHeap(&lpPool->maHeaps[0], "Main Mem", "MM");
        AddHeap(&lpPool->maHeaps[1], "Physical Mem", "PY");
        SetSize(800.0f, 150.0f);
    }

    // ARTIST828D9958. Both named heaps are retained even when they are empty.
    void DebugPoolWindow::AddHeap(const Heap* lpHeap, const char* lpcName, const char* lpcShortName)
    {
        if (miNumHeaps >= 6)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Too many heaps in pool window\n",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsDebugPoolWindow.cpp", 63);
            CgsDev::Assert::EndAssert();
        }
        HeapDisplayInfo& lrInfo = maHeaps[miNumHeaps++];
        lrInfo.muScroll = 0;
        lrInfo.mpHeap = const_cast<Heap*>(lpHeap); //DecFIGS declares the stored pointer non-const
        const u32 luTotalSize = lpHeap->GetTotalSizeBytes();
        lrInfo.muBytesPerSegment = luTotalSize < 0x2000000u
            ? (luTotalSize < 0x100000u ? 1024u : 0x8000u) : 0x100000u;
        CopyHeapName(lrInfo.macName, lpcName);
        CopyHeapName(lrInfo.macShortName, lpcShortName);
    }

    // ARTIST828D9D10. Its base call is the empty ICF body8284CB38.
    void DebugPoolWindow::Update(f32 /*lfTimeStep*/, CgsDev::DebugUI::InputEvent leEvent)
    {
        using namespace CgsDev::DebugUI;
        switch (leEvent)
        {
        case E_INPUTEVENT_SELECT:
        {
            u32& luSegment = maHeaps[miSelectedHeap].muBytesPerSegment;
            luSegment >>= 1;
            if (luSegment <= 1u) luSegment = 1u;
            break;
        }
        case E_INPUTEVENT_BACK:
        {
            u32& luSegment = maHeaps[miSelectedHeap].muBytesPerSegment;
            luSegment *= 2u;
            if (luSegment >= 0x20000000u) luSegment = 0x20000000u;
            break;
        }
        case E_INPUTEVENT_CURSORUP:
            --miSelectedHeap;
            if (miSelectedHeap <= 0) miSelectedHeap = 0;
            break;
        case E_INPUTEVENT_CURSORDOWN:
        {
            s32 liNext = miNumHeaps - 1;
            if (miSelectedHeap + 1 < liNext) liNext = miSelectedHeap + 1;
            miSelectedHeap = liNext;
            break;
        }
        case E_INPUTEVENT_CURSORLEFT:
        case E_INPUTEVENT_CURSORRIGHT:
        {
            HeapDisplayInfo& lrInfo = maHeaps[miSelectedHeap];
            const u32 luPage = ScrollSegments(GetWidth()) * lrInfo.muBytesPerSegment;
            const u32 luStep = luPage > lrInfo.muBytesPerSegment ? luPage : lrInfo.muBytesPerSegment;
            if (leEvent == E_INPUTEVENT_CURSORLEFT)
                lrInfo.muScroll = lrInfo.muScroll < luStep ? 0 : lrInfo.muScroll - luStep;
            else
                lrInfo.muScroll += luStep;
            break;
        }
        default:
            break;
        }
    }

    CgsDev::RGBA DebugPoolWindow::CalculateColour(f32 lfRatio)
    {
        f32 lfFirst, lfSecond, lfThird;
        if (lfRatio > 0.5f)
        {
            const f32 lfAngle = (lfRatio - 0.5f) * 3.1415927410125732421875f;
            lfSecond = static_cast<f32>(std::sin(static_cast<f64>(lfAngle)));
            lfThird = static_cast<f32>(std::cos(static_cast<f64>(lfAngle)));
            lfFirst = 0.0f;
        }
        else
        {
            const f32 lfAngle = lfRatio * 3.1415927410125732421875f;
            lfThird = static_cast<f32>(std::sin(static_cast<f64>(lfAngle)));
            lfFirst = static_cast<f32>(std::cos(static_cast<f64>(lfAngle)));
            lfSecond = 0.0f;
        }
        return 0xFF000000u | (ColourByte(lfFirst) << 16) |
               (ColourByte(lfSecond) << 8) | ColourByte(lfThird);
    }

    // DecFIGS CgsDebugPoolWindow.cpp:387; inlined throughout ARTIST828F7250.
    void DebugPoolWindow::AddTextLine(CgsDev::Debug2DImmediateRender* lpRender, char* lpcText,
                                      CgsDev::RGBA lColour)
    {
        lpRender->DrawText(lpcText, GetX() + 2.0f, mrTextDrawPos, 14.0f, lColour);
        mrTextDrawPos += 15.0f;
    }

    // ARTIST828F7250. The resource/selection lines precede the validity test;
    // only the selected heap statistics and all-heap map are conditional on it.
    void DebugPoolWindow::Render(CgsDev::Debug2DImmediateRender* lpRender)
    {
        CgsDev::DebugUI::Window::Render(lpRender);
        mrTextDrawPos = GetY();
        char lacText[1024];
        const s32 liFreeResources = mpPool->muNumFreeResources;
        const s32 liUsedResources = static_cast<s32>(mpPool->GetMaxResources()) - liFreeResources;
        CgsCore::SPrintf(lacText, sizeof(lacText), "Resources: %d used, %d free",
                        liUsedResources, liFreeResources);
        AddTextLine(lpRender, lacText, 0xFF000000u);
        CgsCore::SPrintf(lacText, sizeof(lacText), "DefragMode: SrcCopy, MM_LO");
        AddTextLine(lpRender, lacText, 0xFF000000u);
        mrTextDrawPos += 2.0f;
        CgsCore::SPrintf(lacText, sizeof(lacText), "Selected Heap: %s", maHeaps[miSelectedHeap].macName);
        AddTextLine(lpRender, lacText, 0xFF000000u);
        CgsCore::SPrintf(lacText, sizeof(lacText), "    Pos: %d, BytesPerPixel: %d",
                        static_cast<s32>(maHeaps[miSelectedHeap].muScroll),
                        static_cast<s32>(maHeaps[miSelectedHeap].muBytesPerSegment));
        AddTextLine(lpRender, lacText, 0xFF000000u);
        if (mpPool->IsValid())
        {
            Heap* lpHeap = maHeaps[miSelectedHeap].mpHeap;
            const u32 luTotal = lpHeap->GetTotalSizeBytes();
            const u32 luFree = lpHeap->GetAmountFreeBytes();
            const u32 luUsed = luTotal - luFree;
            const u32 luLargestFree = static_cast<u32>(lpHeap->GetLargestFreeBlock());
            const f32 lfFragmentation = lpHeap->GetFragmentationLevel();
            CgsCore::SPrintf(lacText, sizeof(lacText),
                            "    Size: %du (%.2fkb), Used: %ub (%.2fkb), Free: %du (%.2fkb)",
                            static_cast<s32>(luTotal), static_cast<f32>(luTotal) * 0.0009765625f,
                            luUsed, static_cast<f32>(luUsed) * 0.0009765625f,
                            static_cast<s32>(luFree), static_cast<f32>(luFree) * 0.0009765625f);
            AddTextLine(lpRender, lacText, 0xFF000000u);
            CgsCore::SPrintf(lacText, sizeof(lacText),
                            "    LargestBlock: %.2fkb, Fragmentation: %.2f",
                            static_cast<f32>(luLargestFree) * 0.0009765625f, lfFragmentation * 100.0f);
            AddTextLine(lpRender, lacText, 0xFF000000u);
            f32 lfHeapY = mrTextDrawPos + 4.0f;
            const f32 lfHeapHeight = ((GetHeight() + GetY()) - lfHeapY) /
                                      static_cast<f32>(miNumHeaps) - 2.0f;
            const f32 lfHeapStep = lfHeapHeight + 2.0f;
            for (s32 liHeap = 0; liHeap < miNumHeaps; ++liHeap)
            {
                RenderHeap(lpRender, &maHeaps[liHeap], GetX(), lfHeapY, GetWidth(), lfHeapHeight);
                lfHeapY = lfHeapStep + lfHeapY;
            }
        }
    }

    // ARTIST828EE540. Walk the original used-node list across each inclusive
    // byte interval; exhaustion aborts the map rather than synthesizing a column.
    void DebugPoolWindow::RenderHeap(CgsDev::Debug2DImmediateRender* lpRender,
                                     HeapDisplayInfo* lpInfo, f32 lfX, f32 lfY,
                                     f32 lfWidth, f32 lfHeight)
    {
        Heap* lpHeap = lpInfo->mpHeap;
        lpRender->DrawText(lpInfo->macShortName, lfX,
                           std::fma(lfHeight, 0.5f, lfY) - 4.0f, 14.0f, 0xFF000000u);
        const u32 luBytesPerSegment = lpInfo->muBytesPerSegment;
        // The original unsigned divides have zero-divisor traps. No extra
        // range, validity, or empty-list guards are placed ahead of them.
        const u32 luTotalSegments = lpHeap->GetTotalSizeBytes() / luBytesPerSegment;
        const f32 lfPixelWidth = lfWidth - 29.0f;
        u32 luPixelWord = 0;
        // ARTIST828EE5D4..E5E0: fctidz, low32 word, then logical shift by1.
        // The PPC indefinite result for non-finite/out-of-s64 values has low32=0.
        if (std::isfinite(lfPixelWidth) && lfPixelWidth >= -9223372036854775808.0 &&
            lfPixelWidth < 9223372036854775808.0)
        {
            luPixelWord = static_cast<u32>(static_cast<u64>(static_cast<s64>(lfPixelWidth)));
        }
        const u32 luColumns = luPixelWord >> 1;
        const f32 lfBarX = lfX + 27.0f;
        if (luTotalSegments != 0)
        {
            u32 luFirstSegment = lpInfo->muScroll / luBytesPerSegment;
            if (luFirstSegment >= luTotalSegments - 1u)
                luFirstSegment = luTotalSegments - 1u;
            Heap::HeapEntryNode* lpNode = lpHeap->mUsedNodes.GetCount() > 0
                ? lpHeap->mUsedNodes.GetHead() : nullptr;
            u32 luBackgroundColumns = luTotalSegments - luFirstSegment;
            if (luBackgroundColumns > luColumns)
                luBackgroundColumns = luColumns;
            const f32 lfBottom = lfY + lfHeight;
            const CgsDev::RGBA lBackground = lpInfo == &maHeaps[miSelectedHeap]
                ? 0xFF555555u : 0xFF222222u;
            lpRender->DrawBox(lfBarX, lfY,
                              static_cast<f32>(luBackgroundColumns << 1) + lfBarX, lfBottom, lBackground);
            u32 luPixelX = 0;
            for (u32 luColumn = 0; luColumn < luColumns; ++luColumn, luPixelX += 2u)
            {
                const u32 luColumnBytes = lpInfo->muBytesPerSegment;
                const u32 luOffset = (luColumn + luFirstSegment) * luColumnBytes;
                char* lpcStart = lpHeap->GetBaseAddress() + luOffset;
                char* lpcEnd = lpcStart + luColumnBytes - 1u;
                if (!lpNode)
                    return;
                while (!lpNode->GetData()->Contains(lpcStart))
                {
                    lpNode = lpHeap->mUsedNodes.GetNext(lpNode);
                    if (!lpNode)
                        return;
                }
                u32 luAllocated = 0;
                for (;;)
                {
                    const HeapEntry* lpEntry = lpNode->GetData();
                    if (lpEntry->IsAllocated())
                    {
                        const u32 luEntrySize = lpEntry->GetSize();
                        u32 luCovered = luEntrySize;
                        if (lpEntry->Contains(lpcStart))
                            luCovered -= static_cast<u32>(lpcStart - lpEntry->GetAddress());
                        if (lpEntry->Contains(lpcEnd))
                        {
                            luCovered -= luEntrySize;
                            luCovered += static_cast<u32>(lpcEnd - lpEntry->GetAddress()) + 1u;
                        }
                        luAllocated += luCovered;
                    }
                    if (lpEntry->Contains(lpcEnd))
                        break;
                    lpNode = lpHeap->mUsedNodes.GetNext(lpNode);
                    if (!lpNode)
                        return;
                }
                if (luAllocated != 0)
                {
                    const f32 lfRatio = static_cast<f32>(luAllocated) /
                                         static_cast<f32>(luColumnBytes);
                    const f32 lfColumnX = static_cast<f32>(luPixelX) + lfBarX;
                    const CgsDev::RGBA lColour = CalculateColour(lfRatio);
                    lpRender->DrawBox(lfColumnX, lfY + 1.0f, lfColumnX + 2.0f, lfBottom - 1.0f, lColour);
                }
            }
        }
    }
}
