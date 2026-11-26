#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnlineMod.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // GuiEventActivateCrashNav (id 191)
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                    // GuiEventShowHideHud (id 148)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"

// BrnGui::CrashNavEnterOnlineNoTitle / CrashNavEnterOnlineFull -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (2 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnlineMod.cpp):
//   CrashNavEnterOnlineNoTitle::OnEnter @0x824B6628   (Mod.cpp:61)
//   CrashNavEnterOnlineFull::OnEnter    @0x824CB0A8   (Mod.cpp:38)
//
// ⚠️ progress/identity.json gives CrashNavEnterOnlineFull::OnEnter the primary_file
// GameShared/GameClasses/Gui/CgsGuiEvent.h. That is the TU-path misattribution this
// project has already been bitten by: the header the ledger names is the one whose
// INLINE (the GuiEvent<N> record builder) got folded into the body. The DWARF puts the
// function at Mod.h:59's class, Mod.cpp:38 -- which BrnCrashNavEnterOnlineMod.h's own
// declaration comment already says -- and that is where it lands.

namespace BrnGui
{
namespace
{
    const s32 KI_CHANNEL_GUI_OUT = 40;
}

// @ 0x824CB0A8 -- the FULL (titled) sign-in screen's entry.
//
// ⭐⭐ WHY THIS BODY MATTERS OUT OF PROPORTION TO ITS SIZE. CN_ENTER_ON(124) is the only
// state the shipped FSM routes CN_SETTINGS's "TO_ACCT_MAN" row to -- the settings row
// does NOT go straight to CN_ACCT_MAN(123); the Lua reads
//     Transition_8CN_SETTINGS_124CN_ENTER_ON()      then
//     NextState_124CN_ENTER_ON: "ADVANCE" -> Transition_124CN_ENTER_ON_123CN_ACCT_MAN()
// so this screen is the gate in front of the account-management tab.
void CrashNavEnterOnlineFull::OnEnter()
{
    CrashNavEnterOnlineX360::OnEnter();

    meSignInType = E_SIGN_IN_TYPE_FULL;   // stw 0, 0xCC(this) -- the NoTitle sibling
                                          // stores 1 at the same offset, which is what
                                          // pins this store to the same member.

    // Stand the crash nav down while the sign-in screen owns the display: the record is
    // { 8, 191, 12 } with BOTH payload words zero, i.e. GuiEventActivateCrashNav(false).
    // The X360 builds it inline (`stw 0, 0x50(r1)` / `stw 0, 0x54(r1)` then the `ld`/`std`
    // pair that copies the zero qword into the payload slot) and posts it on channel 40
    // with size 0x14; spelled through the committed type so the queued bytes are the same.
    GuiEventActivateCrashNav lActivateCrashNav;
    lActivateCrashNav.muActivate = 0u;
    lActivateCrashNav.muParam = 0;
    mpStateInterface->OutputGuiEvent(lActivateCrashNav);

    // Hide the in-race HUD header: { 1, 148, 12 } with the payload byte clear, channel 40,
    // 16 bytes (`stb 0, 0x5C(r1)` @0x824CB128).
    GuiEventShowHideHud lShowHideHudEvent;
    lShowHideHudEvent.mbShow = false;
    mpStateInterface->OutputGuiEvent(lShowHideHudEvent);
}

// @ 0x824B6628
void CrashNavEnterOnlineNoTitle::OnEnter()
{
    CrashNavEnterOnlineX360::OnEnter();
    meSignInType = E_SIGN_IN_TYPE_NO_TITLE;   // stw 1, 0xCC(this)
}

}
