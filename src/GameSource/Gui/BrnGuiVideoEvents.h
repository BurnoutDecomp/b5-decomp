#ifndef BRN_GUI_VIDEO_EVENTS_H
#define BRN_GUI_VIDEO_EVENTS_H

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include <cstddef>

namespace BrnGui
{
    enum
    {
        KI_GUIEVENT_AUDIO_READY = 504,
        KI_GUIEVENT_PLAY_VIDEO = 508,
        KI_GUIEVENT_STOP_VIDEO = 509,
    };

    // ARTIST82476C68: the event is a raw VideoDefinition, not a GUI wrapper.
    // The vector's alignment rounds the 39 meaningful bytes to 48.
    struct alignas(16) GuiEventPlayVideo : public CgsModule::Event
    {
        f32 mafRectangle[4];
        u64 muVideoResourceId;
        u32 muSoundStreamName;
        s32 miCrossfadeInFrames;
        s32 miCrossfadeOutFrames;
        bool mbPreload;
        bool mbKeepMemoryWhenFinished;
        bool mbDisableCustomSoundtracks;

        GuiEventPlayVideo()
            : muVideoResourceId(0), muSoundStreamName(0),
              miCrossfadeInFrames(0), miCrossfadeOutFrames(0),
              mbPreload(false), mbKeepMemoryWhenFinished(false),
              mbDisableCustomSoundtracks(false)
        {
            mafRectangle[0] = mafRectangle[1] = 0.0f;
            mafRectangle[2] = mafRectangle[3] = 1.0f;
        }

        s32 GetEventType() const { return KI_GUIEVENT_PLAY_VIDEO; }
    };

    // ARTIST82476CF8 copies the same complete definition for the stop event.
    // MovieManager::HandleStopVideoEvent824F17C0 selects behavior by state;
    // it does not read an invented immediate-stop flag from the payload.
    struct GuiEventStopVideo : public GuiEventPlayVideo
    {
        s32 GetEventType() const { return KI_GUIEVENT_STOP_VIDEO; }
    };

    static_assert(sizeof(GuiEventPlayVideo) == 48 && sizeof(GuiEventStopVideo) == 48,
        "ARTIST video events contain a 48-byte raw definition");
    static_assert(offsetof(GuiEventPlayVideo, mafRectangle) == 0 &&
        offsetof(GuiEventPlayVideo, muVideoResourceId) == 16 &&
        offsetof(GuiEventPlayVideo, muSoundStreamName) == 24 &&
        offsetof(GuiEventPlayVideo, miCrossfadeInFrames) == 28 &&
        offsetof(GuiEventPlayVideo, miCrossfadeOutFrames) == 32 &&
        offsetof(GuiEventPlayVideo, mbPreload) == 36 &&
        offsetof(GuiEventPlayVideo, mbKeepMemoryWhenFinished) == 37 &&
        offsetof(GuiEventPlayVideo, mbDisableCustomSoundtracks) == 38,
        "ARTIST82476C88..CD4 video field offsets");
}

namespace CgsGui
{
    // ARTIST82476C68/82476CF8: {48,508/509,16}, channel40, 64 bytes.
    // Keep these recovered instances separate from the legacy generic emitter.
    template <>
    inline void StateInterface::OutputGuiEvent<BrnGui::GuiEventPlayVideo>(
        BrnGui::GuiEventPlayVideo& lrEvent)
    {
        GuiEventWrapper<BrnGui::GuiEventPlayVideo, 40> lWrapper(lrEvent);
        mOutEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lWrapper),
            40, static_cast<s32>(sizeof(lWrapper)));
    }

    template <>
    inline void StateInterface::OutputGuiEvent<BrnGui::GuiEventStopVideo>(
        BrnGui::GuiEventStopVideo& lrEvent)
    {
        GuiEventWrapper<BrnGui::GuiEventStopVideo, 40> lWrapper(lrEvent);
        mOutEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lWrapper),
            40, static_cast<s32>(sizeof(lWrapper)));
    }

    static_assert(sizeof(GuiEventWrapper<BrnGui::GuiEventPlayVideo, 40>) == 64 &&
        sizeof(GuiEventWrapper<BrnGui::GuiEventStopVideo, 40>) == 64,
        "ARTIST video output records are 64 bytes");
}

#endif
