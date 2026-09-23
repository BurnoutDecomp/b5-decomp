#include "GameShared/GameClasses/Network/Time/CgsStartTimeMessage.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (CgsNetwork::StartTimeMessage)
//
//   GetName @ 0x827DE0E8:  return "Start Time Message";
//
// GetName is the inline header-homed accessor (CgsStartTimeMessage.h); this .cpp is its
// definition home and pulls in the owning header so the class is the real
// ReliableMessage-derived type rather than a throwaway local shape.

namespace CgsNetwork
{
    // The start time travels as one quantised float over [KF_MIN_START_TIME,
    // KF_MAX_START_TIME] (three days) at KF_MAX_START_TIME_ERROR resolution. The minimum is
    // a zero-initialised data word that nothing writes; the other two are initialised data.
    const f32 KF_MIN_START_TIME       = 0.0f;
    const f32 KF_MAX_START_TIME       = 259200.0f;
    const f32 KF_MAX_START_TIME_ERROR = 0.02f;

    // Size the message with every field at its zero value: the start time, the host id
    // and the eight client ids, then the reliable-message base.
    s32 StartTimeMessage::GetPackedMessageSize()
    {
        mStartTime.mStartTime.SetFloatVal(0.0f);
        mStartTime.mHostID = 0;
        for (s32 liClient = 0; liClient < KI_START_TIME_CLIENT_COUNT; ++liClient)
        {
            maReceivedClientsIDs[liClient] = 0;
        }
        return ReliableMessage::GetPackedMessageSize();
    }

    // The reliable id, the start time (as seconds plus fraction in one float), then the host
    // id and the eight client ids as ints over [-1, INT_MAX]; each status is OR-ed into the
    // result.
    PackOrUnpackResult StartTimeMessage::PackOrUnpack()
    {
        PackOrUnpackResult lxResult = ReliableMessage::PackOrUnpack();

        f32 lfStartTime = mStartTime.mStartTime.GetFloatVal();
        lxResult |= PackOrUnpackFloat(this, &lfStartTime,
                                      KF_MIN_START_TIME, KF_MAX_START_TIME, KF_MAX_START_TIME_ERROR);
        mStartTime.mStartTime.SetFloatVal(lfStartTime);

        lxResult |= PackOrUnpackInt(this, &mStartTime.mHostID,
                                    -1, 0x7FFFFFFF);
        for (s32 liClient = 0; liClient < KI_START_TIME_CLIENT_COUNT; ++liClient)
        {
            lxResult |= PackOrUnpackInt(this, &maReceivedClientsIDs[liClient],
                                        -1, 0x7FFFFFFF);
        }
        return lxResult;
    }
}
