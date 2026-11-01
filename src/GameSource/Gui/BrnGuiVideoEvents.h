#pragma once

#include "GameSource/Gui/BrnGuiMovieManager.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

namespace BrnGui
{
    enum { KI_GUIEVENT_AUDIO_READY = 504, KI_GUIEVENT_PLAY_VIDEO = 508, KI_GUIEVENT_STOP_VIDEO = 509 };

    // ARTIST 82476C68/82476CF8 copy the 48-byte definition as the whole payload.
    class GuiEventPlayVideo : public CgsModule::Event
    {
    public:
        MovieManager::VideoDefinition mVideo;
        s32 GetEventType() const { return KI_GUIEVENT_PLAY_VIDEO; }
    };
    class GuiEventStopVideo : public CgsModule::Event
    {
    public:
        MovieManager::VideoDefinition mVideo;
        s32 GetEventType() const { return KI_GUIEVENT_STOP_VIDEO; }
    };
    CGS_STATIC_ASSERT((sizeof(GuiEventPlayVideo) == 0x30), "native play video payload");
    CGS_STATIC_ASSERT((sizeof(GuiEventStopVideo) == 0x30), "native stop video payload");
}
