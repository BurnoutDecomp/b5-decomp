#include "GameSource/Gui/Flow/Overlay/States/BrnInvisibleOverlayState.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameSource/Gui/Flapt/BrnFlaptManager.h"                         // BrnFlapt::FlaptManager
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                         // BrnFlapt::FileRef

// BrnGui::InvisibleOverlayState -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (2 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Overlay/States/BrnInvisibleOverlayState.cpp):
//   InvisibleOverlayState::OnEnter @0x824B1568
//   InvisibleOverlayState::OnLeave @0x824B1678
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
}
