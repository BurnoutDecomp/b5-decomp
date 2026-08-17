#ifndef BRN_GUI_VIDEO_EVENTS_H
#define BRN_GUI_VIDEO_EVENTS_H

#include "types.hpp"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"   // CgsGui::GuiEvent<N> (-> CgsModule::Event)

// BrnGui video events -- the GUI vocabulary the front-end fires at BrnGui::MovieManager (DecFIGS
// GameSource/Gui/BrnGuiMovieManager.{h,cpp}: HandlePlayVideoEvent / HandleStopVideoEvent). Each derives
// from CgsGui::GuiEvent<N> so it can be pushed onto a CgsModule::VariableEventQueue by byte image and
// dispatched by its type id.
namespace BrnGui
{
    // GUI event-type ids the MovieManager dispatches on (from ARTIST BrnGui::MovieManager::RecvEvent
    // 0x824F9688: switch(eventId) { 504 -> audio ready; 508 -> play video (copy into mQueuedMovie);
    // 509 -> stop video }).
    enum
    {
        KI_GUIEVENT_AUDIO_READY = 504,   // 0x1F8: localized audio stream ready -> WAITING_FOR_AUDIO -> PLAYING
        KI_GUIEVENT_PLAY_VIDEO  = 508,   // 0x1FC: GuiEventPlayVideo  -> copied into mQueuedMovie
        KI_GUIEVENT_STOP_VIDEO  = 509,   // 0x1FD: GuiEventStopVideo  -> HandleStopVideoEvent
    };

    // Ask the MovieManager to play a video: carries the VideoDefinition parameters it copies into its
    // queued movie (DecFIGS BrnGui::GuiEventPlayVideo).
    struct GuiEventPlayVideo : public CgsGui::GuiEvent<KI_GUIEVENT_PLAY_VIDEO>
    {
        u32  muVideoResourceId;            // VideoDataResource id to load + play
        f32  mafRectangle[4];              // left, top, right, bottom (logical 1280x720) -- X360 Vector4
        s32  miCrossfadeInFrames;
        s32  miCrossfadeOutFrames;
        bool mbPreload;
        bool mbKeepMemoryWhenFinished;
        bool mbDisableCustomSoundtracks;
        // The video's SOUND stream name, CgsSound::Playback::Name::MakeHash(name). The X360
        // producers write it into the VideoDefinition slot at +0x18 alongside the resource id
        // (boot audit F-P8b-5); it was a "[follow-on]" comment and the field did not exist, so
        // every boot video was played with no sound name attached.
        u32  muSoundStreamName;

        GuiEventPlayVideo()
            : muVideoResourceId(0)
            , muSoundStreamName(0)
            , miCrossfadeInFrames(0)
            , miCrossfadeOutFrames(0)
            , mbPreload(false)
            , mbKeepMemoryWhenFinished(false)
            , mbDisableCustomSoundtracks(false)
        {
            mafRectangle[0] = 0.0f;    mafRectangle[1] = 0.0f;
            mafRectangle[2] = 1280.0f; mafRectangle[3] = 720.0f;
        }
    };

    // Stop the currently-playing video (DecFIGS BrnGui::GuiEventStopVideo).
    struct GuiEventStopVideo : public CgsGui::GuiEvent<KI_GUIEVENT_STOP_VIDEO>
    {
        bool mbStopStraightAway;

        GuiEventStopVideo() : mbStopStraightAway(false) {}
    };
}

#endif
