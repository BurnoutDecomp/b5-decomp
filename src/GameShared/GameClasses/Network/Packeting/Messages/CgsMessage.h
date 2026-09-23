#pragma once

#include <cstddef>   // offsetof

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsSmartBitStream.h"
#include "rw/math/vpu/types.h"

// CgsNetwork::Message
//
// Canonical class home for the network-message base. Console layout (byte
// offsets the per-function asm dereferences):
//
//   +0x00  vptr             (5 slots: IsReliable, OldMessagesAreValid,
//                           GetPackedMessageSize, GetName, PackOrUnpack)
//   +0x04  mePackOrUnpack   (EPackOrUnpack; used as a 3-state lifecycle marker:
//                           0 = packing, 1 = unpacking, 2 = idle)
//   +0x08  mBitstream       (CgsNetwork::SmartBitStream, by value, 0x10 bytes:
//                           write position, read position, buffer, size)
//   +0x18  mu8GameID        (uint8)
//   +0x19  mx8Flags         (uint8 bit-set: VALID|RELIABLE|ACK|NACK)
//   +0x1A  mi8Type          (int8 EMessageType)
//   +0x1C  mu16Frame        (uint16)
//
// The bitstream holds a buffer pointer, so on a 64-bit host every member from
// mBitstream on sits later than its console offset; members are addressed by
// name only.

// Forward declarations for the Time / DateAndTime field primitives below; the concrete
// classes live in the GameShared/GameClasses/System/Timer headers.
namespace CgsSystem { class Time; class DateAndTime; }

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
    const PackOrUnpackResult KX_PACK_FAILED_NO_SPACE   = 1;

    struct Message
    {
        // Lifecycle states stored in mePackOrUnpack during Pack/UnPack.
        enum EPackOrUnpack
        {
            E_PACK_INTO_BITSTREAM   = 0,
            E_UNPACK_FROM_BITSTREAM = 1,
            E_PACK_OR_UNPACK_COUNT  = 2,
        };

        // --- virtual interface (console vtable order, slots 0..4) ---
        // Slot 0: the base reports "not reliable"; ReliableMessage overrides it.
        virtual bool               IsReliable() const;
        // Slot 1: whether messages of this type tolerate older-frame arrivals
        // (ReliableMessageManager::MessageIsDuplicate). The base reports false.
        virtual bool               OldMessagesAreValid() const;
        // Slot 2: packs the message into a scratch buffer of KI_MAX_PACKED_MESSAGE_SIZE
        // bytes and returns the packed length in whole bytes. Subclasses zero their
        // payload first so the size is the same every call.
        virtual s32                GetPackedMessageSize();
        // Slot 3: every concrete message names itself; the base has no body anywhere
        // in the image (no base vtable is ever emitted), so it stays pure.
        virtual const char*        GetName() const = 0;

    protected:
        // Slot 4: the base serialises nothing and reports success. Subclasses
        // override it and OR the per-field statuses together.
        virtual PackOrUnpackResult PackOrUnpack();

    public:
        // --- layout (frozen; the compiler's vptr is +0x00) ---
        s32            mePackOrUnpack;           // +0x04
        SmartBitStream mBitstream;               // +0x08 (0x10 console bytes)
        u8             mu8GameID;                // +0x18
        u8             mx8Flags;                 // +0x19
        s8             mi8Type;                  // +0x1A
        u16            mu16Frame;                // +0x1C

        // --- reconstructed members (this TU) ---
        // Placement-style initialiser the console build calls "Construct" (returns this).
        Message* Construct();
        u8   GetGameID() const;
        Message* SetType(s32 leType);

        // --- inline flag/scalar accessors (used by the NetworkPlayer send pump) ------------
        // The "valid" flag (KX8_FLAGS_VALID, bit 0 of mx8Flags) marks a message that has been
        // queued for (re)send and not yet consumed. The pump reads it to decide which slots to
        // pack, clears it once an unreliable message is sent, and re-sets it when re-queuing a
        // reliable message for resend.
        void SetGameID(u8 lu8GameID)        { mu8GameID = lu8GameID; }
        s8   GetType() const                { return mi8Type; }
        bool IsMessageValid() const         { return (mx8Flags & KX8_FLAGS_VALID) != 0; }
        void SetMessageValid()              { mx8Flags |=  KX8_FLAGS_VALID; }
        void SetMessageInvalid()            { mx8Flags &= ~KX8_FLAGS_VALID; }
        void PrepareForSend(s32 leType, u16 lu16Frame);
        void PrepareAck(s32 leType, u16 lu16Frame, u8 lu8GameID);
        void PrepareNack(s32 leType, u16 lu16Frame, u8 lu8GameID);

        // Pack/UnPack attach mBitstream to the caller's buffer, call the virtual
        // PackOrUnpack() through the vtable (slot 4, +0x10), report the bits
        // written / read, and detach the stream again. UnPack also re-derives the
        // VALID and RELIABLE flags (IsReliable, slot 0).
        bool Pack(u8* lpu8Buffer, s32 liBufferOffsetInBits, s32 liBufferLengthInBits,
                  s32* lpiBitsWritten);
        void UnPack(u8* lpu8Buffer, s32 liBufferReadOffsetInBits, s32 liBufferLengthInBits,
                    s32* lpiBitsRead);

        // (De)serialise a raw byte buffer through the message's SmartBitStream:
        // mePackOrUnpack == 0 packs (AddRawData), == 1 unpacks (GetRawData),
        // anything else asserts. KX_PACK_OR_UNPACK_SUCCESS on success.
        PackOrUnpackResult PackOrUnpackBuffer(char* lpcBuffer, s32 liNumBytes);

        // (De)serialise one quantised signed-byte field in [liMin, liMax] through
        // the message's bitstream. mePackOrUnpack == 0 packs (quantise via
        // IntQuantiser::Pack, write via BitStream::AddBits), == 1 unpacks (read the
        // quantised bits back into the field), anything else asserts.
        // KX_PACK_OR_UNPACK_SUCCESS on success.
        PackOrUnpackResult PackOrUnpack(s8* lpi8Field, s32 liMin, s32 liMax);

        // Build a scaled direction vector from two angles: the X360 build computes
        // (cos(B)*cos(A), sin(B), cos(B)*sin(A), 0) and scales it by lfMagnitude.
        // Static helper (writes the result through the output vector pointer).
        static void GetVectorFromAngles(rw::math::vpu::Vector3* lpvOut,
                                        f32 lfAngleA, f32 lfAngleB, f32 lfMagnitude);

        // The rotation rows of an affine from roll / pitch / yaw (the translation row and
        // every w lane are left as they are), and back. Used by the matrix field primitive.
        static void SetMatrixFromEulerAngles(rw::math::vpu::Matrix44Affine* lpMatrix,
                                             f32 lfRoll, f32 lfPitch, f32 lfYaw);
        // Declaration only: vector code (an inlined arc-sine and two arc-tangents built on
        // reciprocal estimates) that is not reconstructed yet.
        static void GetEulerAnglesFromMatrix(rw::math::vpu::Matrix44Affine lMatrix,
                                             f32* lpfRollOut, f32* lpfPitchOut, f32* lpfYawOut);
        // Declaration only, for the same reason: the two angles and the magnitude of a vector.
        static void GetAnglesFromVector(rw::math::vpu::Vector3 lVector,
                                        f32* lpfAngleAOut, f32* lpfAngleBOut, f32* lpfMagnitudeOut);
    };

    // Console layout, checked on a 32-bit build (the vptr is the +0x00 word).
    static_assert(sizeof(void*) != 4 || offsetof(Message, mePackOrUnpack) == 0x04, "Message::mePackOrUnpack @ +0x04");
    static_assert(sizeof(void*) != 4 || offsetof(Message, mBitstream)     == 0x08, "Message::mBitstream @ +0x08");
    static_assert(sizeof(void*) != 4 || offsetof(Message, mu8GameID)      == 0x18, "Message::mu8GameID @ +0x18");
    static_assert(sizeof(void*) != 4 || offsetof(Message, mx8Flags)       == 0x19, "Message::mx8Flags @ +0x19");
    static_assert(sizeof(void*) != 4 || offsetof(Message, mi8Type)        == 0x1A, "Message::mi8Type @ +0x1A");
    static_assert(sizeof(void*) != 4 || offsetof(Message, mu16Frame)      == 0x1C, "Message::mu16Frame @ +0x1C");
    static_assert(sizeof(void*) != 4 || sizeof(Message) == 0x20, "sizeof(Message) == 0x20");

    // 16-bit frame-ring helpers (homed in CgsMessageFrameUtils.cpp; declared in
    // CgsMessageFrameUtils.h). Declared here so the rest of the Message hierarchy
    // -- e.g. HostMigrationManager::IsHostAlive -- can call them by name.
    bool UInt16IsLargerWrapped(u16 lu16A, u16 lu16B);
    bool UInt16IsLargerOrEqualWrapped(u16 lu16A, u16 lu16B);
    u16  GetFrameDiffWrapped16(u16 lu16FrameA, u16 lu16FrameB);

    // 50 Hz <-> 60 Hz translation of a 16-bit frame counter received from a console
    // running the other simulation rate (TrafficManager frame conversion,
    // BrnNetworkPlayer::RetrieveBufferedMessage).
    u16 TranslateFrame50HzTo60Hz(u16 lu16Frame50Hz, u16 lu16CurrentFrame50Hz, u16 lu16NumWraps);
    u16 TranslateFrame60HzTo50Hz(u16 lu16Frame60Hz, u16 lu16CurrentFrame60Hz, u16 lu16NumWraps);
    // The float reference versions the translations are cross-checked against
    // (CgsOldFrameConversionFunctions.cpp).
    u16 OLDTranslateFrame50HzTo60Hz(u16 lu16Frame50Hz, u16 lu16CurrentFrame50Hz, u16 lu16NumWraps);
    u16 OLDTranslateFrame60HzTo50Hz(u16 lu16Frame60Hz, u16 lu16CurrentFrame60Hz, u16 lu16NumWraps);

    // ------------------------------------------------------------------------
    // Shared field (de)serialise primitives, homed in CgsMessage.cpp: the overloads of
    // the Message::PackOrUnpack field primitive, spelled as free functions that take
    // the message. The lifecycle word (mePackOrUnpack) picks pack vs unpack; each
    // returns a per-field status the callers OR together (0 == all fields succeeded ==
    // KX_PACK_OR_UNPACK_SUCCESS). Fields are passed by pointer.
    //   Int / U8 / S16 / U16 / UInt -- a quantised integer in [liMin, liMax].
    //   Bool  -- a single flag (a u8 in [0, 1]).
    //   CgsID -- the low and high 32-bit halves as two full-range ints.
    //   Float -- a float in [lfMin, lfMax], quantised either to lfResolution or to
    //            liNumBits bits.
    //   Time  -- whole seconds as an int in [liMinSeconds, liMaxSeconds - 1] plus the
    //            fraction in [0, 1] at lfResolution; the two-argument form uses
    //            [0, INT_MAX - 1] seconds.
    //   DateAndTime -- second, minute, hour, day, month and year as bounded ints.
    //   Matrix / Vector -- the Euler-angle and direction-angle quantisers; declaration
    //            only (their angle helpers are vector code not reconstructed yet).
    PackOrUnpackResult PackOrUnpackInt(Message* lpMessage, s32* lpiField, s32 liMin, s32 liMax);
    PackOrUnpackResult PackOrUnpackU8(Message* lpMessage, u8* lpu8Field, s32 liMin, s32 liMax);
    PackOrUnpackResult PackOrUnpackS16(Message* lpMessage, s16* lps16Field, s32 liMin, s32 liMax);
    PackOrUnpackResult PackOrUnpackU16(Message* lpMessage, u16* lpu16Field, s32 liMin, s32 liMax);
    PackOrUnpackResult PackOrUnpackUInt(Message* lpMessage, u32* lpu32Field, s32 liMin, s32 liMax);
    PackOrUnpackResult PackOrUnpackBool(Message* lpMessage, bool* lpbField);
    PackOrUnpackResult PackOrUnpackCgsID(Message* lpMessage, u64* lpu64Field);
    PackOrUnpackResult PackOrUnpackFloat(Message* lpMessage, f32* lpfField, f32 lfMin, f32 lfMax, f32 lfResolution);
    PackOrUnpackResult PackOrUnpackFloat(Message* lpMessage, f32* lpfField, f32 lfMin, f32 lfMax, s32 liNumBits);
    PackOrUnpackResult PackOrUnpackTime(Message* lpMessage, CgsSystem::Time* lpTimeField,
                                        s32 liMinSeconds, s32 liMaxSeconds, f32 lfResolution);
    PackOrUnpackResult PackOrUnpackTime(Message* lpMessage, CgsSystem::Time* lpTimeField, f32 lfResolution);
    PackOrUnpackResult PackOrUnpackDateAndTime(Message* lpMessage, CgsSystem::DateAndTime* lpDateAndTime);
    // Matrix: the rotation as three Euler angles (roll/pitch/yaw bit widths), then the
    // translation row per axis into [lPosMin, lPosMax] (per-axis bit widths).
    PackOrUnpackResult PackOrUnpackMatrix(Message* lpMessage, rw::math::vpu::Matrix44Affine* lpMatrix,
                                          s32 liRollBits, s32 liPitchBits, s32 liYawBits,
                                          rw::math::vpu::Vector3 lPosMin, rw::math::vpu::Vector3 lPosMax,
                                          s32 liPosXBits, s32 liPosYBits, s32 liPosZBits);
    // Vector: a bounded-magnitude vector as two angles plus a magnitude. The f32 bound
    // sits between the second and third bit widths in the parameter list.
    PackOrUnpackResult PackOrUnpackVector(Message* lpMessage, rw::math::vpu::Vector3* lpvField,
                                          s32 liXBits, s32 liYBits, f32 lfMagnitudeBound, s32 liZBits);

    // CgsNetwork::MessageWithPlayerIDs and CgsNetwork::ReliableMessage are the next two
    // rungs of the message hierarchy. They now live in their proper home headers
    // (CgsMessageWithPlayerIDs.h / CgsReliableMessage.h), which #include this file --
    // include those when you need a ReliableMessage-derived type. (They used to be a
    // single bare `ReliableMessage : Message` stub here; recovering the real DWARF
    // hierarchy moved them out and gave MessageWithPlayerIDs its two player-id fields.)
}

// CgsNetwork::HostMigrationManager now has its real owning home in
//   GameShared/GameClasses/Network/Players/CgsHostMigrationManager.h
// (full peer-to-peer host-migration manager). The earlier minimal opaque placeholder
// that used to live here -- modelling only mu16HostFrame/mu16AliveWindow so the
// other-TU IsHostAlive body could compile -- was a catch-all stand-in and would
// collide (ODR) with the real definition, so it has been removed in favour of a plain
// forward declaration. IsHostAlive @ 0x82872258 is bodied in CgsMessageSubclasses.cpp
// against the real class; it reads mu16LastHostKeepAliveReceivedTime (the last host
// heartbeat frame, +0x5BA) and mu16HostKeepAliveTimeout (the alive window, +0x5D2).
namespace CgsNetwork
{
    struct HostMigrationManager;   // real home: Network/Players/CgsHostMigrationManager.h
}
