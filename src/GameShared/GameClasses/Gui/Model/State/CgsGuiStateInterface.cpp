#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. CgsGui::StateInterface is the
// outward channel a GUI state drives: accessors over its shared access-pointers and
// allocator, plus the event emitters that push records onto its large output queue.
// Behaviour-faithful to the X360 pseudocode; the event byte sizes the X360 passed
// to AddEvent (20 / 16 / 24) are noted inline. The queue's AddEvent body is its own
// (out-of-line) ledger TU, resolved at link time.

namespace CgsGui
{
    // @ 0x8240E3E0
    GuiAccessPointers* StateInterface::GetAccessPointers()
    {
        CGS_ASSERT(mpAccessPointers != nullptr, "mpAccessPointers != NULL");
        return mpAccessPointers;
    }

    // @ 0x8240E4E0 - reads the metric-units flag off the language manager reached
    // through the access pointers.
    bool StateInterface::IsUsingMetricUnits()
    {
        CGS_ASSERT(mpAccessPointers != nullptr, "mpAccessPointers != NULL");
        CGS_ASSERT(GetAccessPointers()->mpLanguageManager != nullptr,
                   "GetAccessPointers()->mpLanguageManager != NULL");
        return GetAccessPointers()->mpLanguageManager->IsUsingMetricUnits();
    }

    // @ 0x82436F10 - queue a "play apt movie" view-state event (type 18, channel 41).
    void StateInterface::PlayAptMovie(const char* lpacMovieName, s32 liLevelNum)
    {
        GuiEventPlayAptMovie lEvent;
        lEvent.mpacMovieName = lpacMovieName;
        lEvent.miLevelNum    = liLevelNum;
        mOutEventQueue.AddEvent(&lEvent, 41, sizeof(lEvent));   // X360 size 20
    }

    // @ 0x82476F98 - queue a "play apt loading movie" event (type 19, channel 40).
    void StateInterface::PlayLoadingScreen()
    {
        GuiEventPlayAptLoadingMovie lEvent;
        mOutEventQueue.AddEvent(&lEvent, 40, sizeof(lEvent));   // X360 size 16
    }

    // @ 0x82476FE0 - queue a "stop apt loading movie" event (type 20, channel 40).
    void StateInterface::StopLoadingScreen()
    {
        GuiEventStopAptLoadingMovie lEvent;
        mOutEventQueue.AddEvent(&lEvent, 40, sizeof(lEvent));   // X360 size 16
    }

    // @ 0x82436E40 - queue a resource load/unload request (channel 39).
    void StateInterface::RequestResource(const char* lpacFileName, ResourceRequestTypes leType,
                                         s32 liUserData, ResourceRequestLoadUnload leLoadUnload)
    {
        CGS_ASSERT(lpacFileName != nullptr, "Invalid file to load in StateInterface::RequestResource");

        GuiEventRequestResource lEvent;
        lEvent.meType       = leType;
        lEvent.meLoadUnload = leLoadUnload;
        lEvent.mpacFileName = lpacFileName;
        lEvent.miUserData   = liUserData;
        mOutEventQueue.AddEvent(&lEvent, 39, sizeof(lEvent));   // X360 size 24
    }

    // @ 0x82846F90 - install the event observer (the X360 asserts on a null pointer).
    void StateInterface::SetEventObserver(CgsGui::EventObserver* lpObserver)
    {
        CGS_ASSERT(lpObserver != nullptr,
                   "Invalid event observer pointer in StateInterface::SetEventObserver");
        mpObserver = lpObserver;
    }

    // Stand up the interface: zero the channel pointers + construct the output event queue. (Cast to the
    // VariableEventQueue base so the inline Construct is used, not the GuiEventQueueBase template decl.)
    void StateInterface::Construct()
    {
        mpObserver = nullptr;
        mpAccessPointers = nullptr;
        mpAllocator = nullptr;
        static_cast<CgsModule::VariableEventQueue<65536, 16>&>(mOutEventQueue).Construct();
    }

    GuiStackEventQueue::GuiEventQueueLarge* StateInterface::GetOutputEventQueue()
    {
        return &mOutEventQueue;
    }

    // [stub] On console, RegisterForEvents installs interest with the EventObserver, which then routes
    // matching events to this state. The boot driver bridges the output queue to the MovieManager directly,
    // so registration is a no-op here (the real observer dispatch is a follow-on).
    void StateInterface::RegisterForEvents(const s32* lpiEventIds, s32 liCount)
    {
        (void)lpiEventIds;
        (void)liCount;
    }

    void StateInterface::UnRegisterForEvents(const s32* lpiEventIds, s32 liCount)
    {
        (void)lpiEventIds;
        (void)liCount;
    }
}
