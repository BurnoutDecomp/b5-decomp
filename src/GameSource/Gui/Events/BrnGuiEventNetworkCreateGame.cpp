// ===================================================================================
// BrnGui::GuiEventNetworkCreateGame  -- implementation
//   class:BrnGui::GuiEventNetworkCreateGame
//
//   Construct          @0x82481F50
//   SetFromGameParams  @0x82481FC8
//
// Reconstructed store-for-store from the X360 pseudocode/asm. Member access is by name;
// the per-event records reuse the committed mode-event Event type by name.
// ===================================================================================
#include "GameSource/Gui/Events/BrnGuiEventNetworkCreateGame.h"

namespace BrnGui
{
    // @0x82481F50
    void GuiEventNetworkCreateGame::Construct()
    {
        // X360 record loop: anchor r3+0x24, stride 0x2C, 10 reps. Per record the X360 writes
        // trigger(+0x20) = -1, numLandmarks(+0x24) = 0, eventID(+0x28) = 0 and leaves the
        // landmark block (+0x00..+0x1F) untouched. Constructing each Event with id 0,
        // trigger 0xFFFFFFFF and zero landmarks reproduces exactly those three tail stores
        // (the landmark copy loop is empty for numLandmarks == 0).
        for (s32 li = 0; li < KI_NUM_EVENTS; ++li)
        {
            maEvents[li].Construct(0, 0xFFFFFFFFu, 0, 0);
        }

        // Scalar option / count block (X360 r3+0x1B8 .. r3+0x1DF).
        miCountA     = 0;   // 0x1C0
        miCountB     = 0;   // 0x1C4
        miCountC     = 0;   // 0x1C8
        miCountD     = 0;   // 0x1CC
        mbFlagK      = 1;   // 0x1DF
        miMaxPlayers = 10;  // 0x1B8
        miGameMode   = 18;  // 0x1BC
        miOptionE    = 1;   // 0x1D0
        miOptionF    = 9;   // 0x1D4
        miOptionG    = 3;   // 0x1D8
        mbFlagH      = 1;   // 0x1DC
        mbFlagI      = 1;   // 0x1DD
        mbFlagJ      = 1;   // 0x1DE
    }

    // @0x82481FC8
    void GuiEventNetworkCreateGame::SetFromGameParams(const GuiEventNetworkCreateGame& lrSource)
    {
        // Rebuild each of the ten Events from lrSource: gather that source event's landmark
        // indices into a temp buffer (X360 loops GetNumLandmarks() halfwords through the
        // landmark accessor into the stack buffer v13) then Event::Construct(dest) with the
        // source event's id, trigger id, the gathered landmarks and the landmark count.
        for (s32 li = 0; li < KI_NUM_EVENTS; ++li)
        {
            const Event& lrSrcEvent = lrSource.maEvents[li];

            const s32 liNumLandmarks = lrSrcEvent.GetNumLandmarks();

            BrnGameState::LandmarkIndex laLandmarks[BrnGameState::GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE];
            for (s32 liIndex = 0; liIndex < liNumLandmarks; ++liIndex)
            {
                laLandmarks[liIndex] = lrSrcEvent.GetLandmark(liIndex);
            }

            maEvents[li].Construct(lrSrcEvent.GetEventID(),
                                   lrSrcEvent.GetTrafficLightTriggerId(),
                                   laLandmarks,
                                   liNumLandmarks);
        }

        // Copy the scalar option / count block verbatim (X360 r3+0x1B8 .. r3+0x1DF).
        mbFlagK      = lrSource.mbFlagK;       // 0x1DF
        miMaxPlayers = lrSource.miMaxPlayers;  // 0x1B8
        miGameMode   = lrSource.miGameMode;    // 0x1BC
        miCountA     = lrSource.miCountA;      // 0x1C0
        miOptionE    = lrSource.miOptionE;     // 0x1D0
        miOptionF    = lrSource.miOptionF;     // 0x1D4
        miOptionG    = lrSource.miOptionG;     // 0x1D8
        miCountB     = lrSource.miCountB;      // 0x1C4
        miCountC     = lrSource.miCountC;      // 0x1C8
        mbFlagH      = lrSource.mbFlagH;       // 0x1DC
        mbFlagI      = lrSource.mbFlagI;       // 0x1DD
        mbFlagJ      = lrSource.mbFlagJ;       // 0x1DE
        miCountD     = lrSource.miCountD;      // 0x1CC
    }
}
