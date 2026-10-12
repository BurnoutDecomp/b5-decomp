#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"
#include <cmath>
#include <cstdio>

// Observers for the whole method bodies. Window layout/rendering is outside this fixture.
namespace CgsResource
{
    struct DebugPoolWindow
    {
        struct HeapDisplayInfo { u32 muBytesPerSegment = 0, muScroll = 0; };
        HeapDisplayInfo maHeaps[6];
        s32 miNumHeaps = 2, miSelectedHeap = 0;
        f32 width = 800.0f;
        f32 GetWidth() const { return width; }
        void Update(f32, CgsDev::DebugUI::InputEvent);
        CgsDev::RGBA CalculateColour(f32);
    };
}
#include "ResourceWindow.inc"
namespace
{
    int checks = 0, failures = 0;
    void Check(bool value, const char* label)
    {
        ++checks;
        if (!value) { ++failures; std::printf("FAIL %s\n", label); }
    }
}
int main()
{
    using namespace CgsDev::DebugUI;
    CgsResource::DebugPoolWindow window;
    auto& first = window.maHeaps[0];
    auto& second = window.maHeaps[1];
    first.muBytesPerSegment = 1024;
    second.muBytesPerSegment = 32768;
    window.Update(0, E_INPUTEVENT_SELECT);
    Check(first.muBytesPerSegment == 512, "Select halves selected segment");
    Check(second.muBytesPerSegment == 32768, "Select leaves other heap untouched");
    first.muBytesPerSegment = 1;
    window.Update(0, E_INPUTEVENT_SELECT);
    Check(first.muBytesPerSegment == 1, "Select has one-byte minimum");
    first.muBytesPerSegment = 0;
    window.Update(0, E_INPUTEVENT_SELECT);
    Check(first.muBytesPerSegment == 1, "Select recovers zero to original minimum");
    window.Update(0, E_INPUTEVENT_BACK);
    Check(first.muBytesPerSegment == 2, "Back doubles segment");
    first.muBytesPerSegment = 0x10000000u;
    window.Update(0, E_INPUTEVENT_BACK);
    Check(first.muBytesPerSegment == 0x20000000u, "Back reaches original cap");
    window.Update(0, E_INPUTEVENT_BACK);
    Check(first.muBytesPerSegment == 0x20000000u, "Back retains original cap");
    window.Update(0, E_INPUTEVENT_CURSORUP);
    Check(window.miSelectedHeap == 0, "Up clamps first heap");
    window.Update(0, E_INPUTEVENT_CURSORDOWN);
    Check(window.miSelectedHeap == 1, "Down selects second heap");
    window.Update(0, E_INPUTEVENT_CURSORDOWN);
    Check(window.miSelectedHeap == 1, "Down clamps last heap");
    window.Update(0, E_INPUTEVENT_CURSORUP);
    Check(window.miSelectedHeap == 0, "Up selects previous heap");
    first.muBytesPerSegment = 1024;
    window.Update(0, E_INPUTEVENT_CURSORRIGHT);
    Check(first.muScroll == 102400, "Right advances width-in-segments page");
    window.Update(0, E_INPUTEVENT_CURSORLEFT);
    Check(first.muScroll == 0, "Left reverses page");
    first.muScroll = 7;
    window.Update(0, E_INPUTEVENT_CURSORLEFT);
    Check(first.muScroll == 0, "Left saturates below page");
    window.width = 0;
    window.Update(0, E_INPUTEVENT_CURSORRIGHT);
    Check(first.muScroll == 1024, "Zero width still advances one segment");
    first.muScroll = 0xFFFFFF00u;
    window.Update(0, E_INPUTEVENT_CURSORRIGHT);
    Check(first.muScroll == 768, "Right scroll wraps at32 bits");
    window.width = 17179871232.0f; //fmuls => 2^31+256; fctidz/stfiwx keeps low32
    first.muScroll = 0;
    window.Update(0, E_INPUTEVENT_CURSORRIGHT);
    Check(first.muScroll == 262144, "Scroll uses low word of64-bit truncation");
    first.muScroll = 77;
    window.Update(0, E_INPUTEVENT_CLOSE);
    Check(first.muScroll == 77, "Close has no heap input action");
    Check(window.CalculateColour(0.0f) == 0xFFFF0000u, "Colour zero endpoint");
    Check(window.CalculateColour(0.25f) == 0xFFB400B4u, "Colour lower quadrant");
    Check(window.CalculateColour(0.5f) == 0xFF0000FFu, "Colour midpoint branch");
    Check(window.CalculateColour(0.75f) == 0xFF00B4B4u, "Colour upper quadrant");
    Check(window.CalculateColour(1.0f) == 0xFF00FF00u, "Colour one endpoint");
    window.miNumHeaps = 0;
    window.miSelectedHeap = 0;
    window.Update(0, E_INPUTEVENT_CURSORDOWN);
    Check(window.miSelectedHeap == -1, "Original zero-heap down result remains minus one");
    std::printf("ResourceWindow: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
