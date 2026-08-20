#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // brings BrnGuiEventTypeDefs + PlayerName
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"         // the four HUD-message payloads below
#include "GameSource/Gui/BrnGuiCache.h"                 // GuiCache (friend access + GetOnlinePlayerInfoFromPlayerId)
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusData
#include "GameSource/GameState/BrnGameStateSharedIO.h"  // GameStateModuleIO::EGameModeType

#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (gateui wave partfile 02). Six more of the fan-out's declaration-only arms, landed to
// shrink the analyzer's link surface: every one of these is dispatched by the committed
// Update switch in _wB_12, so each missing body was an LNK2019 standing between this tree
// and a mountable HudMessageAnalyzer. They do NOT close that surface on their own -- the
// measured residual after this partfile was twelve more bodyless Handle*/Trigger* arms
// (rounds 2 and 3 landed them in _gUI_03 / _gUI_04).
// [gateui r3] The `report_r2_guipump.md` this line used to point at was never written; the
// dumpbin UNDEF sweep that measures the surface is in
// scratch/gateui_wave/verify_r2_guipump/VERDICT.md (section 2, WRONG-2).
//
//   HandleComboPerformed            @0x8251BE40  (31 insns)
//   HandleShowtimeMultiplierMessage @0x8251BEC0  (31 insns)
//   HandleCrushComboMessage         @0x8251BFA8  (25 insns)
//   HandleAllEventsOfTypeComplete   @0x82520318  (38 insns)
//   HandleAllJunctionsOfTypeFound   @0x825203D0  (38 insns)
//   HandlePlayerLeftLobby           @0x824F3348  (37 insns, cpp:5059)
//   GetOnlineName(s32, bool*)       @0x824ECF10  (the player-id keyed overload)

namespace BrnGui
{

namespace
{
    // The two metres-to-display-unit factors the boundary handlers below bake in
    // (X360 rodata flt_8203AF08 / flt_82013F90).
    const f32 KF_METRES_TO_MILES      = 0.0006213712f;
    const f32 KF_METRES_TO_KILOMETRES = 0.001f;
}

// @ 0x824ECF10
// The network-player-id keyed sibling of the EActiveRaceCarIndex overload in the class
// TU. Where that one linear-scans the cache's eight player records by car index, this one
// hands the id to the cache's own lookup and reads the name off the record it returns
// (X360 `sub_82482738(mpGuiCache, id)` == GuiCache::GetOnlinePlayerInfoFromPlayerId, then
// `record + 256` == mPlayerName). PITFALL, per the header: pass a genuine s32 player id --
// an EActiveRaceCarIndex argument silently binds the OTHER overload.
const char* HudMessageAnalyzer::GetOnlineName(s32 liNetworkPlayerID, bool* lpbValid) const
{
    // Non-gating tripwires (h:827/828).
    CGS_ASSERT(liNetworkPlayerID != -1, "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
        mpGuiCache->GetOnlinePlayerInfoFromPlayerId(liNetworkPlayerID);

    if (lpPlayerInfo != NULL)
    {
        *lpbValid = true;
        return lpPlayerInfo->mPlayerName.macName;
    }

    // No matching player: empty name, flagged invalid (callers skip the message).
    *lpbValid = false;
    return "";
}

// @ 0x8251BE40
// Crash-mode combo popup. The BEST-SCORE byte picks the boosted message id; the score
// itself rides as an INT param of display string 1.
void HudMessageAnalyzer::HandleComboPerformed(const GuiHUDMessageComboPerformed* lpEvent)
{
    GuiHudMessage lMessage;
    lMessage.Construct(lpEvent->mbBestScore ? "StuntCmbBst" : "StuntCmb");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1, lpEvent->miScore);
    TriggerMessage(&lMessage);
}

// @ 0x8251BEC0
// Crash-mode multiplier popup, EDGE-TRIGGERED: it only fires when the incoming multiplier
// differs from the one the analyzer last showed, and the latch is updated AFTER the
// message goes out (X360 `lwz r11,0(r30); stw r11,0x4B0(r31)` @0x8251BF1C, i.e. re-read
// from the event, not from a saved register). Note the value SHOWN is the EARNED field
// (+0x04) while the value LATCHED is the NEW multiplier (+0x00) -- they are different
// words; do not collapse them.
void HudMessageAnalyzer::HandleShowtimeMultiplierMessage(const GuiHUDMessageShowtimeMultiplier* lpEvent)
{
    if (lpEvent->miNewMultiplier == miLastSignMultiplier)
        return;

    GuiHudMessage lMessage;
    lMessage.Construct("ShowSmash");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1, lpEvent->miMultiplierEarned);
    TriggerMessage(&lMessage);

    miLastSignMultiplier = lpEvent->miNewMultiplier;
}

// @ 0x8251BFA8
// Crash-mode crush-combo popup ("ShowCombo" + the count).
void HudMessageAnalyzer::HandleCrushComboMessage(const GuiHUDMessageCrushCombo* lpEvent)
{
    GuiHudMessage lMessage;
    lMessage.Construct("ShowCombo");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1, lpEvent->miCrushComboCount);
    TriggerMessage(&lMessage);
}

// @ 0x82520318
// "Every event of this mode is complete" announce. The X360 jump table covers exactly five
// of the eighteen EGameModeType values (0/3/5/7/8 -- the five offline event families that
// have city-wide completion); cases 1/2/4/6 and everything above 8 fall through the
// default arm and fire NOTHING. Faithful: no default message, no assert.
void HudMessageAnalyzer::HandleAllEventsOfTypeComplete(
        BrnGameState::GameStateModuleIO::EGameModeType leGameModeType)
{
    const char* lpcMessageId = NULL;
    switch (leGameModeType)
    {
        case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE:  lpcMessageId = "EvryRaceCmp";  break;
        case BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE:     lpcMessageId = "EvryRRCmp";    break;
        case BrnGameState::GameStateModuleIO::E_MODE_BURNING_ROUTE: lpcMessageId = "EvryBrnRCmp";  break;
        case BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK:  lpcMessageId = "EvryStuntCmp"; break;
        case BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN:    lpcMessageId = "EvryMMCmp";    break;
        default:                                                    return;
    }

    GuiHudMessage lMessage;
    lMessage.Construct(lpcMessageId);
    TriggerMessage(&lMessage);
}

// @ 0x825203D0
// The junction ("event start location") twin of the above -- same five modes, same
// fall-through-does-nothing default, different message-id family.
void HudMessageAnalyzer::HandleAllJunctionsOfTypeFound(
        BrnGameState::GameStateModuleIO::EGameModeType leGameModeType)
{
    const char* lpcMessageId = NULL;
    switch (leGameModeType)
    {
        case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE:  lpcMessageId = "EvryRaceJnc";  break;
        case BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE:     lpcMessageId = "EvryRRJnc";    break;
        case BrnGameState::GameStateModuleIO::E_MODE_BURNING_ROUTE: lpcMessageId = "EvryBrnRJnc";  break;
        case BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK:  lpcMessageId = "EvryStuntJnc"; break;
        case BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN:    lpcMessageId = "EvryMMJnc";    break;
        default:                                                    return;
    }

    GuiHudMessage lMessage;
    lMessage.Construct(lpcMessageId);
    TriggerMessage(&lMessage);
}

// @ 0x824F3348
// A remote player left the freeburn lobby. This handler does NOT fire the message: it only
// PARKS the departing player's name and raises the pending flag, and the committed
// TriggerPlayerLeftLobby @0x8251F5C0 (in the class TU) fires "OnlFBLeave" from Update's
// deferred pass. Both gates are load-bearing -- the local player must be in-game
// (`lbz 0x14(event)`) and the cache's game mode must be exactly
// E_MODE_ONLINE_FREE_BURN_LOBBY (`cmpwi r11, 0xF` against mpGuiCache+0x9E58).
void HudMessageAnalyzer::HandlePlayerLeftLobby(const GuiEventNetworkPlayerLeftLobby* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "lpEvent");   // cpp:5059

    if (!lpEvent->mbIsLocalPlayerInGame)
        return;

    if (mpGuiCache->meGameModeType != BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY)
        return;

    mbOnlinePlayerLeft = true;
    mOnlinePlayerLeftName.Construct(lpEvent->mPlayerName.macName);
}

// @ 0x8251C270 / @ 0x8251C318
// Online road-rage progress chatter: "<leader> is N miles/km from the finish". Both are
// the same 42-instruction shape -- resolve the leader's online name, append it as STRING
// param 0, and only if the name RESOLVED append the distance as FLOAT param 0 and fire.
// (An unresolved name aborts the whole message: the X360 falls straight out past both the
// float param and TriggerMessage, `beq cr6, loc_8251C300`.)
//
// FLAG (console defect, reproduced verbatim): BOTH handlers construct the SAME message id
// "OnlRRMiToFin" -- the KM variant never gets a kilometre-specific line. The two differ
// only in the metres conversion factor (flt_8203AF08 == 0.0006213712 miles/m at
// 0x8251C2D4, flt_82013F90 == 0.001 km/m at 0x8251C37C). This is the shipped behaviour;
// do not "fix" it here.
void HudMessageAnalyzer::HandleLeaderPassedMileBoundary(const GuiLeaderPassedMileBoundaryEvent* lpEvent) const
{
    GuiHudMessage lMessage;
    lMessage.Construct("OnlRRMiToFin");

    bool lbNameValid = true;   // the X360 seeds the out-flag true before the call
    const char* lpcOnlineName = GetOnlineName(lpEvent->meActiveRaceCarIndex, &lbNameValid);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lpcOnlineName);

    if (!lbNameValid)
        return;

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_FLOAT, 0,
                      lpEvent->mfDistanceToFinishInMetres * KF_METRES_TO_MILES);
    TriggerMessage(&lMessage);
}

void HudMessageAnalyzer::HandleLeaderPassedKMBoundary(const GuiLeaderPassedKMBoundaryEvent* lpEvent) const
{
    GuiHudMessage lMessage;
    lMessage.Construct("OnlRRMiToFin");   // FLAG: console reuses the MILE id -- see above.

    bool lbNameValid = true;
    const char* lpcOnlineName = GetOnlineName(lpEvent->meActiveRaceCarIndex, &lbNameValid);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lpcOnlineName);

    if (!lbNameValid)
        return;

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_FLOAT, 0,
                      lpEvent->mfDistanceToFinishInMetres * KF_METRES_TO_KILOMETRES);
    TriggerMessage(&lMessage);
}

} // namespace BrnGui
