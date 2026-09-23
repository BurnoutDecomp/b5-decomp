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

        // Scalar option block (+0x1B8 .. +0x1DF), in the console's store order.
        meSecurity          = 0;      // 0x1C0
        meBoostType         = 0;      // 0x1C4
        meVehicleChoice     = 0;      // 0x1C8
        miTimeLimit         = 0;      // 0x1CC
        mbRanked            = true;   // 0x1DF
        meGameMode          = 10;     // 0x1B8
        mePreviousGameMode  = 18;     // 0x1BC
        miNumRounds         = 1;      // 0x1D0
        miVehicleClass      = 9;      // 0x1D4
        miNumRunnerCrashes  = 3;      // 0x1D8
        mbInfiniteBoost     = true;   // 0x1DC
        mbTrafficOn         = true;   // 0x1DD
        mbTrafficCheckingOn = true;   // 0x1DE
    }

    // @0x82481FC8
    void GuiEventNetworkCreateGame::SetFromGameParams(const GuiEventNetworkGameParams* lpGameParams)
    {
        const GuiEventNetworkGameParams& lrSource = *lpGameParams;

        // Rebuild each of the ten Events from lrSource: gather that source event's landmark
        // indices into a temp buffer (X360 loops GetNumLandmarks() halfwords through the
        // landmark accessor into the stack buffer v13) then Event::Construct(dest) with the
        // source event's id, trigger id, the gathered landmarks and the landmark count.
        for (s32 li = 0; li < KI_NUM_EVENTS; ++li)
        {
            const GuiEventNetworkGameParams::Event& lrSrcEvent = lrSource.maEvents[li];

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

        // Copy the scalar option block verbatim (+0x1B8 .. +0x1DF), in the console's order.
        mbRanked            = lrSource.mbRanked;             // 0x1DF
        meGameMode          = lrSource.meGameMode;           // 0x1B8
        mePreviousGameMode  = lrSource.mePreviousGameMode;   // 0x1BC
        meSecurity          = lrSource.meSecurity;           // 0x1C0
        miNumRounds         = lrSource.miNumRounds;          // 0x1D0
        miVehicleClass      = lrSource.miVehicleClass;       // 0x1D4
        miNumRunnerCrashes  = lrSource.miNumRunnerCrashes;   // 0x1D8
        meBoostType         = lrSource.meBoostType;          // 0x1C4
        meVehicleChoice     = lrSource.meVehicleChoice;      // 0x1C8
        mbInfiniteBoost     = lrSource.mbInfiniteBoost;      // 0x1DC
        mbTrafficOn         = lrSource.mbTrafficOn;          // 0x1DD
        mbTrafficCheckingOn = lrSource.mbTrafficCheckingOn;  // 0x1DE
        miTimeLimit         = lrSource.miTimeLimit;          // 0x1CC
    }
}
