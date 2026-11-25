#pragma once

// ===================================================================================
// CgsNetwork::CompressionAndEncryptionUtils -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Packeting/CgsCompressionAndEncryptionUtils.h
//
// The per-player packet packer (NetworkPlayer::mPacketPacker) plus the class-static
// bandwidth ledger every packer shares.
//
// Packet layout Pack writes (all byte-aligned, in this order):
//   PackedPacketHeader (8 bytes) or ReliablePackedPacketHeader (16 bytes, when any packed
//   message is reliable), then one type byte per packed message, then the bit-packed
//   signal (ack/nack) messages rounded up to a byte, then the bit-packed message bodies.
//
// The ledger is indexed [send/recv][ledger entry + all][message type + header + all]:
// a ledger entry is a slot of maNetworkPlayerID (GetEntryIndex), row KI_ALL_PLAYERS is
// the all-players total, column KE_ALL_MESSAGE_TYPES the all-types total. The bounds the
// asm hard-codes give the enum sizes:
//   leSendRecv    in [0, 2)   -> E_SEND_RECV_COUNT      == 2
//   leAverageType in [0, 2)   -> E_AVERAGE_TYPE_COUNT   == 2
//   liPlayerIndex in [0, 9)   -> KI_ALL_PLAYERS + 1     == 9
//   liMessageType in [0, 46)  -> KE_ALL_MESSAGE_TYPES+1 == 46
// Ack/nack bits are booked under type KE_ACK_OR_NACK (0, the unused "not set" type) and
// the header + type bytes under KE_HEADER (44, one past the last real message type).
// ===================================================================================

#include "types.hpp"
#include "GameSource/Network/MessageTypes.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"

namespace CgsSystem { class TimerStatus; }

namespace CgsNetwork
{
    struct Message;
    struct SignalMessage;
    typedef s32 NetworkPlayerID;   // mirrors MessageWithPlayerIDs::NetworkPlayerID

    struct CompressionAndEncryptionUtils
    {
        // Enumerated keys into the bandwidth table.
        enum EAverageType
        {
            E_AVERAGE_TYPE_START   = 0,
            E_AVERAGE_OVER_SECOND  = 0,
            E_MAX_OVER_SECOND      = 1,
            E_AVERAGE_TYPE_COUNT   = 2,
        };
        enum ESendRecvBandwidth
        {
            E_SEND_RECV_START = 0,
            E_SEND_BANDWIDTH  = 0,
            E_RECV_BANDWIDTH  = 1,
            E_SEND_RECV_COUNT = 2,
        };

        static const s32 KI_ALL_PLAYERS         = 8;   // liPlayerIndex < KI_ALL_PLAYERS+1
        static const s32 KI_MAX_PLAYERS         = 8;   // ledger entries (maNetworkPlayerID)
        static const EMessageType KE_ACK_OR_NACK = E_MESSAGE_TYPE_NOT_SET;
        static const EMessageType KE_HEADER = E_MESSAGE_TYPE_COUNT;
        static const EMessageType KE_ALL_MESSAGE_TYPES = static_cast<EMessageType>(E_MESSAGE_TYPE_COUNT + 1);
        static const s32 KI_MAX_BANDWIDTH_HISTORY = 30;

        // Read one cell of the bandwidth ledger, rounded up to whole bytes.
        s32 GetBandwidthUsedInBytes(EAverageType leAverageType, ESendRecvBandwidth leSendRecv,
                                     s32 liPlayerIndex, s32 liMessageType)
        {
            return (GetBandwidthUsedInBits(leAverageType, leSendRecv, liPlayerIndex, liMessageType) + 7) / 8;
        }

        // Reset the whole ledger and restart the averaging clock.
        void ResetBandwidthUsed(const CgsSystem::TimerStatus* lpTimerStatus);
        // Once per K_TIME_FOR_AVERAGE: publish the period's counts, fold them into the
        // per-second maxima and the history ring, then clear the period.
        void Update(const CgsSystem::TimerStatus* lpTimerStatus);
        // Pack the valid messages of lpapSendMessages / lpapSignalMessages bound for
        // liPlayerID into lpu8Buffer (at most liMaxBytes); *lpiBytesPacked receives the packed
        // length. Every packed message is marked invalid. Returns false when a message did not
        // fit (it stays valid, so the player pump calls again for another packet).
        bool Pack(NetworkPlayerID liPlayerID,
                  Message** lpapSendMessages, s32 liNumSendMessages,
                  SignalMessage** lpapSignalMessages, s32 liNumSignalMessages,
                  u8* lpu8Buffer, s32 liMaxBytes, s32* lpiBytesPacked);

        // One registered receive slot handed to UnPack: the message a packed body of its type
        // is unpacked into, and the callback told about it (with its user data).
        struct RecvMessageData
        {
            typedef void OnMessageUnpackedCallback(Message* lpMsg, void* lpUserData);

            Message*                   mpMessage;               // +0x00
            OnMessageUnpackedCallback* mpMsgUnpackedCallback;   // +0x04
            void*                      mpCallbackUserData;      // +0x08
        };

        // Unpack a received packet from liPlayerID into the per-type receive slots and the
        // signal-message array.
        void UnPack(NetworkPlayerID liPlayerID, RecvMessageData* lpaRecvMessageData,
                    s32 liNumRecvMessageData, SignalMessage** lpapSignalMessages,
                    s32 liNumSignalMessages, u8* lpu8Buffer, s32 liBufferSize);

        // Read the sending player's id out of a packed packet's header.
        static NetworkPlayerID ExtractSendingPlayerID(u8* lpacBufferToUnPackFrom, s32 liBufferSize);

        // The packet header Pack writes first. Byte-for-byte the wire layout (8 bytes).
        struct PackedPacketHeader
        {
            u8  mx8Flags;           // +0x00  (bit 1: reliable header follows)
            u8  mu8GameID;          // +0x01
            u16 mu16Frame;          // +0x02
            u8  mu8NumTypes;        // +0x04
            u8  mu8NumSignalMsgs;   // +0x05
            u8  mu8_Pad[2];         // +0x06
        };

        // The header used when any packed message is reliable (16 bytes).
        struct ReliablePackedPacketHeader : PackedPacketHeader
        {
            NetworkPlayerID mSendingPlayerID;   // +0x08
            NetworkPlayerID mRecvingPlayerID;   // +0x0C
        };

    private:
        void UpdateMaxBandwidth();
        void UpdateBandwidthHistory();
        s32 GetBandwidthUsedInBits(EAverageType leAverageType, ESendRecvBandwidth leSendRecv,
                                    s32 liPlayerIndex, s32 liMessageType);
        void RecordBitsTransmitted(ESendRecvBandwidth leSendRecv, NetworkPlayerID liPlayerID,
                                   EMessageType leMessageType, s32 liBits);
        s32  GetEntryIndex(NetworkPlayerID liPlayerID);

        u8* mpu8TmpBuffer;   // +0x00

        // ---- class-static ledger (shared by every player's packer) ----
        static NetworkPlayerID maNetworkPlayerID[KI_MAX_PLAYERS];
        static s32 maaaiBitsTransmitted[E_SEND_RECV_COUNT][KI_ALL_PLAYERS + 1]
                                       [KE_ALL_MESSAGE_TYPES + 1];
        static CgsSystem::Time mLastAverageTime;
        static s32 maaaaiAvgBitsTransmitted[E_AVERAGE_TYPE_COUNT][E_SEND_RECV_COUNT]
                                           [KI_ALL_PLAYERS + 1][KE_ALL_MESSAGE_TYPES + 1];
        static s32 miHistoricalIndex;
        static s32 maaaaiHistoricalBitsTransmitted[KI_MAX_BANDWIDTH_HISTORY][E_SEND_RECV_COUNT]
                                                  [KI_ALL_PLAYERS + 1][KE_ALL_MESSAGE_TYPES + 1];
    };

    inline s32 CompressionAndEncryptionUtils::GetBandwidthUsedInBits(
            EAverageType leAverageType, ESendRecvBandwidth leSendRecv, s32 liPlayerIndex, s32 liMessageType)
    {
        const char* lpcArtistHeader = "..\\..\\..\\GameShared\\GameClasses\\Network/Packeting/CgsCompressionAndEncryptionUtils.h";
        CGS_ASSERT_AT(leSendRecv < E_SEND_RECV_COUNT, "leSendRecv < E_SEND_RECV_COUNT", lpcArtistHeader, 281);
        CGS_ASSERT_AT(leSendRecv >= 0, "leSendRecv >= 0", lpcArtistHeader, 282);
        CGS_ASSERT_AT(leAverageType < E_AVERAGE_TYPE_COUNT, "leAverageType < E_AVERAGE_TYPE_COUNT", lpcArtistHeader, 283);
        CGS_ASSERT_AT(leAverageType >= 0, "leAverageType >= 0", lpcArtistHeader, 284);
        CGS_ASSERT_AT(liPlayerIndex < KI_ALL_PLAYERS + 1, "liPlayerIndex < KI_ALL_PLAYERS+1", lpcArtistHeader, 285);
        CGS_ASSERT_AT(liPlayerIndex >= 0, "liPlayerIndex >= 0", lpcArtistHeader, 286);
        CGS_ASSERT_AT(liMessageType < KE_ALL_MESSAGE_TYPES + 1, "liMessageType < KE_ALL_MESSAGE_TYPES+1", lpcArtistHeader, 287);
        CGS_ASSERT_AT(liMessageType >= 0, "liMessageType >= 0", lpcArtistHeader, 288);

        return maaaaiAvgBitsTransmitted[leAverageType][leSendRecv][liPlayerIndex][liMessageType];
    }

    inline CompressionAndEncryptionUtils::ESendRecvBandwidth operator++(
        CompressionAndEncryptionUtils::ESendRecvBandwidth& leEnumIndex, int)
    {
        const char* lpcArtistHeader = "..\\..\\..\\GameShared\\GameClasses\\Network/Packeting/CgsCompressionAndEncryptionUtils.h";
        const CompressionAndEncryptionUtils::ESendRecvBandwidth lePreviousIndex = leEnumIndex;
        leEnumIndex = static_cast<CompressionAndEncryptionUtils::ESendRecvBandwidth>(leEnumIndex + 1);
        CGS_ASSERT_AT(leEnumIndex <= CompressionAndEncryptionUtils::E_SEND_RECV_COUNT,
                      "leEnumIndex <= CompressionAndEncryptionUtils::E_SEND_RECV_COUNT", lpcArtistHeader, 240);
        return lePreviousIndex;
    }

    CGS_STATIC_ASSERT((sizeof(CompressionAndEncryptionUtils::PackedPacketHeader) == 8),
                  "PackedPacketHeader is 8 wire bytes");
    CGS_STATIC_ASSERT((sizeof(CompressionAndEncryptionUtils::ReliablePackedPacketHeader) == 16),
                  "ReliablePackedPacketHeader is 16 wire bytes");
}
