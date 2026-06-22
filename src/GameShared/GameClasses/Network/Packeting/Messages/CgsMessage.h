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
        // Placement-style initialiser the X360 build calls "Construct" (returns this);
        // and the packed-size query. Both have their bodies in their own TUs; declared
        // here so every subclass ctor / GetPackedMessageSize can chain to the base.
        Message* Construct();
        s32      GetPackedMessageSize();
        u8   GetGameID() const;
        // vtable slot 0 in the X360 build: the base reports "not reliable"; only
        // ReliableMessage overrides it. Subclass PrepareForSend asserts on it.
        bool IsReliable() const;
        Message* SetType(s32 leType);
        void PrepareForSend(s32 leType, u16 lu16Frame);
        void PrepareAck(s32 leType, u16 lu16Frame, u8 lu8GameID);
        void PrepareNack(s32 leType, u16 lu16Frame, u8 lu8GameID);

        // Pack/UnPack call the virtual PackOrUnpack() through the vtable; the slot
        // is at vtable+0x10 (4th entry) and returns a PackOrUnpackResult.
        bool Pack(s32 liA, s32 liB, s32 liC, s32* lpiBitsWritten);
        Message* UnPack(s32 liA, s32 liB, s32 liC, s32* lpiBitsRead);
    };

    // 16-bit frame-ring helpers (homed in CgsMessageFrameUtils.cpp; declared in
    // CgsMessage.h in the leak). Declared here so the rest of the Message hierarchy
    // -- e.g. HostMigrationManager::IsHostAlive -- can call them by name.
    bool UInt16IsLargerWrapped(u16 lu16A, u16 lu16B);
    bool UInt16IsLargerOrEqualWrapped(u16 lu16A, u16 lu16B);
    u16  GetFrameDiffWrapped16(u16 lu16FrameA, u16 lu16FrameB);

    // ------------------------------------------------------------------------
    // Shared field (de)serialise primitives (homed in CgsMessage.cpp; each is its
    // own not-yet-reconstructed TU, so they are declared here -- the whole message
    // hierarchy packs/unpacks fields by name through these). Every Pack/Unpack pass
    // routes the field through the message's bitstream and the lifecycle word
    // (mePackOrUnpack) decides pack vs unpack. Each returns a per-field status that
    // the callers OR together (0 == all fields succeeded == KX_PACK_OR_UNPACK_SUCCESS).
    //   PackOrUnpackInt  -- sub_82881370: a quantised 32-bit int in [liMin, liMax].
    //   PackOrUnpackU16  -- sub_82881250: a quantised 16-bit value in [liMin, liMax].
    //   PackOrUnpackBool -- sub_8288DDA0: a single boolean flag.
    //   PackOrUnpackCgsID-- sub_82881C00: a 64-bit CgsID.
    // The field to (de)serialise is passed by pointer; the return is a per-field
    // status the callers bitwise-OR together into the message's PackOrUnpackResult.
    PackOrUnpackResult PackOrUnpackInt(Message* lpMessage, s32* lpiField, s32 liMin, s32 liMax);
    PackOrUnpackResult PackOrUnpackU16(Message* lpMessage, u16* lpu16Field, s32 liMin, s32 liMax);
    PackOrUnpackResult PackOrUnpackBool(Message* lpMessage, bool* lpbField);
    PackOrUnpackResult PackOrUnpackCgsID(Message* lpMessage, u64* lpu64Field);

    // CgsNetwork::MessageWithPlayerIDs and CgsNetwork::ReliableMessage are the next two
    // rungs of the message hierarchy. They now live in their proper home headers
    // (CgsMessageWithPlayerIDs.h / CgsReliableMessage.h), which #include this file --
    // include those when you need a ReliableMessage-derived type. (They used to be a
    // single bare `ReliableMessage : Message` stub here; recovering the real DWARF
    // hierarchy moved them out and gave MessageWithPlayerIDs its two player-id fields.)
}

// CgsNetwork::HostMigrationManager  (no committed home -- minimal owning header).
//
// IsHostAlive @ 0x82872258 reads two u16 frame fields out of the manager:
//   +0x5BA  mu16HostFrame    (the last host heartbeat frame)
//   +0x5D2  mu16AliveWindow  (allowed frame gap before the host is dead)
// The surrounding bytes are not modelled by this TU, so they are reserved as a
// sized opaque placeholder so those two named fields land at the exact offsets the
// asm dereferences. FLAGGED: only the two frame fields are reconstructed by name;
// everything else in HostMigrationManager is an opaque sized placeholder.
namespace CgsNetwork
{
    struct HostMigrationManager
    {
        u8  mReserved0[0x5BA];      // 0x000 .. 0x5B9 (opaque, FLAGGED placeholder)
        u16 mu16HostFrame;          // 0x5BA
        u8  mReserved1[0x5D2 - 0x5BA - 2]; // 0x5BC .. 0x5D1 (opaque placeholder)
        u16 mu16AliveWindow;        // 0x5D2

        bool IsHostAlive(u16 lu16CurrentFrame) const;
    };
}
