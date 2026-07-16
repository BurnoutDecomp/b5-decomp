#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // brings BrnGuiEventTypeDefs + PlayerName
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"         // GuiEventStuntAreaComplete / GuiEventFailedToStartEvent
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"   // remaining opaque event placeholders (frozen set)
#include "GameSource/Gui/BrnGuiCache.h"                 // GuiCache (frozen-header include set)

#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"          // CgsID / CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SnPrintf

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (wave-B partfile 11). Stunt-area completion + burning-route start-failure chatter,
// driven from Update's event drain:
//   HandleStuntsComplete(AreaComplete)      @0x8251F6E8  (cpp:5147-)
//   HandleFailedToStartBurningRoute         @0x8251D1A0  (cpp:3147-)
//
// The third fan-out function -- HandleStuntPerformed @0x8251B608 -- is reported blocked
// (see funcs_blocked): its skill-line body calls the undeclared
// CgsLanguage::LanguageManager::FormatTextFromInt.

namespace BrnGui
{

// @ 0x8251F6E8
// "All stunts of one collectible family are done in this county" -- fire "StntAllDone"
// tagged with the family-completion string id and the county string id (both looked up
// in the committed X360 rodata tables). Bounds-asserts both indices first.
void HudMessageAnalyzer::HandleStuntsComplete(const GuiEventStuntAreaComplete* lpEvent)
{
    // Non-gating tripwires (cpp:5147/5151/5152; all streamed on the X360, folded static).
    CGS_ASSERT(lpEvent != NULL, "lpStuntAreaCompleteEvent");
    CGS_ASSERT(lpEvent->meStuntElementType < E_STUNTTYPE_COUNT,
               "lpStuntAreaCompleteEvent->meStuntElementType < E_STUNTTYPE_COUNT");
    CGS_ASSERT(lpEvent->meCounty < BrnWorld::E_COUNTY_VALID_COUNT,
               "lpStuntAreaCompleteEvent->meCounty < BrnWorld::E_COUNTY_VALID_COUNT");

    GuiHudMessage lMessage;
    lMessage.Construct("StntAllDone");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0,
                      KAPC_COLLECTABLE_COMPLETION_STRINGID[lpEvent->meStuntElementType]);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1,
                      KAPC_COUNTY_STRINGID[lpEvent->meCounty]);
    TriggerMessage(&lMessage);
}

// @ 0x8251D1A0
// The burning route could not start because the player is not in the required special
// car -- fire "BRWrongCar" naming the wanted car ("CAR_CAPS_<id>", by string id).
void HudMessageAnalyzer::HandleFailedToStartBurningRoute(const GuiEventFailedToStartEvent* lpEvent)
{
    // Non-gating tripwires (cpp:3147/3148; both streamed on the X360, folded static).
    CGS_ASSERT(lpEvent != NULL, "lpEvent");
    CGS_ASSERT(lpEvent->mSpecialCarID != static_cast<CgsID>(0),
               "kCGSID_NULL != lpEvent->mSpecialCarID");

    GuiHudMessage lMessage;
    lMessage.Construct("BRWrongCar");

    char lacCarId[16];
    CgsIDConvertToString(lpEvent->mSpecialCarID, lacCarId);

    char lacCarName[32];
    CgsCore::SnPrintf(lacCarName, 32, "CAR_CAPS_%s", lacCarId);   // the asm names SnPrintf here
    lacCarName[31] = '\0';                                // asm @0x8251D250

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1, lacCarName);
    TriggerMessage(&lMessage);
}

} // namespace BrnGui
