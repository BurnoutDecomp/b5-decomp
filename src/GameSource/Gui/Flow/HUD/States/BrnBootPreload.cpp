#include "GameSource/Gui/Flow/HUD/States/BrnBootPreload.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::VariableEventQueue (the in-queue)
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT

// BrnGui::BootPreload -- reconstructed from BURNOUT_X360_ARTIST.XEX
//   (OnEnter @0x82473A10, OnLeave @0x82473A60, Update @0x82477F28,
//    GetResourcesToLoad @0x82508070 -- inline in the header).
//
// BF_PRELOAD is the FIRST HUD-flow state: it waits for the GUI cache's preload
// resource set, plays the AS FRAMEWORK movie "main" at display level 0 (the movie
// whose frame-0 DoAction runs `new AptCommunicator` + the Object.registerClass
// bootstrap -- the asm string xref @0x82478110 pins the name), raises the loading
// screen, and signals phase-complete: command 70 on the GUI-out channel (40) --
// the game main flow's generic "GUI phase done" -- plus the preload-done command
// 72 on channels 42 and 40.
namespace BrnGui
{
    namespace
    {
        const s32 KI_EVENT_GUI_CACHE = 64;   // the per-frame cache event (GuiCache* payload)

        const s32 KI_CHANNEL_GUI_OUT  = 40;  // GuiEventOut
        const s32 KI_CHANNEL_INTERNAL = 42;  // internal command channel

        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // The event-64 record: the GUI module posts the cache pointer each frame.
        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpGuiCache;
        };

        // 16-byte GuiEvent<N> command with header { 1, N, 12 } and one trailing flag byte
        // (the same record + helper shape BrnBootLegal.cpp uses for its channel commands).
        template <s32 N>
        struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
        {
            u8 mu8Flag;    // +0x0C (the X360 stores a single byte then leaves the record at 16)
            u8 maPad[3];
            GuiCommandEvent16(u8 lu8Flag = 0) : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag)
            { maPad[0] = maPad[1] = maPad[2] = 0; }
        };

        template <s32 N>
        void PostCommand16(CgsGui::StateInterface* lpInterface, s32 liChannel)
        {
            GuiCommandEvent16<N> lEvent(0);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel, 16);
        }
    }

    // The one event this state observes (X360 .rdata table the OnEnter registration
    // points at; the Update body dispatches only 64).
    const s32 BootPreload::maiEventToObserve[] = { KI_EVENT_GUI_CACHE };
    const s32 BootPreload::miNumEventsObserved = 1;

    // FLAG (unrecovered .rdata): the X360 second-phase preload resource-tuple table
    // (unk_82F25F30 family) is data the exports do not carry. The PC cache boundary
    // treats the preload set as resident (synchronous loads), so an empty table is the
    // faithful PC stand-in until the .rdata is recovered.
    const CgsGui::sResourceTuple BootPreload::maSecondPhaseResourcesToLoad[1] = { { 0u, CgsGui::E_GUI_RESOURCETYPE_START } };
    const u32 BootPreload::muSecondPhaseNumResourcesToLoad = 0;

    // @ 0x82473A10 -- seed the wait-cache stage, register for the cache event, clear the cache.
    void BootPreload::OnEnter()
    {
        meUpdateStage = E_PRELOAD_WAIT_CACHE;
        if (mpStateInterface != 0)
            mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);
        mpGuiCache = 0;
    }

    // @ 0x82473A60 -- unregister the observed set.
    void BootPreload::OnLeave()
    {
        if (mpStateInterface != 0)
            mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    // The preload-resource gate. The X360 gates each stage on
    // GuiCache::EnsureResourcesAreLoaded(cache, <.rdata tuple table>, <count>); the PC
    // cache boundary loads synchronously, so a live cache pointer is the gate (the same
    // faithful default BrnBootLegalBoundary.cpp uses for BF_LEGAL's resource waits).
    static bool PreloadResourcesReady(GuiCache* lpGuiCache)
    {
        return lpGuiCache != 0;
    }

    // @ 0x82477F28 -- consume the cache event, then walk the preload stage machine.
    void BootPreload::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_GUI_CACHE)
            {
                if (mpGuiCache == 0)
                {
                    GuiCache* lpCache = reinterpret_cast<const GuiEventCache*>(lpEvent)->mpGuiCache;
                    CGS_ASSERT(lpCache != 0, "Invalid cache in BrnBootPreload::Update");
                    mpGuiCache = lpCache;
                }
            }
            else
            {
                CGS_ASSERT(false, "Unexpected event in IntroHudState::Update");
            }
        }

        switch (meUpdateStage)
        {
        case E_PRELOAD_WAIT_CACHE:
            // Wait for the cache + its preload set, then play the AS framework movie
            // "main" at display level 0 (the level-0 root the component classes live in).
            if (!PreloadResourcesReady(mpGuiCache))
                break;
            mpStateInterface->PlayAptMovie("main", 0);
            meUpdateStage = E_PRELOAD_LOADING_SCREEN;
            lpInQueue->Clear();
            return;

        case E_PRELOAD_LOADING_SCREEN:
            if (!PreloadResourcesReady(mpGuiCache))
                break;
            mpStateInterface->PlayLoadingScreen();
            meUpdateStage = E_PRELOAD_SETTLE;
            lpInQueue->Clear();
            return;

        case E_PRELOAD_SETTLE:
            meUpdateStage = E_PRELOAD_SIGNAL_DONE;
            lpInQueue->Clear();
            return;

        case E_PRELOAD_SIGNAL_DONE:
            if (!PreloadResourcesReady(mpGuiCache))
                break;
            // Phase complete: 70 on the GUI-out channel (the game main flow advances on
            // it), then the preload-done command 72 on the internal + GUI-out channels.
            PostCommand16<70>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            PostCommand16<72>(mpStateInterface, KI_CHANNEL_INTERNAL);
            PostCommand16<72>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            meUpdateStage = E_PRELOAD_DRAIN;
            lpInQueue->Clear();
            return;

        case E_PRELOAD_DRAIN:
            meUpdateStage = E_PRELOAD_DONE;
            break;

        case E_PRELOAD_DONE:
        default:
            break;
        }

        lpInQueue->Clear();
    }
}
