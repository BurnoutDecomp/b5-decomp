#include "GameShared/GameClasses/Network/Packeting/CgsCompressionAndEncryptionUtils.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // mNetStreamLogChannelOutput (0x82F335C8)
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"          // CgsSystem::TimerStatus
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsSignalMessage.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                         // CgsDev::Log::gpDebugPrint

#include <xtl.h>
#include <cstring>   // memcpy

// CgsNetwork::CompressionAndEncryptionUtils -- the packet packer and the shared bandwidth
// ledger.
//
// The ledger is class-static state (zero at startup; ResetBandwidthUsed seeds it):
//   maaaiBitsTransmitted             the current averaging period's counts
//   maaaaiAvgBitsTransmitted         [E_AVERAGE_OVER_SECOND] = the last whole period,
//                                    [E_MAX_OVER_SECOND]     = the per-cell maximum so far
//   maaaaiHistoricalBitsTransmitted  a KI_MAX_BANDWIDTH_HISTORY-deep ring of periods
// and a period is K_TIME_FOR_AVERAGE long (Time(1.0f), built by a dynamic initialiser).
//
// Log lines stream through CgsDev::Log::gpDebugPrint; assert messages the console built in
// the assert buffer keep their literal text.

#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Network\\Packeting\\CgsCompressionAndEncryptionUtils.cpp"

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameShared\\GameClasses\\Network\\Packeting\\CgsCompressionAndEncryptionUtils.cpp".

namespace CgsNetwork
{

namespace
{
    const CgsSystem::Time K_TIME_FOR_AVERAGE(1.0f);

    const s32 KI_MAX_PACKET_DATA_SIZE                  = 1000;
    const s32 KI_MAX_MESSAGES_TO_PACK_TOGETHER         = KI_E_MESSAGE_TYPE_COUNT;       // 44
    const s32 KI_MAX_SIGNAL_MESSAGES_TO_PACK_TOGETHER  = 2 * KI_E_MESSAGE_TYPE_COUNT;   // 88
    const s32 KI_SIZE_PACKED_TYPES_BUFFER              = KI_E_MESSAGE_TYPE_COUNT;

    // Pack's two bit-packing scratch areas; the spans are the console frame's (1008 and
    // 1152 bytes).
    const s32 KI_PACKED_SIGNAL_MESSAGES_BUFFER_SIZE    = 1008;
    const s32 KI_PACKED_MESSAGE_BODIES_BUFFER_SIZE     = 1152;
}

NetworkPlayerID CompressionAndEncryptionUtils::maNetworkPlayerID[KI_MAX_PLAYERS];
s32 CompressionAndEncryptionUtils::maaaiBitsTransmitted[E_SEND_RECV_COUNT][KI_ALL_PLAYERS + 1]
                                                       [KE_ALL_MESSAGE_TYPES + 1];
CgsSystem::Time CompressionAndEncryptionUtils::mLastAverageTime;
s32 CompressionAndEncryptionUtils::maaaaiAvgBitsTransmitted[E_AVERAGE_TYPE_COUNT][E_SEND_RECV_COUNT]
                                                           [KI_ALL_PLAYERS + 1][KE_ALL_MESSAGE_TYPES + 1];
s32 CompressionAndEncryptionUtils::miHistoricalIndex;
s32 CompressionAndEncryptionUtils::maaaaiHistoricalBitsTransmitted[KI_MAX_BANDWIDTH_HISTORY][E_SEND_RECV_COUNT]
                                                                  [KI_ALL_PLAYERS + 1][KE_ALL_MESSAGE_TYPES + 1];

// ---- ResetBandwidthUsed --------------------------------------------------------------
// Restart the history ring and the averaging clock, forget every ledger entry's player
// and zero all three tables.
void CompressionAndEncryptionUtils::ResetBandwidthUsed(const CgsSystem::TimerStatus* lpTimerStatus)
{
    const CgsSystem::Time lTime = lpTimerStatus->GetTime();
    miHistoricalIndex = 0;
    mLastAverageTime = lTime;

    for (s32 liEntryIndex = 0; liEntryIndex < KI_MAX_PLAYERS; ++liEntryIndex)
    {
        maNetworkPlayerID[liEntryIndex] = K_INVALID_PLAYER_ID;
    }

    XMemSet(maaaiBitsTransmitted, 0, sizeof(maaaiBitsTransmitted));
    XMemSet(maaaaiAvgBitsTransmitted, 0, sizeof(maaaaiAvgBitsTransmitted));
    XMemSet(maaaaiHistoricalBitsTransmitted, 0, sizeof(maaaaiHistoricalBitsTransmitted));
}

// ---- UpdateBandwidthHistory ----------------------------------------------------------
// Copy the current period into the history ring and advance the ring cursor.
void CompressionAndEncryptionUtils::UpdateBandwidthHistory()
{
    CGS_ASSERT_AT(miHistoricalIndex >= 0,
        "miHistoricalIndex >= 0",
        CGS_ARTIST_FILE, 668 /* ARTIST 0x8286FEC8 */);
    CGS_ASSERT_AT(miHistoricalIndex < KI_MAX_BANDWIDTH_HISTORY,
        "miHistoricalIndex < KI_MAX_BANDWIDTH_HISTORY",
        CGS_ARTIST_FILE, 669 /* ARTIST 0x8286FEF0 */);

    XMemCpy(maaaaiHistoricalBitsTransmitted[miHistoricalIndex], maaaiBitsTransmitted,
           sizeof(maaaiBitsTransmitted));

    miHistoricalIndex = (miHistoricalIndex + 1) % KI_MAX_BANDWIDTH_HISTORY;
}

// ---- UpdateMaxBandwidth --------------------------------------------------------------
// Raise every per-second maximum to this period's count, then check the maxima stay at or
// above the period's counts for the header column (the console loop covers types
// KE_HEADER..KE_ALL_MESSAGE_TYPES only). Each send/recv step carries the enum iterator's
// range assert.
void CompressionAndEncryptionUtils::UpdateMaxBandwidth()
{
    for (ESendRecvBandwidth leSendRecv = E_SEND_RECV_START; leSendRecv < E_SEND_RECV_COUNT; leSendRecv++)
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_ALL_PLAYERS + 1; ++liPlayerIndex)
        {
            for (s32 liMessageType = 0; liMessageType < KE_ALL_MESSAGE_TYPES + 1; ++liMessageType)
            {
                const s32 liBits = maaaiBitsTransmitted[leSendRecv][liPlayerIndex][liMessageType];
                s32& lriMax = maaaaiAvgBitsTransmitted[E_MAX_OVER_SECOND][leSendRecv][liPlayerIndex][liMessageType];
                if (lriMax < liBits)
                {
                    lriMax = liBits;
                }
            }
        }
    }

    for (ESendRecvBandwidth leSendRecv = E_SEND_RECV_START; leSendRecv < E_SEND_RECV_COUNT; leSendRecv++)
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_ALL_PLAYERS + 1; ++liPlayerIndex)
        {
            for (s32 liMessageType = KE_HEADER; liMessageType <= KE_ALL_MESSAGE_TYPES; ++liMessageType)
            {
                CGS_ASSERT_AT(GetBandwidthUsedInBits(E_MAX_OVER_SECOND, leSendRecv, liPlayerIndex,
                                                     liMessageType) >= maaaiBitsTransmitted[leSendRecv][liPlayerIndex][liMessageType],
                              "GetBandwidthUsedInBits(E_MAX_OVER_SECOND, leSendRecv, liPlayerIndex, static_cast<EMessageType>( liMessageType ) ) >= maaaiBitsTransmitted[leSendRecv][liPlayerIndex][liMessageType]",
                              CGS_ARTIST_FILE, 725);
                CGS_ASSERT_AT(GetBandwidthUsedInBits(E_MAX_OVER_SECOND, leSendRecv, KI_ALL_PLAYERS,
                                                     liMessageType) >= maaaiBitsTransmitted[leSendRecv][liPlayerIndex][liMessageType],
                              "GetBandwidthUsedInBits(E_MAX_OVER_SECOND, leSendRecv, KI_ALL_PLAYERS, static_cast<EMessageType>( liMessageType ) ) >= maaaiBitsTransmitted[leSendRecv][liPlayerIndex][liMessageType]",
                              CGS_ARTIST_FILE, 727);
                CGS_ASSERT_AT(GetBandwidthUsedInBits(E_MAX_OVER_SECOND, leSendRecv, liPlayerIndex,
                                                     KE_ALL_MESSAGE_TYPES) >= maaaiBitsTransmitted[leSendRecv][liPlayerIndex][liMessageType],
                              "GetBandwidthUsedInBits(E_MAX_OVER_SECOND, leSendRecv, liPlayerIndex, KE_ALL_MESSAGE_TYPES) >= maaaiBitsTransmitted[leSendRecv][liPlayerIndex][liMessageType]",
                              CGS_ARTIST_FILE, 729);
                CGS_ASSERT_AT(GetBandwidthUsedInBits(E_MAX_OVER_SECOND, leSendRecv, KI_ALL_PLAYERS,
                                                     KE_ALL_MESSAGE_TYPES) >= maaaiBitsTransmitted[leSendRecv][liPlayerIndex][liMessageType],
                              "GetBandwidthUsedInBits(E_MAX_OVER_SECOND, leSendRecv, KI_ALL_PLAYERS, KE_ALL_MESSAGE_TYPES) >= maaaiBitsTransmitted[leSendRecv][liPlayerIndex][liMessageType]",
                              CGS_ARTIST_FILE, 731);
            }
        }
    }
}

// ---- Update --------------------------------------------------------------------------
// When a whole averaging period has elapsed: restart the clock, publish the period as the
// last-second averages, fold it into the maxima and the history ring, and clear it.
void CompressionAndEncryptionUtils::Update(const CgsSystem::TimerStatus* lpTimerStatus)
{
    if (lpTimerStatus->GetTime() > mLastAverageTime + K_TIME_FOR_AVERAGE)
    {
        mLastAverageTime = lpTimerStatus->GetTime();
        XMemCpy(maaaaiAvgBitsTransmitted[E_AVERAGE_OVER_SECOND], maaaiBitsTransmitted,
               sizeof(maaaiBitsTransmitted));
        UpdateMaxBandwidth();
        UpdateBandwidthHistory();
        XMemSet(maaaiBitsTransmitted, 0, sizeof(maaaiBitsTransmitted));
    }
}

// ---- GetEntryIndex -------------------------------------------------------------------
// The ledger entry already holding liPlayerID, else the first free entry (claimed for it),
// else -1.
s32 CompressionAndEncryptionUtils::GetEntryIndex(NetworkPlayerID liPlayerID)
{
    for (s32 liEntryIndex = 0; liEntryIndex < KI_MAX_PLAYERS; ++liEntryIndex)
    {
        if (maNetworkPlayerID[liEntryIndex] == liPlayerID)
        {
            return liEntryIndex;
        }
    }

    for (s32 liEntryIndex = 0; liEntryIndex < KI_MAX_PLAYERS; ++liEntryIndex)
    {
        if (maNetworkPlayerID[liEntryIndex] == K_INVALID_PLAYER_ID)
        {
            maNetworkPlayerID[liEntryIndex] = liPlayerID;
            return liEntryIndex;
        }
    }

    return -1;
}

// ---- RecordBitsTransmitted -----------------------------------------------------------
// Book liBits against (player, type), (player, all types), (all players, type) and
// (all players, all types) of the current period.
void CompressionAndEncryptionUtils::RecordBitsTransmitted(ESendRecvBandwidth leSendRecv,
                                                          NetworkPlayerID liPlayerID,
                                                          EMessageType leMessageType, s32 liBits)
{
    const s32 liEntryIndex = GetEntryIndex(liPlayerID);
    CGS_ASSERT_AT(liEntryIndex >= 0, "liEntryIndex >= 0", CGS_ARTIST_FILE, 647);
    CGS_ASSERT_AT(liEntryIndex < KI_MAX_PLAYERS, "liEntryIndex < KI_MAX_PLAYERS", CGS_ARTIST_FILE, 648);

    maaaiBitsTransmitted[leSendRecv][liEntryIndex][leMessageType]          += liBits;
    maaaiBitsTransmitted[leSendRecv][liEntryIndex][KE_ALL_MESSAGE_TYPES]   += liBits;
    maaaiBitsTransmitted[leSendRecv][KI_ALL_PLAYERS][leMessageType]        += liBits;
    maaaiBitsTransmitted[leSendRecv][KI_ALL_PLAYERS][KE_ALL_MESSAGE_TYPES] += liBits;
}

// ---- Pack ----------------------------------------------------------------------------
// Bit-pack every valid signal message, then every valid message that still fits, and lay
// the packet out header / type bytes / signal bits / body bits. A message that does not
// fit stays valid and makes the call return false; everything packed is marked invalid.
// All messages in one packet must agree on the game id, the unreliable ones on the frame
// and the reliable ones on the sending / receiving player.
bool CompressionAndEncryptionUtils::Pack(NetworkPlayerID liPlayerID,
                                         Message** lppMessagesToPack, s32 liNumMessagesInArray,
                                         SignalMessage** lppSignalMessagesToPack,
                                         s32 liNumSignalMessagesInArray,
                                         u8* lpacBufferToPackInto, s32 liBufferSize,
                                         s32* lpiBufferUsed)
{
    bool lbReliable          = false;
    bool lbAllMessagesPacked = true;

    CGS_ASSERT(liNumMessagesInArray < 255, "liNumMessagesInArray < 255");
    CGS_ASSERT(liNumSignalMessagesInArray < 255, "liNumSignalMessagesInArray < 255");
    CGS_ASSERT(liNumSignalMessagesInArray < KI_MAX_SIGNAL_MESSAGES_TO_PACK_TOGETHER,
               "liNumSignalMessagesInArray < KI_MAX_SIGNAL_MESSAGES_TO_PACK_TOGETHER");
    CGS_ASSERT(liBufferSize <= KI_MAX_PACKET_DATA_SIZE, "liBufferSize <= KI_MAX_PACKET_DATA_SIZE");
    CGS_ASSERT(liBufferSize <= KI_MAX_PACKET_DATA_SIZE, "liBufferSize <= KI_MAX_PACKET_DATA_SIZE");

    if (liNumMessagesInArray >= KI_MAX_MESSAGES_TO_PACK_TOGETHER)
    {
        CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "---- Start ----\n";
        CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "liNumMessagesInArray = " << liNumMessagesInArray << "\n";
        for (s32 liType = 0; liType < KI_E_MESSAGE_TYPE_COUNT; ++liType)
        {
            s32 liNumOfType = 0;
            for (s32 liMessageIndex = 0; liMessageIndex < liNumMessagesInArray; ++liMessageIndex)
            {
                if (lppMessagesToPack[liMessageIndex]->IsMessageValid()
                    && lppMessagesToPack[liMessageIndex]->GetType() == liType)
                {
                    ++liNumOfType;
                }
            }
            if (liNumOfType > 0)
            {
                CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Message Type " << liType << " has " << liNumOfType
                                           << " entries to send\n";
            }
        }
        CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "---- Done ----\n";
        CGS_FAIL("Too many messages to send! Please see TTY output for more details");
    }

    *lpiBufferUsed = 0;
    if (liNumMessagesInArray == 0 && liNumSignalMessagesInArray == 0)
    {
        return true;
    }

    ReliablePackedPacketHeader lHeader;
    lHeader.mx8Flags         = 0;
    lHeader.mu8NumTypes      = 0;
    lHeader.mu8NumSignalMsgs = 0;
    lHeader.mu8GameID        = KU8_INVALID_GAME_ID;
    lHeader.mu16Frame        = KU16_INVALID_FRAME;
    lHeader.mRecvingPlayerID = K_INVALID_PLAYER_ID;
    lHeader.mSendingPlayerID = K_INVALID_PLAYER_ID;

    CGS_ALIGN(8) u8 lacPackedTypes[KI_SIZE_PACKED_TYPES_BUFFER];
    CGS_ALIGN(8) u8 lacPackedSignalMessages[KI_PACKED_SIGNAL_MESSAGES_BUFFER_SIZE];
    CGS_ALIGN(8) u8 lacPackedMessageBodies[KI_PACKED_MESSAGE_BODIES_BUFFER_SIZE];

    // ---- signal messages: one bit stream, no size limit beyond the whole packet ----
    s32 liPackedSignalMessageBufferOffestInBits = 0;
    const s32 liPackedSignalMessageBufferSizeInBits = liBufferSize * 8;
    for (s32 liMessageIndex = 0; liMessageIndex < liNumSignalMessagesInArray; ++liMessageIndex)
    {
        if (lppSignalMessagesToPack[liMessageIndex]->IsMessageValid())
        {
            s32 liPackedSizeInBits;
            CGS_ASSERT(lppSignalMessagesToPack[liMessageIndex]->Pack(lacPackedSignalMessages,
                                                                     liPackedSignalMessageBufferOffestInBits,
                                                                     liPackedSignalMessageBufferSizeInBits,
                                                                     &liPackedSizeInBits),
                       "lppSignalMessagesToPack[liMessageIndex]->Pack(lacPackedSignalMessages, liPackedSignalMessageBufferOffestInBits, liPackedSignalMessageBufferSizeInBits, &liPackedSizeInBits)");
            liPackedSignalMessageBufferOffestInBits += liPackedSizeInBits;
            lppSignalMessagesToPack[liMessageIndex]->SetMessageInvalid();
            ++lHeader.mu8NumSignalMsgs;
        }
    }
    const s32 liPackedSignalMessagesSizeInBits = 8 * ((liPackedSignalMessageBufferOffestInBits + 7) / 8);

    // ---- message bodies: whatever room the header, the type bytes and the signals leave ----
    s32 liPackedMessageBodiesBufferOffestInBits = 0;
    s32 liNumPackedTypes = 0;
    const s32 liPackedMessageBodiesBufferSizeInBits =
        8 * (liBufferSize - liNumMessagesInArray - static_cast<s32>(sizeof(ReliablePackedPacketHeader)))
        - liPackedSignalMessagesSizeInBits;
    for (s32 liMessageIndex = 0; liMessageIndex < liNumMessagesInArray; ++liMessageIndex)
    {
        if (!lppMessagesToPack[liMessageIndex]->IsMessageValid())
        {
            continue;
        }

        s32 liPackedSizeInBits;
        if (!lppMessagesToPack[liMessageIndex]->Pack(lacPackedMessageBodies,
                                                      liPackedMessageBodiesBufferOffestInBits,
                                                      liPackedMessageBodiesBufferSizeInBits,
                                                      &liPackedSizeInBits))
        {
            lbAllMessagesPacked = false;
            continue;
        }

        liPackedMessageBodiesBufferOffestInBits += liPackedSizeInBits;
        CGS_ASSERT(liPackedMessageBodiesBufferSizeInBits - liPackedMessageBodiesBufferOffestInBits >= 0,
                   "liPackedMessageBodiesBufferSizeInBits - liPackedMessageBodiesBufferOffestInBits >= 0");

        lppMessagesToPack[liMessageIndex]->SetMessageInvalid();
        RecordBitsTransmitted(E_SEND_BANDWIDTH, liPlayerID,
                              static_cast<EMessageType>(lppMessagesToPack[liMessageIndex]->GetType()), liPackedSizeInBits);

        if (lHeader.mu8GameID == KU8_INVALID_GAME_ID)
        {
            lHeader.mu8GameID = lppMessagesToPack[liMessageIndex]->GetGameID();
        }
        else
        {
            CGS_ASSERT(lHeader.mu8GameID == lppMessagesToPack[liMessageIndex]->GetGameID(),
                       "lHeader.mu8GameID == lppMessagesToPack[liMessageIndex]->GetGameID()");
        }

        if (lppMessagesToPack[liMessageIndex]->IsReliable())
        {
            MessageWithPlayerIDs* lpReliableMsg =
                static_cast<MessageWithPlayerIDs*>(lppMessagesToPack[liMessageIndex]);
            lbReliable = true;

            CGS_ASSERT(lHeader.mSendingPlayerID == K_INVALID_PLAYER_ID
                           || lHeader.mSendingPlayerID == lpReliableMsg->GetSendingPlayerID(),
                       "lHeader.mSendingPlayerID == K_INVALID_PLAYER_ID || lHeader.mSendingPlayerID == lpReliableMsg->GetSendingPlayerID()");
            CGS_ASSERT(lHeader.mRecvingPlayerID == K_INVALID_PLAYER_ID
                           || lHeader.mRecvingPlayerID == lpReliableMsg->GetRecvingPlayerID(),
                       "lHeader.mRecvingPlayerID == K_INVALID_PLAYER_ID || lHeader.mRecvingPlayerID == lpReliableMsg->GetRecvingPlayerID()");

            lHeader.mSendingPlayerID = lpReliableMsg->GetSendingPlayerID();
            lHeader.mRecvingPlayerID = lpReliableMsg->GetRecvingPlayerID();
            lHeader.mx8Flags |= KX8_FLAGS_RELIABLE;
        }
        else
        {
            CGS_ASSERT(lHeader.mu16Frame == KU16_INVALID_FRAME
                           || lHeader.mu16Frame == lppMessagesToPack[liMessageIndex]->mu16Frame,
                       "lHeader.mu16Frame == KU16_INVALID_FRAME || lHeader.mu16Frame == lppMessagesToPack[liMessageIndex]->GetU16Frame()");
            lHeader.mu16Frame = lppMessagesToPack[liMessageIndex]->mu16Frame;
        }

        ++liNumPackedTypes;
        ++lHeader.mu8NumTypes;
        lacPackedTypes[liNumPackedTypes - 1] =
            static_cast<u8>(lppMessagesToPack[liMessageIndex]->GetType());
    }

    // ---- lay the packet out ----
    const s32 liHeaderSize = lbReliable ? static_cast<s32>(sizeof(ReliablePackedPacketHeader))
                                        : static_cast<s32>(sizeof(PackedPacketHeader));
    memcpy(lpacBufferToPackInto + *lpiBufferUsed, &lHeader, liHeaderSize);
    *lpiBufferUsed += liHeaderSize;
    RecordBitsTransmitted(E_SEND_BANDWIDTH, liPlayerID, KE_HEADER, liHeaderSize * 8);

    memcpy(lpacBufferToPackInto + *lpiBufferUsed, lacPackedTypes, liNumPackedTypes);
    *lpiBufferUsed += liNumPackedTypes;
    RecordBitsTransmitted(E_SEND_BANDWIDTH, liPlayerID, KE_HEADER, liNumPackedTypes * 8);

    const s32 liPackedSignalMessagesSizeInBytes = (liPackedSignalMessagesSizeInBits + 7) / 8;
    memcpy(lpacBufferToPackInto + *lpiBufferUsed, lacPackedSignalMessages,
           liPackedSignalMessagesSizeInBytes);
    *lpiBufferUsed += liPackedSignalMessagesSizeInBytes;
    RecordBitsTransmitted(E_SEND_BANDWIDTH, liPlayerID, KE_ACK_OR_NACK,
                          liPackedSignalMessagesSizeInBits);

    const s32 liPackedMessageBodiesSizeInBytes = (liPackedMessageBodiesBufferOffestInBits + 7) / 8;
    memcpy(lpacBufferToPackInto + *lpiBufferUsed, lacPackedMessageBodies,
           liPackedMessageBodiesSizeInBytes);
    *lpiBufferUsed += liPackedMessageBodiesSizeInBytes;

    CGS_ASSERT(*lpiBufferUsed <= KI_MAX_PACKET_DATA_SIZE, "*lpiBufferUsed <= KI_MAX_PACKET_DATA_SIZE");
    // The console streams both values into the assert buffer between these two labels.
    CGS_ASSERT(*lpiBufferUsed <= liBufferSize, "liBufferSize= *lpiBufferUsed=");

    return lbAllMessagesPacked;
}

// ---- UnPack --------------------------------------------------------------------------
// Read the header, unpack every signal message into the caller's signal array, then hand
// each packed body to the registered receive slot of its type: the slot's message is reset,
// stamped with the packet's game id and type (and the packet's player ids when reliable,
// else its frame), unpacked, and the slot's callback told. A body with no slot of its type
// asserts and abandons the rest of the packet.
void CompressionAndEncryptionUtils::UnPack(NetworkPlayerID liPlayerID,
                                           RecvMessageData* lpaMessagesToUnPackInto,
                                           s32 liNumMessagesInArray,
                                           SignalMessage** lppSignalMessagesToUnPackInto,
                                           s32 liNumSignalMessagesInArray,
                                           u8* lpacBufferToUnPackFrom, s32 liBufferSize)
{
    const PackedPacketHeader* lpHeader =
        reinterpret_cast<const PackedPacketHeader*>(lpacBufferToUnPackFrom);

    NetworkPlayerID lSendingPlayerID = K_INVALID_PLAYER_ID;
    NetworkPlayerID lRecvingPlayerID = K_INVALID_PLAYER_ID;
    const u8  lu8GameID           = lpHeader->mu8GameID;
    const s32 lnNumMessages       = lpHeader->mu8NumTypes;
    const s32 lnNumSignalMessages = lpHeader->mu8NumSignalMsgs;
    const u16 lu16Frame           = lpHeader->mu16Frame;

    CGS_ASSERT(lnNumMessages < KI_MAX_MESSAGES_TO_PACK_TOGETHER,
               "lnNumMessages < KI_MAX_MESSAGES_TO_PACK_TOGETHER");
    CGS_ASSERT(lnNumSignalMessages < KI_MAX_SIGNAL_MESSAGES_TO_PACK_TOGETHER,
               "lnNumSignalMessages < KI_MAX_SIGNAL_MESSAGES_TO_PACK_TOGETHER");

    u8* lpPackedTypes;
    if ((lpHeader->mx8Flags & KX8_FLAGS_RELIABLE) == KX8_FLAGS_RELIABLE)
    {
        const ReliablePackedPacketHeader* lpReliableHeader =
            reinterpret_cast<const ReliablePackedPacketHeader*>(lpacBufferToUnPackFrom);
        lSendingPlayerID = lpReliableHeader->mSendingPlayerID;
        lRecvingPlayerID = lpReliableHeader->mRecvingPlayerID;
        lpPackedTypes    = lpacBufferToUnPackFrom + sizeof(ReliablePackedPacketHeader);
    }
    else
    {
        lpPackedTypes = lpacBufferToUnPackFrom + sizeof(PackedPacketHeader);
    }

    // ---- signal messages ----
    u8* lpPackedSignalMessages = lpPackedTypes + lnNumMessages;
    s32 liBufferOffsetInBits   = 0;
    s32 liBufferLengthInBits   =
        (liBufferSize - static_cast<s32>(lpPackedSignalMessages - lpacBufferToUnPackFrom)) * 8;
    for (s32 liMessageIndex = 0; liMessageIndex < lnNumSignalMessages; ++liMessageIndex)
    {
        CGS_ASSERT(liMessageIndex < liNumSignalMessagesInArray,
                   "liMessageIndex < liNumSignalMessagesInArray");
        CGS_ASSERT(lppSignalMessagesToUnPackInto[liMessageIndex],
                   "lppSignalMessagesToUnPackInto[liMessageIndex]");

        s32 liPackedSizeInBits;
        lppSignalMessagesToUnPackInto[liMessageIndex]->Construct();
        lppSignalMessagesToUnPackInto[liMessageIndex]->UnPack(lpPackedSignalMessages,
                                                              liBufferOffsetInBits,
                                                              liBufferLengthInBits,
                                                              &liPackedSizeInBits);
        liBufferOffsetInBits += liPackedSizeInBits;
        RecordBitsTransmitted(E_RECV_BANDWIDTH, liPlayerID, KE_ACK_OR_NACK, liPackedSizeInBits);
    }

    // The header, type bytes and signal bytes are booked under KE_HEADER as a byte count.
    const s32 liPackedSignalMessagesSize = (liBufferOffsetInBits + 7) / 8;
    u8* lpPackedBody = lpPackedSignalMessages + liPackedSignalMessagesSize;
    const s32 liHeaderSize = static_cast<s32>(lpPackedBody - lpacBufferToUnPackFrom);
    RecordBitsTransmitted(E_RECV_BANDWIDTH, liPlayerID, KE_HEADER, liHeaderSize);

    // ---- message bodies ----
    liBufferOffsetInBits = 0;
    liBufferLengthInBits = (liBufferSize - liHeaderSize) * 8;
    for (s32 liMessageIndex = 0; liMessageIndex < lnNumMessages; ++liMessageIndex)
    {
        const s32 leMessageType = lpPackedTypes[liMessageIndex];

        Message* lpMessageToUnpackInto = 0;
        s32 liMessageToUnPackInto = 0;
        for (; liMessageToUnPackInto < liNumMessagesInArray; ++liMessageToUnPackInto)
        {
            lpMessageToUnpackInto = lpaMessagesToUnPackInto[liMessageToUnPackInto].mpMessage;
            if (lpMessageToUnpackInto != 0 && lpMessageToUnpackInto->GetType() == leMessageType)
            {
                break;
            }
        }

        if (liMessageToUnPackInto == liNumMessagesInArray)
        {
            // The console streams the type between these two literals.
            CGS_FAIL("Failed to find msg of type  to unpack into - we're going to lose this message\n");
            return;
        }

        lpMessageToUnpackInto->Construct();
        CGS_ASSERT(lu8GameID != KU8_INVALID_GAME_ID, "lu8GameID != KU8_INVALID_GAME_ID");
        lpMessageToUnpackInto->SetGameID(lu8GameID);
        lpMessageToUnpackInto->SetType(static_cast<::EMessageType>(leMessageType));
        if (lpMessageToUnpackInto->IsReliable())
        {
            MessageWithPlayerIDs* lpReliableMsg = static_cast<MessageWithPlayerIDs*>(lpMessageToUnpackInto);
            lpReliableMsg->SetSendingPlayerID(lSendingPlayerID);
            lpReliableMsg->SetRecvingPlayerID(lRecvingPlayerID);
        }
        else
        {
            lpMessageToUnpackInto->mu16Frame = lu16Frame;
        }

        s32 liPackedSizeInBits;
        lpMessageToUnpackInto->UnPack(lpPackedBody, liBufferOffsetInBits, liBufferLengthInBits,
                                      &liPackedSizeInBits);
        liBufferOffsetInBits += liPackedSizeInBits;
        RecordBitsTransmitted(E_RECV_BANDWIDTH, liPlayerID, static_cast<EMessageType>(leMessageType), liPackedSizeInBits);

        const RecvMessageData& lSlot = lpaMessagesToUnPackInto[liMessageToUnPackInto];
        lSlot.mpMsgUnpackedCallback(lpMessageToUnpackInto, lSlot.mpCallbackUserData);
    }
}

// ---- ExtractSendingPlayerID ----------------------------------------------------------
// The sending player's id from a reliable packet's header; K_INVALID_PLAYER_ID for an
// unreliable packet (its header carries no ids).
NetworkPlayerID CompressionAndEncryptionUtils::ExtractSendingPlayerID(u8* lpacBufferToUnPackFrom,
                                                                      s32 liBufferSize)
{
    CGS_ASSERT_AT(static_cast<u32>(liBufferSize) >= sizeof(PackedPacketHeader),
        "(uint32_t)liBufferSize >= sizeof(PackedPacketHeader)",
            __FILE__, 581 /* ARTIST 0x8286FD64 */);

    CGS_ASSERT_AT(lpacBufferToUnPackFrom,
        "lpacBufferToUnPackFrom", __FILE__, 582 /* ARTIST 0x8286FD88 */);

    const PackedPacketHeader* lpHeader =
        reinterpret_cast<const PackedPacketHeader*>(lpacBufferToUnPackFrom);
    if ((lpHeader->mx8Flags & KX8_FLAGS_RELIABLE) == KX8_FLAGS_RELIABLE)
    {
        CGS_ASSERT_AT(static_cast<u32>(liBufferSize) > sizeof(ReliablePackedPacketHeader),
            "(uint32_t)liBufferSize > sizeof(ReliablePackedPacketHeader)",
                __FILE__, 590 /* ARTIST 0x8286FDBC */);

        const ReliablePackedPacketHeader* lpReliableHeader =
            reinterpret_cast<const ReliablePackedPacketHeader*>(lpacBufferToUnPackFrom);
        return lpReliableHeader->mSendingPlayerID;
    }

    return K_INVALID_PLAYER_ID;
}

}

#undef CGS_ARTIST_FILE
