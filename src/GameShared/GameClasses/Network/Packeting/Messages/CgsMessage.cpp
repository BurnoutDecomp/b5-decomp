#include "CgsMessage.h"

// CgsNetwork::Message  -- the 7 reconstructed functions of the
// class:CgsNetwork::Message translation unit.
//
//   GetGameID         @ 0x8286F3E8
//   Pack              @ 0x82880160
//   PrepareAck        @ 0x8286F488
//   PrepareForSend    @ 0x8257D5F8
//   PrepareNack       @ 0x8286F508
//   SetType           @ 0x82579C30
//   UnPack            @ 0x828801F0
//
// All offsets/stores are store-for-store faithful to the X360 pseudocode + asm.
// Members are referenced by name; asserts use the house CGS_ASSERT machinery.

namespace CgsNetwork
{
    // ---- GetGameID @ 0x8286F3E8 ------------------------------------------------
    // The X360 build, when the game-id is still invalid, streams a diagnostic line
    // ("We have received a message with GameID -1 ...") into an ostream-like sink
    // (off_82F335C8) and then asserts mu8GameID != KU8_INVALID_GAME_ID. The sink is
    // an un-homed global stream object that this TU cannot model by name, so the
    // logging side-channel is reduced to the assert it guards (the assert is the
    // observable contract). FLAGGED: the off_82F335C8 stream-logging side effect is
    // dropped (un-homed data-global), the assert is preserved.
    u8 Message::GetGameID() const
    {
        CGS_ASSERT(mu8GameID != KU8_INVALID_GAME_ID,
                   "mu8GameID != KU8_INVALID_GAME_ID");
        return mu8GameID;
    }

    // ---- SetType @ 0x82579C30 --------------------------------------------------
    Message* Message::SetType(s32 leType)
    {
        CGS_ASSERT(leType >= 0, "leType >= 0");
        CGS_ASSERT(leType < KI_E_MESSAGE_TYPE_COUNT, "leType < E_MESSAGE_TYPE_COUNT");
        CGS_ASSERT(leType < 255, "leType < 255");
        mi8Type = static_cast<s8>(leType);
        return this;
    }

    // ---- PrepareForSend @ 0x8257D5F8 ------------------------------------------
    void Message::PrepareForSend(s32 leType, u16 lu16Frame)
    {
        CGS_ASSERT(lu16Frame != KU16_INVALID_FRAME, "lu16Frame != KU16_INVALID_FRAME");
        SetType(leType);
        mu16Frame = lu16Frame;
        mx8Flags |= KX8_FLAGS_VALID;
    }

    // ---- PrepareAck @ 0x8286F488 ----------------------------------------------
    void Message::PrepareAck(s32 leType, u16 lu16Frame, u8 lu8GameID)
    {
        CGS_ASSERT((mx8Flags & KX8_FLAGS_VALID) == 0, "!IsMessageValid()");
        mu16Frame = lu16Frame;
        mx8Flags  = KX8_FLAGS_ACK;
        SetType(leType);
        mu8GameID = lu8GameID;
        mx8Flags |= KX8_FLAGS_VALID;
    }

    // ---- PrepareNack @ 0x8286F508 ---------------------------------------------
    void Message::PrepareNack(s32 leType, u16 lu16Frame, u8 lu8GameID)
    {
        CGS_ASSERT((mx8Flags & KX8_FLAGS_VALID) == 0, "!IsMessageValid()");
        mu16Frame = lu16Frame;
        mx8Flags  = KX8_FLAGS_NACK;
        SetType(leType);
        mu8GameID = lu8GameID;
        mx8Flags |= KX8_FLAGS_VALID;
    }

    // ---- Pack @ 0x82880160 -----------------------------------------------------
    // Lays the bitstream cursor words out for a pack pass, calls the virtual
    // PackOrUnpack() (vtable slot +0x10), records how many bits were written, then
    // resets the cursor words and marks the message done (mePackOrUnpack = 2).
    // Returns true when PackOrUnpack reported success (result == 0).
    bool Message::Pack(s32 liA, s32 liB, s32 liC, s32* lpiBitsWritten)
    {
        // X360 0x82880160 masks the FIRST arg (liA=r4): liByteBits=8*(liA&7), cursor2=liA-(liA&7);
        // cursor0/cursor1 = liByteBits+liB (r5); cursor3 = liByteBits+liC (r6).
        const s32 liByteBits = 8 * (liA & 7);

        mePackOrUnpack    = E_PACK_INTO_BITSTREAM;   // a1[1] = 0
        muBitstreamCursor0 = static_cast<u32>(liByteBits + liB);          // a1[2]
        muBitstreamCursor1 = static_cast<u32>(liByteBits + liB);          // a1[3]
        muBitstreamCursor2 = static_cast<u32>(liA - (liA & 7));           // a1[4]
        muBitstreamCursor3 = static_cast<u32>(liByteBits + liC);          // a1[5]

        const s32 liStart = static_cast<s32>(muBitstreamCursor0);

        // virtual PackOrUnpack() -- vtable entry 4 (+0x10).
        typedef PackOrUnpackResult (*PackOrUnpackFn)(Message*);
        PackOrUnpackFn* lpVTable = static_cast<PackOrUnpackFn*>(mpVTable);
        const PackOrUnpackResult lxResult = lpVTable[4](this);

        *lpiBitsWritten = static_cast<s32>(muBitstreamCursor0) - liStart;

        muBitstreamCursor0 = 0;
        muBitstreamCursor1 = 0;
        muBitstreamCursor2 = 0;
        muBitstreamCursor3 = 0;
        mePackOrUnpack     = E_PACK_OR_UNPACK_COUNT;  // a1[1] = 2

        return lxResult == KX_PACK_OR_UNPACK_SUCCESS;
    }

    // ---- UnPack @ 0x828801F0 ---------------------------------------------------
    // Symmetric to Pack for an unpack pass. Sets mePackOrUnpack = 1, lays the
    // cursor words, calls virtual PackOrUnpack() (+0x10) to deserialise, then calls
    // the first virtual slot (vtable+0x00) for post-unpack processing; that second
    // call's truthy result sets the RELIABLE flag bit. Records bits read, resets
    // the cursors, marks done, and asserts the pack/unpack succeeded.
    Message* Message::UnPack(s32 liA, s32 liB, s32 liC, s32* lpiBitsRead)
    {
        mePackOrUnpack = E_UNPACK_FROM_BITSTREAM;   // *(a1+4) = 1

        // X360 0x828801F0 masks the FIRST arg (liA=r4): liByteBits=8*(liA&7), cursor2=liA-(liA&7);
        // cursor0/cursor3 = liByteBits+liC (r6); cursor1 = liByteBits+liB (r5).
        const s32 liByteBits = 8 * (liA & 7);
        muBitstreamCursor2 = static_cast<u32>(liA - (liA & 7));          // *(a1+0x10)
        muBitstreamCursor0 = static_cast<u32>(liByteBits + liC);         // *(a1+8)
        muBitstreamCursor3 = static_cast<u32>(liByteBits + liC);         // *(a1+0x14)
        muBitstreamCursor1 = static_cast<u32>(liByteBits + liB);         // *(a1+0xC)

        typedef PackOrUnpackResult (*PackOrUnpackFn)(Message*);
        typedef Message* (*PostUnpackFn)(Message*);
        void** lpVTable = static_cast<void**>(mpVTable);

        const u32 luStartHi = muBitstreamCursor0;
        const u32 luStartLo = muBitstreamCursor1;
        const s32 liDelta    = static_cast<s32>(luStartHi - luStartLo);

        mx8Flags = 0;

        // virtual PackOrUnpack() -- vtable entry 4 (+0x10).
        PackOrUnpackFn lpPackOrUnpack = reinterpret_cast<PackOrUnpackFn>(lpVTable[4]);
        const PackOrUnpackResult lxResult = lpPackOrUnpack(this);

        // virtual slot 0 (+0x00) -- post-unpack handler; truthy => RELIABLE.
        PostUnpackFn lpPostUnpack = reinterpret_cast<PostUnpackFn>(lpVTable[0]);
        mx8Flags |= KX8_FLAGS_VALID;
        Message* lpResult = lpPostUnpack(this);
        if (lpResult)
        {
            mx8Flags |= KX8_FLAGS_RELIABLE;
        }

        *lpiBitsRead = static_cast<s32>(muBitstreamCursor1) - static_cast<s32>(muBitstreamCursor0) + liDelta;

        muBitstreamCursor0 = 0;
        muBitstreamCursor1 = 0;
        muBitstreamCursor2 = 0;
        muBitstreamCursor3 = 0;
        mePackOrUnpack     = E_PACK_OR_UNPACK_COUNT;  // *(a1+4) = 2

        CGS_ASSERT(lxResult == KX_PACK_OR_UNPACK_SUCCESS,
                   "lxPackOrUnpackResult == KX_PACK_OR_UNPACK_SUCCESS");
        return lpResult;
    }
}
