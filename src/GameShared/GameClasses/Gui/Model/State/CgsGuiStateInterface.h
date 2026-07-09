#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

namespace rw { struct IResourceAllocator; }
namespace CgsLanguage { class LanguageManager; }

// CgsGui::StateInterface - the channel a GUI state uses to reach the rest of the
// game: it owns the large output event queue the state writes to, plus the access
// pointers / allocator the state needs. Recovered from the X360 spine; layout,
// method set and the emitted event records are from the DecFIGS DWARF
// (CgsGuiStateInterface.h).
namespace CgsGui
{
    class EventObserver;

    // Events the interface emits onto its output queue. The numeric template id is
    // the GuiEvent<N> type; the queue "channel" id (passed to AddEvent) selects the
    // output wrapper: 40 = GuiEventOut, 41 = GuiOutViewState, 42 = internal,
    // 39 = resource request.
    struct GuiEventPlayAptMovie : public GuiEvent<18>
    {
        const char* mpacMovieName;
        s32         miLevelNum;
        GuiEventPlayAptMovie() : GuiEvent<18>(8, 12) {}
    };

    struct GuiEventPlayAptLoadingMovie : public GuiEvent<19>
    {
        GuiEventPlayAptLoadingMovie() : GuiEvent<19>(1, 12) {}
    };

    struct GuiEventStopAptLoadingMovie : public GuiEvent<20>
    {
        GuiEventStopAptLoadingMovie() : GuiEvent<20>(1, 12) {}
    };

    struct GuiEventRequestResource : public CgsModule::Event
    {
        ResourceRequestTypes      meType;
        ResourceRequestLoadUnload meLoadUnload;
        const char*               mpacFileName;
        s32                       miUserData;
    };

    // The "play a music stream on the menu" GUI event. X360-attested by the
    // OutputGuiEvent<GuiEventPlayMusicOnMenuStream> instantiation @0x82476C00: record
    // { muHeader0 = 8, muEventType = 23, muHeader2 = 12 } + { the sound-name hash,
    // two flag bytes }, channel 40, 20 bytes. (BrnGui::Credits posts it with
    // CgsSound::Playback::Name::MakeHash("Credits") and both flags clear.)
    struct GuiEventPlayMusicOnMenuStream : public GuiEvent<23>
    {
        u32  muStreamNameHash;   // +0x0C  (CgsSound::Playback::Name::MakeHash result)
        bool mbFlagA;            // +0x10  (roles not recovered; Credits passes false/false)
        bool mbFlagB;            // +0x11

        explicit GuiEventPlayMusicOnMenuStream(u32 luStreamNameHash,
                                               bool lbFlagA = false, bool lbFlagB = false)
            : GuiEvent<23>(8, 12)
            , muStreamNameHash(luStreamNameHash), mbFlagA(lbFlagA), mbFlagB(lbFlagB) {}
    };

    // The "suspend / resume network processing" GUI event. X360-attested by the
    // OutputGuiEvent<CgsGui::GuiEventNetworkSuspension> instantiation @0x82493A88: the
    // queued record is { muHeader0 = 4 (payload bytes), muEventType = 45, muHeader2 = 12
    // (payload offset) } + one payload word (the suspend flag), channel 40, 16 bytes.
    // (BrnGui::PauseScreen posts it with the flag false when leaving the pause menu.)
    struct GuiEventNetworkSuspension : public GuiEvent<45>
    {
        u32 muSuspend;   // +0x0C payload word: nonzero = suspend network processing

        explicit GuiEventNetworkSuspension(bool lbSuspend)
            : GuiEvent<45>(4, 12), muSuspend(lbSuspend ? 1u : 0u) {}
    };

    struct StateInterface
    {
        CgsGui::EventObserver* mpObserver;

    private:
        GuiAccessPointers*               mpAccessPointers;
        rw::IResourceAllocator*          mpAllocator;
        GuiStackEventQueue::GuiEventQueueLarge mOutEventQueue;

    public:
        void Construct();
        void Prepare(rw::IResourceAllocator* lpAllocator, GuiAccessPointers* lpAccessPointers);

        void RegisterForEvents(const s32* lpiEventIds, s32 liCount);
        void UnRegisterForEvents(const s32* lpiEventIds, s32 liCount);
        void PriorityRegisterForEvent(s32 liPriority, const s32* lpiEventIds, u32 luCount);
        void PriorityUnRegisterForEvent(s32 liPriority);
        void StopPriorityEventBlocking();

        void RequestResource(const char* lpacFileName, ResourceRequestTypes leType,
                             s32 liUserData, ResourceRequestLoadUnload leLoadUnload);
        void UnloadResource(const char* lpacFileName, ResourceRequestTypes leType, s32 liUserData);

        GuiStackEventQueue::GuiEventQueueLarge* GetOutputEventQueue();
        void SetEventObserver(CgsGui::EventObserver* lpObserver);

        // Push a GUI event onto the state's output queue. The X360 body (e.g.
        // OutputGuiEvent<BrnGui::GuiAudioEvent> @0x824367D8) stack-builds a
        // GuiEventWrapper<TEvent,40> -- { miOutEventSize = sizeof(TEvent),
        // miOutEventType = TEvent::GetEventType(), miOutEventOffset = offsetof(mOutEvent),
        // mOutEvent = copy of the event } -- then pushes it onto mOutEventQueue
        // (VariableEventQueue<65536,16>, this+12) via AddEvent(&wrapper, /*channel*/40,
        // /*record size*/sizeof(wrapper)). Channel 40 is the OutputGuiEvent output-event tag
        // (OutputViewState uses 41, OutputInternalState 42). The template is generic so it can
        // carry a higher-layer (GameSource) event type without this GameShared header depending
        // on it -- it is only instantiated where TEvent is complete. The queued wrapper is then
        // routed to the registered observers (e.g. the MovieManager for the video events).
        template <typename TEvent>
        int OutputGuiEvent(TEvent& lrEvent)
        {
            CgsGui::GuiEventWrapper<TEvent, 40> lWrapper(lrEvent);
            return mOutEventQueue.AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWrapper),
                40, static_cast<s32>(sizeof(lWrapper)));
        }

        // Push a GUI event onto the state's VIEW-state output channel (channel 41). The X360
        // body is the exact OutputGuiEvent wrapper-build with a different AddEvent channel tag:
        // it stack-builds a GuiEventWrapper<TEvent,41> -- { miOutEventSize = sizeof(TEvent),
        // miOutEventType = TEvent::GetEventType(), miOutEventOffset = offsetof(mOutEvent),
        // mOutEvent = copy of the event } -- then pushes it onto mOutEventQueue via
        // AddEvent(&wrapper, /*channel*/41, /*record size*/sizeof(wrapper)). Channel 41 is the
        // OutputViewState output tag (40 = OutputGuiEvent, 42 = OutputInternalState); it is the
        // record type the view-side bridge consumes. Every X360 instance (e.g.
        // OutputViewState<GuiEventFilterEventIcons> @0x824C2FA0, ...<GuiEventRenderMainMap>
        // @0x82465E50) compiles to the identical sequence; the per-T size/id/offset constants
        // fall out of TEvent.
        template <typename TEvent>
        int OutputViewState(TEvent& lrEvent)
        {
            CgsGui::GuiEventWrapper<TEvent, 41> lWrapper(lrEvent);
            return mOutEventQueue.AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWrapper),
                41, static_cast<s32>(sizeof(lWrapper)));
        }

        // Push a GUI event onto the state's INTERNAL-state output channel (channel 42). Same
        // wrapper-build as OutputGuiEvent / OutputViewState; only the AddEvent channel tag
        // differs -- 42 is the OutputInternalState record type. X360 instances e.g.
        // OutputInternalState<GuiEventShowHideHud> @0x82493C98,
        // ...<GuiEventPerformOnlineMainMenuOption> @0x82436A30.
        template <typename TEvent>
        int OutputInternalState(TEvent& lrEvent)
        {
            CgsGui::GuiEventWrapper<TEvent, 42> lWrapper(lrEvent);
            return mOutEventQueue.AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWrapper),
                42, static_cast<s32>(sizeof(lWrapper)));
        }

        void PlayAptMovie(const char* lpacMovieName, s32 liLevelNum);
        void PlayVideo(const char* lpacVideoName);
        void PlayLoadingScreen();
        void StopLoadingScreen();
        void Clear();

        rw::IResourceAllocator*       GetAllocator();
        GuiAccessPointers*            GetAccessPointers();
        CgsLanguage::LanguageManager* GetLanguageManager();
        bool                          IsUsingMetricUnits();
    };
}
