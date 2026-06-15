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
