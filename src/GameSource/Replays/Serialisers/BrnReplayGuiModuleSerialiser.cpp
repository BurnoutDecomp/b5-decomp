#include "BrnReplayGuiModuleSerialiser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnReplays
{
    int GuiModuleSerialiser::Construct()
    {
        int liResult = BaseSerialiser::Construct(10, 0, 2048, 992, "GuiModule", 0);
        mbStaticLayoutReset = false;
        return liResult;
    }

    GuiModuleStaticLayout* GuiModuleSerialiser::GetStaticLayout()
    {
        CGS_ASSERT(GetStaticBufferSize() >= 992, "Static buffer size is too small\n");
        return reinterpret_cast<GuiModuleStaticLayout*>(GetStaticBuffer());
    }

    int GuiModuleSerialiser::Read()
    {
        BaseSerialiser::Lock();

        // X360 mode cascade (outer {4,5,6}, skipping {1,3,4,6,7}) reduces to
        // PLAYING-only: read the static layout in during playback.
        if (GetMode() == E_MODE_PLAYING)
        {
            GuiModuleStaticLayout* lpStaticLayout = GetStaticLayout();
            CGS_ASSERT(lpStaticLayout != nullptr, "lpStaticLayout");
            BaseSerialiser::Read(lpStaticLayout, 992);
        }

        return BaseSerialiser::Unlock();
    }

    int GuiModuleSerialiser::Write()
    {
        BaseSerialiser::Lock();

        // X360 mode gate reduces to the RECORDING family: write the static
        // layout out during recording (preparing / recording / stalled).
        if (GetMode() == E_MODE_RECORDING_PREPARING ||
            GetMode() == E_MODE_RECORDING ||
            GetMode() == E_MODE_RECORDING_STALLED)
        {
            // Reset the static layout exactly once, on first use.
            if (!mbStaticLayoutReset)
            {
                mbStaticLayoutReset = true;
                GetStaticLayout()->Reset();
            }

            // The X360 fetches the layout a second time after the reset branch.
            GuiModuleStaticLayout* lpStaticLayout = GetStaticLayout();
            CGS_ASSERT(lpStaticLayout != nullptr, "lpStaticLayout");
            BaseSerialiser::Write(lpStaticLayout, 992);
        }

        return BaseSerialiser::Unlock();
    }
}
