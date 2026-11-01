#pragma once

#include <cstddef>
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameSource/BurnoutConstants.h"
#include "GameShared/GameClasses/Core/CgsID.h"

namespace CgsGui
{
    // ARTIST823D08E8 queues the 24-byte payload; its first word is the road ID.
    template <>
    struct GuiEvent<325> : public CgsModule::Event
    {
        s32 GetEventType() const { return 325; }
    };
}

namespace BrnGui
{
    // ARTIST82474B20/82474D10/82475CD0 and the game/network producers agree on
    // these fields. Names follow the producer roles; the final three bytes are
    // natural tail padding and remain unwritten.
    struct GuiMugshotControlEvent : public CgsGui::GuiEvent<325>
    {
        CgsID mRoadId;
        BrnGameState::GameStateModuleIO::EMugshotResponse meResponse;
        BrnGameState::GameStateModuleIO::EImageType meImageType;
        EActiveRaceCarIndex meOpponentARCI;
        bool mbLocalAggressor;
    };

    CGS_STATIC_ASSERT((sizeof(GuiMugshotControlEvent) == 24), "ARTIST mugshot payload size");
    CGS_STATIC_ASSERT((offsetof(GuiMugshotControlEvent, meResponse) == 8), "ARTIST mugshot response");
    CGS_STATIC_ASSERT((offsetof(GuiMugshotControlEvent, meImageType) == 12), "ARTIST mugshot image type");
    CGS_STATIC_ASSERT((offsetof(GuiMugshotControlEvent, meOpponentARCI) == 16), "ARTIST mugshot opponent");
    CGS_STATIC_ASSERT((offsetof(GuiMugshotControlEvent, mbLocalAggressor) == 20), "ARTIST mugshot aggressor");
}
