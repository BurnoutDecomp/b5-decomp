#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnlineMod.h"

// BrnGui::CrashNavEnterOnlineNoTitle -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, DWARF primary file
// GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnlineMod.cpp):
//   CrashNavEnterOnlineNoTitle::OnEnter @0x824B6628

namespace BrnGui
{

// @ 0x824B6628
void CrashNavEnterOnlineNoTitle::OnEnter()
{
    CrashNavEnterOnlineX360::OnEnter();
    meSignInType = E_SIGN_IN_TYPE_NO_TITLE;   // stw 1, 0xCC(this)
}

}
