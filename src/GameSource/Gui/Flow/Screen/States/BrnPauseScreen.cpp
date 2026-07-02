#include "GameSource/Gui/Flow/Screen/States/BrnPauseScreen.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventNetworkSuspension
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiEventActivateCrashNav

// BrnGui::PauseScreen -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, DWARF primary file
// GameSource/Gui/Flow/Screen/States/BrnPauseScreen.cpp):
//   PauseScreen::HandleControllerInput @0x824CFE78  (called by PauseScreen::Update)
//
// X360 asm walk: gate on meSubState == E_SUBSTATE_PROMPT (this+0x38 == 2), then switch on
// the controller-action sub-id (payload word +4 of the action event; jump table base 38,
// 13 cases):
//   38            -> SendStateEvent("TO_COLOUR")   (the colour-calibration screen hand-off)
//   45 / 49 / 50  -> the "resume game" path: assert the highlighted pause option is the
//                    single valid one (lbz this+0xE5 == mPauseOptions.miHighlightedIndex;
//                    the pause menu has exactly one option -- KAPC_PAUSE_OPTION_STRING_IDS
//                    is a 1-entry table -- so a non-zero index is "Invalid pause option
//                    selected", cpp:277), then:
//                      OutputGuiEvent<GuiEventActivateCrashNav>({ 1, 0 })   (@0x82493938)
//                      OutputGuiEvent<GuiEventNetworkSuspension>({ 0 })     (@0x82493A88)
//                      AddEvent(out-queue, GuiEvent<533>{ 1, 533, 12 }, channel 40, 16)
//                      SendStateEvent("GO_BACK")
//   anything else -> ignored.

namespace BrnGui
{
namespace
{
    // ---- Controller action sub-ids (payload word +4 of the action event). 45/49 carry
    //      the same roles as in BrnBootLegal.cpp; 50 and 38 are this screen's additions
    //      (FLAG: their producer-side names are not recovered -- roles from this switch). ----
    const s32 KI_ACTION_TO_COLOUR = 38;   // -> hand off to the colour-calibration screen
    const s32 KI_ACTION_BACK      = 45;   // -> leave the pause menu (resume)
    const s32 KI_ACTION_STOP      = 49;   // -> leave the pause menu (resume)
    const s32 KI_ACTION_START     = 50;   // -> leave the pause menu (resume)

    // The GuiEvent<533> command the pause screen posts on its out-queue when the pause
    // menu closes back into the game (the sibling of the GuiEvent<532> record
    // BrnPausedHudState posts on enter). The X360 fills { muHeader0 = 1, muEventType = 533,
    // muHeader2 = 12 } and pushes a 16-byte record on channel 40; the trailing word is
    // left uninitialised.
    struct GuiEventPauseScreenClosed : public CgsGui::GuiEvent<533>
    {
        u32 muReserved;   // +0x0C (X360 leaves this gap word uninitialised; record size 16)

        GuiEventPauseScreenClosed() : CgsGui::GuiEvent<533>(1, 12) {}
    };
}

// @ 0x824CFE78
void PauseScreen::HandleControllerInput(const CgsModule::Event* lpEvent)
{
    if (meSubState != E_SUBSTATE_PROMPT)
        return;

    // The action sub-id rides in the event's +4 payload word (same idiom as the boot
    // states -- see BrnBootLegal.cpp's in-queue drain).
    const s32 liAction =
        *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4);

    switch (liAction)
    {
    case KI_ACTION_TO_COLOUR:
        SendStateEvent("TO_COLOUR");
        break;

    case KI_ACTION_BACK:
    case KI_ACTION_STOP:
    case KI_ACTION_START:
    {
        // The pause menu carries exactly one option (KAPC_PAUSE_OPTION_STRING_IDS), so any
        // non-zero highlighted index is invalid (X360 cpp:277) -- fire the assert and do
        // NOT run the resume path (the X360 returns straight out of the assert branch).
        if (mPauseOptions.miHighlightedIndex == 0)
        {
            // Wake the CrashNav (front-end map) flow, lift the network suspension, post
            // the pause-closed command record, and back out of this screen.
            GuiEventActivateCrashNav lActivateCrashNav(true);
            mpStateInterface->OutputGuiEvent(lActivateCrashNav);

            CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
            mpStateInterface->OutputGuiEvent(lNetworkSuspension);

            GuiEventPauseScreenClosed lPauseClosed;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lPauseClosed), 40, 16);

            SendStateEvent("GO_BACK");
        }
        else
        {
            CGS_ASSERT(false, "Invalid pause option selected");
        }
        break;
    }

    default:
        break;
    }
}
}
