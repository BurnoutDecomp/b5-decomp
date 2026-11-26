#pragma once

#include "types.hpp"
#include <cstddef>
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

namespace rw { struct IResourceAllocator; }
namespace CgsLanguage { class LanguageManager; }
namespace BrnGui { struct GuiAudioTriggerEvent; class GuiEventPlayVideo; class GuiEventStopVideo; }

// CgsGui::StateInterface - the channel a GUI state uses to reach the rest of the
// game: it owns the large output event queue the state writes to, plus the access
// pointers / allocator the state needs. Recovered from the X360 spine; layout,
// method set and the emitted event records are from the DecFIGS DWARF
// (CgsGuiStateInterface.h).
// The original header path ARTIST bakes into the asserts of this header's inline bodies
// (file string 0x8204D2F8).
#define CGS_GUI_STATE_INTERFACE_H_FILE "..\\..\\..\\GameShared\\GameClasses\\Gui/Model/State/CgsGuiStateInterface.h"
namespace CgsGui
{
    class EventObserver;

    // The output channels' records (DecFIGS CgsGuiStateInterface.h:131 / 142 / 153): a
    // GuiEventWrapper per channel, built on the stack by OutputGuiEvent (40), OutputViewState
    // (41) and OutputInternalState (42) below; the EventInterpreterModule unboxes them
    // (ProcessOutEvents 0x8285E1D0).
    template <class T>
    struct GuiEventOut : public GuiEventWrapper<T, 40>
    {
    };

    template <class T>
    struct GuiOutViewState : public GuiEventWrapper<T, 41>
    {
    };

    template <class T>
    struct GuiOutInternalEvent : public GuiEventWrapper<T, 42>
    {
    };

    // Events the interface emits onto its output queue. The numeric template id is
    // the GuiEvent<N> type; the queue "channel" id (passed to AddEvent) selects the
    // output wrapper: 40 = GuiEventOut, 41 = GuiOutViewState, 42 = internal,
    // 39 = resource request.
    struct GuiEventPlayAptMovie : public GuiEvent<18>
    {
        const char* mpacMovieName;
        s32         miLevelNum;
    };

    // The loading-screen tags (DecFIGS CgsGuiStateInterface.h:210 `typedef GuiEvent<19>
    // GuiEventPlayAptLoadingMovie`, and GuiEvent<20> for the stop): one-byte payloads, so
    // PlayLoadingScreen / StopLoadingScreen queue a 16-byte {1, 19|20, 12} channel-40 record
    // (ARTIST 0x82476F98 / 0x82476FE0).
    typedef GuiEvent<19> GuiEventPlayAptLoadingMovie;
    typedef GuiEvent<20> GuiEventStopAptLoadingMovie;

    // The view's clear-screen (black backdrop) control record (DecFIGS
    // CgsGuiEventTypeDefs.h:130; X360-attested by the ViewModule case-25 consumer
    // @0x8285FCE8 and the boot states' posts: BootLegal::OnEnter ACTIVE alpha 1.0,
    // BootLegal::OnLeave / BootProfile::OnEnter INACTIVE -- the prompt renders over
    // the save/load loading-screen background, not over black).
    struct GuiEventClearScreenSet : public GuiEvent<25>
    {
        enum EClearScreen
        {
            E_CLEAR_SCREEN_ACTIVE   = 0,
            E_CLEAR_SCREEN_INACTIVE = 1,
            E_CLEAR_SCREEN_MAX      = 2,
        };

        EClearScreen meClearScreen;
        f32          mfAlpha;

    };

    struct GuiEventRequestResource : public CgsModule::Event
    {
        ResourceRequestTypes      meType;
        ResourceRequestLoadUnload meLoadUnload;
        const char*               mpacFileName;
        s32                       miUserData;
    };

    // The "play a music stream on the menu" GUI event (DecFIGS CgsGuiStateInterface.h:193,
    // `: GuiEvent<23> { Name mName; bool mbRewardsScreenCredits; bool mbOverrideCustomSoundtracks; }`,
    // no constructor; the tree keeps the Name as its 32-bit hash and the two flags under its own
    // names). ARTIST OutputGuiEvent<GuiEventPlayMusicOnMenuStream> 0x82476C00 copies the hash
    // and the two bytes into a {8, 23, 12} channel-40 wrapper, 20 bytes.
    class GuiEventPlayMusicOnMenuStream : public GuiEvent<23>
    {
    public:
        u32  muStreamNameHash;   // +0x00  (CgsSound::Playback::Name::MakeHash result)
        bool mbFlagA;            // +0x04  (roles not recovered; Credits passes false/false)
        bool mbFlagB;            // +0x05
    };
    CGS_STATIC_ASSERT((sizeof(GuiEventPlayMusicOnMenuStream) == 8), "ARTIST8250C038 menu stream is eight bytes");

    // The "suspend / resume network processing" GUI event (DecFIGS CgsGuiEventTypeDefs.h:358,
    // one field at h:368, no constructor). ARTIST OutputGuiEvent<CgsGui::GuiEventNetworkSuspension>
    // 0x82493A88 queues { 4, 45, 12 } + the payload word on channel 40, 16 bytes.
    // The payload word is the suspension type: the network state manager suspends on 0 and
    // calls SuspensionManager::Resume on 1. (BrnGui::PauseScreen posts 0 when leaving the
    // pause menu.)
    struct GuiEventNetworkSuspension : public GuiEvent<45>
    {
        enum ESuspensionType
        {
            E_SUSPENSION_TYPE_SUSPEND = 0,
            E_SUSPENSION_TYPE_RESUME  = 1,
            E_SUSPENSION_TYPE_COUNT   = 2,
        };

        ESuspensionType meSuspensionType;   // +0x00 payload word
    };

    // ARTIST8285B8A0/B900/B960/B9C0 and ProcessOutEvents8285E1D0.
    // These are flat queue payloads; GuiEvent<N>'s host wrapper is not copied.
    struct GuiEventRegisterForEvents : public CgsModule::Event
    {
        s32 miEventType;
        EventObserver* lpEventObserver;
    };
    struct GuiEventUnRegisterForEvents : public GuiEventRegisterForEvents {};
    struct GuiEventPriorityRegisterForEvents : public CgsModule::Event
    {
        s32 miEventType;
        s32 miEventTypeOverridden[KI_MAX_EVENTS_PER_OBSERVER];
        u32 muOverrideCount;
        EventObserver* lpEventObserver;
    };
    struct GuiEventPriorityUnRegisterForEvents : public GuiEventRegisterForEvents {};
    struct GuiEventPriorityStopBlocking : public CgsModule::Event
    {
        EventObserver* lpEventObserver;
    };

    CGS_STATIC_ASSERT((offsetof(GuiEventPriorityRegisterForEvents, muOverrideCount) == 0x964),
        "ARTIST600-element priority list followed by count");
    CGS_STATIC_ASSERT((offsetof(GuiEventPriorityRegisterForEvents, lpEventObserver) == 0x968),
        "Priority observer follows the count");

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

        // Header-inline (DWARF h:304; no ARTIST body: inlined at every caller).
        GuiStackEventQueue::GuiEventQueueLarge* GetOutputEventQueue() { return &mOutEventQueue; }
        void SetEventObserver(CgsGui::EventObserver* lpObserver);

        // The three output channels (DecFIGS CgsGuiStateInterface.h:404 / 423 / 442; ARTIST
        // decorates every instance `QAAXAAV<T>`: void, the event by non-const reference). Each
        // boxes the event in its channel's wrapper (local `lOutEventWrapper`, h:406 / 425 / 444),
        // fills it with SetRawEvent and queues it through the inline GuiEventQueueBase::
        // AddGuiEvent<T>, which passes the wrapper's GetEventType() (the channel) and sizeof.
        // ARTIST e.g. OutputGuiEvent<GuiEventRequestTraining> 0x82436840: payload, 0xC, 4, 0x23C,
        // AddEvent(&wrapper, 40, 16); OutputViewState<GuiEventShowHideBoostBar> 0x82476B60:
        // {1, 214, 12} + 1 byte on channel 41, 16 bytes. The explicit instantiations ARTIST
        // emits out of line are in the sibling CgsGuiStateInterface_Output*_Inst.cpp files.
        template <typename TEvent>
        void OutputGuiEvent(TEvent& lOutEvent);
        template <typename TEvent>
        void OutputViewState(TEvent& lOutEvent);
        template <typename TEvent>
        void OutputInternalState(TEvent& lOutEvent);

        void PlayAptMovie(const char* lpacMovieName, s32 liLevelNum);
        void PlayVideo(const char* lpacVideoName);
        void PlayLoadingScreen();
        void StopLoadingScreen();
        void Clear();

        // Header-inline in the class (DWARF h:335/342/349; the asserts carry h:337/344/351).
        // ARTIST inlines them at most call sites; the standalone copies are COMDATs in the Gui
        // code (GetAccessPointers 0x8240E3E0, GetLanguageManager 0x8240E438), and GetAllocator
        // has none (e.g. MapManager::Construct @0x82458590 inlines it).
        rw::IResourceAllocator* GetAllocator()
        {
            CGS_ASSERT_AT(mpAllocator != 0, "mpAllocator != NULL", CGS_GUI_STATE_INTERFACE_H_FILE, 337);
            return mpAllocator;
        }
        GuiAccessPointers* GetAccessPointers()
        {
            CGS_ASSERT_AT(mpAccessPointers != 0, "mpAccessPointers != NULL", CGS_GUI_STATE_INTERFACE_H_FILE, 344);
            return mpAccessPointers;
        }
        CgsLanguage::LanguageManager* GetLanguageManager()
        {
            CGS_ASSERT_AT(GetAccessPointers()->mpLanguageManager != 0,
                          "GetAccessPointers()->mpLanguageManager != NULL", CGS_GUI_STATE_INTERFACE_H_FILE, 351);
            return GetAccessPointers()->mpLanguageManager;
        }
        // DWARF h:356 (header-inline too); kept out of line in the .cpp because its body needs
        // the complete CgsLanguage::LanguageManager, which this header does not include. Both
        // ARTIST callers (EventInfoComponent::Construct / UpdateRace) call 0x8240E4E0.
        bool IsUsingMetricUnits();
    };

    template <typename TEvent>
    void StateInterface::OutputGuiEvent(TEvent& lOutEvent)
    {
        GuiEventOut<TEvent> lOutEventWrapper;
        lOutEventWrapper.SetRawEvent(lOutEvent);
        mOutEventQueue.AddGuiEvent(lOutEventWrapper);
    }

    template <typename TEvent>
    void StateInterface::OutputViewState(TEvent& lOutEvent)
    {
        GuiOutViewState<TEvent> lOutEventWrapper;
        lOutEventWrapper.SetRawEvent(lOutEvent);
        mOutEventQueue.AddGuiEvent(lOutEventWrapper);
    }

    template <typename TEvent>
    void StateInterface::OutputInternalState(TEvent& lOutEvent)
    {
        GuiOutInternalEvent<TEvent> lOutEventWrapper;
        lOutEventWrapper.SetRawEvent(lOutEvent);
        mOutEventQueue.AddGuiEvent(lOutEventWrapper);
    }

    // ---- header-inline bodies after the class (DWARF: they appear in the game unity CUs
    // with these header lines; ARTIST: their standalone copies are COMDATs in the Gui code,
    // emitted where a caller did not inline them) ----

    // h:476, streamed assert h:478. Standalone ARTIST copy 0x82846F90.
    inline void StateInterface::SetEventObserver(CgsGui::EventObserver* lpObserver)
    {
        CGS_ASSERT(lpObserver != 0, CGS_ASSERT_STREAM_MESSAGE_AT("Invalid event observer pointer in StateInterface::SetEventObserver", CGS_GUI_STATE_INTERFACE_H_FILE, 478));

        mpObserver = lpObserver;
    }

    // h:496, streamed assert h:498, local lLoadRequest h:500. Standalone ARTIST copy 0x82436E40.
    inline void StateInterface::RequestResource(const char* lpacFileName, ResourceRequestTypes leFileType,
                                                s32 liRequestId, ResourceRequestLoadUnload leLoadUnload)
    {
        CGS_ASSERT_STREAM_AT(lpacFileName != 0, "Invalid file to load in StateInterface::RequestResource",
                             CGS_GUI_STATE_INTERFACE_H_FILE, 498);

        // Fields filled in parameter order (ARTIST's stores: +8, +0, +0xC, +4 @0x82436EE4..EFC).
        GuiEventLoadRequest lLoadRequest;
        lLoadRequest.mpacFileToLoad  = lpacFileName;
        lLoadRequest.meRequestType   = leFileType;
        lLoadRequest.muLoadRequestId = liRequestId;
        lLoadRequest.meLoadUnload    = leLoadUnload;
        mOutEventQueue.AddEvent(&lLoadRequest, 39, sizeof(lLoadRequest));   // X360 size 24
    }

    // h:540, local lPlayMovieEvent h:542; {8, 18, 12} + 8 bytes on channel 41, size 20.
    // Standalone ARTIST copy 0x82436F10.
    inline void StateInterface::PlayAptMovie(const char* lpacMovieName, s32 liLevelNum)
    {
        GuiEventPlayAptMovie lPlayMovieEvent;
        lPlayMovieEvent.mpacMovieName = lpacMovieName;
        lPlayMovieEvent.miLevelNum    = liLevelNum;
        OutputViewState(lPlayMovieEvent);
    }

    // h:581, local lPlayMovieEvent h:583; {1, 19, 12} on channel 40, size 16. ARTIST 0x82476F98.
    inline void StateInterface::PlayLoadingScreen()
    {
        GuiEventPlayAptLoadingMovie lPlayMovieEvent;
        OutputGuiEvent(lPlayMovieEvent);
    }

    // h:598, local lStopMovieEvent h:600; {1, 20, 12} on channel 40, size 16. ARTIST 0x82476FE0.
    inline void StateInterface::StopLoadingScreen()
    {
        GuiEventStopAptLoadingMovie lStopMovieEvent;
        OutputGuiEvent(lStopMovieEvent);
    }
}
