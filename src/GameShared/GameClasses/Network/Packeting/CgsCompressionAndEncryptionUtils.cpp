#include "GameShared/GameClasses/Network/Packeting/CgsCompressionAndEncryptionUtils.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::CompressionAndEncryptionUtils::GetBandwidthUsedInBits @ 0x8286F878
//   (called by CgsNetwork::CompressionAndEncryptionUtils::UpdateMaxBandwidth)
//
// The asm bounds-checks each of the four keys (CGS_ASSERT pairs, one < upper / one >= 0
// per key), then reads one cell of a flat per-build bandwidth ledger. The index the asm
// computes (store-for-store):
//   slwi r11, avg, 1      ; avg*2
//   add  r11, r11, sr     ; avg*2 + sendrecv
//   slwi r9,  r11, 3      ; (avg*2+sendrecv)*8
//   add  r11, r11, r9     ; (avg*2+sendrecv)*9
//   add  r11, r11, pi     ; ((avg*2+sendrecv)*9) + playerIndex
//   mulli r11, r11, 46    ; * 46
//   add  r11, r11, mt     ; + messageType
//   lwzx r3, r11<<2, base ; ldgBandwidth[index]
// which is exactly row-major [E_AVERAGE_TYPE_COUNT][E_SEND_RECV_COUNT][9][46] indexing.
//
// gaBandwidthUsedInBits is the accumulated bandwidth ledger -- mutable static state that
// starts zeroed and is updated elsewhere (UpdateMaxBandwidth path); it is NOT rodata, so
// it is modelled here as a zero-initialised file-scope counter array, accessed by name.

namespace CgsNetwork
{

namespace
{
    // [average-type][send/recv][player+all][message-type] accumulated bandwidth, in bits.
    s32 gaBandwidthUsedInBits[CompressionAndEncryptionUtils::E_AVERAGE_TYPE_COUNT]
                             [CompressionAndEncryptionUtils::E_SEND_RECV_COUNT]
                             [CompressionAndEncryptionUtils::KI_ALL_PLAYERS + 1]
                             [CompressionAndEncryptionUtils::KE_ALL_MESSAGE_TYPES + 1] = {};
}

s32 CompressionAndEncryptionUtils::GetBandwidthUsedInBits(
        s32 leAverageType, s32 leSendRecv, s32 liPlayerIndex, s32 liMessageType)
{
    CGS_ASSERT(leSendRecv < E_SEND_RECV_COUNT,     "leSendRecv < E_SEND_RECV_COUNT");
    CGS_ASSERT(leSendRecv >= 0,                    "leSendRecv >= 0");
    CGS_ASSERT(leAverageType < E_AVERAGE_TYPE_COUNT,"leAverageType < E_AVERAGE_TYPE_COUNT");
    CGS_ASSERT(leAverageType >= 0,                 "leAverageType >= 0");
    CGS_ASSERT(liPlayerIndex < KI_ALL_PLAYERS + 1, "liPlayerIndex < KI_ALL_PLAYERS+1");
    CGS_ASSERT(liPlayerIndex >= 0,                 "liPlayerIndex >= 0");
    CGS_ASSERT(liMessageType < KE_ALL_MESSAGE_TYPES + 1, "liMessageType < KE_ALL_MESSAGE_TYPES+1");
    CGS_ASSERT(liMessageType >= 0,                 "liMessageType >= 0");

    return gaBandwidthUsedInBits[leAverageType][leSendRecv][liPlayerIndex][liMessageType];
}

}
