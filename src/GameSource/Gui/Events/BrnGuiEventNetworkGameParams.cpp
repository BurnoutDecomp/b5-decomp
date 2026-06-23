// ===================================================================================
// BrnGui::GuiEventNetworkGameParams  -- implementation
//   class:BrnGui::GuiEventNetworkGameParams
//
// Construct @ 0x824820C8
//   Default-initialises the option-record array (10 records, each: selected = -1,
//   value = 0, flags = 0) then seeds the scalar option / count block with the default
//   online-match parameters. Reconstructed store-for-store from the X360 pseudocode/asm.
// ===================================================================================
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"

namespace BrnGui
{
    // @ 0x824820C8
    void GuiEventNetworkGameParams::Construct()
    {
        // X360 loop: anchor r3+0x24, stride 0x2C, 10 reps. Each record: the X360 writes
        // value(+4)=0, selected(-4)=-1, flags(+0)=0 relative to the walking anchor.
        for (s32 li = 0; li < KI_NUM_OPTION_RECORDS; ++li)
        {
            maOptionRecords[li].miValue    = 0;
            maOptionRecords[li].miSelected = -1;
            maOptionRecords[li].miFlags    = 0;
        }

        // Scalar option / count block (X360 r3+0x1B8 .. r3+0x1DF).
        miCountD     = 0;   // 0x1CC
        miCountA     = 0;   // 0x1C0
        miCountB     = 0;   // 0x1C4
        miCountC     = 0;   // 0x1C8
        miMaxPlayers = 10;  // 0x1B8
        mbFlagK      = 1;   // 0x1DF
        miGameMode   = 18;  // 0x1BC
        miOptionE    = 1;   // 0x1D0
        miOptionF    = 9;   // 0x1D4
        miOptionG    = 3;   // 0x1D8
        mbFlagH      = 1;   // 0x1DC
        mbFlagI      = 1;   // 0x1DD
        mbFlagJ      = 1;   // 0x1DE
    }
}
