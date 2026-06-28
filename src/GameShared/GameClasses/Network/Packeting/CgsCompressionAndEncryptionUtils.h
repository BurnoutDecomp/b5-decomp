#pragma once

// ===================================================================================
// CgsNetwork::CompressionAndEncryptionUtils -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Packeting/CgsCompressionAndEncryptionUtils.h
//
// Bandwidth-accounting helper for the network packeting layer. The asm @ 0x8286F878
// (GetBandwidthUsedInBits) indexes a flat per-build statistics table by four enumerated
// keys -- average-type, send/recv direction, player index, and message type -- after
// bounds-asserting each key. The bounds the asm hard-codes give the table shape and the
// enum sizes:
//
//   leSendRecv    in [0, 2)   -> E_SEND_RECV_COUNT      == 2
//   leAverageType in [0, 2)   -> E_AVERAGE_TYPE_COUNT   == 2
//   liPlayerIndex in [0, 9)   -> KI_ALL_PLAYERS + 1     == 9   (KI_ALL_PLAYERS == 8)
//   liMessageType in [0, 46)  -> KE_ALL_MESSAGE_TYPES+1 == 46
//
// The lookup index the asm builds is
//   ((leAverageType*2 + leSendRecv)*9 + liPlayerIndex)*46 + liMessageType
// i.e. a row-major [E_AVERAGE_TYPE_COUNT][E_SEND_RECV_COUNT][9][46] table of ints. That
// table is the per-build accumulated bandwidth ledger (mutable static state, zero at
// startup -- NOT rodata), so it is modelled here as a file-scope counter array, accessed
// by name. UpdateMaxBandwidth (its own TU, the sole caller) feeds it.
// ===================================================================================

#include "types.hpp"

namespace CgsSystem { class TimerStatus; }

namespace CgsNetwork
{
    struct Message;
    struct SignalMessage;
    typedef s32 NetworkPlayerID;   // mirrors MessageWithPlayerIDs::NetworkPlayerID

    struct CompressionAndEncryptionUtils
    {
        // Enumerated keys into the bandwidth table (counts taken from the asm bounds).
        enum EAverageType
        {
            E_AVERAGE_TYPE_INSTANT = 0,
            E_AVERAGE_TYPE_RUNNING = 1,
            E_AVERAGE_TYPE_COUNT   = 2,
        };
        enum ESendRecv
        {
            E_SEND   = 0,
            E_RECV   = 1,
            E_SEND_RECV_COUNT = 2,
        };

        static const s32 KI_ALL_PLAYERS         = 8;   // liPlayerIndex < KI_ALL_PLAYERS+1
        static const s32 KE_ALL_MESSAGE_TYPES   = 45;  // liMessageType < KE_ALL_MESSAGE_TYPES+1

        // @ 0x8286F878 -- read the accumulated bandwidth (in bits) for one
        // (average-type, send/recv, player, message-type) cell of the ledger.
        static s32 GetBandwidthUsedInBits(s32 leAverageType, s32 leSendRecv,
                                          s32 liPlayerIndex, s32 liMessageType);

        // --- per-player packet packer (instance methods; bodies in this class's own TU) ---
        // Reset the per-frame bandwidth accounting at the start of a player's life (DWARF :197).
        void ResetBandwidthUsed(const CgsSystem::TimerStatus* lpTimerStatus);
        // Advance the running bandwidth averages once per frame (DWARF :167).
        void Update(const CgsSystem::TimerStatus* lpTimerStatus);
        // Compress + encrypt up to liMaxBytes of the queued send/signal messages for liPlayerID
        // into lpu8Buffer; *lpiBytesPacked receives the packed length. Returns true once every
        // queued message has been packed (the player pump loops until it does). DWARF :174.
        bool Pack(NetworkPlayerID liPlayerID,
                  Message** lpapSendMessages, s32 liNumSendMessages,
                  SignalMessage** lpapSignalMessages, s32 liNumSignalMessages,
                  u8* lpu8Buffer, s32 liMaxBytes, s32* lpiBytesPacked);
    };
}
