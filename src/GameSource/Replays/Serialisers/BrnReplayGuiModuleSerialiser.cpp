#include "BrnReplayGuiModuleSerialiser.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Replays/Serialisers/BrnReplayGuiModuleSerialiser.cpp"
namespace BrnReplays
{
    void GuiModuleSerialiser::Construct()
    {
        BaseSerialiser::Construct(static_cast<ESerialiserId>(10), E_CONTEXT_NORMAL,
                                 2048, 992, "GuiModule", false);
        mbStaticLayoutReset = false;
    }

    GuiModuleStaticLayout* GuiModuleSerialiser::GetStaticLayout()
    {
        CGS_ASSERT_STREAM_AT(GetStaticBufferSize() >= 992, "Static buffer size is too small\n", "..\\..\\..\\GameSource\\Replays/Serialisers/BrnReplayGuiModuleSerialiser.h", 155);
        return reinterpret_cast<GuiModuleStaticLayout*>(GetStaticBuffer());
    }

    // Read / Write return Unlock's bool unconverted (no clrlwi before the epilogue,
    // 0x82650828 / 0x8265072C): their return type is bool.
    bool GuiModuleSerialiser::Read()
    {
        BaseSerialiser::Lock();

        // ARTIST tests the inlined DWARF predicates in turn (0x82650764..0x826507DC).
        if (IsPlaying() && !IsStalled() && !IsPreparing() && !IsRestoring())
        {
            GuiModuleStaticLayout* lpStaticLayout = GetStaticLayout();
            CGS_ASSERT_AT(lpStaticLayout != 0, "lpStaticLayout", CGS_ARTIST_FILE, 199);
            BaseSerialiser::Read(lpStaticLayout, 992);
        }

        return BaseSerialiser::Unlock();
    }

    bool GuiModuleSerialiser::Write()
    {
        BaseSerialiser::Lock();

        // X360 mode gate reduces to the RECORDING family: write the static
        // layout out during recording (preparing / recording / stalled).
        if (IsRecording())
        {
            // Reset the static layout exactly once, on first use.
            if (!mbStaticLayoutReset)
            {
                mbStaticLayoutReset = true;
                GetStaticLayout()->Reset();
            }

            // The X360 fetches the layout a second time after the reset branch.
            GuiModuleStaticLayout* lpStaticLayout = GetStaticLayout();
            CGS_ASSERT_AT(lpStaticLayout != 0, "lpStaticLayout", CGS_ARTIST_FILE, 166);
            BaseSerialiser::Write(lpStaticLayout, 992);
        }

        return BaseSerialiser::Unlock();
    }
}

#undef CGS_ARTIST_FILE
