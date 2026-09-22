#ifndef BRN_NETWORK_PLAYER_H
#define BRN_NETWORK_PLAYER_H

#include <cstddef>                                                           // offsetof (_AssertLayout)

#include "types.hpp"
#include "rw/math/vpu/types.h"                                               // rw::math::vpu::Vector3 (world bounds)
#include "GameShared/GameClasses/Core/CgsID.h"                               // CgsID
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"         // CgsNetwork::NetworkPlayer base, construct params
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h" // CgsNetwork::ReliableMessage (callback arg)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                       // CompletedFburnChallenges
#include "GameSource/GameState/ModeManager/Scoring/BrnBurnoutSkillzData.h"   // BrnGameState::BurnoutSkillzData
#include "GameSource/BurnoutConstants.h"                                     // BrnGameState::EChallengeStatus
#include "GameSource/Network/BrnNetworkModuleIO.h"                           // BrnNetworkModuleIO::EChallengeEventType
#include "GameSource/Network/Messages/BrnUpdateMessage.h"
#include "GameSource/Network/Messages/BrnCarSelectMessage.h"
#include "GameSource/Network/Messages/BrnCollectableMessage.h"
#include "GameSource/Network/Messages/BrnBurningHomeRunSwitchRunnerMessage.h"
#include "GameSource/Network/Messages/BrnBurnoutSkillzMessage.h"
#include "GameSource/Network/Messages/BrnShowtimeUpdateMessage.h"
#include "GameSource/Network/Messages/BrnShowtimeSwitchMessage.h"
#include "GameSource/Network/Messages/BrnFreeburnChallengeMessage.h"
#include "GameSource/Network/Messages/BrnFburnChallengeStatusMessage.h"
#include "GameSource/Network/Messages/BrnActiveFburnChallengeMessage.h"
#include "GameSource/Network/Messages/BrnCheckpointTriggeredMessage.h"
#include "GameSource/Network/Messages/BrnStuntScoreUpdatedMessage.h"
#include "GameSource/Network/Messages/BrnStuntMultiplierMessage.h"

// ===========================================================================
// BrnNetwork::BrnNetworkPlayer -- the Burnout per-peer session object
//   Home: GameSource/Network/BrnNetworkPlayer.{h,cpp}
//
// Derives from CgsNetwork::NetworkPlayer (console span +0x000..+0xC48) and adds the
// buffered update-message ring, the paired send/recv message sub-objects for every
// Burnout reliable message type, the latency window and the per-player state flags.
// BrnNetworkManager embeds seven of these at +0x86AB0 with a console stride of 0x2240.
//
// LAYOUT (console byte offsets; every member is placed by name and pinned in a 32-bit
// build by _AssertLayout, inert on the 64-bit host where pointers widen):
//   +0x0C48  mCompletedChallengesToSend      (256 bytes; SendFburnChallengeStatusMessage memcpy)
//   +0x0D48  mCachedBurnoutSkillzData        (56 bytes; SendNewBurnoutSkillzData memcpy)
//   +0x0D80  mafPastLatencies[30]            (UpdateLatencyValue)
//   +0x0E00  maUpdateMessageBuffer[16]       (0xB0 stride; OnRoundLoadingStart walks it)
//   +0x1900  mUpdateMessageSend / +0x19B0 mUpdateMessageRecv          (message type 11)
//   +0x1A60  mCarSelectMessageSend / +0x1AA8 Recv                     (type 15)
//   +0x1AF0  mCollectableMessageSend / +0x1B28 Recv                   (type 29)
//   +0x1B60  mBurningHomeRunSwitchRunnerMessageSend / +0x1B8C Recv    (type 30)
//   +0x1BB8  mBurnoutSkillzMessageSend / +0x1C1C Recv                 (type 31)
//   +0x1C80  mShowtimeUpdateMessageSend / +0x1CA4 Recv                (type 32)
//   +0x1CC8  mShowtimeSwitchMessageSend / +0x1CF8 Recv                (type 33)
//   +0x1D28  mFreeburnChallengeMessageSend / +0x1D68 Recv             (type 34)
//   +0x1DA8  mFburnChallengeStatusMessageSend / +0x1ED0 Recv          (type 35)
//   +0x1FF8  mActiveFburnChallengeMessageSend / +0x2050 Recv          (type 38)
//   +0x20A8  mCheckpointTriggeredMessageSend / +0x20D4 Recv           (type 39)
//   +0x2100  mStuntScoreUpdatedMessageSend / +0x212C Recv             (type 41)
//   +0x2158  mStuntMultiplierMessageSend / +0x218C Recv               (type 42)
//   +0x21C0  mWorldMinPos / +0x21D0 mWorldMaxPos                      (OnRoundStart)
//   +0x21E0  mSelectedChallengeID, +0x21E8 mCollectableID, then the scalar block to
//   +0x2224  mpTimeManager, +0x2228 mpNetworkModule, the twelve flags +0x222C..+0x2237.
//   sizeof == 0x2240 (16-byte alignment from the Vector3 / update-message members).
//
// The stunt-score and stunt-multiplier message pairs are specific to this build; the
// other members follow the class's declared member order.
//
// The vtable carries seven slots: the four NetworkPlayer overrides (Construct, Prepare,
// Release, Update) then OnRoundLoadingStart, OnRoundStart, SendDirtySockConnectionTelemetry.
// There is no virtual destructor. The constructor is compiler-generated: every store it
// makes is a member default-construction (the base timer and the message vtables).
//
// Every reliable message registered in Prepare uses one empty delivered callback; the
// eleven *DeliveredCallback functions below are those (identical, folded) empty bodies.
// ===========================================================================

namespace CgsNetwork
{
    struct TimeManager;         // pointer-only (mpTimeManager)
    struct SignalMessage;       // pointer-only (delivered-callback arg)
}

namespace CgsSystem
{
    class TimerStatus;          // pointer-only (Prepare / Update)
}

namespace BrnNetwork
{
    class BrnNetworkModule;     // pointer-only (mpNetworkModule)

    namespace BrnNetworkModuleIO
    {
        struct NetworkInActiveFburnChallengeEvent;   // pointer-only (SendActiveFburnChallengeMessage)
    }

    // Construct() parameters: the base params plus the two dependencies Construct latches
    // (read at params +0x4 / +0x8).
    struct BrnNetworkPlayerConstructParams : public CgsNetwork::CgsNetworkPlayerConstructParams
    {
        CgsNetwork::TimeManager* mpTimeManager;       // +0x4
        BrnNetworkModule*        mpNetworkModule;     // +0x8
    };

    const s32 KI_LATENCY_FRAMES_TO_AVERAGE_OVER = 30;
    const s32 KI_NUM_UPDATE_MESSAGES_TO_BUFFER  = 16;
    const s32 KI_MAX_STUNTS_IN_ONE_ROUND        = 10000;

    class BrnNetworkPlayer : public CgsNetwork::NetworkPlayer
    {
    public:
        // ---- NetworkPlayer overrides (vtable slots 0..3) ------------------------------
        virtual void Construct(CgsNetwork::CgsNetworkPlayerConstructParams* lpConstructParams);
        virtual bool Prepare(CgsNetwork::NetworkAdapter* lpNetworkAdapter,
                             const CgsSystem::TimerStatus* lpTimerStatus,
                             const char* lpcName,
                             CgsSystem::EFrameRate leLocalConsoleFrameRate,
                             CgsSystem::EFrameRate leRemoteConsoleFrameRate,
                             CgsNetwork::NetworkPlayerID liPlayerID);
        virtual bool Release();
        virtual void Update(const CgsSystem::TimerStatus* lpTimerStatus,
                            u16 lu16CurrentFrame, bool lbInGame);

        // ---- own virtuals (vtable slots 4..6) -----------------------------------------
        virtual void OnRoundLoadingStart();
        virtual void OnRoundStart();
        virtual void SendDirtySockConnectionTelemetry(u32 luConnectionStatus, u32 luConnectionFlags);

        // ---- reliable-message senders --------------------------------------------------
        void SendSelectedCar(CgsID lCarID, CgsID lWheelID, u16 lu16CarColourIndex,
                             u16 lu16PaintFinishIndex, bool lbFinalCarSelection);
        void SendLandmarkTriggeredMessage(s32 liLandmarkIndex);
        void SendCollectableDone(CgsID lCollectableID,
                                 CollectableMessage::ECollectableType leCollectableType);
        void SendSwitchBurningHomeRunRunnerMessage(CgsNetwork::NetworkPlayerID lNewBurningHomeRunRunner);
        void SendNewBurnoutSkillzData(const BrnGameState::BurnoutSkillzData* lpBurnoutSkillzData,
                                      bool lbIsInitial);
        void SendShowtimeUpdateData(s32 liShowtimeScore);
        void SendShowtimeModeSwitchMessage(s32 liShowtimeValue, bool lbSwitchOn);
        void SendFreeburnChallengeMessage(CgsID lChallengeID,
                                          BrnNetworkModuleIO::EChallengeEventType leEventType,
                                          BrnGameState::EChallengeStatus leChallengeStatus,
                                          s32 liValue);
        void SendFburnChallengeStatusMessage(
                const BrnGameState::GameStateModuleIO::CompletedFburnChallenges* lpCompletedChallenges);
        void SendActiveFburnChallengeMessage(
                const BrnNetworkModuleIO::NetworkInActiveFburnChallengeEvent* lpActiveFreeburnChallengeEvent,
                CgsNetwork::NetworkPlayerID* lpPlayerIDs);

        // Stamp the stunt-score send slot for the current network frame.
        void SendStuntScoreUpdatedMessage(s32 liStuntScore);

        // Stamp the stunt-multiplier send slot for the current network frame, recording the
        // frames-since-start the multiplier was sampled on.
        void SendStuntMultiplierMessage(s32 liStuntMultiplier);

        // A remote multiplier arrived: convert it from the sender's console frame rate to
        // ours and queue a network event (type 34, 16-byte payload) for the game side.
        void ProcessReceivedStuntMultiplier(CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                            s32 liStuntMultiplier, s64 li64MultiplierData);

        // ---- header-inline state accessors ---------------------------------------------
        // The standings manager stores the eliminated flag inline (+0x2232); the update
        // sender reads it.
        void SetEliminated(bool lbEliminated) { mbIsEliminated = lbEliminated; }
        bool IsEliminated() const             { return mbIsEliminated; }

        // ---- legacy caller-facing declarations (no console function of this class) -----
        // Kept only so the committed callers keep compiling; see the lane report.
        //   IsNatable     -- the caller reads the base's paused flag at +0xBA9.
        //   GetPlayerName -- the caller wants the base name at +0xB98 (NetworkPlayer::GetName).
        //   SetHasWonRound -- the caller's store is the inline SetEliminated(true).
        bool        IsNatable() const;
        const char* GetPlayerName() const;
        void        SetHasWonRound(bool lbHasWonRound);

        // ---- reliable-message arrived callbacks (registered by Prepare with this player
        //      as the user data) -------------------------------------------------------
        static void _CarSelectMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                     CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                     void* lpUserData);
        static void _CollectableMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                       CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                       void* lpUserData);
        static void _BurningHomeRunSwitchRunnerMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                      CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                      void* lpUserData);
        static void _BurnoutSkillzMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                         CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                         void* lpUserData);
        static void _ShowtimeSwitchMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                          CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                          void* lpUserData);
        static void _FreeburnChallengeMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                             CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                             void* lpUserData);
        static void _FburnChallengeStatusMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                void* lpUserData);
        static void _ActiveFreeburnChallengeMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                   CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                   void* lpUserData);
        static void _CheckpointTriggeredMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                               CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                               void* lpUserData);
        static void _StuntScoreUpdatedMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                             CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                             void* lpUserData);
        static void _StuntMultiplierMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                           CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                           void* lpUserData);

    private:
        // ---- per-frame helpers ---------------------------------------------------------
        void UpdateLatencyValue(u16 lu16SentFrame, u16 lu16CurrentFrame);
        bool IsOurTurnToSend();
        void CheckForNewMessages(u16 lu16CurrentFrame);
        void CheckForPlayerPausedTimeout(u16 lu16CurrentFrame);
        UpdateMessage* RetrieveBufferedMessage(u16 lu16Frame);
        void RemoveBufferedMessage(UpdateMessage* lpUpdateMessage);
        void TellNetworkCarAboutRecievedUpdateMessage(u16 lu16CurrentFrame, UpdateMessage* lpUpdateMessage);
        void TellNetworkCarAboutBufferedUpdateMessage(u16 lu16CurrentFrame, UpdateMessage* lpUpdateMessage);
        void HandleSendingUpdateMessage();
        void HandleSendingFburnChallengeMessages(f32 lfTimeStep);

        // ---- reliable-message delivered callbacks (all one empty body) -----------------
        static void _CarSelectMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                       CgsNetwork::SignalMessage* lpMessage,
                                                       CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                       void* lpUserData);
        static void _CollectableMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                         CgsNetwork::SignalMessage* lpMessage,
                                                         CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                         void* lpUserData);
        static void _BurningHomeRunSwitchRunnerMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                                        CgsNetwork::SignalMessage* lpMessage,
                                                                        CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                        void* lpUserData);
        static void _BurnoutSkillzMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                           CgsNetwork::SignalMessage* lpMessage,
                                                           CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                           void* lpUserData);
        static void _ShowtimeSwitchMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                            CgsNetwork::SignalMessage* lpMessage,
                                                            CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                            void* lpUserData);
        static void _FreeburnChallengeMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                               CgsNetwork::SignalMessage* lpMessage,
                                                               CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                               void* lpUserData);
        static void _FburnChallengeStatusMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                                  CgsNetwork::SignalMessage* lpMessage,
                                                                  CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                  void* lpUserData);
        static void _ActiveFreeburnChallengeMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                                     CgsNetwork::SignalMessage* lpMessage,
                                                                     CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                     void* lpUserData);
        static void _CheckpointTriggeredMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                                 CgsNetwork::SignalMessage* lpMessage,
                                                                 CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                 void* lpUserData);
        // The two stunt message types register the same empty body; their names follow the
        // class's callback naming (FLAG: not independently attested).
        static void _StuntScoreUpdatedMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                               CgsNetwork::SignalMessage* lpMessage,
                                                               CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                               void* lpUserData);
        static void _StuntMultiplierMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                             CgsNetwork::SignalMessage* lpMessage,
                                                             CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                             void* lpUserData);

        // Console offsets pinned in a 32-bit build; inert on the host.
        static void _AssertLayout();

        // ---- layout --------------------------------------------------------------------
        BrnGameState::GameStateModuleIO::CompletedFburnChallenges mCompletedChallengesToSend; // +0x0C48
        BrnGameState::BurnoutSkillzData mCachedBurnoutSkillzData;                      // +0x0D48
        f32                             mafPastLatencies[KI_LATENCY_FRAMES_TO_AVERAGE_OVER]; // +0x0D80

        UpdateMessage                   maUpdateMessageBuffer[KI_NUM_UPDATE_MESSAGES_TO_BUFFER]; // +0x0E00
        UpdateMessage                   mUpdateMessageSend;                            // +0x1900
        UpdateMessage                   mUpdateMessageRecv;                            // +0x19B0
        CarSelectMessage                mCarSelectMessageSend;                         // +0x1A60
        CarSelectMessage                mCarSelectMessageRecv;                         // +0x1AA8
        CollectableMessage              mCollectableMessageSend;                       // +0x1AF0
        CollectableMessage              mCollectableMessageRecv;                       // +0x1B28
        BurningHomeRunSwitchRunnerMessage mBurningHomeRunSwitchRunnerMessageSend;      // +0x1B60
        BurningHomeRunSwitchRunnerMessage mBurningHomeRunSwitchRunnerMessageRecv;      // +0x1B8C
        BurnoutSkillzMessage            mBurnoutSkillzMessageSend;                     // +0x1BB8
        BurnoutSkillzMessage            mBurnoutSkillzMessageRecv;                     // +0x1C1C
        ShowtimeUpdateMessage           mShowtimeUpdateMessageSend;                    // +0x1C80
        ShowtimeUpdateMessage           mShowtimeUpdateMessageRecv;                    // +0x1CA4
        ShowtimeSwitchMessage           mShowtimeSwitchMessageSend;                    // +0x1CC8
        ShowtimeSwitchMessage           mShowtimeSwitchMessageRecv;                    // +0x1CF8
        FreeburnChallengeMessage        mFreeburnChallengeMessageSend;                 // +0x1D28
        FreeburnChallengeMessage        mFreeburnChallengeMessageRecv;                 // +0x1D68
        FburnChallengeStatusMessage     mFburnChallengeStatusMessageSend;              // +0x1DA8
        FburnChallengeStatusMessage     mFburnChallengeStatusMessageRecv;              // +0x1ED0
        ActiveFburnChallengeMessage     mActiveFburnChallengeMessageSend;              // +0x1FF8
        ActiveFburnChallengeMessage     mActiveFburnChallengeMessageRecv;              // +0x2050
        CheckpointTriggeredMessage      mCheckpointTriggeredMessageSend;               // +0x20A8
        CheckpointTriggeredMessage      mCheckpointTriggeredMessageRecv;               // +0x20D4
        StuntScoreUpdatedMessage        mStuntScoreUpdatedMessageSend;                 // +0x2100
        StuntScoreUpdatedMessage        mStuntScoreUpdatedMessageRecv;                 // +0x212C
        StuntMultiplierMessage          mStuntMultiplierMessageSend;                   // +0x2158
        StuntMultiplierMessage          mStuntMultiplierMessageRecv;                   // +0x218C

        rw::math::vpu::Vector3          mWorldMinPos;                                  // +0x21C0
        rw::math::vpu::Vector3          mWorldMaxPos;                                  // +0x21D0
        CgsID                           mSelectedChallengeID;                          // +0x21E0
        CgsID                           mCollectableID;                                // +0x21E8
        CgsNetwork::NetworkPlayerID     mLocalPlayerRaceCarCrasherID;                  // +0x21F0
        f32                             mfTimeBeforeSelectedChallengeSend;             // +0x21F4
        s32                             miPastLatenciesCount;                          // +0x21F8
        s32                             miLatencyIndex;                                // +0x21FC
        s32                             miLatencyFrames;                               // +0x2200
        s32                             miCurrentRoundNumber;                          // +0x2204
        s32                             miFlashFrequency;                              // +0x2208
        s32                             miNumUpdateMessagesBuffered;                   // +0x220C
        s32                             miUnsentMessageCount;                          // +0x2210
        CollectableMessage::ECollectableType meCollectableType;                        // +0x2214
        u16                             mu16LastPacketReceivedFrame;                   // +0x2218
        u16                             mu16LastBufferedUpdateAppliedFrame;            // +0x221A
        u16                             mu16LastAnyUpdateAppliedFrame;                 // +0x221C
        u16                             mu16LastFrameWeSentUpdateMessageOn;            // +0x221E
        u8                              mu8NumForcedUpdates;                           // +0x2220
        CgsNetwork::TimeManager*        mpTimeManager;                                 // +0x2224
        BrnNetworkModule*               mpNetworkModule;                               // +0x2228
        bool                            mbHasResetCarTransformSinceLastUpdateSent;     // +0x222C
        bool                            mbHasBeenSlammedSinceLastUpdateSent;           // +0x222D
        bool                            mbIsPrepared;                                  // +0x222E
        bool                            mbIsInRound;                                   // +0x222F
        bool                            mbIsPaused;                                    // +0x2230
        bool                            mbIsUpdateMessageSendingEnabled;               // +0x2231
        bool                            mbIsEliminated;                                // +0x2232
        bool                            mbWorldSet;                                    // +0x2233
        bool                            mbFirstUpdateMessage;                          // +0x2234
        bool                            mbBurnoutSkillzToSend;                         // +0x2235
        bool                            mbCachedBurnoutSkillzAreIntial;                // +0x2236
        bool                            mbIsOnStartLine;                               // +0x2237
    };

    inline void BrnNetworkPlayer::_AssertLayout()
    {
#define BRN_NP_AT(member, off) \
        static_assert(sizeof(void*) != 4 || offsetof(BrnNetworkPlayer, member) == off, #member " @ " #off)
        BRN_NP_AT(mCompletedChallengesToSend,              0x0C48);
        BRN_NP_AT(mCachedBurnoutSkillzData,                0x0D48);
        BRN_NP_AT(mafPastLatencies,                        0x0D80);
        BRN_NP_AT(maUpdateMessageBuffer,                   0x0E00);
        BRN_NP_AT(mUpdateMessageSend,                      0x1900);
        BRN_NP_AT(mCarSelectMessageSend,                   0x1A60);
        BRN_NP_AT(mCollectableMessageSend,                 0x1AF0);
        BRN_NP_AT(mBurningHomeRunSwitchRunnerMessageSend,  0x1B60);
        BRN_NP_AT(mBurnoutSkillzMessageSend,               0x1BB8);
        BRN_NP_AT(mShowtimeUpdateMessageSend,              0x1C80);
        BRN_NP_AT(mShowtimeSwitchMessageSend,              0x1CC8);
        BRN_NP_AT(mFreeburnChallengeMessageSend,           0x1D28);
        BRN_NP_AT(mFburnChallengeStatusMessageSend,        0x1DA8);
        BRN_NP_AT(mActiveFburnChallengeMessageSend,        0x1FF8);
        BRN_NP_AT(mCheckpointTriggeredMessageSend,         0x20A8);
        BRN_NP_AT(mStuntScoreUpdatedMessageSend,           0x2100);
        BRN_NP_AT(mStuntMultiplierMessageSend,             0x2158);
        BRN_NP_AT(mStuntMultiplierMessageRecv,             0x218C);
        BRN_NP_AT(mWorldMinPos,                            0x21C0);
        BRN_NP_AT(mSelectedChallengeID,                    0x21E0);
        BRN_NP_AT(mLocalPlayerRaceCarCrasherID,            0x21F0);
        BRN_NP_AT(miPastLatenciesCount,                    0x21F8);
        BRN_NP_AT(miNumUpdateMessagesBuffered,             0x220C);
        BRN_NP_AT(meCollectableType,                       0x2214);
        BRN_NP_AT(mu16LastPacketReceivedFrame,             0x2218);
        BRN_NP_AT(mu8NumForcedUpdates,                     0x2220);
        BRN_NP_AT(mpTimeManager,                           0x2224);
        BRN_NP_AT(mpNetworkModule,                         0x2228);
        BRN_NP_AT(mbHasResetCarTransformSinceLastUpdateSent, 0x222C);
        BRN_NP_AT(mbIsEliminated,                          0x2232);
        BRN_NP_AT(mbIsOnStartLine,                         0x2237);
        static_assert(sizeof(void*) != 4 || sizeof(BrnNetworkPlayer) == 0x2240, "sizeof(BrnNetworkPlayer) == 0x2240");
#undef BRN_NP_AT
    }
}

#endif // BRN_NETWORK_PLAYER_H
