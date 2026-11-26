#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
#include <float.h> // FLT_MAX: the native 0x7F7FFFFF float bound
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Network/Messages/BrnFburnChallengeSuccessMessage.cpp"
#include "GameSource/Network/Messages/BrnFburnChallengeSuccessMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <xtl.h>

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
    static const f32 KF_SUCCESS_SCORE_MAX        = FLT_MAX;   // FLT_MAX
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
            CgsNetwork::Message::PackOrUnpack(&miFramesSinceStart, 0, 0x7FFFFFFF) | lxBase;

        for (s32 liAction = 0; liAction < KI_NUM_ACTIONS; ++liAction)
        {
            const CgsNetwork::PackOrUnpackResult lxSuccess =
                CgsNetwork::Message::PackOrUnpack(&mabSuccessfulActions[liAction]) | lxResult;
            const CgsNetwork::PackOrUnpackResult lxAccum =
                CgsNetwork::Message::PackOrUnpack(&mabAccumulationThisFrame[liAction]) | lxSuccess;
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
        CGS_ASSERT_AT(lu16FrameCount != CgsNetwork::KU16_INVALID_FRAME,
            "lu16FrameCount != KU16_INVALID_FRAME",
            CGS_ARTIST_FILE, 158 /* ARTIST 0x8257CB70 */);

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
        {
            miFramesSinceStart = liFramesSinceStart;
            XMemCpy(mafActionScores, lpaActionScores, sizeof(mafActionScores));
            XMemCpy(mabSuccessfulActions, lpaSuccessfulActions, sizeof(mabSuccessfulActions));
            XMemCpy(mabAccumulationThisFrame, lpaAccumulationsThisFrame, sizeof(mabAccumulationThisFrame));
            CgsNetwork::ReliableMessage::PrepareForSend(KI_FBURN_CHALLENGE_SUCCESS_MESSAGE_TYPE,
                                                        lu16FrameCount);
        }
    }

    bool FburnChallengeSuccessMessage::Retrieve(s32* lpiFramesSinceStart,
                                                bool* lpaSuccessfulActions,
                                                bool* lpaAccumulationsThisFrame,
                                                f32* lpaActionScores)
    {
        CGS_ASSERT_AT(lpaSuccessfulActions,
            "lpaSuccessfulActions",
            CGS_ARTIST_FILE, 189 /* ARTIST 0x8257FBDC */);
        CGS_ASSERT_AT(lpaAccumulationsThisFrame,
            "lpaAccumulationsThisFrame",
            CGS_ARTIST_FILE, 190 /* ARTIST 0x8257FC00 */);
        CGS_ASSERT_AT(lpaActionScores,
            "lpaActionScores",
            CGS_ARTIST_FILE, 191 /* ARTIST 0x8257FC24 */);
        CGS_ASSERT_AT(lpiFramesSinceStart,
            "lpiFramesSinceStart",
            CGS_ARTIST_FILE, 192 /* ARTIST 0x8257FC48 */);

        // ARTIST 0x8257FC5C branches to a trailing `return false` (0x8257FCB0): the copy-out is the body
        // of the valid test.
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
        {
            *lpiFramesSinceStart = miFramesSinceStart;
            XMemCpy(lpaActionScores, mafActionScores, sizeof(mafActionScores));
            XMemCpy(lpaSuccessfulActions, mabSuccessfulActions, sizeof(mabSuccessfulActions));
            XMemCpy(lpaAccumulationsThisFrame, mabAccumulationThisFrame, sizeof(mabAccumulationThisFrame));
            mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
            return true;
        }
        return false;
    }

    // An older message is still accepted (slot 1 is the shared `return true` leaf).
    bool FburnChallengeSuccessMessage::OldMessagesAreValid() const
    {
        return true;
    }
} // namespace BrnNetwork

#undef CGS_ARTIST_FILE
