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

#include "GameShared/GameClasses/Network/Packeting/Messages/CgsPingMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsNetwork
{
    // Message type ids (asm `li r4,6` / `li r4,7`), following the committed
    // KI_E_MESSAGE_TYPE_SYNC_TIME convention (CgsSyncTimeBase.h).
    static const s32 KI_E_MESSAGE_TYPE_PING       = 6;
    static const s32 KI_E_MESSAGE_TYPE_PING_REPLY = 7;

    // Ping-time quantiser range + resolution passed to PackOrUnpackFloat.
    // DWARF-named (CgsPingMessage.cpp:29-31). asm rodata: flt_8307A2B0 (min),
    // flt_82F335D4 (max), flt_82F335D8 (resolution). Exact float values were not
    // recoverable in scope -- CONSOLIDATOR: pin these from rodata.
    // FLAG: placeholder VALUES (0.0f). Logic is faithful; the constants are TBD.
    const f32 KF_MIN_PING_TIME       = /* flt_8307A2B0  TBD */ 0.0f;
    const f32 KF_MAX_PING_TIME       = /* flt_82F335D4  TBD */ 0.0f;
    const f32 KF_MAX_PING_TIME_ERROR = /* flt_82F335D8  TBD */ 0.0f;

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
            CGS_ASSERT(!IsReliable(), "!IsReliable()");              // vtable slot0
        }
    }

    // X360 0x8288EB68. Consumes a received ping: copies out the time, clears VALID,
    // asserts the message is now invalid.
    bool PingMessage::Retrieve(f32* lpfPingTime)
    {
        if ((mx8Flags & KX8_FLAGS_VALID) == 0)
            return false;

        *lpfPingTime = mfPingTime;                 // lfs +0x20 -> stfs 0(lpfPingTime)
        mx8Flags &= ~KX8_FLAGS_VALID;              // clrrwi bit0, stb +0x19
        CGS_ASSERT(!IsMessageValid(), "!IsMessageValid()");
        return true;
    }

    // ---- PingReplyMessage -----------------------------------------------------

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
            CGS_ASSERT(!IsReliable(), "!IsReliable()");              // vtable slot0
        }
    }

    // X360 0x8288EC10. Byte-identical body to PingMessage::Retrieve on the reply's own
    // +0x20 ping-time field.
    bool PingReplyMessage::Retrieve(f32* lpfPingTime)
    {
        if ((mx8Flags & KX8_FLAGS_VALID) == 0)
            return false;

        *lpfPingTime = mfPingTime;                 // lfs +0x20 -> stfs 0(lpfPingTime)
        mx8Flags &= ~KX8_FLAGS_VALID;              // clrrwi bit0, stb +0x19
        CGS_ASSERT(!IsMessageValid(), "!IsMessageValid()");
        return true;
    }
} // namespace CgsNetwork
