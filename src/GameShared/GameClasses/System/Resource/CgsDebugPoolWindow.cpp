#include "GameShared/GameClasses/System/Resource/CgsDebugPoolWindow.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

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
}
