#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsNetwork::Message  (CgsMessage.h:99 in the leak)
//
// Canonical class home for the network-message base. The X360 layout is taken
// from the FULL dwarfdump struct cross-checked against the per-function asm
// (offsets below are the byte offsets the asm dereferences):
//
//   0x00  vptr
//   0x04  mePackOrUnpack   (EPackOrUnpack; used as a 3-state lifecycle marker:
//                           0 = idle, 1 = packing/unpacking, 2 = done)
//   0x08  mBitstream       (CgsNetwork::SmartBitStream, by value) -- Pack/UnPack
//                          drive its four 32-bit cursor words directly at
//                          +0x08/+0x0C/+0x10/+0x14, so they are modelled by name
//                          here as a faithful cursor view rather than pulling in
//                          the full BitStream layout (which this TU never touches
//                          by anything but these four words).
//   0x18  mu8GameID        (uint8)
//   0x19  mx8Flags         (uint8 bit-set: VALID|RELIABLE|ACK|NACK)
//   0x1A  mi8Type          (int8 EMessageType)
//   0x1C  mu16Frame        (uint16)
//
// The Pack/UnPack/PackOrUnpack engine and the SmartBitStream's own methods live
// in other TUs; here the SmartBitStream is reduced to the four cursor words this
// TU manipulates plus padding to the 0x18 boundary the trailing scalars sit on.

namespace CgsNetwork
{
    // Flag bits (CgsNetworkConstants.h / CgsMessage.h:40-43).
    const u8 KX8_FLAGS_VALID    = 1;
    const u8 KX8_FLAGS_RELIABLE = 2;
    const u8 KX8_FLAGS_ACK      = 4;
    const u8 KX8_FLAGS_NACK     = 8;

    // Sentinels (CgsMessage.h:45-48).
    const u8  KU8_INVALID_GAME_ID = 255;
    const u16 KU16_INVALID_FRAME  = 65535;

    // Message type bounds (CgsMessage.h). The asm hard-codes the upper bound as
    // 44 (E_MESSAGE_TYPE_COUNT) and a separate < 255 fits-in-int8 guard.
    const s32 KI_E_MESSAGE_TYPE_COUNT = 44;

    // PackOrUnpack result (CgsMessage.h:85-88). 0 == success.
    typedef u8 PackOrUnpackResult;
    const PackOrUnpackResult KX_PACK_OR_UNPACK_SUCCESS = 0;

    struct Message
    {
        // Lifecycle states stored in mePackOrUnpack during Pack/UnPack.
        enum EPackOrUnpack
        {
            E_PACK_INTO_BITSTREAM   = 0,
            E_UNPACK_FROM_BITSTREAM = 1,
            E_PACK_OR_UNPACK_COUNT  = 2,
        };

        // --- layout (frozen) ---
        void* mpVTable;                 // 0x00
        s32   mePackOrUnpack;           // 0x04
        // mBitstream @ 0x08: the four 32-bit cursor words Pack/UnPack drive.
        u32   muBitstreamCursor0;       // 0x08
        u32   muBitstreamCursor1;       // 0x0C
        u32   muBitstreamCursor2;       // 0x10
        u32   muBitstreamCursor3;       // 0x14
        u8    mu8GameID;                // 0x18
        u8    mx8Flags;                 // 0x19
        s8    mi8Type;                  // 0x1A
        u16   mu16Frame;                // 0x1C

        // --- reconstructed members (this TU) ---
        u8   GetGameID() const;
        Message* SetType(s32 leType);
        void PrepareForSend(s32 leType, u16 lu16Frame);
        void PrepareAck(s32 leType, u16 lu16Frame, u8 lu8GameID);
        void PrepareNack(s32 leType, u16 lu16Frame, u8 lu8GameID);

        // Pack/UnPack call the virtual PackOrUnpack() through the vtable; the slot
        // is at vtable+0x10 (4th entry) and returns a PackOrUnpackResult.
        bool Pack(s32 liA, s32 liB, s32 liC, s32* lpiBitsWritten);
        Message* UnPack(s32 liA, s32 liB, s32 liC, s32* lpiBitsRead);
    };
}
