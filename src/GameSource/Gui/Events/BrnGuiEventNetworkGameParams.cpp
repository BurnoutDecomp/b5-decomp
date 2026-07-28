// ===================================================================================
// BrnGui::GuiEventNetworkGameParams  -- implementation
//   class:BrnGui::GuiEventNetworkGameParams
//
// Construct @ 0x824820C8
//   Reset each of the 10 round Events (the X360 loop is the inlined
//   Event::Construct(0, -1, 0, 0): trigger id -1, landmark count 0, event id 0 --
//   the constant-0 landmark copy and its bounds assert compile out), then seed the
//   scalar match-option tail with the default online-match parameters.
//   Reconstructed store-for-store from the X360 pseudocode/asm; the field
//   identification is the wave-H layout correction (see the header banner).
// ===================================================================================
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"

namespace BrnGui
{
    // @ 0x824820C8
    void GuiEventNetworkGameParams::Construct()
    {
        // X360 loop: anchor r3+0x24, stride 0x2C, 10 reps -- per record the stores hit
        // Event+0x20/+0x24/+0x28 == the inlined Event::Construct(0, u32(-1), 0, 0).
        for (s32 li = 0; li < KI_NUM_EVENTS; ++li)
            maEvents[li].Construct(0, static_cast<u32>(-1), 0, 0);

        // Scalar match-option tail (X360 r3+0x1B8 .. r3+0x1DF).
        miTimeLimit         = 0;     // 0x1CC
        meSecurity          = 0;     // 0x1C0 (E_GAME_SECURITY public)
        meBoostType         = 0;     // 0x1C4
        meVehicleChoice     = 0;     // 0x1C8
        meGameMode          = 10;    // 0x1B8 (GsmIO E_MODE_ONLINE_RACE)
        mbRanked            = true;  // 0x1DF
        mePreviousGameMode  = 18;    // 0x1BC (ARTIST E_MODE_COUNT sentinel; Dec-07 enum ends at 17)
        miNumRounds         = 1;     // 0x1D0
        miVehicleClass      = 9;     // 0x1D4
        miNumRunnerCrashes  = 3;     // 0x1D8
        mbInfiniteBoost     = true;  // 0x1DC
        mbTrafficOn         = true;  // 0x1DD
        mbTrafficCheckingOn = true;  // 0x1DE
    }
}
