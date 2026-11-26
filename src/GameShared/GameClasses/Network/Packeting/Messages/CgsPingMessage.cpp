// ===================================================================================
// CgsNetwork::PingMessage / CgsNetwork::PingReplyMessage -- method bodies
//   b5-decomp/src/GameShared/GameClasses/Network/Packeting/Messages/CgsPingMessage.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   PingMessage::GetPackedMessageSize      @ 0x828826B0
//   PingMessage::PrepareForSend            @ 0x82882628   (called by NetworkPlayer::UpdatePing)
//   PingMessage::Retrieve                  @ 0x8288EB68   (called by NetworkPlayer::UpdatePing)
//   PingReplyMessage::PackOrUnpack         @ 0x8288EBF0
//   PingReplyMessage::PrepareForSend       @ 0x828826C0   (called by NetworkPlayer::UpdatePing)
//   PingReplyMessage::Retrieve             @ 0x8288EC10   (called by NetworkPlayer::UpdatePing)
//
// A PingMessage / PingReplyMessage each carry a single f32 mfPingTime at +0x20 (the
// derived-class first field, after the 0x20-byte Message base). GetPackedMessageSize
// zeroes that field then delegates to the base packed-size (mirrors HeadsetStatusMessage
// @0x82882748). PrepareForSend gates on the VALID flag, stores the ping time, chains to
// Message::PrepareForSend with the wire type id (6 = PING, 7 = PING_REPLY), then asserts
// the message is not reliable. Retrieve copies the ping time out, clears VALID, and
// asserts the message is now invalid. PingReplyMessage::PackOrUnpack routes mfPingTime
// through the shared quantised-float (de)serialise primitive PackOrUnpackFloat.
//
// The three ping-time range constants (KF_MIN_PING_TIME / KF_MAX_PING_TIME /
// KF_MAX_PING_TIME_ERROR) are DWARF-named at CgsPingMessage.cpp:29-31 and passed to
// PackOrUnpackFloat as (fMin, fMax, resolution) [asm f1=flt_8307A2B0, f2=flt_82F335D4,
// f3=flt_82F335D8]. Their exact rodata float values were NOT recoverable in scope --
// left as named TU constants with placeholder 0.0f; CONSOLIDATOR: pin from rodata.
#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Network\\Packeting\\Messages\\CgsPingMessage.cpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsPingMessage.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameShared\\GameClasses\\Network\\Packeting\\Messages\\CgsPingMessage.cpp".

namespace CgsNetwork
{
    // Message type ids (asm `li r4,6` / `li r4,7`), following the committed
    // KI_E_MESSAGE_TYPE_SYNC_TIME convention (CgsSyncTimeBase.h).
    static const s32 KI_E_MESSAGE_TYPE_PING       = 6;
    static const s32 KI_E_MESSAGE_TYPE_PING_REPLY = 7;

    // Ping-time quantiser range + resolution passed to PackOrUnpackFloat. The maximum
    // (259200 s, three days) and the resolution (0.02 s) are the initialised data words.
    // FLAG: the minimum lives in zero-initialised data; 0.0f is its image value and no
    // run-time initialiser for it has been searched for yet.
    const f32 KF_MIN_PING_TIME       = 0.0f;
    const f32 KF_MAX_PING_TIME       = 259200.0f;
    const f32 KF_MAX_PING_TIME_ERROR = 0.02f;

    // ---- PingMessage ----------------------------------------------------------

    // X360 0x828826B0. Zeroes mfPingTime (stfs 0.0f @ +0x20) then tail-calls the base
    // packed-size query. Mirrors HeadsetStatusMessage::GetPackedMessageSize.
    s32 PingMessage::GetPackedMessageSize()
    {
        mfPingTime = 0.0f;
        return Message::GetPackedMessageSize();
    }

    // X360 0x82882628. Type 6 = ping.
    void PingMessage::PrepareForSend(u16 lu16Frame, f32 lfPingTime)
    {
        if ((mx8Flags & KX8_FLAGS_VALID) == 0)
        {
            mfPingTime = lfPingTime;                                 // stfs @ +0x20
            Message::PrepareForSend(KI_E_MESSAGE_TYPE_PING, lu16Frame);
            CGS_ASSERT_AT(!IsReliable(),
                "!IsReliable()", CGS_ARTIST_FILE, 88 /* ARTIST 0x82882684 */);              // vtable slot0
        }
    }

    // X360 0x8288EB68. Consumes a received ping: copies out the time, clears VALID,
    // asserts the message is now invalid.
    bool PingMessage::Retrieve(f32* lpfPingTime)
    {
        // ARTIST 0x8288EB80 branches to a trailing `return false` (0x8288EBD8): the copy-out is the body
        // of the valid test.
        if ((mx8Flags & KX8_FLAGS_VALID) != 0)
        {
            *lpfPingTime = mfPingTime;                 // lfs +0x20 -> stfs 0(lpfPingTime)
            mx8Flags &= ~KX8_FLAGS_VALID;              // clrrwi bit0, stb +0x19
            CGS_ASSERT_AT(!IsMessageValid(),
                "!IsMessageValid()",
                CGS_ARTIST_FILE, 116 /* ARTIST 0x8288EBBC */);
            return true;
        }
        return false;
    }

    // The ping time travels as a quantised float; the console body is the same code as
    // PingReplyMessage::PackOrUnpack (one folded copy).
    PackOrUnpackResult PingMessage::PackOrUnpack()
    {
        return PackOrUnpackFloat(this, &mfPingTime,
                                 KF_MIN_PING_TIME, KF_MAX_PING_TIME,
                                 KF_MAX_PING_TIME_ERROR);
    }

    // ---- PingReplyMessage -----------------------------------------------------

    // Same code as PingMessage::GetPackedMessageSize (one folded copy): zero the ping
    // time, then size through the base.
    s32 PingReplyMessage::GetPackedMessageSize()
    {
        mfPingTime = 0.0f;
        return Message::GetPackedMessageSize();
    }

    // X360 0x8288EBF0. Tail-calls the shared quantised-float (de)serialise primitive
    // on mfPingTime. Return type is the namespace-scope typedef CgsNetwork::PackOrUnpackResult.
    PackOrUnpackResult PingReplyMessage::PackOrUnpack()
    {
        return PackOrUnpackFloat(this, &mfPingTime,
                                 KF_MIN_PING_TIME, KF_MAX_PING_TIME,
                                 KF_MAX_PING_TIME_ERROR);
    }

    // X360 0x828826C0. Type 7 = ping reply.
    void PingReplyMessage::PrepareForSend(u16 lu16Frame, f32 lfPingTime)
    {
        if ((mx8Flags & KX8_FLAGS_VALID) == 0)
        {
            mfPingTime = lfPingTime;                                 // stfs @ +0x20
            Message::PrepareForSend(KI_E_MESSAGE_TYPE_PING_REPLY, lu16Frame);
            CGS_ASSERT_AT(!IsReliable(),
                "!IsReliable()", CGS_ARTIST_FILE, 253 /* ARTIST 0x8288271C */);              // vtable slot0
        }
    }

    // X360 0x8288EC10. Byte-identical body to PingMessage::Retrieve on the reply's own
    // +0x20 ping-time field.
    bool PingReplyMessage::Retrieve(f32* lpfPingTime)
    {
        // ARTIST 0x8288EC28 branches to a trailing `return false` (0x8288EC80): the copy-out is the body
        // of the valid test.
        if ((mx8Flags & KX8_FLAGS_VALID) != 0)
        {
            *lpfPingTime = mfPingTime;                 // lfs +0x20 -> stfs 0(lpfPingTime)
            mx8Flags &= ~KX8_FLAGS_VALID;              // clrrwi bit0, stb +0x19
            CGS_ASSERT_AT(!IsMessageValid(),
                "!IsMessageValid()",
                CGS_ARTIST_FILE, 281 /* ARTIST 0x8288EC64 */);
            return true;
        }
        return false;
    }
} // namespace CgsNetwork

#undef CGS_ARTIST_FILE
