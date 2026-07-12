#include "GameSource/Gui/Flow/Overlay/States/BrnInvisibleOverlayState.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::VariableEventQueue<18432,16> (Update's in-queue drain)
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT (Update's unknown-overlay tripwire)
#include "GameSource/Gui/Flapt/BrnFlaptManager.h"                         // BrnFlapt::FlaptManager
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                         // BrnFlapt::FileRef

// BrnGui::InvisibleOverlayState -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (DWARF primary file
// GameSource/Gui/Flow/Overlay/States/BrnInvisibleOverlayState.cpp):
//   InvisibleOverlayState::OnEnter @0x824B1568
//   InvisibleOverlayState::OnLeave @0x824B1678
//   InvisibleOverlayState::Update  @0x824B2188
//   InvisibleOverlayState::GetResourcesToLoad -- no standalone X360 symbol: the
//     override's body is identical to CgsGui::State's empty default (the invisible
//     overlay pins nothing), so the linker ICF-folded it away. Reconstructed as
//     that empty list.
//
// OnEnter (asm walk): register the two observed events (.data @0x82063CAC == { 6, 185 },
// read from the decrypted XEX; 6 is the controller-action event -- see BrnBootLegal.cpp --
// and 185's producer is not yet named), construct the overlay component onto "Overlays_mc",
// fetch flapt file 0 through the asserting StateInterface::GetAccessPointers() (h:344) /
// GuiAccessPointers::GetFlaptManager() (CgsGuiShared.h:194) accessors, prepare the
// component from it, then RunOverlay("invisible") (the header-inline whose flash-id assert
// + string-keyed GotoAndPlayLabel @0x8246F3E8 the asm carries). OnLeave tail-calls the
// unregister.

namespace BrnGui
{
    const s32  InvisibleOverlayState::maiEventToObserve[]     = { 6, 185 };
    const s32  InvisibleOverlayState::miNumEventsObserved     = 2;
    const char InvisibleOverlayState::macOverlayComponentName[] = "Overlays_mc";

    // @ 0x824B1568
    void InvisibleOverlayState::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mOverlayComponent.Construct(macOverlayComponentName, mpStateInterface, NULL);

        // Both accessors carry the X360's inlined NULL asserts (h:344 / CgsGuiShared.h:194).
        BrnFlapt::FlaptManager* lpFlaptManager =
            mpStateInterface->GetAccessPointers()->GetFlaptManager();

        BrnFlapt::FileRef lFlaptFile;
        lpFlaptManager->GetFile(&lFlaptFile, 0);

        mOverlayComponent.Prepare(macOverlayComponentName, lFlaptFile, NULL);
        mOverlayComponent.RunOverlay("invisible");
    }

    // @ 0x824B1678
    void InvisibleOverlayState::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    // @ 0x824B2188 -- drain the in-event queue; each overlay-request event (id 185)
    // carries the requested overlay kind in its first word, and Update answers it by
    // sending the matching FSM transition event (the target overlay state's id -- see
    // BrnOverlayFlow::Prepare's CgsIDCompress table). Controller-action events (id 6,
    // also observed) are skipped. The queue is cleared once drained.
    void InvisibleOverlayState::Update()
    {
        typedef CgsModule::VariableEventQueue<18432, 16> GuiInEventQueue;
        GuiInEventQueue* lpQueue = reinterpret_cast<GuiInEventQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = NULL;
        s32 liEventSize = 0;
        s32 liEventType = lpQueue->GetFirstEvent(&lpEvent, &liEventSize);

        while (lpEvent != NULL)
        {
            if (liEventType == 185)   // the overlay-request event (second observed id)
            {
                // The requested overlay kind is the event's first word.
                const s32 liRequestedOverlay = *reinterpret_cast<const s32*>(lpEvent);
                switch (liRequestedOverlay)
                {
                case 0:  SendStateEvent("CN_WAIT");      break;
                case 1:  SendStateEvent("CN_OK");        break;
                case 2:  SendStateEvent("CN_OKCANCEL");  break;
                case 3:  SendStateEvent("CNO_WAIT");     break;
                case 4:  SendStateEvent("CNO_OK");       break;
                case 5:  SendStateEvent("CNO_OKCANCEL"); break;
                case 6:  SendStateEvent("IG_WAIT");      break;
                case 7:  SendStateEvent("IG_OK");        break;
                case 8:  SendStateEvent("IG_OKCANCEL");  break;
                case 9:  SendStateEvent("IGO_WAIT");     break;
                case 10: SendStateEvent("IGO_OK");       break;
                case 11: SendStateEvent("IGO_OKCANCEL"); break;
                case 12: SendStateEvent("IGO_ENTER_ON"); break;
                default:
                    // cpp:208 -- the X360 streams the offending kind after the text;
                    // folded static per convention.
                    CGS_ASSERT(false, "Invalid state requesting an overlay : ");
                    break;
                }
            }

            liEventType = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }

        lpQueue->Clear();
    }

    // No standalone X360 symbol (ICF-folded with CgsGui::State's empty default --
    // the invisible overlay pins no resources).
    void InvisibleOverlayState::GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                                   u32* lpuNumberOfResources) const
    {
        *lppResourceTuples    = NULL;
        *lpuNumberOfResources = 0;
    }
}
