#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                  // ReceiveMessage, GetConnectionData
#include "GameShared/GameClasses/Network/Packeting/CgsNetworkAdapterBase.h"          // ReceiveFrom, ConnectionData
#include "GameShared/GameClasses/Network/Packeting/CgsCompressionAndEncryptionUtils.h" // ExtractSendingPlayerID, header layout
#include "GameShared/GameClasses/Network/CgsNetworkUtils.h"                          // IPAddressIntToString
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                            // CgsDev::Log::gpDebugPrint

#include <cstdint>   // uintptr_t

// CgsNetwork::PlayerManager::ReceiveMessages -- the per-frame receive pump.

namespace CgsNetwork
{

namespace
{
    // Packets received during the previous pump (reset every pump, logged when high).
    s32 s_iNumMessagesReceivedThisFrame = 0;

    const s32 KI_MANY_MESSAGES_RECEIVED_THIS_FRAME = 25;
}

// ---- ReceiveMessages -----------------------------------------------------------------
// For every remote player with a live link, drain the adapter and hand each packet to the
// player. While the disk is not accessible packets are read and dropped. A reliable packet
// whose header names a different sender than the player it arrived from is dumped to the
// log (the packet's header and every known player's connection data) and asserted.
void PlayerManager::ReceiveMessages()
{
    if (mpNetworkAdapter == nullptr)
    {
        return;
    }

    if (s_iNumMessagesReceivedThisFrame > KI_MANY_MESSAGES_RECEIVED_THIS_FRAME)
    {
        *CgsDev::Log::gpDebugPrint << "We received " << s_iNumMessagesReceivedThisFrame
                                   << " messages this frame, which is rather high\n";
    }
    s_iNumMessagesReceivedThisFrame = 0;

    NetworkPlayerID lPlayerId = K_INVALID_PLAYER_ID;
    while (GetNextPlayerID(&lPlayerId, E_CONSIDER_ALL_PLAYERS))
    {
        NetworkPlayer* lpNetPlayer = GetPlayerByID(lPlayerId);
        if (lpNetPlayer == nullptr)
        {
            continue;
        }

        ConnectionData lConnectionData = lpNetPlayer->GetConnectionData();
        if (!lConnectionData.IsValid())
        {
            continue;
        }

        for (;;)
        {
            void* lpMsgData;
            const s32 liBytesRecvd = mpNetworkAdapter->ReceiveFrom(&lpMsgData, lConnectionData);
            if (liBytesRecvd <= 0)
            {
                break;
            }
            if (!mbDiskAccessible)
            {
                continue;
            }

            ++s_iNumMessagesReceivedThisFrame;

            u8* lpu8MsgData = static_cast<u8*>(lpMsgData);
            const NetworkPlayerID lSendingPlayerIDFromMessageHeader =
                CompressionAndEncryptionUtils::ExtractSendingPlayerID(lpu8MsgData, liBytesRecvd);
            if (lSendingPlayerIDFromMessageHeader != K_INVALID_PLAYER_ID)
            {
                if (lSendingPlayerIDFromMessageHeader != lpNetPlayer->GetPlayerID())
                {
                    s32 liPlayerIndex = 0;
                    NetworkPlayerID lOutputPlayerId = K_INVALID_PLAYER_ID;
                    const CompressionAndEncryptionUtils::PackedPacketHeader* lpHeader =
                        reinterpret_cast<const CompressionAndEncryptionUtils::PackedPacketHeader*>(lpu8MsgData);
                    const u8* lpPackedTypes =
                        ((lpHeader->mx8Flags & KX8_FLAGS_RELIABLE) == KX8_FLAGS_RELIABLE)
                            ? lpu8MsgData + sizeof(CompressionAndEncryptionUtils::ReliablePackedPacketHeader)
                            : lpu8MsgData + sizeof(CompressionAndEncryptionUtils::PackedPacketHeader);
                    static const char* const KPAC_CONNECTION_TYPE_STRING[2] = { "Peer-peer", "GameServer" };

                    CgsDev::StrStreamBase& lLog = *CgsDev::Log::gpDebugPrint;
                    lLog << "Player ID in message doesn't match ID of player received from!\n";
                    lLog << "==============================================================\n\n";
                    lLog << "Message contains:\n";
                    lLog << "Sending player ID of " << lSendingPlayerIDFromMessageHeader
                         << " we expected " << lpNetPlayer->GetPlayerID() << "\n";
                    lLog << "Number of signal messages: " << static_cast<s32>(lpHeader->mu8NumSignalMsgs) << "\n";
                    lLog << "Number of normal messages: " << static_cast<s32>(lpHeader->mu8NumTypes) << "\n";
                    lLog << "Message types:\n";
                    for (u8 lu8MessageIndex = 0; lu8MessageIndex < lpHeader->mu8NumTypes; ++lu8MessageIndex)
                    {
                        lLog << static_cast<s32>(lpPackedTypes[lu8MessageIndex]) << "\n";
                    }
                    lLog << "\n";
                    lLog << "==============================================================\n\n";
                    lLog << "Active players: " << miNumActivePlayers << "\n\n";

                    while (GetNextPlayerID(&lOutputPlayerId, E_CONSIDER_ALL_PLAYERS))
                    {
                        NetworkPlayer*  lpOutputNetPlayer = GetPlayerByID(lOutputPlayerId);
                        PlayerMenuData* lpOutputMenuData  = GetMenuDataByID(lOutputPlayerId);

                        lLog << "Player: " << liPlayerIndex << "\n";
                        lLog << "Name: " << lpOutputMenuData->macName << "\n";
                        lLog << "Player ID: " << lOutputPlayerId << "\n";
                        lLog << "Local Player: " << (lpOutputNetPlayer != nullptr ? "no" : "yes") << "\n";

                        if (lpOutputNetPlayer != nullptr)
                        {
                            ConnectionData lOutputConnectionData = lpOutputNetPlayer->GetConnectionData();
                            if (!lOutputConnectionData.IsValid())
                            {
                                lLog << "Connection data not valid\n";
                            }
                            else
                            {
                                char lacIPAddress[17];
                                IPAddressIntToString(lacIPAddress, lOutputConnectionData.miIPAddress);
                                lLog << "External IP Address: " << lacIPAddress << "\n";
                                IPAddressIntToString(lacIPAddress, lOutputConnectionData.miLocalIPAddress);
                                lLog << "Internal IP Address: " << lacIPAddress << "\n";
                                lLog << "Game Port: " << lOutputConnectionData.muGamePort << "\n";
                                lLog << "Voip Port: " << lOutputConnectionData.muVoipPort << "\n";
                                lLog << "Game Local Port: " << lOutputConnectionData.muLocalGamePort << "\n";
                                lLog << "Game Mangle Port: " << lOutputConnectionData.muMnglGamePort << "\n";
                                lLog << "Voip Local Port: " << lOutputConnectionData.muLocalVoipPort << "\n";
                                lLog << "Voip Mangle Port: " << lOutputConnectionData.muMnglVoipPort << "\n";
                                // +0x30 holds the peer's DirtyAddrT text.
                                lLog << "DirtyAddrT: "
                                     << reinterpret_cast<const char*>(lOutputConnectionData.maReserved30) << "\n";
                                lLog << "Game connection type: "
                                     << KPAC_CONNECTION_TYPE_STRING[lOutputConnectionData.meGameConnectionType] << "\n";
                                lLog << "Voip connection type: "
                                     << KPAC_CONNECTION_TYPE_STRING[lOutputConnectionData.meVoipConnectionType] << "\n";
                                lLog << "NetGameLinkRef address " << CgsDev::E_PRINTMODE_HEX
                                     << static_cast<u32>(reinterpret_cast<uintptr_t>(lOutputConnectionData.mpNetGameLink))
                                     << CgsDev::E_PRINTMODE_HEXONCE << "\n";
                            }
                        }

                        lLog << "\n----------------------------------------------------------\n\n";
                        ++liPlayerIndex;
                    }

                    lLog << "==============================================================\n\n";
                }

                CGS_ASSERT(lSendingPlayerIDFromMessageHeader == lpNetPlayer->GetPlayerID(),
                           "lSendingPlayerIDFromMessageHeader == lpNetPlayer->GetPlayerID()");
            }

            lpNetPlayer->ReceiveMessage(lpu8MsgData, liBytesRecvd, lpNetPlayer->GetPlayerID());
        }
    }
}

}
