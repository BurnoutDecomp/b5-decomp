#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // brings BrnGuiEventTypeDefs + PlayerName

#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"          // CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf
#include "GameSource/Gui/BrnGuiCache.h"                 // GetGameMode / GetPursuitCarID

// BrnGui::HudMessageAnalyzer -- pursuit chatter, reconstructed from
// BURNOUT_X360_ARTIST.XEX.  Group hint 4.
//
// NOTE: HandleRivalryChangeEvent (@0x8251D270), HandleRivalFleeingEvent
// (@0x8251D430) and HandleSignatureStunt (@0x8251D7E8) are declared `const` in the
// frozen header but their X360 bodies tail-call the non-const
// TriggerMessage(const GuiHudMessage*) (BrnGuiHudMessageAnalyzer.h:139).  A const
// method cannot invoke that overload, so those three are reported blocked rather
// than hacked around; only the non-const HandleStartPursuitEvent is bodied here.

namespace BrnGui
{

// @ 0x8251D4A0 -- pursuit pre-announce ("EventPrePurs" + "CAR_CAPS_<id>").  The X360
// body takes no payload (the pursued car id comes from the cache).
void HudMessageAnalyzer::HandleStartPursuitEvent()
{
    CGS_ASSERT(mpGuiCache->GetGameMode() == 4,
               "GsmIO::E_MODE_PURSUIT == mpGuiCache->GetGameMode()");

    const CgsID lPursuitCarID = mpGuiCache->GetPursuitCarID();

    char lCarIdBuffer[16];
    CgsIDConvertToString(lPursuitCarID, lCarIdBuffer);
    lCarIdBuffer[12] = '\0';                              // asm @0x8251D51C

    char lCarNameBuffer[32];
    CgsCore::SPrintf(lCarNameBuffer, 32, "CAR_CAPS_%s", lCarIdBuffer);
    lCarNameBuffer[31] = '\0';                            // asm @0x8251D52C

    GuiHudMessage lMessage;
    lMessage.Construct("EventPrePurs");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0, lCarNameBuffer);
    TriggerMessage(&lMessage);
}

} // namespace BrnGui
