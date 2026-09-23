#include "GameShared/GameClasses/Network/VoIP/DirtySock/CgsVoIPManagerDirtySock.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Network/CgsNetworkManager.h"
#include "GameShared/GameClasses/Network/CgsNetworkUtils.h"
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceGamesX360.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::VoIPClient::operator=        @ 0x82893618
//   CgsNetwork::VoIPManager::GetRegisteredTalker    @ 0x8287E4D8
//   CgsNetwork::VoIPManager::DisableCommsWithPlayer @ 0x828937B8
//   CgsNetwork::VoIPManager::EnableAllComms         @ 0x82893868
//
// The Voip* entry points (voice ref, headset flags, speaker / microphone routing)
// come from voip.h through the manager's header. The mute-list query is a
// platform call; its shape is taken from the call site.
extern "C"
{
    u32  XUserMuteListQuery(u32 luUserIndex, u64 luXuidRemoteTalker, s32* lpbOnMuteList);
}

namespace CgsNetwork
{
    // ---- VoIPClient::operator= @ 0x82893618 -----------------------------------
    //
    // The X360 codegen is the compiler-synthesised memberwise copy-assignment:
    // the scalar prefix (mPlayerID/miConnectionID/mbIsBlocked plus the +9 byte)
    // followed by the two HeadsetStatusMessage members copied field-for-field
    // through the Message base. The reconstruction assigns each member by name.
    VoIPClient& VoIPClient::operator=(const VoIPClient& arOther)
    {
        mPlayerID       = arOther.mPlayerID;
        miConnectionID  = arOther.miConnectionID;
        mbIsBlocked     = arOther.mbIsBlocked;
        mbCanTalk       = arOther.mbCanTalk;
        mHeadsetSendMsg = arOther.mHeadsetSendMsg;
        mHeadsetRecMsg  = arOther.mHeadsetRecMsg;
        return *this;
    }

    // ---- GetRegisteredTalker @ 0x8287E4D8 -------------------------------------
    //
    // Linear-scan the seven-entry maRegisteredTalkers array for the talker whose
    // mPlayerID matches liPlayerID. On a hit return &maRegisteredTalkers[i]; if
    // the scan exhausts all seven slots, fire the assert and return null.
    //
    // The X360 loop walks a raw pointer from (this+0x18) in 0x54-byte strides,
    // comparing the word at each slot's +0 (mPlayerID) against liPlayerID; the
    // matched address it returns is (this + 0x18 + 0x54*i), i.e. &maRegisteredTalkers[i].
    VoIPClient* VoIPManager::GetRegisteredTalker(VoIPNetworkPlayerID liPlayerID)
    {
        for (s32 liIndex = 0; liIndex < 7; ++liIndex)
        {
            if (maRegisteredTalkers[liIndex].mPlayerID == liPlayerID)
                return &maRegisteredTalkers[liIndex];
        }

        CGS_ASSERT(false, "Failed to find registered talker\n");
        return nullptr;
    }

    // ---- DisableCommsWithPlayer @ 0x828937B8 ----------------------------------
    //
    // Find the talker for liPlayerID, clear its mbCanTalk flag, then mute the
    // player: route the speaker to their connection mask and mute the microphone
    // toward them (the mic is only fed the mask when the local player actually has
    // a headset, otherwise 0 -- i.e. we never transmit if we have no mic).
    //
    // The X360 code forwards this function's r4 (liPlayerID) straight into
    // GetRegisteredTalker; if no talker is found (or on the mpVoiceRef assert
    // path) it returns that null result unchanged.
    VoIPClient* VoIPManager::DisableCommsWithPlayer(VoIPNetworkPlayerID liPlayerID)
    {
        VoIPClient* lpTalker = GetRegisteredTalker(liPlayerID);
        if (lpTalker)
        {
            lpTalker->mbCanTalk = false;
            if (mpVoiceRef)
            {
                u32 lu32ConnectionMask = GetConnectionMask();
                CGS_ASSERT(mpVoiceRef, "mpVoiceRef");

                VoipSpeaker(mpVoiceRef, lu32ConnectionMask);

                u32 lu32MicMask = 0;
                if (mbLocalPlayerHasHeadset)
                    lu32MicMask = lu32ConnectionMask;
                VoipMicrophone(mpVoiceRef, lu32MicMask);
            }
        }
        return lpTalker;
    }

    // ---- EnableAllComms @ 0x82893868 ------------------------------------------
    //
    // Re-enable voice for every registered talker: set mbCanTalk on all seven
    // slots, then (if voice is up) restore full speaker + headset-gated microphone
    // routing to the current connection mask.
    VoIPManager* VoIPManager::EnableAllComms()
    {
        for (s32 liIndex = 0; liIndex < 7; ++liIndex)
            maRegisteredTalkers[liIndex].mbCanTalk = true;

        if (mpVoiceRef)
        {
            u32 lu32ConnectionMask = GetConnectionMask();
            CGS_ASSERT(mpVoiceRef, "mpVoiceRef");

            VoipSpeaker(mpVoiceRef, lu32ConnectionMask);

            u32 lu32MicMask = 0;
            if (mbLocalPlayerHasHeadset)
                lu32MicMask = lu32ConnectionMask;
            VoipMicrophone(mpVoiceRef, lu32MicMask);
        }
        return this;
    }

    // ---------------------------------------------------------------------------
    // The talker slots, the headset-status message type registered with each remote
    // player, the two VoipControl selectors ('port' and 'plvl' as four-character
    // codes) and the two VoipLocal flag bits the headset queries test.
    static const s32 KI_NUM_TALKERS                   = 7;
    static const s32 KI_E_MESSAGE_TYPE_HEADSET_STATUS = 10;
    static const s32 KI_VOIP_CONTROL_PORT             = 0x706F7274;
    static const s32 KI_VOIP_CONTROL_VOLUME           = 0x706C766C;
    static const u32 KU_VOIP_LOCAL_HEADSET            = 0x40;
    static const u32 KU_VOIP_LOCAL_TALKING            = 0x100;

    // ---- ClearData --------------------------------------------------------------
    // No voice ref, no local headset state, unprepared, and every talker slot free
    // with both headset messages reset. The blocked flag is left alone.
    void VoIPManager::ClearData()
    {
        meVoIPOutput            = E_NETWORK_HEADSET_OUTPUT_MODE_COUNT;
        mpVoiceRef              = nullptr;
        mbIsVoiceAllowed        = false;
        mbLocalPlayerHasHeadset = false;
        mbIsLocalPlayerInGame   = false;
        mbIsChatRestricted      = false;
        mu8HeadsetStatus        = E_NETWORK_HEADSET_PLAYER_STATUS_NONE;
        meStatus                = E_VOIP_MANAGER_STATUS_UNPREPARED;

        for (s32 liIndex = 0; liIndex < KI_NUM_TALKERS; ++liIndex)
        {
            VoIPClient& lClient = maRegisteredTalkers[liIndex];
            lClient.mPlayerID      = K_INVALID_PLAYER_ID;
            lClient.miConnectionID = -1;
            lClient.mHeadsetSendMsg.Construct();
            lClient.mHeadsetRecMsg.Construct();
            lClient.mbCanTalk      = true;
        }
    }

    // ---- Prepare ----------------------------------------------------------------
    // Latch the session services, fetch the voice ref once, and hand it the active
    // controller port. Ready only when the voice layer accepts the port.
    bool VoIPManager::Prepare(PlayerManager* lpPlayerManager, TimeManager* lpTimeManager,
                              ServerInterface* lpServerInterface, BuddyManagerBase* lpBuddyManager,
                              bool lbIsVoiceAllowed)
    {
        mpPlayerManager               = lpPlayerManager;
        mpTimeManager                 = lpTimeManager;
        mpServerInterface             = lpServerInterface;
        mpBuddyManager                = lpBuddyManager;
        mbIsVoiceAllowed              = lbIsVoiceAllowed;
        mbIsLocalPlayerInGame         = false;
        mbGameIsHandlingHeadsetStatus = false;

        if (mpVoiceRef == nullptr)
        {
            // The original dev-logs the available memory before and after.
            mpVoiceRef = VoipGetRef();
        }

        if (VoipControl(mpVoiceRef, KI_VOIP_CONTROL_PORT, mpNetworkManager->GetActiveControllerPort()) == 0)
        {
            return false;
        }

        meStatus = E_VOIP_MANAGER_STATUS_READY;
        return true;
    }

    // ---- Release ----------------------------------------------------------------
    // Disconnect and unregister every talker, shut the voice layer down and go back
    // to unprepared.
    bool VoIPManager::Release()
    {
        for (s32 liIndex = 0; liIndex < KI_NUM_TALKERS; ++liIndex)
        {
            VoIPClient& lClient = maRegisteredTalkers[liIndex];
            if (lClient.mPlayerID == K_INVALID_PLAYER_ID)
            {
                continue;
            }

            if (lClient.miConnectionID != -1)
            {
                VoipDisconnect(mpVoiceRef, lClient.miConnectionID);
            }

            UnregisterMessage(lClient.mPlayerID);
            lClient.miConnectionID = -1;
            lClient.mPlayerID      = K_INVALID_PLAYER_ID;
            lClient.mbCanTalk      = true;
        }

        if (mpVoiceRef != nullptr)
        {
            VoipShutdown(mpVoiceRef);
            mpVoiceRef = nullptr;
        }

        meStatus                      = E_VOIP_MANAGER_STATUS_UNPREPARED;
        mbIsLocalPlayerInGame         = false;
        mbGameIsHandlingHeadsetStatus = false;
        return true;
    }

    // ---- Update -----------------------------------------------------------------
    void VoIPManager::Update(bool lbIsLocalPlayerInGame)
    {
        if (meStatus == E_VOIP_MANAGER_STATUS_UNPREPARED)
        {
            return;
        }

        CGS_ASSERT(mpVoiceRef, "mpVoiceRef");

        UpdateLocalPlayer();
        UpdateHeadsetStatusMessages(lbIsLocalPlayerInGame);
    }

    // ---- UpdateLocalPlayer ------------------------------------------------------
    // Derive our headset status from the voice layer (none while chat is
    // restricted), publish a change to our menu entry while in game, and re-route the
    // microphone when the headset comes or goes.
    void VoIPManager::UpdateLocalPlayer()
    {
        CGS_ASSERT(mpVoiceRef, "mpVoiceRef");

        const u32 liFlags = VoipLocal(mpVoiceRef);

        ENetworkHeadsetPlayerStatus leNewHeadsetStatus = E_NETWORK_HEADSET_PLAYER_STATUS_NONE;
        if (!mbIsChatRestricted && (liFlags & KU_VOIP_LOCAL_HEADSET) != 0)
        {
            leNewHeadsetStatus = (liFlags & KU_VOIP_LOCAL_TALKING) != 0
                                     ? E_NETWORK_HEADSET_PLAYER_STATUS_TALKING
                                     : E_NETWORK_HEADSET_PLAYER_STATUS_HAS_HEADSET;
        }

        if (mu8HeadsetStatus != leNewHeadsetStatus)
        {
            mu8HeadsetStatus = static_cast<u8>(leNewHeadsetStatus);

            if (mbIsLocalPlayerInGame)
            {
                NetworkPlayerID lLocalPlayerID;
                CGS_ASSERT(mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID),
                           "mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID)");
                mpPlayerManager->GetMenuDataByID(lLocalPlayerID)->mu8HeadsetStatus = mu8HeadsetStatus;
            }
        }

        const bool lbHasHeadset = DoesLocalPlayerHaveHeadset();
        if (lbHasHeadset != mbLocalPlayerHasHeadset)
        {
            mbLocalPlayerHasHeadset = lbHasHeadset;

            const u32 luMask = GetConnectionMask();
            VoipMicrophone(mpVoiceRef, mbLocalPlayerHasHeadset ? luMask : 0);
        }
    }

    // ---- UpdateHeadsetStatusMessages --------------------------------------------
    // Unless the game sends headset status itself, queue our status to each player
    // whose round-robin turn it is. Then consume every received status.
    void VoIPManager::UpdateHeadsetStatusMessages(bool lbIsLocalPlayerInGame)
    {
        if (!mbGameIsHandlingHeadsetStatus)
        {
            CGS_ASSERT(mpPlayerManager, "mpPlayerManager");
            CGS_ASSERT(mpTimeManager, "mpTimeManager");

            const u16 lu16FrameCount = mpTimeManager->GetU16FrameCount();

            NetworkPlayerID lPlayerID = K_INVALID_PLAYER_ID;
            while (mpPlayerManager->GetNextPlayerID(&lPlayerID, PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
            {
                if (mpPlayerManager->IsPlayerTurnToSendRoundRobinMessage(lPlayerID, lbIsLocalPlayerInGame, 0))
                {
                    HeadsetStatusMessage& lMessage = maRegisteredTalkers[GetIndexFromID(lPlayerID)].mHeadsetSendMsg;
                    if (!lMessage.IsMessageValid())
                    {
                        lMessage.PrepareForSend(lu16FrameCount, mu8HeadsetStatus);
                    }
                }
            }
        }

        for (s32 liIndex = 0; liIndex < KI_NUM_TALKERS; ++liIndex)
        {
            VoIPClient& lClient = maRegisteredTalkers[liIndex];
            if (lClient.mPlayerID == K_INVALID_PLAYER_ID)
            {
                continue;
            }

            u8 lu8HeadsetStatus;
            if (lClient.mHeadsetRecMsg.Retrieve(&lu8HeadsetStatus))
            {
                HandleReceivedHeadsetStatus(lClient.mPlayerID,
                                            static_cast<ENetworkHeadsetPlayerStatus>(lu8HeadsetStatus));
            }
        }
    }

    // ---- HandleReceivedHeadsetStatus --------------------------------------------
    // Show a remote player's headset status on their menu entry. A talking player we
    // have muted (or any talker while chat is restricted) shows as merely having a
    // headset.
    void VoIPManager::HandleReceivedHeadsetStatus(VoIPNetworkPlayerID lPlayerID,
                                                  ENetworkHeadsetPlayerStatus leHeadsetStatus)
    {
        NetworkPlayer* lpNetPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        if (lpNetPlayer == nullptr)
        {
            return;
        }

        if (leHeadsetStatus == E_NETWORK_HEADSET_PLAYER_STATUS_TALKING)
        {
            const s32 liControllerPort = mpNetworkManager->GetActiveControllerPort();

            u64 luXUID;
            static_cast<ServerInterfaceGamesX360*>(mpServerInterface->GetGameComponent())
                ->GetPlayerXUIDByID(lPlayerID, &luXUID);

            s32 lbIsMuted;
            XUserMuteListQuery(static_cast<u32>(liControllerPort), luXUID, &lbIsMuted);

            if (lbIsMuted != 0 || mbIsChatRestricted)
            {
                leHeadsetStatus = E_NETWORK_HEADSET_PLAYER_STATUS_HAS_HEADSET;
            }
        }

        mpPlayerManager->GetMenuDataByID(lPlayerID)->mu8HeadsetStatus = static_cast<u8>(leHeadsetStatus);
    }

    // ---- AddPlayer --------------------------------------------------------------
    // Our own player only marks us in game. A remote player takes a free talker slot
    // (once) and gets its headset-status messages registered.
    bool VoIPManager::AddPlayer(VoIPNetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(lPlayerID != K_INVALID_PLAYER_ID, "lPlayerID != K_INVALID_PLAYER_ID");
        CGS_ASSERT(meStatus != E_VOIP_MANAGER_STATUS_UNPREPARED, "meStatus != E_VOIP_MANAGER_STATUS_UNPREPARED");
        CGS_ASSERT(mpVoiceRef, "mpVoiceRef");

        if (mpPlayerManager->IsLocalPlayer(lPlayerID))
        {
            mbIsLocalPlayerInGame = true;
            return true;
        }

        for (s32 liIndex = 0; liIndex < KI_NUM_TALKERS; ++liIndex)
        {
            if (maRegisteredTalkers[liIndex].mPlayerID == lPlayerID)
            {
                return true;
            }
        }

        s32 liIndex;
        for (liIndex = 0; liIndex < KI_NUM_TALKERS; ++liIndex)
        {
            VoIPClient& lClient = maRegisteredTalkers[liIndex];
            if (lClient.mPlayerID == K_INVALID_PLAYER_ID)
            {
                lClient.mPlayerID      = lPlayerID;
                lClient.miConnectionID = -1;
                lClient.mbCanTalk      = true;
                break;
            }
        }
        CGS_ASSERT(liIndex < KI_NUM_TALKERS, "liIndex < KI_MAX_NETWORK_PLAYERS");

        RegisterMessage(lPlayerID);
        return true;
    }

    // ---- RemotePlayerFinalised --------------------------------------------------
    // A remote connection completed: refresh the voice connection ids and masks.
    void VoIPManager::RemotePlayerFinalised(ServerInterface* lpServerInterface, VoIPNetworkPlayerID)
    {
        UpdateConnectionIDsAndSendMask(lpServerInterface);
    }

    // ---- RemovePlayer -----------------------------------------------------------
    // Our own player only marks us out of game. A registered remote player frees its
    // slot and the voice masks are refreshed. False when unprepared or not found.
    bool VoIPManager::RemovePlayer(ServerInterface* lpServerInterface, VoIPNetworkPlayerID lPlayerID)
    {
        VoIPClient lClient;

        CGS_ASSERT(lPlayerID != K_INVALID_PLAYER_ID, "lPlayerID != K_INVALID_PLAYER_ID");

        if (meStatus == E_VOIP_MANAGER_STATUS_UNPREPARED)
        {
            return false;
        }

        CGS_ASSERT(mpVoiceRef, "mpVoiceRef");

        if (mpPlayerManager->IsLocalPlayer(lPlayerID))
        {
            mbIsLocalPlayerInGame = false;
            return true;
        }

        for (s32 liIndex = 0; liIndex < KI_NUM_TALKERS; ++liIndex)
        {
            lClient = maRegisteredTalkers[liIndex];
            if (lPlayerID == lClient.mPlayerID)
            {
                UnregisterMessage(lClient.mPlayerID);

                maRegisteredTalkers[liIndex].miConnectionID = -1;
                maRegisteredTalkers[liIndex].mPlayerID      = K_INVALID_PLAYER_ID;
                maRegisteredTalkers[liIndex].mbCanTalk      = true;

                UpdateConnectionIDsAndSendMask(lpServerInterface);
                return true;
            }
        }

        return false;
    }

    // ---- SetVoipVolume ----------------------------------------------------------
    void VoIPManager::SetVoipVolume(s32 liVoipVolumePercent)
    {
        miVoipVolumePercent = liVoipVolumePercent;

        if (mpVoiceRef != nullptr)
        {
            VoipControl(mpVoiceRef, KI_VOIP_CONTROL_VOLUME, liVoipVolumePercent);
        }
    }

    // ---- GetConnectionMask ------------------------------------------------------
    // One bit per voice connection of every registered, connected, unblocked talker
    // we may talk to. Nothing when voice is not allowed.
    u32 VoIPManager::GetConnectionMask()
    {
        VoIPClient lClient;
        u32        luMask = 0;

        if (!mbIsVoiceAllowed)
        {
            return 0;
        }

        for (s32 liIndex = 0; liIndex < KI_NUM_TALKERS; ++liIndex)
        {
            lClient = maRegisteredTalkers[liIndex];
            if (lClient.mPlayerID != K_INVALID_PLAYER_ID
                && lClient.miConnectionID != -1
                && !lClient.mbIsBlocked
                && lClient.mbCanTalk)
            {
                luMask |= 1u << lClient.miConnectionID;
            }
        }

        return luMask;
    }

    // ---- RegisterMessage --------------------------------------------------------
    // Register the talker's two headset-status messages with the player (no
    // callbacks).
    void VoIPManager::RegisterMessage(VoIPNetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(mpPlayerManager, "mpPlayerManager");
        CGS_ASSERT(lPlayerID != K_INVALID_PLAYER_ID, "lPlayerID != K_INVALID_PLAYER_ID");

        NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer == nullptr)
        {
            return;
        }

        const s32 liIndex = GetIndexFromID(lPlayerID);
        lpNetworkPlayer->RegisterMessageType(KI_E_MESSAGE_TYPE_HEADSET_STATUS, sizeof(HeadsetStatusMessage),
                                             &maRegisteredTalkers[liIndex].mHeadsetSendMsg,
                                             &maRegisteredTalkers[liIndex].mHeadsetRecMsg,
                                             nullptr, nullptr, nullptr);
    }

    // ---- UnregisterMessage ------------------------------------------------------
    void VoIPManager::UnregisterMessage(VoIPNetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(mpPlayerManager, "mpPlayerManager");

        NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer != nullptr)
        {
            lpNetworkPlayer->UnRegisterMessageType(KI_E_MESSAGE_TYPE_HEADSET_STATUS);
        }
    }

    // ---- DoesLocalPlayerHaveHeadset ---------------------------------------------
    // Never while voice is not allowed; otherwise the voice layer's headset bit.
    bool VoIPManager::DoesLocalPlayerHaveHeadset()
    {
        CGS_ASSERT(mpVoiceRef, "mpVoiceRef");

        if (!mbIsVoiceAllowed)
        {
            return false;
        }

        return (VoipLocal(mpVoiceRef) & KU_VOIP_LOCAL_HEADSET) != 0;
    }

    // ---- GetIndexFromID ---------------------------------------------------------
    // The talker slot holding the player. Slot 0 (after the assert) when missing.
    s32 VoIPManager::GetIndexFromID(VoIPNetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(lPlayerID != K_INVALID_PLAYER_ID, "lPlayerID != K_INVALID_PLAYER_ID");

        for (s32 liIndex = 0; liIndex < KI_NUM_TALKERS; ++liIndex)
        {
            if (maRegisteredTalkers[liIndex].mPlayerID == lPlayerID)
            {
                return liIndex;
            }
        }

        // The original follows the function name with ": PlayerID not found in VoIPManager".
        CGS_ASSERT(false, "CgsNetwork::VoIPManager::GetIndexFromID");
        return 0;
    }

    // ---- GetIndexFromPlayerName -------------------------------------------------
    // The talker slot whose player has the given name, or -1.
    s32 VoIPManager::GetIndexFromPlayerName(const char* lpcPlayerName) const
    {
        CGS_ASSERT(lpcPlayerName, "lpcPlayerName");

        for (s32 liIndex = 0; liIndex < KI_NUM_TALKERS; ++liIndex)
        {
            const VoIPNetworkPlayerID lPlayerID = maRegisteredTalkers[liIndex].mPlayerID;
            if (lPlayerID == K_INVALID_PLAYER_ID)
            {
                continue;
            }

            const char* lpcName = mpPlayerManager->GetPlayerByID(lPlayerID)->GetName();
            if (UsernameCompare(lpcPlayerName, lpcName) == 0)
            {
                return liIndex;
            }
        }

        return -1;
    }
}
