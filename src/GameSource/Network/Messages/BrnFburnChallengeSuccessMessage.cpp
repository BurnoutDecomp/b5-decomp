#include "types.hpp"

#include "GameSource/Network/Messages/BrnFburnChallengeSuccessMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>   // memcpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::FburnChallengeSuccessMessage::Construct             @ 0x8257C940
//   BrnNetwork::FburnChallengeSuccessMessage::Destruct              @ 0x8257C9B8
//   BrnNetwork::FburnChallengeSuccessMessage::GetName              @ 0x8257CB18
//   BrnNetwork::FburnChallengeSuccessMessage::GetPackedMessageSize  @ 0x8257C9F8
//   BrnNetwork::FburnChallengeSuccessMessage::PackOrUnpack          @ 0x8257CA38
//   BrnNetwork::FburnChallengeSuccessMessage::PrepareForSend        @ 0x8257CB28
//   BrnNetwork::FburnChallengeSuccessMessage::Retrieve              @ 0x8257FB98
//
// Construct sets the two action scores to -1.0, clears the two flag pairs and the
// frames-since-start counter; Destruct restores the same "empty" state, and
// GetPackedMessageSize re-zeroes the scores/flags before deferring to the base reliable
// size (its X360 tail-call resolves to a COMDAT-folded copy of the reliable size stub --
// semantically CgsNetwork::ReliableMessage::GetPackedMessageSize()).
//
// PackOrUnpack ORs together: the base reliable status, the int frames-since-start (range
// [0, 0x7FFFFFFF]), the two success flags, the two accumulation flags, and the two action
// scores (each a quantised float in [0, FLT_MAX] at 0.005 resolution), exactly as the
// X360 build chains them. Message type id 37 (0x25) is the reliable success wire type.

namespace BrnNetwork
{
    static const s32 KI_FBURN_CHALLENGE_SUCCESS_MESSAGE_TYPE = 37;
    static const s32 KI_NUM_ACTIONS = 2;

    // The X360 builds these float bounds inline (lis 0x7FFF / FLT_MAX, 0.005 resolution).
    static const f32 KF_SUCCESS_SCORE_MIN        = 0.0f;
    static const f32 KF_SUCCESS_SCORE_MAX        = 3.4028235e38f;   // FLT_MAX
    static const f32 KF_SUCCESS_SCORE_RESOLUTION = 0.0049999999f;

    void FburnChallengeSuccessMessage::Construct()
    {
        // Inlined MessageWithPlayerIDs base init (the two player ids -> -1).
        mSendingPlayerID = KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = KI_INVALID_PLAYER_ID;
        CgsNetwork::Message::Construct();

        miFramesSinceStart = 0;
        for (s32 liAction = 0; liAction < KI_NUM_ACTIONS; ++liAction)
        {
            mafActionScores[liAction]         = -1.0f;
            mabSuccessfulActions[liAction]     = false;
            mabAccumulationThisFrame[liAction] = false;
        }
    }

    void FburnChallengeSuccessMessage::Destruct()
    {
        for (s32 liAction = 0; liAction < KI_NUM_ACTIONS; ++liAction)
        {
            mafActionScores[liAction]         = -1.0f;
            mabSuccessfulActions[liAction]     = false;
            mabAccumulationThisFrame[liAction] = false;
        }
        miFramesSinceStart = 0;
    }

    const char* FburnChallengeSuccessMessage::GetName() const
    {
        return "Freeburn Challenge Success Message";
    }

    s32 FburnChallengeSuccessMessage::GetPackedMessageSize()
    {
        for (s32 liAction = 0; liAction < KI_NUM_ACTIONS; ++liAction)
        {
            mafActionScores[liAction]         = 0.0f;
            mabSuccessfulActions[liAction]     = false;
            mabAccumulationThisFrame[liAction] = false;
        }
        miFramesSinceStart = 0;
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult FburnChallengeSuccessMessage::PackOrUnpack()
    {
        const CgsNetwork::PackOrUnpackResult lxBase = CgsNetwork::ReliableMessage::PackOrUnpack();
        CgsNetwork::PackOrUnpackResult lxResult =
            CgsNetwork::PackOrUnpackInt(this, &miFramesSinceStart, 0, 0x7FFFFFFF) | lxBase;

        for (s32 liAction = 0; liAction < KI_NUM_ACTIONS; ++liAction)
        {
            const CgsNetwork::PackOrUnpackResult lxSuccess =
                CgsNetwork::PackOrUnpackBool(this, &mabSuccessfulActions[liAction]) | lxResult;
            const CgsNetwork::PackOrUnpackResult lxAccum =
                CgsNetwork::PackOrUnpackBool(this, &mabAccumulationThisFrame[liAction]) | lxSuccess;
            const CgsNetwork::PackOrUnpackResult lxScore =
                CgsNetwork::PackOrUnpackFloat(this, &mafActionScores[liAction],
                                              KF_SUCCESS_SCORE_MIN, KF_SUCCESS_SCORE_MAX,
                                              KF_SUCCESS_SCORE_RESOLUTION);
            lxResult = lxScore | lxAccum;
        }
        return lxResult;
    }

    void FburnChallengeSuccessMessage::PrepareForSend(u16 lu16FrameCount,
                                                      s32 liFramesSinceStart,
                                                      const bool* lpaSuccessfulActions,
                                                      const bool* lpaAccumulationsThisFrame,
                                                      const f32* lpaActionScores)
    {
        CGS_ASSERT(lu16FrameCount != CgsNetwork::KU16_INVALID_FRAME,
                   "lu16FrameCount != KU16_INVALID_FRAME");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
        {
            miFramesSinceStart = liFramesSinceStart;
            memcpy(mafActionScores, lpaActionScores, 8);            // 2 x f32
            memcpy(mabSuccessfulActions, lpaSuccessfulActions, 2);   // 2 x bool
            memcpy(mabAccumulationThisFrame, lpaAccumulationsThisFrame, 2);
            CgsNetwork::ReliableMessage::PrepareForSend(KI_FBURN_CHALLENGE_SUCCESS_MESSAGE_TYPE,
                                                        lu16FrameCount);
        }
    }

    bool FburnChallengeSuccessMessage::Retrieve(s32* lpiFramesSinceStart,
                                                bool* lpaSuccessfulActions,
                                                bool* lpaAccumulationsThisFrame,
                                                f32* lpaActionScores)
    {
        CGS_ASSERT(lpaSuccessfulActions, "lpaSuccessfulActions");
        CGS_ASSERT(lpaAccumulationsThisFrame, "lpaAccumulationsThisFrame");
        CGS_ASSERT(lpaActionScores, "lpaActionScores");
        CGS_ASSERT(lpiFramesSinceStart, "lpiFramesSinceStart");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        *lpiFramesSinceStart = miFramesSinceStart;
        memcpy(lpaActionScores, mafActionScores, 8);
        memcpy(lpaSuccessfulActions, mabSuccessfulActions, 2);
        memcpy(lpaAccumulationsThisFrame, mabAccumulationThisFrame, 2);
        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        return true;
    }
} // namespace BrnNetwork
