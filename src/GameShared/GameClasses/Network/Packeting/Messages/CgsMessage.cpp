#include "CgsMessage.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsSmartBitStream.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsIntQuantiser.h"

#include <cmath>   // sinf, cosf

// CgsNetwork::Message  -- the reconstructed functions of the
// class:CgsNetwork::Message translation unit.
//
//   GetGameID            @ 0x8286F3E8
//   Pack                 @ 0x82880160
//   PrepareAck           @ 0x8286F488
//   PrepareForSend       @ 0x8257D5F8
//   PrepareNack          @ 0x8286F508
//   SetType              @ 0x82579C30
//   UnPack               @ 0x828801F0
//   Construct            @ 0x82870B90
//   GetVectorFromAngles  @ 0x82870D78
//   PackOrUnpack         @ 0x82880F50
//   PackOrUnpackBuffer   @ 0x8288EA70
//
// All offsets/stores are store-for-store faithful to the X360 pseudocode + asm.
// Members are referenced by name; asserts use the house CGS_ASSERT machinery.

namespace CgsNetwork
{
    // ---- Construct @ 0x82870B90 ------------------------------------------------
    // Field initialiser the X360 build calls "Construct": stores the invalid
    // sentinels into the trailing scalars and returns the object. The asm writes
    //   stb 0xFF, 0x18  -> mu8GameID = KU8_INVALID_GAME_ID (255)
    //   stb 0x00, 0x19  -> mx8Flags  = 0
    //   stb -1,   0x1A  -> mi8Type   = -1 (KI8_INVALID_TYPE)
    //   sth -1,   0x1C  -> mu16Frame = 0xFFFF (KU16_INVALID_FRAME)
    Message* Message::Construct()
    {
        mu8GameID = KU8_INVALID_GAME_ID;   // 0x18 <- 0xFF
        mx8Flags  = 0;                     // 0x19 <- 0
        mi8Type   = static_cast<s8>(-1);   // 0x1A <- -1
        mu16Frame = KU16_INVALID_FRAME;    // 0x1C <- 0xFFFF
        return this;
    }

    // ---- GetVectorFromAngles @ 0x82870D78 --------------------------------------
    // Builds a scaled direction vector from two angles. The X360 build evaluates
    // sinf/cosf of the two angle args (the asm calls sin/cos on doubles and frsps
    // each result back to f32), assembles the unit direction
    //   ( cos(B)*cos(A), sin(B), cos(B)*sin(A), 0 )
    // on the stack, splats the magnitude across all four lanes, multiplies
    // (vmulfp128) and stores the 16-byte result through the output pointer. The
    // VMX is lowered to the equivalent scalar f32 stores it computes lane-for-lane
    // (lane0=x .. lane3=w), preserving the exact lane mapping (var_60/5C/58/54).
    void Message::GetVectorFromAngles(rw::math::vpu::Vector3* lpvOut,
                                      f32 lfAngleA, f32 lfAngleB, f32 lfMagnitude)
    {
        const f32 lfSinA = sinf(lfAngleA);
        const f32 lfCosA = cosf(lfAngleA);
        const f32 lfSinB = sinf(lfAngleB);
        const f32 lfCosB = cosf(lfAngleB);

        lpvOut->x = lfMagnitude * (lfCosB * lfCosA);   // var_60, lane 0
        lpvOut->y = lfMagnitude * lfSinB;              // var_5C, lane 1
        lpvOut->z = lfMagnitude * (lfCosB * lfSinA);   // var_58, lane 2
        lpvOut->w = 0.0f;                              // var_54, lane 3 (zeroed)
    }

    // ---- sub_828800F0 (quantised-int unpack helper) ----------------------------
    // Read one quantised int in [liMin, liMax] back out of the bitstream: derive
    // the field's bit width as bit_width(liMax - liMin) -- the GetNumBits formula
    // the X360 inlines here (no call: it shifts (max-min) right to zero
    // and counts the shifts) -- read that many bits via BitStream::GetBits, then
    // hand the packed offset to IntQuantiser::UnPack to fold it back onto liMin and
    // clamp. Returns the reconstructed value.
    static s32 UnPackQuantisedInt(BitStream* lpStream, s32 liMin, s32 liMax)
    {
        // Inlined GetNumBits(liMin, liMax): count the right-shifts of (max-min).
        u32 luSpan   = static_cast<u32>(liMax) - static_cast<u32>(liMin);
        s32 liNumBits = 0;
        if (liMax != liMin)
        {
            do
            {
                luSpan >>= 1;
                ++liNumBits;
            }
            while (luSpan);
        }

        const u32 luPacked = static_cast<u32>(lpStream->GetBits(liNumBits));
        s32 liValue = 0;
        IntQuantiser::UnPack(&liValue, liMin, liMax, luPacked);
        return liValue;
    }

    // ---- PackOrUnpack @ 0x82880F50 ---------------------------------------------
    // (De)serialise one quantised signed-byte field in [liMin, liMax] through the
    // message's bitstream (which lives at this+8; the four cursor words modelled
    // here reinterpreted as the BitStream object the asm addresses with
    // `addi r3, r31, 8`). The lifecycle word decides direction: 0 packs (quantise
    // the sign-extended field via IntQuantiser::Pack, then BitStream::AddBits the
    // packed offset for the reported number of bits), 1 unpacks (read the value
    // back via the quantised-int helper and store it into the byte field), any
    // other value is the "called without telling it which it is doing" assert
    // (CgsMessage.cpp:518). The pack path returns (AddBits(...) == 0): the asm
    // treats a zero result as success and a non-zero result as failure.
    // FLAGGED: the committed BitStream::AddBits returns a true/false sense
    // (true == success); this caller's asm compares its result against 0, so the
    // boolean polarity is reproduced exactly as the asm dictates rather than
    // re-interpreted against AddBits's own convention.
    bool Message::PackOrUnpack(s8* lpi8Field, s32 liMin, s32 liMax)
    {
        BitStream* lpStream =
            reinterpret_cast<BitStream*>(&muBitstreamCursor0);   // this + 8

        if (mePackOrUnpack == E_PACK_INTO_BITSTREAM)          // *(a1+4) == 0
        {
            u32 luPacked  = 0;
            s32 liNumBits = 0;
            IntQuantiser::Pack(*lpi8Field, liMin, liMax, &luPacked, &liNumBits);
            return lpStream->AddBits(luPacked, liNumBits) == 0;
        }

        if (mePackOrUnpack == E_UNPACK_FROM_BITSTREAM)        // *(a1+4) == 1
        {
            *lpi8Field = static_cast<s8>(UnPackQuantisedInt(lpStream, liMin, liMax));
            return false;
        }

        CGS_ASSERT(false,
                   "CgsNetwork::Message::PackOrUnpack called without telling "
                   "it which it is doing");
        return false;
    }

    // ---- PackOrUnpackBuffer @ 0x8288EA70 ---------------------------------------
    // (De)serialise a raw byte buffer through the message's SmartBitStream (which
    // lives at this+8; modelled here as the four cursor words, reinterpreted as the
    // SmartBitStream object the asm addresses with `addi r3, r3, 8`). The lifecycle
    // word decides direction: 0 packs (AddRawData), 1 unpacks (GetRawData), any
    // other value is the "called without telling it which it is doing" assert
    // (CgsMessage.cpp:1417). The pack path returns (AddRawData(...) == 0): the asm
    // treats a zero result as success and a non-zero result as failure.
    // FLAGGED: the committed SmartBitStream::AddRawData returns a true/false sense
    // (true == success); this caller's asm compares its result against 0, so the
    // boolean polarity is reproduced exactly as the asm dictates rather than
    // re-interpreted against AddRawData's own convention.
    bool Message::PackOrUnpackBuffer(char* lpcBuffer, s32 liNumBytes)
    {
        SmartBitStream* lpStream =
            reinterpret_cast<SmartBitStream*>(&muBitstreamCursor0);   // this + 8

        if (mePackOrUnpack == E_PACK_INTO_BITSTREAM)          // *(a1+4) == 0
        {
            return lpStream->AddRawData(lpcBuffer, liNumBytes) == 0;
        }

        if (mePackOrUnpack == E_UNPACK_FROM_BITSTREAM)        // *(a1+4) == 1
        {
            lpStream->GetRawData(lpcBuffer, liNumBytes);
            return false;
        }

        CGS_ASSERT(false,
                   "CgsNetwork::Message::PackOrUnpackBuffer called without telling "
                   "it which it is doing");
        return false;
    }

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
