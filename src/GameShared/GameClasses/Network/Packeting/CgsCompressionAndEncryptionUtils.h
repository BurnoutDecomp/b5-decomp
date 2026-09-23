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
#include "GameShared/GameClasses/System/Timer/CgsTime.h"

namespace CgsSystem { class TimerStatus; }

namespace CgsNetwork
{
    struct Message;
    struct SignalMessage;
    typedef s32 NetworkPlayerID;   // mirrors MessageWithPlayerIDs::NetworkPlayerID

    struct CompressionAndEncryptionUtils
    {
        // Enumerated keys into the bandwidth table (counts taken from the asm bounds). The
        // second spelling of each value is the source name.
        enum EAverageType
        {
            E_AVERAGE_TYPE_INSTANT = 0,
            E_AVERAGE_TYPE_RUNNING = 1,
            E_AVERAGE_OVER_SECOND  = 0,
            E_MAX_OVER_SECOND      = 1,
            E_AVERAGE_TYPE_COUNT   = 2,
        };
        enum ESendRecv
        {
            E_SEND   = 0,
            E_RECV   = 1,
            E_SEND_BANDWIDTH = 0,
            E_RECV_BANDWIDTH = 1,
            E_SEND_RECV_COUNT = 2,
        };

        static const s32 KI_ALL_PLAYERS         = 8;   // liPlayerIndex < KI_ALL_PLAYERS+1
        static const s32 KI_MAX_PLAYERS         = 8;   // ledger entries (maNetworkPlayerID)
        static const s32 KE_ACK_OR_NACK         = 0;
        static const s32 KE_HEADER              = 44;
        static const s32 KE_ALL_MESSAGE_TYPES   = 45;  // liMessageType < KE_ALL_MESSAGE_TYPES+1
        static const s32 KI_MAX_BANDWIDTH_HISTORY = 30;

        // Read the accumulated bandwidth (in bits) for one (average-type, send/recv, player,
        // message-type) cell of the ledger.
        static s32 GetBandwidthUsedInBits(s32 leAverageType, s32 leSendRecv,
                                          s32 liPlayerIndex, s32 liMessageType);

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
        void RecordBitsTransmitted(ESendRecv leSendRecv, NetworkPlayerID liPlayerID,
                                   s32 leMessageType, s32 liBits);
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

    static_assert(sizeof(CompressionAndEncryptionUtils::PackedPacketHeader) == 8,
                  "PackedPacketHeader is 8 wire bytes");
    static_assert(sizeof(CompressionAndEncryptionUtils::ReliablePackedPacketHeader) == 16,
                  "ReliablePackedPacketHeader is 16 wire bytes");
}
