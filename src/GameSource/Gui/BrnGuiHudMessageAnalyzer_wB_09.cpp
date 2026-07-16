#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint / gxMessageFilterFlags (join-lobby debug trace)
#include "GameSource/Gui/BrnGuiCache.h"                // GuiCache accessors (player info / disconnect table)
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusData (online name)

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX (wave-B).
//
// Bodied here (ledger group 9 -- lobby chatter):
//   HudMessageAnalyzer::HandlePlayerJoinedLobby    @0x8251F458
//   HudMessageAnalyzer::TriggerPlayerLeftLobby     @0x8251F5C0
//
// NOTE: HandleRemotePlayerDisconnect (@0x8251EBD0) is declared `const` in the frozen
// header, but its X360 body tail-calls the non-const TriggerMessage(const
// GuiHudMessage*) (BrnGuiHudMessageAnalyzer.h:139) -- a const method cannot invoke
// that overload.  Same header const/non-const gap that blocks the const handlers in
// _wB_04.cpp; reported blocked rather than hacked around (no const_cast).

namespace BrnGui
{

// CgsNetwork::K_INVALID_PLAYER_ID -- the X360 'no network player' sentinel.
static const s32 KI_INVALID_PLAYER_ID = -1;

// @ 0x8251F458
// Freeburn lobby-join chatter: a new remote player entered the lobby -- announce
// "OnlFBJoin" tagged with their online name.
void HudMessageAnalyzer::HandlePlayerJoinedLobby(const GuiEventNetworkPlayerJoinedLobby* lpEvent)
{
    // Non-gating pointer/range tripwires (cpp:5032-5034).
    CGS_ASSERT(lpEvent != NULL, "lpEvent");
    CGS_ASSERT(lpEvent->mNetworkPlayerID != KI_INVALID_PLAYER_ID,
               "lpEvent->mNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    // Debug trace of the joining player id (log-category gated).
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "NetworkPlayerID: " << lpEvent->mNetworkPlayerID << "\n";
    }

    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerData =
        mpGuiCache->GetOnlinePlayerInfoFromPlayerId(lpEvent->mNetworkPlayerID);
    CGS_ASSERT(lpPlayerData != NULL, "lpPlayerData");

    GuiHudMessage lMessage;
    lMessage.Construct("OnlFBJoin");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lpPlayerData->mPlayerName.macName);
    TriggerMessage(&lMessage);
}

// @ 0x8251F5C0
// Fire the parked "player left the lobby" message: "OnlFBLeave" tagged with the
// stored name, then clear the pending flag + name.
void HudMessageAnalyzer::TriggerPlayerLeftLobby()
{
    CGS_ASSERT(mbOnlinePlayerLeft == true, "mbOnlinePlayerLeft == true");   // cpp:5079

    GuiHudMessage lMessage;
    lMessage.Construct("OnlFBLeave");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, mOnlinePlayerLeftName.macName);
    TriggerMessage(&lMessage);

    mbOnlinePlayerLeft = false;
    mOnlinePlayerLeftName.macName[0] = 0;
}

}
