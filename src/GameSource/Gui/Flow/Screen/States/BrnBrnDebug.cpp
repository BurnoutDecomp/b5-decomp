#include "GameSource/Gui/Flow/Screen/States/BrnBrnDebug.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase (unexpected-event log)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags
#include "GameSource/Input/GameInputActions.h"                            // EGameInputActions

// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnDebug (the debug GUI screen state)
// registers for the two GUI events it watches when entered, and unregisters on leave --
// the same tail-call pair as the sibling states (BrnBootAttract et al.).
//
// Bodied here (3 ledger functions + the vtable's GetResourcesToLoad, primary file
// GameSource/Gui/Flow/Screen/States/BrnBrnDebug.cpp):
//   BrnDebug::OnEnter @0x824B5150  (b CgsGui::StateInterface::RegisterForEvents)
//   BrnDebug::OnLeave @0x824B5168  (b CgsGui::StateInterface::UnRegisterForEvents)
//   BrnDebug::Update               (drain the in-queue; the left shoulder backs out)
//   BrnDebug::GetResourcesToLoad   (identical-code fold with Video::GetResourcesToLoad)
//
// The observed-event id table lives in .data @0x82065DB0; its two big-endian dwords read
// { 14, 6 } straight out of the decrypted ARTIST XEX (file_off = 0x3000 + vaddr -
// 0x82000000). Id 6 is the controller-action GUI event (same id the boot states observe,
// see BrnBootLegal.cpp); id 14's producer is not yet named -- kept as the raw id.

namespace BrnGui
{
    namespace
    {
        // The state's in-event queue (CgsGui::State +0x18) is the incomplete
        // InputBuffer::GuiEventQueue; the console walks the concrete instantiation.
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        const s32 KI_EVENT_CONTROLLER_INPUT_PRESSED = 6;
        const s32 KI_EVENT_UNNAMED_14               = 14;   // the second observed id; producer not yet named

        // The event-6 payload as the queue delivers it (header-stripped): the action id is
        // the second word, after the pad id.
        struct ControllerInputPressedPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (EGameInputActions)
        };
    }

    const s32 BrnDebug::maiEventToObserve[] = { 14, 6 };
    const s32 BrnDebug::miNumEventsObserved = 2;

    // @ 0x824B5150
    void BrnDebug::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    // @ 0x824B5168
    void BrnDebug::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    // Drain the in-queue: a left-shoulder press sends "GO_BACK", event 14 is
    // swallowed, anything else goes to the filtered debug log. Then clear the queue.
    void BrnDebug::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_CONTROLLER_INPUT_PRESSED)
            {
                const ControllerInputPressedPayload* lpInput =
                    static_cast<const ControllerInputPressedPayload*>(lpEvent);
                if (lpInput->miButtonId == E_GAMEINPUTACTIONS_GUI_LSHOULDER)
                {
                    SendStateEvent("GO_BACK");
                }
            }
            else if (liEventId != KI_EVENT_UNNAMED_14 && (CgsDev::Message::gxMessageFilterFlags & 1))
            {
                *CgsDev::Log::gpDebugPrint
                    << "Unexpected event received : " << liEventId
                    << " in "
                    << "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnBrnDebug.cpp"
                    << " at line " << 109 << "\n";
            }
        }

        lpInQueue->Clear();
    }

    // The vtable slot is the identical-code fold of Video::GetResourcesToLoad: only the
    // count is zeroed, the tuple out-pointer is left untouched. The debug screen loads nothing.
    void BrnDebug::GetResourcesToLoad(const CgsGui::sResourceTuple** /*lppResourceTuples*/,
                                      u32* lpuNumberOfResources) const
    {
        *lpuNumberOfResources = 0;
    }
}
