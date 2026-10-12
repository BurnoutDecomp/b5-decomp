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
        GuiEventPlayAptMovie()  {}
    };

    // Empty payload tags boxed by GuiEventWrapper<...,40>.  On X360 sizeof(T)
    // is 1, producing the exact 16-byte {1,type,12,payload/pad} wire record in
    // PlayLoadingScreen / StopLoadingScreen.
    struct GuiEventPlayAptLoadingMovie : public CgsModule::Event
    {
        s32 GetEventType() const { return 19; }
    };

    struct GuiEventStopAptLoadingMovie : public CgsModule::Event
    {
        s32 GetEventType() const { return 20; }
    };

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

        GuiEventClearScreenSet()  {}
    };

    struct GuiEventRequestResource : public CgsModule::Event
    {
        ResourceRequestTypes      meType;
        ResourceRequestLoadUnload meLoadUnload;
        const char*               mpacFileName;
        s32                       miUserData;
    };

    // The "play a music stream on the menu" GUI payload. OutputGuiEvent adds the
    // channel-40 {size,type,offset} record; this payload contains the sound-name hash
    // and two flags. (BrnGui::Credits posts the "Credits" hash and clears both flags.)
    struct GuiEventPlayMusicOnMenuStream : public GuiEvent<23>
    {
        u32  muStreamNameHash;   // +0x00 (CgsSound::Playback::Name::MakeHash result)
        bool mbFlagA;            // +0x04 (roles not recovered; Credits passes false/false)
        bool mbFlagB;            // +0x05

        explicit GuiEventPlayMusicOnMenuStream(u32 luStreamNameHash,
                                               bool lbFlagA = false, bool lbFlagB = false)
            :  muStreamNameHash(luStreamNameHash), mbFlagA(lbFlagA), mbFlagB(lbFlagB) {}
    };

    // The "suspend / resume network processing" GUI payload. OutputGuiEvent adds
    // the channel-40 wrapper; the payload is one 32-bit suspension word.
    // The payload word is the suspension type: the network state manager suspends on 0 and
    // calls SuspensionManager::Resume on 1, so a true constructor argument means RESUME.
    // (BrnGui::PauseScreen posts 0 when leaving the pause menu.)
    struct GuiEventNetworkSuspension : public GuiEvent<45>
    {
        enum ESuspensionType
        {
            E_SUSPENSION_TYPE_SUSPEND = 0,
            E_SUSPENSION_TYPE_RESUME  = 1,
            E_SUSPENSION_TYPE_COUNT   = 2,
        };

        ESuspensionType meSuspensionType;   // +0x00 payload word

        explicit GuiEventNetworkSuspension(bool lbResume)
            : meSuspensionType(lbResume ? E_SUSPENSION_TYPE_RESUME : E_SUSPENSION_TYPE_SUSPEND) {}
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

        // Push a typed event payload through the original channel-40 wrapper.
        // GuiEvent<N> is an empty tag base; GuiEventWrapper carries the separate
        // size/type/offset words and aligns its payload exactly as ARTIST does.
        template <typename TEvent>
        void OutputGuiEvent(TEvent& lrEvent)
        {
            GuiEventWrapper<TEvent, 40> lWrapper(lrEvent);
            mOutEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lWrapper),
                                    40, static_cast<s32>(sizeof(lWrapper)));
        }

        // The two sibling output channels of OutputGuiEvent: 41 == "view state", 42 ==
        // "internal state" (the channel legend is in CgsGuiEvent.h's GuiEventWrapper
        // comment). Both return **void**: the X360 mangled names are
        //   ??$OutputViewState@V<T>@BrnGui@@@StateInterface@CgsGui@@QAAXAAV<T>@BrnGui@@@Z
        // and the same with OutputInternalState -- `QAAX` == public / non-const / __cdecl /
        // **X = void return**. Checked on all 12 attested instantiations -- view state
        // 0x824C2FA0 / 0x82436CF0 / 0x824C2EE8 / 0x82493D98 / 0x82476B60 / 0x82476DD8 /
        // 0x82465E50, internal state 0x82436A30 / 0x82436A80 / 0x82493DE8 / 0x82493C98 /
        // 0x82476E38 -- every one of them spells `QAAX`. Hex-Rays renders them `int`
        // only because it propagates the tail-called AddEvent's bool return.
        //
        // BODY (X360 ARTIST, rung 1). These three output helpers stack-build a
        // GuiEventWrapper<TEvent, channel> and queue that wrapper. Measured on
        // OutputViewState<GuiEventShowHideSatNav> @0x82476DD8 -- the record written is
        //     +0 = 12 (sizeof(T))   +4 = 213 (GetEventType())   +8 = 12 (payload offset)
        //     +12..+23 = a 3-word copy of the caller's object, loaded from 0/4/8(r4)
        // then AddEvent(&record, /*channel*/41, /*size*/24). The payload words come from
        // offsets 0/4/8 of the passed event -- confirmed at the emitter, PreRaceFlyByState::
        // Update @0x824DC804..0x824DC820, which fills its local at +0/+4/+8 -- so the
        // wrapper's three header words are NOT part of TEvent. The channel is a compile-time
        // constant (`li r5,0x29` / `li r5,0x2A`), not the event's type id.
        // This shape reproduces every attested record exactly, including the align-8 cases:
        //   <GuiEventFilterEventIcons>     {4,557,12}  + 4  -> 16   @0x824C2FA0
        //   <GuiEventShowHideBoostBar>     {1,214,12}  + 1  -> 16   @0x82476B60
        //   <GuiEventShowHideHud>          {1,148,12}  + 1  -> 16   @0x82493C98
        //   <GuiEventShowHideSatNav>       {12,213,12} + 12 -> 24   @0x82476DD8 / 0x82476E38
        //   <GuiEventSetHoveredEventIcon>  {24,559,16} + 24 -> 40   @0x824C2EE8 (alignas(8))
        // Bodies live here (not in a .cpp) because the X360 emits one out-of-line copy per
        // T; the explicit instantiations are in the sibling CgsGuiStateInterface_Output*_Inst.cpp.
        template <typename TEvent>
        void OutputViewState(TEvent& lrEvent)
        {
            GuiEventWrapper<TEvent, 41> lWrapper(lrEvent);
            mOutEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lWrapper),
                                    41, static_cast<s32>(sizeof(lWrapper)));
        }

        template <typename TEvent>
        void OutputInternalState(TEvent& lrEvent)
        {
            GuiEventWrapper<TEvent, 42> lWrapper(lrEvent);
            mOutEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lWrapper),
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
