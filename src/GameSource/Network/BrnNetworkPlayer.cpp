#include "types.hpp"

#include "GameSource/Network/BrnNetworkPlayer.h"
#include "GameSource/Network/BrnNetworkModule.h"          // BrnNetworkModule::GetNetworkManager / GetNetworkEventQueue
#include "GameSource/Network/BrnNetworkManager.h"         // BrnNetworkManager::GetLocalConsoleFrameRate / GetRound
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"  // NetworkInStuntMultiplierEvent / NetworkInActiveFburnChallengeEvent
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h" // the NetworkOut* records the arrived callbacks queue
#include "GameSource/Network/BrnNetworkPlayerMenuData.h"   // BrnNetwork::PlayerMenuData (car-select arrival)
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // NetworkToGameStateInterface
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h" // PlayerManager::GetPlayerByID / GetMenuDataByID
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"      // TimeManager frame queries
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // VariableEventQueue<14000,16>::AddEvent
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // TimerStatus::GetCurrentTimeStep
#include "GameSource/Network/BrnServerInterface.h"                 // BrnServerInterface::GetGameComponent
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h" // PlayerParams (remote console frame rate)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h" // GetPlayerParametersByPlayerID
#include "GameShared/GameClasses/Development/CgsStrStream.h"        // CgsDev::StrStream (streamed asserts)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"        // BrnNetworkDriverControls
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"  // VehicleDriverInputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"                // RaceCarState
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // active race cars
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h" // PlayerVehicleControls
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/PC/BrnNetHarnessPC.h"   // [net] witness lines (PC harness)

#include <cmath>   // fmaf

// ===================================================================================
// BrnNetwork::BrnNetworkPlayer -- the Burnout per-peer session object.
//
// Bodied here: the lifecycle virtuals (Construct, Release, Update, OnRoundLoadingStart,
// OnRoundStart, SendDirtySockConnectionTelemetry), the reliable-message senders, the
// update-message buffer (RetrieveBufferedMessage / RemoveBufferedMessage), the pause and
// turn-to-send helpers, every reliable-message arrived callback and the shared empty
// delivered callback.
//
// Also bodied: Prepare (registers every Burnout message pair), the freeburn-challenge send
// helpers, the latency estimate, CheckForNewMessages and the update message's send and
// receive sides (HandleSendingUpdateMessage, TellNetworkCarAboutRecievedUpdateMessage).
//
// Every reliable message is stamped with the running network frame folded into the 16-bit
// wire space (TimeManager::GetU16FrameCount, which the console inlines at each sender).
// ===================================================================================

namespace BrnNetwork
{
    namespace
    {
        // PAL/NTSC stunt rates differ by the 50:60 frame-rate ratio; a multiplier sampled on a
        // remote console at one rate is rescaled to the local rate when they disagree.
        const s32 KI_FRAMERATE_50HZ = CgsSystem::E_FRAMERATE_50HZ;   // 50
        const s32 KI_FRAMERATE_60HZ = CgsSystem::E_FRAMERATE_60HZ;   // 60

        // A player is treated as paused once nothing has arrived from it for this many frames.
        const u16 KU16_PAUSE_TIMEOUT_FRAMES = 300;

        // The paused-player indicator flashes on for 30 frames, off for 30.
        const s32 KI_FLASH_ON_FRAMES  = 30;
        const s32 KI_FLASH_OFF_FRAMES = 60;

        // A buffered update is not allowed to fall more than this many frames behind the most
        // recent update that was applied.
        const u16 KU16_MAX_BUFFERED_UPDATE_LAG = 60;

        // Frames subtracted on top of the measured latency before looking up a buffered update.
        const u16 KU16_UPDATE_BUFFER_EXTRA_FRAMES = 4;

        // Burnout message-type ids registered by Prepare (the network message-type enum).
        const s32 KI_MESSAGE_TYPE_UPDATE                        = 11;
        const s32 KI_MESSAGE_TYPE_CAR_SELECT                    = 15;
        const s32 KI_MESSAGE_TYPE_COLLECTABLE                   = 29;
        const s32 KI_MESSAGE_TYPE_BURNING_HOME_RUN_SWITCH_RUNNER = 30;
        const s32 KI_MESSAGE_TYPE_BURNOUT_SKILLZ                = 31;
        const s32 KI_MESSAGE_TYPE_SHOWTIME_UPDATE               = 32;
        const s32 KI_MESSAGE_TYPE_SHOWTIME_SWITCH               = 33;
        const s32 KI_MESSAGE_TYPE_FREEBURN_CHALLENGE            = 34;
        const s32 KI_MESSAGE_TYPE_FBURN_CHALLENGE_STATUS        = 35;
        const s32 KI_MESSAGE_TYPE_ACTIVE_FREEBURN_CHALLENGE     = 38;
        const s32 KI_MESSAGE_TYPE_CHECKPOINT_TRIGGERED          = 39;
        // FLAG: the two stunt ids are specific to this build; the names follow the others.
        const s32 KI_MESSAGE_TYPE_STUNT_SCORE_UPDATED           = 41;
        const s32 KI_MESSAGE_TYPE_STUNT_MULTIPLIER              = 42;

        // A received frame more than a third of the 16-bit frame space away from the
        // latency-affected frame means the latency estimate has gone wrong.
        const s16 KI16_MAX_LATENCY_AFFECTED_FRAME_DIFF = 0x5555;

        // Latency frames are rounded to the nearest frame.
        const f32 KF_ROUND_TO_NEAREST = 0.5f;

        // Frames per second at each console rate (the latency estimate is capped at one
        // second of frames).
        const f32 KF_FRAMES_PER_SECOND_50HZ = 50.0f;
        const f32 KF_FRAMES_PER_SECOND_60HZ = 60.0f;

        // mfTimeBeforeSelectedChallengeSend while no selected-challenge send is pending.
        const f32 KF_NO_SELECTED_CHALLENGE_SEND = -1.0f;

        // The round's world bounds before the game side reports the real ones.
        const f32 KF_DEFAULT_WORLD_HALF_EXTENT_XZ = 20000.0f;
        const f32 KF_DEFAULT_WORLD_HALF_EXTENT_Y  = 2000.0f;
    }

    // ---- lifecycle ---------------------------------------------------------------------

    // Construct the base player, latch the time manager and network module from the Burnout
    // construct params, reset every per-player field and construct the update, runner-switch,
    // skillz, showtime and freeburn-challenge message slots.
    void BrnNetworkPlayer::Construct(CgsNetwork::CgsNetworkPlayerConstructParams* lpConstructParams)
    {
        CgsNetwork::NetworkPlayer::Construct(lpConstructParams);

        const BrnNetworkPlayerConstructParams* lpParams =
            static_cast<const BrnNetworkPlayerConstructParams*>(lpConstructParams);
        mpTimeManager   = lpParams->mpTimeManager;
        mpNetworkModule = lpParams->mpNetworkModule;
        CGS_ASSERT(mpTimeManager, "mpTimeManager");
        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");

        mfTimeBeforeSelectedChallengeSend         = KF_NO_SELECTED_CHALLENGE_SEND;
        mSelectedChallengeID                      = 0;
        mbIsPrepared                              = false;
        mbIsInRound                               = false;
        mbIsPaused                                = false;
        mLocalPlayerRaceCarCrasherID              = CgsNetwork::K_INVALID_PLAYER_ID;
        mbIsUpdateMessageSendingEnabled           = true;
        mbWorldSet                                = false;
        mbFirstUpdateMessage                      = true;
        miNumUpdateMessagesBuffered               = 0;
        miLatencyIndex                            = 0;
        miLatencyFrames                           = 0;
        mu16LastPacketReceivedFrame               = CgsNetwork::KU16_INVALID_FRAME;
        mu16LastBufferedUpdateAppliedFrame        = CgsNetwork::KU16_INVALID_FRAME;
        mu16LastAnyUpdateAppliedFrame             = CgsNetwork::KU16_INVALID_FRAME;
        miPastLatenciesCount                      = 0;
        mu8NumForcedUpdates                       = 0;
        mbHasResetCarTransformSinceLastUpdateSent = false;
        mbHasBeenSlammedSinceLastUpdateSent       = false;
        mbBurnoutSkillzToSend                     = false;
        mbCachedBurnoutSkillzAreIntial            = false;
        mbIsOnStartLine                           = false;
        meCollectableType                         = CollectableMessage::E_COLLECTABLE_TYPE_MAX;
        mCollectableID                            = 0;
        mCachedBurnoutSkillzData.Clear();

        for (u32 luField = 0; luField < BrnGameState::GameStateModuleIO::CompletedFburnChallenges::KU_NUM_BIT_WORDS; ++luField)
        {
            mCompletedChallengesToSend.maxBits[luField] = 0;
        }

        mUpdateMessageSend.Construct();
        mUpdateMessageRecv.Construct();
        mBurningHomeRunSwitchRunnerMessageSend.Construct();
        mBurningHomeRunSwitchRunnerMessageRecv.Construct();
        mBurnoutSkillzMessageSend.Construct();
        mBurnoutSkillzMessageRecv.Construct();
        mShowtimeUpdateMessageSend.Construct();
        mShowtimeUpdateMessageRecv.Construct();
        mShowtimeSwitchMessageSend.Construct();
        mShowtimeSwitchMessageRecv.Construct();
        mFreeburnChallengeMessageSend.Construct();
        mFreeburnChallengeMessageRecv.Construct();
        mFburnChallengeStatusMessageSend.Construct();
        mFburnChallengeStatusMessageRecv.Construct();
        mActiveFburnChallengeMessageSend.Construct();
        mActiveFburnChallengeMessageRecv.Construct();
    }

    // Drop the prepared state, the pending skillz / collectable sends and the queued
    // challenge-completion bits, then release the base player.
    bool BrnNetworkPlayer::Release()
    {
        mbIsPrepared                              = false;
        mbFirstUpdateMessage                      = true;
        mbHasResetCarTransformSinceLastUpdateSent = false;
        mbHasBeenSlammedSinceLastUpdateSent       = false;
        mbBurnoutSkillzToSend                     = false;
        mbCachedBurnoutSkillzAreIntial            = false;
        meCollectableType                         = CollectableMessage::E_COLLECTABLE_TYPE_MAX;
        mCollectableID                            = 0;
        mCachedBurnoutSkillzData.Clear();

        for (u32 luField = 0; luField < BrnGameState::GameStateModuleIO::CompletedFburnChallenges::KU_NUM_BIT_WORDS; ++luField)
        {
            mCompletedChallengesToSend.maxBits[luField] = 0;
        }

        return CgsNetwork::NetworkPlayer::Release();
    }

    // Reset the update-message slots and the buffered-update bookkeeping for the round that
    // is about to load.
    void BrnNetworkPlayer::OnRoundLoadingStart()
    {
        mUpdateMessageSend.Construct();
        mUpdateMessageRecv.Construct();

        for (s32 liIndex = 0; liIndex < KI_NUM_UPDATE_MESSAGES_TO_BUFFER; ++liIndex)
        {
            maUpdateMessageBuffer[liIndex].Construct();
        }

        miNumUpdateMessagesBuffered        = 0;
        mu16LastPacketReceivedFrame        = CgsNetwork::KU16_INVALID_FRAME;
        mu16LastBufferedUpdateAppliedFrame = CgsNetwork::KU16_INVALID_FRAME;
        mu16LastAnyUpdateAppliedFrame      = CgsNetwork::KU16_INVALID_FRAME;
    }

    // The round has started: default world bounds, in round and unpaused, and fresh update /
    // runner-switch message slots.
    void BrnNetworkPlayer::OnRoundStart()
    {
        mWorldMinPos = rw::math::vpu::Vector3{ -KF_DEFAULT_WORLD_HALF_EXTENT_XZ, -KF_DEFAULT_WORLD_HALF_EXTENT_Y,
                                               -KF_DEFAULT_WORLD_HALF_EXTENT_XZ, 0.0f };
        mWorldMaxPos = rw::math::vpu::Vector3{ KF_DEFAULT_WORLD_HALF_EXTENT_XZ, KF_DEFAULT_WORLD_HALF_EXTENT_Y,
                                               KF_DEFAULT_WORLD_HALF_EXTENT_XZ, 0.0f };

        mbIsPaused           = false;
        mu8NumForcedUpdates  = 0;
        mbIsEliminated       = false;
        mbIsInRound          = true;
        mbWorldSet           = true;
        mbFirstUpdateMessage = true;

        mUpdateMessageSend.Construct();
        mUpdateMessageRecv.Construct();
        mBurningHomeRunSwitchRunnerMessageSend.Construct();
        mBurningHomeRunSwitchRunnerMessageRecv.Construct();
    }

    // The per-frame pump: the base update, the buffered-update catch-up, the pause check,
    // then (once the start frame is known) receive, apply the buffered update due this
    // frame, and send the update / challenge / skillz / collectable messages.
    void BrnNetworkPlayer::Update(const CgsSystem::TimerStatus* lpTimerStatus,
                                  u16 lu16CurrentFrame, bool /*lbInGame*/)
    {
        if (!mbIsPrepared)
        {
            return;
        }

        // The base pump is driven by this player's own in-round flag.
        CgsNetwork::NetworkPlayer::Update(lpTimerStatus, lu16CurrentFrame, mbIsInRound);

        if (mu16LastBufferedUpdateAppliedFrame != CgsNetwork::KU16_INVALID_FRAME)
        {
            CGS_ASSERT(mu16LastAnyUpdateAppliedFrame != CgsNetwork::KU16_INVALID_FRAME,
                       "mu16LastAnyUpdateAppliedFrame != KU16_INVALID_FRAME");

            u16 lu16OldestAllowedFrame =
                static_cast<u16>(mu16LastBufferedUpdateAppliedFrame + KU16_MAX_BUFFERED_UPDATE_LAG);
            if (lu16OldestAllowedFrame == CgsNetwork::KU16_INVALID_FRAME)
            {
                lu16OldestAllowedFrame = 0;
            }

            if (CgsNetwork::UInt16IsLargerWrapped(mu16LastAnyUpdateAppliedFrame, lu16OldestAllowedFrame))
            {
                mu16LastBufferedUpdateAppliedFrame =
                    static_cast<u16>(mu16LastAnyUpdateAppliedFrame - KU16_MAX_BUFFERED_UPDATE_LAG);
                if (mu16LastBufferedUpdateAppliedFrame == CgsNetwork::KU16_INVALID_FRAME)
                {
                    mu16LastBufferedUpdateAppliedFrame = 0;
                }
            }
        }

        CheckForPlayerPausedTimeout(lu16CurrentFrame);

        if (mpTimeManager == nullptr || !mpTimeManager->HasStartFrameBeenSetValid())
        {
            return;
        }

        const u16 lu16FramesSinceStart = mpTimeManager->GetU16FrameCountSinceStart();

        u16 lu16BufferedFrame = static_cast<u16>(lu16FramesSinceStart - miLatencyFrames
                                                 - KU16_UPDATE_BUFFER_EXTRA_FRAMES);
        if (lu16BufferedFrame == CgsNetwork::KU16_INVALID_FRAME)
        {
            lu16BufferedFrame = 0;
        }

        CheckForNewMessages(lu16FramesSinceStart);

        UpdateMessage* lpBufferedMessage = RetrieveBufferedMessage(lu16BufferedFrame);
        if (lpBufferedMessage != nullptr)
        {
            // Only apply an update sent from the same lobby / round as ours.
            const s32 liRoundNumber = mpNetworkModule->GetNetworkManager()->GetRound();
            if (lpBufferedMessage->GetUpdateData()->mbIsFreeBurnLobby == (liRoundNumber == -1))
            {
                const s32 liCurrentRound = mpNetworkModule->GetNetworkManager()->GetRound();
                if (static_cast<s32>(lpBufferedMessage->GetUpdateData()->mbIsRoundNumberOdd) == (liCurrentRound & 1))
                {
                    if (mu16LastBufferedUpdateAppliedFrame == CgsNetwork::KU16_INVALID_FRAME
                        || CgsNetwork::UInt16IsLargerWrapped(lpBufferedMessage->mu16Frame,
                                                             mu16LastBufferedUpdateAppliedFrame))
                    {
                        TellNetworkCarAboutBufferedUpdateMessage(lu16FramesSinceStart, lpBufferedMessage);
                    }
                }
            }
            RemoveBufferedMessage(lpBufferedMessage);
        }

        HandleSendingUpdateMessage();
        HandleSendingFburnChallengeMessages(lpTimerStatus->GetCurrentTimeStep());

        if (mbBurnoutSkillzToSend && IsOurTurnToSend())
        {
            mBurnoutSkillzMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount(),
                                                     &mCachedBurnoutSkillzData,
                                                     mbCachedBurnoutSkillzAreIntial);
            mbBurnoutSkillzToSend = false;
        }

        if (mCollectableID != 0 && IsOurTurnToSend())
        {
            mCollectableMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount(), mCollectableID,
                                                   meCollectableType);
            mCollectableID    = 0;
            meCollectableType = CollectableMessage::E_COLLECTABLE_TYPE_MAX;
        }
    }

    // Report this player's DirtySock connection state to telemetry as a one-character code:
    // the flags shifted up three bits over the status, on the 0x40 base.
    void BrnNetworkPlayer::SendDirtySockConnectionTelemetry(u32 luConnectionStatus, u32 luConnectionFlags)
    {
        const u32 KU_CONNECTION_TELEMETRY_BASE = 0x40;

        // FLAG: the console fills only the first character of its stack buffer; the second
        // character terminates the string on the host.
        char lacConnectionData[2];
        lacConnectionData[0] = static_cast<char>((luConnectionFlags << 3) | KU_CONNECTION_TELEMETRY_BASE
                                                 | luConnectionStatus);
        lacConnectionData[1] = '\0';

        mpNetworkModule->GetNetworkManager()->CaptureTelemetryEvent(E_TELEMETRY_DIRTYSOCK_CONNECTION,
                                                                    lacConnectionData);
    }

    // ---- per-frame helpers -------------------------------------------------------------

    // Unreliable traffic goes round-robin between the players; a paused player never sends,
    // and a queue of forced updates jumps the round-robin.
    bool BrnNetworkPlayer::IsOurTurnToSend()
    {
        if (mbNetworkPlayerPaused)
        {
            return false;
        }

        if (mu8NumForcedUpdates != 0)
        {
            --mu8NumForcedUpdates;
            return true;
        }

        return GetPlayerManager()->IsPlayerTurnToSendRoundRobinMessage(GetPlayerID(), true, 0);
    }

    // Pause the player when nothing has arrived from it for the timeout, and drive the
    // paused indicator's flash counter. The console also logs each pause / unpause edge to
    // its debug stream (not reproduced).
    void BrnNetworkPlayer::CheckForPlayerPausedTimeout(u16 lu16CurrentFrame)
    {
        const bool lbWasPaused = mbIsPaused;

        if (mu16LastPacketReceivedFrame != CgsNetwork::KU16_INVALID_FRAME)
        {
            mbIsPaused = CgsNetwork::GetFrameDiffWrapped16(lu16CurrentFrame, mu16LastPacketReceivedFrame)
                         > KU16_PAUSE_TIMEOUT_FRAMES;
        }

        if (mbIsPaused)
        {
            ++miFlashFrequency;
            if (miFlashFrequency > KI_FLASH_ON_FRAMES)
            {
                if (miFlashFrequency > KI_FLASH_OFF_FRAMES)
                {
                    miFlashFrequency = 0;
                }
            }
        }

        if (!lbWasPaused)
        {
            if (mbIsPaused)
            {
                miFlashFrequency = 0;
            }
        }
    }

    // Find the buffered update for the wanted frame (translated to the sender's frame rate),
    // discard every buffered update older than it, and hand it back (or null).
    UpdateMessage* BrnNetworkPlayer::RetrieveBufferedMessage(u16 lu16Frame)
    {
        CGS_ASSERT(GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ
                   || GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_60HZ,
                   "GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ || GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_60HZ");
        CGS_ASSERT(GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ
                   || GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_60HZ,
                   "GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ || GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_60HZ");
        CGS_ASSERT(miNumUpdateMessagesBuffered <= KI_NUM_UPDATE_MESSAGES_TO_BUFFER,
                   "miNumUpdateMessagesBuffered <= KI_NUM_UPDATE_MESSAGES_TO_BUFFER");

        u16 lu16MessageThatWeWantFrame;
        if (GetLocalConsoleFrameRate() == GetRemoteConsoleFrameRate())
        {
            lu16MessageThatWeWantFrame = lu16Frame;
        }
        else if (GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ)
        {
            const u16 lu16NumWraps     = static_cast<u16>(mpTimeManager->GetFrameWrapCountSinceStart());
            const u16 lu16CurrentFrame = mpTimeManager->GetU16FrameCountSinceStart();
            lu16MessageThatWeWantFrame = CgsNetwork::TranslateFrame50HzTo60Hz(lu16Frame, lu16CurrentFrame, lu16NumWraps);
        }
        else
        {
            const u16 lu16NumWraps     = static_cast<u16>(mpTimeManager->GetFrameWrapCountSinceStart());
            const u16 lu16CurrentFrame = mpTimeManager->GetU16FrameCountSinceStart();
            lu16MessageThatWeWantFrame = CgsNetwork::TranslateFrame60HzTo50Hz(lu16Frame, lu16CurrentFrame, lu16NumWraps);
        }

        UpdateMessage* lpUpdateMessage = nullptr;
        for (s32 liIndex = 0; liIndex < miNumUpdateMessagesBuffered; ++liIndex)
        {
            if (maUpdateMessageBuffer[liIndex].GetU16FramesSinceStart() == lu16MessageThatWeWantFrame)
            {
                lpUpdateMessage = &maUpdateMessageBuffer[liIndex];
                break;
            }
        }

        if (lpUpdateMessage == nullptr)
        {
            return nullptr;
        }

        CGS_ASSERT(lu16MessageThatWeWantFrame == lpUpdateMessage->GetU16FramesSinceStart(),
                   "lu16MessageThatWeWantFrame == lpUpdateMessage->GetU16FramesSinceStart()");

        // Throw away everything older than the update we are about to use. Removal moves the
        // last buffered update into the freed slot, so that slot is looked at again.
        for (s32 liIndex = 0; liIndex < miNumUpdateMessagesBuffered; ++liIndex)
        {
            if (CgsNetwork::UInt16IsLargerWrapped(lpUpdateMessage->mu16Frame,
                                                  maUpdateMessageBuffer[liIndex].mu16Frame))
            {
                RemoveBufferedMessage(&maUpdateMessageBuffer[liIndex]);
                --liIndex;
            }
        }

        // The wanted update may itself have been moved by the removals: find it again.
        lpUpdateMessage = nullptr;
        for (s32 liIndex = 0; liIndex < miNumUpdateMessagesBuffered; ++liIndex)
        {
            if (maUpdateMessageBuffer[liIndex].GetU16FramesSinceStart() == lu16MessageThatWeWantFrame)
            {
                lpUpdateMessage = &maUpdateMessageBuffer[liIndex];
                break;
            }
        }
        CGS_ASSERT(lpUpdateMessage != nullptr, "lpUpdateMessage");

        return lpUpdateMessage;
    }

    // Remove one buffered update by moving the last buffered update into its slot.
    void BrnNetworkPlayer::RemoveBufferedMessage(UpdateMessage* lpUpdateMessage)
    {
        CGS_ASSERT(lpUpdateMessage->mu16Frame != CgsNetwork::KU16_INVALID_FRAME,
                   "lpUpdateMessage->GetU16Frame() != KU16_INVALID_FRAME");

        const s32 liBufIndex = static_cast<s32>(lpUpdateMessage - maUpdateMessageBuffer);
        CGS_ASSERT(liBufIndex >= 0, "liBufIndex >= 0");
        CGS_ASSERT(liBufIndex < miNumUpdateMessagesBuffered, "liBufIndex < miNumUpdateMessagesBuffered");

        maUpdateMessageBuffer[liBufIndex] = maUpdateMessageBuffer[miNumUpdateMessagesBuffered - 1];
        --miNumUpdateMessagesBuffered;
    }

    // A buffered update is applied like a freshly received one, after recording its frame as
    // the last buffered and the last applied update.
    void BrnNetworkPlayer::TellNetworkCarAboutBufferedUpdateMessage(u16 lu16CurrentFrame,
                                                                    UpdateMessage* lpUpdateMessage)
    {
        CGS_ASSERT(lpUpdateMessage->mu16Frame != CgsNetwork::KU16_INVALID_FRAME,
                   "lpUpdateMessage->GetU16Frame() != KU16_INVALID_FRAME");

        mu16LastBufferedUpdateAppliedFrame = lpUpdateMessage->mu16Frame;
        mu16LastAnyUpdateAppliedFrame      = lpUpdateMessage->mu16Frame;
        TellNetworkCarAboutRecievedUpdateMessage(lu16CurrentFrame, lpUpdateMessage);
    }

    // ---- reliable-message senders ------------------------------------------------------

    // Tail-calls CarSelectMessage::PrepareForSend on the send slot; every argument moves
    // up one place behind the frame id and the deformation stays in its float register.
    void BrnNetworkPlayer::SendSelectedCar(CgsID lCarID, CgsID lWheelID, u16 lu16CarColourIndex,
                                           u16 lu16PaintFinishIndex, f32 lfDeformation,
                                           bool lbFinalCarSelection)
    {
        mCarSelectMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount(), lCarID, lWheelID,
                                             lu16CarColourIndex, lu16PaintFinishIndex,
                                             lfDeformation, lbFinalCarSelection);
    }

    void BrnNetworkPlayer::SendLandmarkTriggeredMessage(s32 liLandmarkIndex)
    {
        mCheckpointTriggeredMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount(), liLandmarkIndex);
    }

    // Collectables are not sent straight away: the pickup is latched and Update sends it on
    // this player's turn.
    void BrnNetworkPlayer::SendCollectableDone(CgsID lCollectableID,
                                               CollectableMessage::ECollectableType leCollectableType)
    {
        meCollectableType = leCollectableType;
        mCollectableID    = lCollectableID;
    }

    void BrnNetworkPlayer::SendSwitchBurningHomeRunRunnerMessage(CgsNetwork::NetworkPlayerID lNewBurningHomeRunRunner)
    {
        CGS_ASSERT(lNewBurningHomeRunRunner != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lNewBurningHomeRunRunner != CgsNetwork::K_INVALID_PLAYER_ID");
        mBurningHomeRunSwitchRunnerMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount(),
                                                              lNewBurningHomeRunRunner);
    }

    // Cache the newest skillz for Update to send on this player's turn. An initial-data send
    // that has not gone out yet stays marked initial.
    void BrnNetworkPlayer::SendNewBurnoutSkillzData(const BrnGameState::BurnoutSkillzData* lpBurnoutSkillzData,
                                                    bool lbIsInitial)
    {
        CGS_ASSERT(lpBurnoutSkillzData != nullptr, "lpBurnoutSkillzData");

        mCachedBurnoutSkillzData = *lpBurnoutSkillzData;

        if (mbBurnoutSkillzToSend)
        {
            mbCachedBurnoutSkillzAreIntial = mbCachedBurnoutSkillzAreIntial || lbIsInitial;
        }
        else
        {
            mbCachedBurnoutSkillzAreIntial = lbIsInitial;
            mbBurnoutSkillzToSend          = true;
        }
    }

    void BrnNetworkPlayer::SendShowtimeUpdateData(s32 liShowtimeScore)
    {
        mShowtimeUpdateMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount(), liShowtimeScore);
    }

    void BrnNetworkPlayer::SendShowtimeModeSwitchMessage(s32 liShowtimeValue, bool lbSwitchOn)
    {
        mShowtimeSwitchMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount(), liShowtimeValue,
                                                  lbSwitchOn);
    }

    // Send the completion bits now if it is our turn; otherwise keep them for
    // HandleSendingFburnChallengeMessages.
    void BrnNetworkPlayer::SendFburnChallengeStatusMessage(
            const BrnGameState::GameStateModuleIO::CompletedFburnChallenges* lpCompletedChallenges)
    {
        if (IsOurTurnToSend())
        {
            mFburnChallengeStatusMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount(),
                                                            lpCompletedChallenges);
        }
        else
        {
            mCompletedChallengesToSend = *lpCompletedChallenges;
        }
    }

    void BrnNetworkPlayer::SendActiveFburnChallengeMessage(
            const BrnNetworkModuleIO::NetworkInActiveFburnChallengeEvent* lpActiveFreeburnChallengeEvent,
            CgsNetwork::NetworkPlayerID* lpPlayerIDs)
    {
        CGS_ASSERT(lpActiveFreeburnChallengeEvent != nullptr, "lpActiveFreeburnChallengeEvent");
        mActiveFburnChallengeMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount(),
                                                        lpActiveFreeburnChallengeEvent->mChallengeID,
                                                        lpPlayerIDs,
                                                        lpActiveFreeburnChallengeEvent->miNumPlayersInChallenge);
    }

    // Tail-calls StuntScoreUpdatedMessage::PrepareForSend on the send slot.
    void BrnNetworkPlayer::SendStuntScoreUpdatedMessage(s32 liStuntScore)
    {
        mStuntScoreUpdatedMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount(), liStuntScore);
    }

    // Samples the frame count, then the frames-since-start, and stamps the multiplier send
    // slot with the whole 8-byte record (one 64-bit register on the console).
    void BrnNetworkPlayer::SendStuntMultiplierMessage(
            BrnNetworkModuleIO::NetworkInStuntMultiplierEvent lStuntMultiplier)
    {
        const u16 lu16FrameId        = mpTimeManager->GetU16FrameCount();
        const s32 liFramesSinceStart = mpTimeManager->GetFramesSinceStart();

        StuntMultiplierMessage::MultiplierInfo lMultiplierInfo;
        lMultiplierInfo.muMultiplierStuntTypes = lStuntMultiplier.muStuntTypes;      // +0x00
        lMultiplierInfo.mu16FlatSpins          = lStuntMultiplier.mu16FlatSpins;     // +0x04
        lMultiplierInfo.mu16BarrelRolls        = lStuntMultiplier.mu16BarrelRolls;   // +0x06
        mStuntMultiplierMessageSend.PrepareForSend(lu16FrameId, liFramesSinceStart, lMultiplierInfo);
    }

    // Rescale the received multiplier from the sender's frame rate to ours and queue it for
    // the game side.
    void BrnNetworkPlayer::ProcessReceivedStuntMultiplier(CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                          s32 liStuntMultiplier,
                                                          s64 li64MultiplierData)
    {
        CGS_ASSERT(mpNetworkModule != 0, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager() != 0,
                   "mpNetworkModule->GetNetworkManager()");

        const s32 liLocalFrameRate =
            static_cast<s32>(mpNetworkModule->GetNetworkManager()->GetLocalConsoleFrameRate());
        CGS_ASSERT(liLocalFrameRate == KI_FRAMERATE_50HZ || liLocalFrameRate == KI_FRAMERATE_60HZ,
                   "( leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ ) || "
                   "( leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ )");

        CGS_ASSERT(GetPlayerManager() != 0, "mpPlayerManager");
        CgsNetwork::NetworkPlayer* lpNetworkPlayer =
            GetPlayerManager()->GetPlayerByID(lNetworkPlayerID);
        CGS_ASSERT(lpNetworkPlayer != 0, "lpNetworkPlayer");

        const s32 liRemoteFrameRate =
            static_cast<s32>(lpNetworkPlayer->GetRemoteConsoleFrameRate());
        CGS_ASSERT(liRemoteFrameRate == KI_FRAMERATE_50HZ || liRemoteFrameRate == KI_FRAMERATE_60HZ,
                   "( leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ ) || "
                   "( leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ )");

        s32 liConvertedMultiplier;
        if (liLocalFrameRate == liRemoteFrameRate)
        {
            liConvertedMultiplier = liStuntMultiplier;
        }
        else if (liLocalFrameRate == KI_FRAMERATE_50HZ)
        {
            // local 50, remote 60: scale 5/6.
            liConvertedMultiplier = 5 * liStuntMultiplier / 6;
        }
        else
        {
            // local 60, remote 50: scale 6/5.
            liConvertedMultiplier = 6 * liStuntMultiplier / 5;
        }

        BrnNetworkModuleIO::NetworkOutStuntMultiplierEvent lEvent;
        lEvent.mi64MultiplierData = li64MultiplierData;
        lEvent.miStuntMultiplier  = liConvertedMultiplier;
        lEvent.mNetworkPlayerID   = lNetworkPlayerID;

        CGS_ASSERT(mpNetworkModule != 0, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetNetworkEventQueue() != 0,
                   "mpNetworkModule->GetNetworkEventQueue()");
        mpNetworkModule->GetNetworkEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent),
            lEvent.GetEventType(), sizeof(lEvent));   // console record: 16 bytes
    }

    // ---- reliable-message arrived callbacks ----------------------------------------------
    // Each is registered by Prepare with the owning player as the user data.

    // Copy the sender's car selection into its menu data.
    void BrnNetworkPlayer::_CarSelectMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                            CgsNetwork::NetworkPlayerID /*lNetworkPlayerID*/,
                                                            void* lpUserData)
    {
        BrnNetworkPlayer* lpNetPlayer        = static_cast<BrnNetworkPlayer*>(lpUserData);
        CarSelectMessage* lpCarSelectMessage = static_cast<CarSelectMessage*>(lpMessage);
        CGS_ASSERT(lpNetPlayer != nullptr, "lpNetPlayer");
        CGS_ASSERT(lpCarSelectMessage != nullptr, "lpCarSelectMessage");

        CgsID lCarId;
        CgsID lWheelId;
        u16   lu16CarColourIndex;
        u16   lu16PaintFinishIndex;
        f32   lfDeformation;
        bool  lbFinalCarSelection;
        lpCarSelectMessage->Retrieve(&lCarId, &lWheelId, &lu16CarColourIndex, &lu16PaintFinishIndex,
                                     &lfDeformation, &lbFinalCarSelection);

        PlayerMenuData* lpMenuData = static_cast<PlayerMenuData*>(
            lpNetPlayer->GetPlayerManager()->GetMenuDataByID(lpNetPlayer->GetPlayerID()));
        lpMenuData->mCarId               = lCarId;
        lpMenuData->mWheelId             = lWheelId;
        lpMenuData->mu16CarColourIndex   = lu16CarColourIndex;
        lpMenuData->mu16PaintFinishIndex = lu16PaintFinishIndex;
        lpMenuData->mfUnknown48          = lfDeformation;
        lpMenuData->mbFinalCarSelection  = lbFinalCarSelection;
    }

    // Queue a remote collectable pickup for the game side (network event 44).
    void BrnNetwork::BrnNetworkPlayer::_CollectableMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                          CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                          void* lpUserData)
    {
        BrnNetworkPlayer*   lpNetPlayer           = static_cast<BrnNetworkPlayer*>(lpUserData);
        CollectableMessage* lpCollectabledMessage = static_cast<CollectableMessage*>(lpMessage);
        CGS_ASSERT(lpNetPlayer != nullptr, "lpNetPlayer");
        CGS_ASSERT(lpCollectabledMessage != nullptr, "lpCollectabledMessage");

        BrnNetworkModuleIO::NetworkOutNetworkPlayerCollectableEvent lNetworkPlayerCollectableEvent;
        CollectableMessage::ECollectableType leCollectableType;
        CGS_ASSERT(lpCollectabledMessage->Retrieve(&lNetworkPlayerCollectableEvent.mID, &leCollectableType),
                   "lpCollectabledMessage->Retrieve( &lNetworkPlayerCollectableEvent.mID, &leCollectableType )");

        lNetworkPlayerCollectableEvent.mNetworkPlayerID = lNetworkPlayerID;
        switch (leCollectableType)
        {
            case CollectableMessage::E_COLLECTABLE_TYPE_JUMP:
                lNetworkPlayerCollectableEvent.meType = BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP;
                break;
            case CollectableMessage::E_COLLECTABLE_TYPE_SMASH:
                lNetworkPlayerCollectableEvent.meType = BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH;
                break;
            case CollectableMessage::E_COLLECTABLE_TYPE_BILLBOARD:
                lNetworkPlayerCollectableEvent.meType = BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD;
                break;
            default:
                CGS_ASSERT(false, "Unknown stunt element type\n");
                break;
        }

        lpNetPlayer->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lNetworkPlayerCollectableEvent),
            lNetworkPlayerCollectableEvent.GetEventType(),
            sizeof(lNetworkPlayerCollectableEvent));   // console record: 16 bytes
    }

    // Queue the new Burning Home Run runner for the game side (network event 61).
    void BrnNetworkPlayer::_BurningHomeRunSwitchRunnerMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                             CgsNetwork::NetworkPlayerID /*lNetworkPlayerID*/,
                                                                             void* lpUserData)
    {
        BrnNetworkPlayer* lpNetPlayer = static_cast<BrnNetworkPlayer*>(lpUserData);
        BurningHomeRunSwitchRunnerMessage* lpSwitchRunnerMessage =
            static_cast<BurningHomeRunSwitchRunnerMessage*>(lpMessage);
        CGS_ASSERT(lpNetPlayer != nullptr, "lpNetPlayer");
        CGS_ASSERT(lpSwitchRunnerMessage != nullptr, "lpSwitchRunnerMessage");

        BrnNetworkModuleIO::NetworkOutSwitchBurningHomeRunRunner lSwitchRunnerEvent;
        lpSwitchRunnerMessage->Retrieve(&lSwitchRunnerEvent.mNewRunnerID);
        CGS_ASSERT(lSwitchRunnerEvent.mNewRunnerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lSwitchRunnerEvent.mNewRunnerID != CgsNetwork::K_INVALID_PLAYER_ID");

        lpNetPlayer->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lSwitchRunnerEvent),
            lSwitchRunnerEvent.GetEventType(), sizeof(lSwitchRunnerEvent));   // console record: 4 bytes
    }

    // Queue a remote player's new skillz for the game side (network event 62).
    void BrnNetworkPlayer::_BurnoutSkillzMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                void* lpUserData)
    {
        BrnNetworkPlayer*     lpPlayer               = static_cast<BrnNetworkPlayer*>(lpUserData);
        BurnoutSkillzMessage* lpBurnoutSkillzMessage = static_cast<BurnoutSkillzMessage*>(lpMessage);
        CGS_ASSERT(lpPlayer != nullptr, "lpPlayer");
        CGS_ASSERT(lpBurnoutSkillzMessage != nullptr, "lpBurnoutSkillzMessage");

        BrnNetworkModuleIO::NetworkOutBurnoutSkillzChanged lSkillzEvent;
        lSkillzEvent.mNewSkillzData.Clear();
        lSkillzEvent.mPlayerID = lNetworkPlayerID;
        if (lpBurnoutSkillzMessage->Retrieve(&lSkillzEvent.mNewSkillzData, &lSkillzEvent.mbInitialData))
        {
            lpPlayer->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lSkillzEvent),
                lSkillzEvent.GetEventType(), sizeof(lSkillzEvent));   // console record: 64 bytes
        }
    }

    // Queue a remote player's showtime switch for the game side (network event 64).
    void BrnNetworkPlayer::_ShowtimeSwitchMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                 CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                 void* lpUserData)
    {
        BrnNetworkPlayer*      lpPlayer                = static_cast<BrnNetworkPlayer*>(lpUserData);
        ShowtimeSwitchMessage* lpShowtimeSwitchMessage = static_cast<ShowtimeSwitchMessage*>(lpMessage);
        CGS_ASSERT(lpPlayer != nullptr, "lpPlayer");
        CGS_ASSERT(lpShowtimeSwitchMessage != nullptr, "lpShowtimeSwitchMessage");

        BrnNetworkModuleIO::NetworkOutShowtimeSwitch lShowtimeSwitchEvent;
        lShowtimeSwitchEvent.mPlayerID = lNetworkPlayerID;
        if (lpShowtimeSwitchMessage->Retrieve(&lShowtimeSwitchEvent.miFinalShowtimeScore,
                                              &lShowtimeSwitchEvent.mbEnteringShowtime))
        {
            lpPlayer->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lShowtimeSwitchEvent),
                lShowtimeSwitchEvent.GetEventType(), sizeof(lShowtimeSwitchEvent));   // console record: 12 bytes
        }
    }

    // Queue a remote player's freeburn-challenge event for the game side (network event 65).
    void BrnNetworkPlayer::_FreeburnChallengeMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                    CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                    void* lpUserData)
    {
        BrnNetworkPlayer*         lpPlayer                   = static_cast<BrnNetworkPlayer*>(lpUserData);
        FreeburnChallengeMessage* lpFreeburnChallengeMessage = static_cast<FreeburnChallengeMessage*>(lpMessage);
        CGS_ASSERT(lpPlayer != nullptr, "lpPlayer");
        CGS_ASSERT(lpFreeburnChallengeMessage != nullptr, "lpFreeburnChallengeMessage");

        BrnNetworkModuleIO::NetworkOutFreeburnChallenge lChallengeEvent;
        lChallengeEvent.mPlayerID = lNetworkPlayerID;
        // The record holds the event type as its underlying word; Retrieve writes it in place.
        if (lpFreeburnChallengeMessage->Retrieve(
                &lChallengeEvent.mChallengeID,
                reinterpret_cast<BrnNetworkModuleIO::EChallengeEventType*>(&lChallengeEvent.meEventType),
                &lChallengeEvent.meChallengeStatus,
                &lChallengeEvent.miActionIndex))
        {
            lpPlayer->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lChallengeEvent),
                lChallengeEvent.GetEventType(), sizeof(lChallengeEvent));   // console record: 32 bytes
        }
    }

    // Hand a remote player's challenge-completion bits to the game-state completed-challenges
    // queue.
    void BrnNetworkPlayer::_FburnChallengeStatusMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                       CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                       void* lpUserData)
    {
        BrnNetworkPlayer* lpPlayer = static_cast<BrnNetworkPlayer*>(lpUserData);
        CGS_ASSERT(lpPlayer != nullptr, "lpPlayer");

        FburnChallengeStatusMessage* lpFreeburnChallengeStatusMessage =
            static_cast<FburnChallengeStatusMessage*>(lpMessage);
        CGS_ASSERT(lpFreeburnChallengeStatusMessage != nullptr, "lpFreeburnChallengeStatusMessage");

        BrnGameState::GameStateModuleIO::CompletedFburnChallengesData lCompletedChallengesData;
        lCompletedChallengesData.mNetworkPlayerID = lNetworkPlayerID;
        if (lpFreeburnChallengeStatusMessage->Retrieve(&lCompletedChallengesData.mCompletedFreeburnChallenges))
        {
            CGS_ASSERT(lpPlayer->mpNetworkModule != nullptr, "lpPlayer->mpNetworkModule");

            BrnNetworkModuleIO::NetworkToGameStateInterface* lpNetworkToGameStateInterface =
                lpPlayer->mpNetworkModule->GetNetworkToGameStateInterface();
            CGS_ASSERT(lpNetworkToGameStateInterface != nullptr, "lpNetworkToGameStateInterface");
            CGS_ASSERT(lpNetworkToGameStateInterface->GetCompletedChallengesQueue() != nullptr,
                       "lpNetworkToGameStateInterface->GetCompletedChallengesQueue()");

            lpNetworkToGameStateInterface->GetCompletedChallengesQueue()->AddEvent(lCompletedChallengesData);
        }
    }

    // Queue the roster of a challenge that was already running (network event 66), with the
    // player ids turned into active-race-car indices.
    void BrnNetworkPlayer::_ActiveFreeburnChallengeMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                          CgsNetwork::NetworkPlayerID /*lNetworkPlayerID*/,
                                                                          void* lpUserData)
    {
        BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(lpUserData);
        ActiveFburnChallengeMessage* lpActiveChallengeMessage =
            static_cast<ActiveFburnChallengeMessage*>(lpMessage);
        CGS_ASSERT(lpNetworkPlayer != nullptr, "lpNetworkPlayer");
        CGS_ASSERT(lpActiveChallengeMessage != nullptr, "lpActiveChallengeMessage");

        BrnNetworkModuleIO::NetworkOutActiveFburnChallengeEvent lActiveChallengeEvent;
        CgsNetwork::NetworkPlayerID laPlayersInChallengeIDs[ActiveFburnChallengeMessage::KI_MAX_NETWORK_PLAYERS];
        if (lpActiveChallengeMessage->Retrieve(&lActiveChallengeEvent.mChallengeID, laPlayersInChallengeIDs,
                                               &lActiveChallengeEvent.miNumPlayersInChallenge))
        {
            CGS_ASSERT(lpNetworkPlayer->mpNetworkModule != nullptr, "lpNetworkPlayer->mpNetworkModule");

            for (s32 liIndex = 0; liIndex < lActiveChallengeEvent.miNumPlayersInChallenge; ++liIndex)
            {
                lActiveChallengeEvent.maePlayersInChallengeARCI[liIndex] =
                    lpNetworkPlayer->mpNetworkModule->GetActiveRaceCarIndex(laPlayersInChallengeIDs[liIndex]);
            }

            CGS_ASSERT(lpNetworkPlayer->mpNetworkModule->GetNetworkEventQueue() != nullptr,
                       "lpNetworkPlayer->mpNetworkModule->GetNetworkEventQueue()");
            lpNetworkPlayer->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lActiveChallengeEvent),
                lActiveChallengeEvent.GetEventType(), sizeof(lActiveChallengeEvent));   // console record: 48 bytes
        }
    }

    // Queue a remote checkpoint hit for the game side (network event 46).
    void BrnNetworkPlayer::_CheckpointTriggeredMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                      CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                      void* lpUserData)
    {
        CheckpointTriggeredMessage* lpCheckpointTriggeredMessage =
            static_cast<CheckpointTriggeredMessage*>(lpMessage);
        CGS_ASSERT(lpCheckpointTriggeredMessage != nullptr, "lpCheckpointTriggeredMessage");

        BrnNetworkModuleIO::NetworkOutRemotePlayerHitCheckpoint lCheckpointEvent;
        if (lpCheckpointTriggeredMessage->Retrieve(&lCheckpointEvent.miCheckpointIndex))
        {
            BrnNetworkPlayer* lpPlayer = static_cast<BrnNetworkPlayer*>(lpUserData);
            CGS_ASSERT(lpPlayer != nullptr, "lpPlayer");

            lCheckpointEvent.mNetworkPlayerID = lNetworkPlayerID;
            lpPlayer->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lCheckpointEvent),
                lCheckpointEvent.GetEventType(), sizeof(lCheckpointEvent));   // console record: 8 bytes
        }
    }

    // Queue a remote stunt score for the game side (network event 47).
    void BrnNetworkPlayer::_StuntScoreUpdatedMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                   CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                   void* lpUserData)
    {
        StuntScoreUpdatedMessage* lpStuntScoreUpdatedMessage =
            static_cast<StuntScoreUpdatedMessage*>(lpMessage);
        CGS_ASSERT(lpStuntScoreUpdatedMessage != 0, "lpStuntScoreUpdatedMessage");

        BrnNetworkModuleIO::NetworkOutStuntScoreUpdatedEvent lEvent;
        if (lpStuntScoreUpdatedMessage->Retrieve(&lEvent.miStuntScore))
        {
            BrnNetworkPlayer* lpPlayer = static_cast<BrnNetworkPlayer*>(lpUserData);
            CGS_ASSERT(lpPlayer != 0, "lpPlayer");

            lEvent.mNetworkPlayerID = lNetworkPlayerID;
            lpPlayer->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent),
                lEvent.GetEventType(), sizeof(lEvent));   // console record: 8 bytes
        }
    }

    // Unpack the arrived multiplier message and forward it to the owning BrnNetworkPlayer
    // (handed in as the registered user data).
    void BrnNetworkPlayer::_StuntMultiplierMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                 CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                 void* lpUserData)
    {
        CGS_ASSERT(lpMessage != 0, "lpStuntMultiplierMessage");

        s32 liStuntMultiplier = 0;
        s64 li64MultiplierData = 0;
        if (static_cast<StuntMultiplierMessage*>(lpMessage)->Retrieve(&liStuntMultiplier,
                                                                      &li64MultiplierData))
        {
            CGS_ASSERT(lpUserData != 0, "lpNetworkPlayer");
            static_cast<BrnNetworkPlayer*>(lpUserData)->ProcessReceivedStuntMultiplier(
                lNetworkPlayerID, liStuntMultiplier, li64MultiplierData);
        }
    }

    // ---- delivered callbacks -----------------------------------------------------------
    // Prepare registers one shared empty function as the delivered callback of every
    // reliable message type; each of these is that empty body.

    void BrnNetworkPlayer::_CarSelectMessageDeliveredCallback(bool, bool, CgsNetwork::SignalMessage*,
                                                              CgsNetwork::NetworkPlayerID, void*)
    {
    }

    void BrnNetworkPlayer::_CollectableMessageDeliveredCallback(bool, bool, CgsNetwork::SignalMessage*,
                                                                CgsNetwork::NetworkPlayerID, void*)
    {
    }

    void BrnNetworkPlayer::_BurningHomeRunSwitchRunnerMessageDeliveredCallback(bool, bool, CgsNetwork::SignalMessage*,
                                                                               CgsNetwork::NetworkPlayerID, void*)
    {
    }

    void BrnNetworkPlayer::_BurnoutSkillzMessageDeliveredCallback(bool, bool, CgsNetwork::SignalMessage*,
                                                                  CgsNetwork::NetworkPlayerID, void*)
    {
    }

    void BrnNetworkPlayer::_ShowtimeSwitchMessageDeliveredCallback(bool, bool, CgsNetwork::SignalMessage*,
                                                                   CgsNetwork::NetworkPlayerID, void*)
    {
    }

    void BrnNetworkPlayer::_FreeburnChallengeMessageDeliveredCallback(bool, bool, CgsNetwork::SignalMessage*,
                                                                      CgsNetwork::NetworkPlayerID, void*)
    {
    }

    void BrnNetworkPlayer::_FburnChallengeStatusMessageDeliveredCallback(bool, bool, CgsNetwork::SignalMessage*,
                                                                         CgsNetwork::NetworkPlayerID, void*)
    {
    }

    void BrnNetworkPlayer::_ActiveFreeburnChallengeMessageDeliveredCallback(bool, bool, CgsNetwork::SignalMessage*,
                                                                            CgsNetwork::NetworkPlayerID, void*)
    {
    }

    void BrnNetworkPlayer::_CheckpointTriggeredMessageDeliveredCallback(bool, bool, CgsNetwork::SignalMessage*,
                                                                        CgsNetwork::NetworkPlayerID, void*)
    {
    }

    void BrnNetworkPlayer::_StuntScoreUpdatedMessageDeliveredCallback(bool, bool, CgsNetwork::SignalMessage*,
                                                                      CgsNetwork::NetworkPlayerID, void*)
    {
    }

    void BrnNetworkPlayer::_StuntMultiplierMessageDeliveredCallback(bool, bool, CgsNetwork::SignalMessage*,
                                                                    CgsNetwork::NetworkPlayerID, void*)
    {
    }

    // ---- bodies that waited on header declarations -----------------------------------

    // Register every Burnout message pair with this player, prepare the base player once (the
    // remote console's frame rate comes from its lobby parameters, not from the caller), and
    // reset the per-round send state and the update-message buffer.
    bool BrnNetworkPlayer::Prepare(CgsNetwork::NetworkAdapter* lpNetworkAdapter,
                                   const CgsSystem::TimerStatus* lpTimerStatus,
                                   const char* lpcName,
                                   CgsSystem::EFrameRate leLocalConsoleFrameRate,
                                   CgsSystem::EFrameRate /*leRemoteConsoleFrameRate*/,
                                   CgsNetwork::NetworkPlayerID liPlayerID)
    {
        CGS_ASSERT(leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ
                   || leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ,
                   "leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ || leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ");

        CgsNetwork::ServerInterfaceGames* lpGameComponent =
            mpNetworkModule->GetNetworkManager()->GetServerInterface()->GetGameComponent();
        CGS_ASSERT(lpGameComponent, "lpGameComponent");

        PlayerParams lPlayerParams;
        lPlayerParams.Prepare();
        lpGameComponent->GetPlayerParametersByPlayerID(liPlayerID, &lPlayerParams);
        const CgsSystem::EFrameRate leRemoteConsoleFrameRate = lPlayerParams.GetConsoleFrameRate();

        // The lengths are the host sizes of the message objects: the reliable-message buffer
        // copies that many bytes of the message (console sizes in the comments).
        RegisterMessageType(KI_MESSAGE_TYPE_UPDATE, static_cast<s32>(sizeof(UpdateMessage)),        // 0xB0
                            &mUpdateMessageSend, &mUpdateMessageRecv, nullptr, nullptr, nullptr);
        RegisterMessageType(KI_MESSAGE_TYPE_CAR_SELECT, static_cast<s32>(sizeof(CarSelectMessage)), // 0x48
                            &mCarSelectMessageSend, &mCarSelectMessageRecv,
                            &_CarSelectMessageArrivedCallback, &_CarSelectMessageDeliveredCallback, this);
        RegisterMessageType(KI_MESSAGE_TYPE_COLLECTABLE, static_cast<s32>(sizeof(CollectableMessage)), // 0x38
                            &mCollectableMessageSend, &mCollectableMessageRecv,
                            &_CollectableMessageArrivedCallback, &_CollectableMessageDeliveredCallback, this);
        RegisterMessageType(KI_MESSAGE_TYPE_BURNING_HOME_RUN_SWITCH_RUNNER,
                            static_cast<s32>(sizeof(BurningHomeRunSwitchRunnerMessage)),               // 0x2C
                            &mBurningHomeRunSwitchRunnerMessageSend, &mBurningHomeRunSwitchRunnerMessageRecv,
                            &_BurningHomeRunSwitchRunnerMessageArrivedCallback,
                            &_BurningHomeRunSwitchRunnerMessageDeliveredCallback, this);
        RegisterMessageType(KI_MESSAGE_TYPE_BURNOUT_SKILLZ, static_cast<s32>(sizeof(BurnoutSkillzMessage)), // 0x64
                            &mBurnoutSkillzMessageSend, &mBurnoutSkillzMessageRecv,
                            &_BurnoutSkillzMessageArrivedCallback, &_BurnoutSkillzMessageDeliveredCallback, this);
        RegisterMessageType(KI_MESSAGE_TYPE_SHOWTIME_UPDATE, static_cast<s32>(sizeof(ShowtimeUpdateMessage)), // 0x24
                            &mShowtimeUpdateMessageSend, &mShowtimeUpdateMessageRecv, nullptr, nullptr, nullptr);
        RegisterMessageType(KI_MESSAGE_TYPE_SHOWTIME_SWITCH, static_cast<s32>(sizeof(ShowtimeSwitchMessage)), // 0x30
                            &mShowtimeSwitchMessageSend, &mShowtimeSwitchMessageRecv,
                            &_ShowtimeSwitchMessageArrivedCallback, &_ShowtimeSwitchMessageDeliveredCallback, this);
        RegisterMessageType(KI_MESSAGE_TYPE_FREEBURN_CHALLENGE, static_cast<s32>(sizeof(FreeburnChallengeMessage)), // 0x40
                            &mFreeburnChallengeMessageSend, &mFreeburnChallengeMessageRecv,
                            &_FreeburnChallengeMessageArrivedCallback, &_FreeburnChallengeMessageDeliveredCallback, this);
        RegisterMessageType(KI_MESSAGE_TYPE_FBURN_CHALLENGE_STATUS,
                            static_cast<s32>(sizeof(FburnChallengeStatusMessage)),                     // 0x128
                            &mFburnChallengeStatusMessageSend, &mFburnChallengeStatusMessageRecv,
                            &_FburnChallengeStatusMessageArrivedCallback,
                            &_FburnChallengeStatusMessageDeliveredCallback, this);
        RegisterMessageType(KI_MESSAGE_TYPE_ACTIVE_FREEBURN_CHALLENGE,
                            static_cast<s32>(sizeof(ActiveFburnChallengeMessage)),                     // 0x58
                            &mActiveFburnChallengeMessageSend, &mActiveFburnChallengeMessageRecv,
                            &_ActiveFreeburnChallengeMessageArrivedCallback,
                            &_ActiveFreeburnChallengeMessageDeliveredCallback, this);
        RegisterMessageType(KI_MESSAGE_TYPE_CHECKPOINT_TRIGGERED,
                            static_cast<s32>(sizeof(CheckpointTriggeredMessage)),                      // 0x2C
                            &mCheckpointTriggeredMessageSend, &mCheckpointTriggeredMessageRecv,
                            &_CheckpointTriggeredMessageArrivedCallback,
                            &_CheckpointTriggeredMessageDeliveredCallback, this);
        RegisterMessageType(KI_MESSAGE_TYPE_STUNT_SCORE_UPDATED,
                            static_cast<s32>(sizeof(StuntScoreUpdatedMessage)),                        // 0x2C
                            &mStuntScoreUpdatedMessageSend, &mStuntScoreUpdatedMessageRecv,
                            &_StuntScoreUpdatedMessageArrivedCallback,
                            &_StuntScoreUpdatedMessageDeliveredCallback, this);
        RegisterMessageType(KI_MESSAGE_TYPE_STUNT_MULTIPLIER, static_cast<s32>(sizeof(StuntMultiplierMessage)), // 0x34
                            &mStuntMultiplierMessageSend, &mStuntMultiplierMessageRecv,
                            &_StuntMultiplierMessageArrivedCallback,
                            &_StuntMultiplierMessageDeliveredCallback, this);

        if (!mbIsPrepared)
        {
            mbIsPrepared = CgsNetwork::NetworkPlayer::Prepare(lpNetworkAdapter, lpTimerStatus, lpcName,
                                                              leLocalConsoleFrameRate, leRemoteConsoleFrameRate,
                                                              liPlayerID);
        }

        mbIsEliminated       = false;
        mbFirstUpdateMessage = true;

        for (u32 luField = 0; luField < BrnGameState::GameStateModuleIO::CompletedFburnChallenges::KU_NUM_BIT_WORDS; ++luField)
        {
            mCompletedChallengesToSend.maxBits[luField] = 0;
        }

        mCollectableID    = 0;
        meCollectableType = CollectableMessage::E_COLLECTABLE_TYPE_MAX;

        for (s32 liIndex = 0; liIndex < KI_NUM_UPDATE_MESSAGES_TO_BUFFER; ++liIndex)
        {
            maUpdateMessageBuffer[liIndex].Construct();
        }

        miNumUpdateMessagesBuffered        = 0;
        mu16LastPacketReceivedFrame        = CgsNetwork::KU16_INVALID_FRAME;
        mu16LastBufferedUpdateAppliedFrame = CgsNetwork::KU16_INVALID_FRAME;
        mu16LastAnyUpdateAppliedFrame      = CgsNetwork::KU16_INVALID_FRAME;

        return mbIsPrepared;
    }

    // A challenge was selected (latch it and start the one-second send countdown), triggered
    // (clear the latch), or ended (an end aborted before starting also clears the latch, and
    // any unsent challenge message is dropped); then send the event.
    void BrnNetworkPlayer::SendFreeburnChallengeMessage(CgsID lChallengeID,
                                                        BrnNetworkModuleIO::EChallengeEventType leEventType,
                                                        BrnGameState::EChallengeStatus leChallengeStatus,
                                                        s32 liValue)
    {
        const f32 KF_SELECTED_CHALLENGE_SEND_DELAY = 1.0f;

        if (leEventType == BrnNetworkModuleIO::E_CHALLENGE_EVENT_SELECTED)
        {
            mSelectedChallengeID              = lChallengeID;
            mfTimeBeforeSelectedChallengeSend = KF_SELECTED_CHALLENGE_SEND_DELAY;
            return;
        }

        if (leEventType == BrnNetworkModuleIO::E_CHALLENGE_EVENT_TRIGGERED)
        {
            mfTimeBeforeSelectedChallengeSend = KF_NO_SELECTED_CHALLENGE_SEND;
            mSelectedChallengeID              = 0;
        }
        else if (leEventType == BrnNetworkModuleIO::E_CHALLENGE_EVENT_ENDED)
        {
            if (leChallengeStatus == BrnGameState::E_CHALLENGE_STATUS_ABORTED_BEFORE_STARTING)
            {
                mfTimeBeforeSelectedChallengeSend = KF_NO_SELECTED_CHALLENGE_SEND;
                mSelectedChallengeID              = 0;
            }
            if (mFreeburnChallengeMessageSend.IsMessageValid())
            {
                mFreeburnChallengeMessageSend.SetMessageInvalid();
            }
        }

        CGS_ASSERT(!mFreeburnChallengeMessageSend.IsMessageValid(),
                   "!mFreeburnChallengeMessageSend.IsMessageValid()");
        mFreeburnChallengeMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount(), lChallengeID,
                                                     leEventType, leChallengeStatus, liValue);
    }

    // Send the queued challenge-completion bits on our turn, then count down the selected
    // challenge's send delay and send it once the delay runs out (retrying next frame while
    // the previous challenge message is still unsent).
    void BrnNetworkPlayer::HandleSendingFburnChallengeMessages(f32 lfTimeStep)
    {
        bool lbNothingToSend = true;
        for (u32 luField = 0; luField < BrnGameState::GameStateModuleIO::CompletedFburnChallenges::KU_NUM_BIT_WORDS; ++luField)
        {
            if (mCompletedChallengesToSend.maxBits[luField] != 0)
            {
                lbNothingToSend = false;
                break;
            }
        }

        if (!lbNothingToSend && IsOurTurnToSend())
        {
            mFburnChallengeStatusMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount(),
                                                            &mCompletedChallengesToSend);
            for (u32 luField = 0; luField < BrnGameState::GameStateModuleIO::CompletedFburnChallenges::KU_NUM_BIT_WORDS; ++luField)
            {
                mCompletedChallengesToSend.maxBits[luField] = 0;
            }
        }

        if (mfTimeBeforeSelectedChallengeSend < 0.0f)
        {
            return;
        }

        mfTimeBeforeSelectedChallengeSend -= lfTimeStep;
        if (!(mfTimeBeforeSelectedChallengeSend < 0.0f))
        {
            return;
        }

        if (mFreeburnChallengeMessageSend.IsMessageValid())
        {
            mfTimeBeforeSelectedChallengeSend = 0.0f;
            return;
        }

        mFreeburnChallengeMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount(), mSelectedChallengeID,
                                                     BrnNetworkModuleIO::E_CHALLENGE_EVENT_SELECTED,
                                                     BrnGameState::E_CHALLENGE_STATUS_ONGOING, 0);
        mSelectedChallengeID              = 0;
        mfTimeBeforeSelectedChallengeSend = KF_NO_SELECTED_CHALLENGE_SEND;
    }

    // Record the one-way time of the latest update (clamped at zero) in the latency ring and
    // turn the ring's average into whole frames at the local rate, capped at one second.
    void BrnNetworkPlayer::UpdateLatencyValue(u16 lu16SentFrame, u16 lu16CurrentFrame)
    {
        f32 lfFramesPerSecond;
        if (GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ)
        {
            lfFramesPerSecond = KF_FRAMES_PER_SECOND_50HZ;
        }
        else
        {
            if (GetLocalConsoleFrameRate() != CgsSystem::E_FRAMERATE_60HZ)
            {
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStream << "Unknown local frame rate enum " << static_cast<s32>(GetLocalConsoleFrameRate()) << "\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }
            lfFramesPerSecond = KF_FRAMES_PER_SECOND_60HZ;
        }

        CGS_ASSERT(GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ
                   || GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_60HZ,
                   "GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ || GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_60HZ");

        const bool lbRemoteIs50Hz = GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ;
        const bool lbLocalIs50Hz  = GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ;
        const f32 lfTimeDiff = CgsNetwork::GetTimeDiffWrapped16(lu16CurrentFrame, lu16SentFrame,
                                                                 mpTimeManager->GetFrameWrapCountSinceStart(),
                                                                 lbLocalIs50Hz, lbRemoteIs50Hz);
        const f32 lfLatency = (lfTimeDiff >= 0.0f) ? lfTimeDiff : 0.0f;

        CGS_ASSERT(miLatencyIndex < KI_LATENCY_FRAMES_TO_AVERAGE_OVER,
                   "miLatencyIndex < KI_LATENCY_FRAMES_TO_AVERAGE_OVER");
        mafPastLatencies[miLatencyIndex] = lfLatency;
        ++miLatencyIndex;
        miPastLatenciesCount = (miPastLatenciesCount > miLatencyIndex) ? miPastLatenciesCount : miLatencyIndex;
        if (miLatencyIndex >= KI_LATENCY_FRAMES_TO_AVERAGE_OVER)
        {
            miLatencyIndex = 0;
        }

        f32 lfTotalLatency = 0.0f;
        for (s32 liIndex = 0; liIndex < miPastLatenciesCount; ++liIndex)
        {
            lfTotalLatency += mafPastLatencies[liIndex];
        }
        CGS_ASSERT(miPastLatenciesCount > 0, "miPastLatenciesCount > 0");
        CGS_ASSERT(miPastLatenciesCount <= KI_LATENCY_FRAMES_TO_AVERAGE_OVER,
                   "miPastLatenciesCount <= KI_LATENCY_FRAMES_TO_AVERAGE_OVER");

        const f32 lfAverageLatency = lfTotalLatency / static_cast<f32>(miPastLatenciesCount);
        const s32 liLatencyFrames  = static_cast<s32>(fmaf(lfAverageLatency, lfFramesPerSecond, KF_ROUND_TO_NEAREST));
        const s32 liMaxLatencyFrames = static_cast<s32>(lfFramesPerSecond);
        miLatencyFrames = (liLatencyFrames < liMaxLatencyFrames) ? liLatencyFrames : liMaxLatencyFrames;
    }

    // Take the received update (when it belongs to this lobby / round): stamp the receive
    // frame, feed the latency estimate, and apply it when it is newer than both the last
    // buffered and the last applied update. Then hand a received showtime score to the game
    // side (network event 63).
    void BrnNetworkPlayer::CheckForNewMessages(u16 lu16FramesSinceStart)
    {
        CGS_ASSERT(lu16FramesSinceStart != CgsNetwork::KU16_INVALID_FRAME,
                   "lu16FramesSinceStart != KU16_INVALID_FRAME");
        CGS_ASSERT(GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ
                   || GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_60HZ,
                   "GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ || GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_60HZ");
        CGS_ASSERT(GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ
                   || GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_60HZ,
                   "GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ || GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_60HZ");

        u16 lu16LatencyAffectedFrame = static_cast<u16>(lu16FramesSinceStart - miLatencyFrames
                                                        - KU16_UPDATE_BUFFER_EXTRA_FRAMES);
        s32 liNumWraps = mpTimeManager->GetFrameWrapCountSinceStart();
        if (lu16LatencyAffectedFrame == CgsNetwork::KU16_INVALID_FRAME)
        {
            lu16LatencyAffectedFrame = 0;
        }
        if (lu16LatencyAffectedFrame > lu16FramesSinceStart)
        {
            --liNumWraps;
        }

        s16 li16FrameDiff = static_cast<s16>(lu16FramesSinceStart - lu16LatencyAffectedFrame);
        if (li16FrameDiff < 0)
        {
            li16FrameDiff = static_cast<s16>(-li16FrameDiff);
        }
        if (!(li16FrameDiff < KI16_MAX_LATENCY_AFFECTED_FRAME_DIFF))
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "miLatencyFrames=" << miLatencyFrames
                    << " lu16FramesSinceStart=" << static_cast<s32>(lu16FramesSinceStart)
                    << " lu16LatencyAffectedFrame=" << static_cast<s32>(lu16LatencyAffectedFrame);
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        if (mUpdateMessageRecv.IsMessageValid())
        {
            if (mpTimeManager->HasStartFrameBeenSetValid())
            {
                const s32 liRoundNumber = mpNetworkModule->GetNetworkManager()->GetRound();
                if (mUpdateMessageRecv.GetUpdateData()->mbIsFreeBurnLobby == (liRoundNumber == -1))
                {
                    const s32 liCurrentRound = mpNetworkModule->GetNetworkManager()->GetRound();
                    if (static_cast<s32>(mUpdateMessageRecv.GetUpdateData()->mbIsRoundNumberOdd) == (liCurrentRound & 1))
                    {
                        mu16LastPacketReceivedFrame = mpTimeManager->GetU16FrameCount();
                        CGS_ASSERT(mUpdateMessageRecv.mu16Frame != CgsNetwork::KU16_INVALID_FRAME,
                                   "mUpdateMessageRecv.GetU16Frame() != KU16_INVALID_FRAME");

                        UpdateLatencyValue(mUpdateMessageRecv.GetU16FramesSinceStart(), lu16FramesSinceStart);

                        if (mu16LastBufferedUpdateAppliedFrame == CgsNetwork::KU16_INVALID_FRAME
                            || CgsNetwork::UInt16IsLargerWrapped(mUpdateMessageRecv.mu16Frame,
                                                                 mu16LastBufferedUpdateAppliedFrame))
                        {
                            const bool lbRemoteIs50Hz = GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ;
                            const bool lbLocalIs50Hz  = GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ;
                            // The time between the latency-affected frame and the update's send
                            // frame is computed and not used further.
                            CgsNetwork::GetTimeDiffWrapped16(lu16LatencyAffectedFrame,
                                                             mUpdateMessageRecv.GetU16FramesSinceStart(),
                                                             liNumWraps, lbLocalIs50Hz, lbRemoteIs50Hz);

                            if (mu16LastAnyUpdateAppliedFrame == CgsNetwork::KU16_INVALID_FRAME
                                || CgsNetwork::UInt16IsLargerWrapped(mUpdateMessageRecv.mu16Frame,
                                                                     mu16LastAnyUpdateAppliedFrame))
                            {
                                TellNetworkCarAboutRecievedUpdateMessage(lu16FramesSinceStart, &mUpdateMessageRecv);
                            }
                        }
                    }
                }
            }

            mUpdateMessageRecv.Release();
            CGS_ASSERT(!mUpdateMessageRecv.IsMessageValid(), "!mUpdateMessageRecv.IsMessageValid()");
        }

        if (mShowtimeUpdateMessageRecv.IsMessageValid())
        {
            BrnNetworkModuleIO::NetworkOutShowtimeUpdate lShowtimeUpdateEvent;
            lShowtimeUpdateEvent.mPlayerID        = GetPlayerID();
            lShowtimeUpdateEvent.miShowtimeScore  = -1;
            if (mShowtimeUpdateMessageRecv.Retrieve(&lShowtimeUpdateEvent.miShowtimeScore))
            {
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lShowtimeUpdateEvent),
                    lShowtimeUpdateEvent.GetEventType(), sizeof(lShowtimeUpdateEvent));   // console record: 8 bytes
            }

            mShowtimeUpdateMessageRecv.SetMessageInvalid();
            CGS_ASSERT(!mShowtimeUpdateMessageRecv.IsMessageValid(), "!mShowtimeUpdateMessageRecv.IsMessageValid()");
        }
    }

    // Send this frame's update for the local car on our turn: the car's transform and
    // velocities, the driver's controls, the frame counters, the lobby / round / elimination /
    // car-select state, the camera and headset status, and whether a transform reset or a
    // slam happened since the last update (latched every frame until one is sent).
    // The console also prints the update to its debug stream when the module asks for it
    // (IsSendUpdateMessageToBeShown); that print needs the vector stream operator, which has
    // no home in the tree (not reproduced).
    void BrnNetworkPlayer::HandleSendingUpdateMessage()
    {
        if (!mbIsPrepared || mpTimeManager == nullptr || !mpTimeManager->HasStartFrameBeenSetValid()
            || !mbIsUpdateMessageSendingEnabled)
        {
            return;
        }

        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface =
            mpNetworkModule->GetActiveRaceCarInterface();
        const BrnWorld::PlayerVehicleControls* lpPlayerVehicleControls = mpNetworkModule->GetPlayerVehicleControls();
        CGS_ASSERT(lpActiveRaceCarInterface, "lpActiveRaceCarInterface");
        CGS_ASSERT(lpPlayerVehicleControls, "lpPlayerVehicleControls");

        const CgsNetwork::PlayerManager* lpPlayerManager = mpNetworkModule->GetNetworkManager()->GetPlayerManager();
        CgsNetwork::NetworkPlayerID lLocalPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        CGS_ASSERT(lpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID),
                   "lpPlayerManager->GetNextLocalPlayerID( &lLocalPlayerID )");

        const EActiveRaceCarIndex leActiveRaceCarIndex = mpNetworkModule->GetActiveRaceCarIndex(lLocalPlayerID);
        if (leActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID
            || !lpActiveRaceCarInterface->IsRaceCarActive(leActiveRaceCarIndex))
        {
            return;
        }

        const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState =
            lpActiveRaceCarInterface->GetRaceCarState(leActiveRaceCarIndex);
        CGS_ASSERT(lpRaceCarState != nullptr, "lpRaceCarState != NULL");

        mbHasResetCarTransformSinceLastUpdateSent = mbHasResetCarTransformSinceLastUpdateSent || lpRaceCarState->mbResetCarTransform;
        mbHasBeenSlammedSinceLastUpdateSent       = mbHasBeenSlammedSinceLastUpdateSent || lpRaceCarState->mbJustBeenSlammed;

        if (!IsOurTurnToSend())
        {
            return;
        }

        const CgsNetwork::PlayerMenuData* lpLocalPlayerMenuData = GetPlayerManager()->GetMenuDataByID(lLocalPlayerID);
        CGS_ASSERT(lpLocalPlayerMenuData, "lpLocalPlayerMenuData");
        CGS_ASSERT(!mUpdateMessageSend.IsMessageValid(), "!mUpdateMessageSend.IsMessageValid()");

        const u16 lu16CurrentFrame = mpTimeManager->GetU16FrameCount();
        CGS_ASSERT(lu16CurrentFrame != CgsNetwork::KU16_INVALID_FRAME, "lu16CurrentFrame != KU16_INVALID_FRAME");
        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetGameStateToNetworkInterface(),
                   "mpNetworkModule->GetGameStateToNetworkInterface()");

        UpdateData lUpdateData;
        lUpdateData.mu16SentFrame              = lu16CurrentFrame;
        lUpdateData.mu16FramesSinceStart       = mpTimeManager->GetU16FrameCountSinceStart();
        lUpdateData.mMatrix                    = lpRaceCarState->mTransform;
        lUpdateData.mLinearVelocity            = lpRaceCarState->mLinearVelocity;
        lUpdateData.mAngularVelocity           = lpRaceCarState->mAngularVelocity;
        lUpdateData.mfSteering                 = lpPlayerVehicleControls->mfXAxis0;
        lUpdateData.mfAcceleration             = lpPlayerVehicleControls->mfAcceleration;
        lUpdateData.mfBraking                  = lpPlayerVehicleControls->mfBraking;
        lUpdateData.mbIsBoosting               = lpPlayerVehicleControls->mbBoost;
        lUpdateData.mbIsCrashing               = lpRaceCarState->mbCrashing;
        lUpdateData.mbReceivingPlayerCrashedUs = (mLocalPlayerRaceCarCrasherID == GetPlayerID());
        lUpdateData.mbIsEliminated             = mbIsEliminated;
        lUpdateData.mbSnap                     = mbHasBeenSlammedSinceLastUpdateSent || mbHasResetCarTransformSinceLastUpdateSent;
        lUpdateData.mbIsFreeBurnLobby          = (mpNetworkModule->GetNetworkManager()->GetRound() == -1);
        lUpdateData.mbIsRoundNumberOdd         = (mpNetworkModule->GetNetworkManager()->GetRound() & 1) != 0;
        lUpdateData.mbIsInCarSelect            = mpNetworkModule->GetGameStateToNetworkInterface()->GetIsInCarSelect();
        lUpdateData.meCameraStatus             =
            static_cast<ECameraStatus>(mpNetworkModule->GetNetworkManager()->GetCamera()->GetLocalCameraStatus());
        lUpdateData.meHeadsetStatus            = lpLocalPlayerMenuData->mu8HeadsetStatus;

        mUpdateMessageSend.PrepareForSend(&lUpdateData);
        mbHasResetCarTransformSinceLastUpdateSent = false;
        mbHasBeenSlammedSinceLastUpdateSent       = false;

        // [PC HARNESS, NOT X360] bounded witness: the first update sent in each 210-frame bucket of
        // the round clock (frames since start), with that clock value, the local car's slot and its
        // position. The receiving side samples on the SAME clock value (the sender's), so the two
        // logs name the same messages.
        {
            static s32 siLastBucket = -1;
            const s32 liBucket = static_cast<s32>(lUpdateData.mu16FramesSinceStart) / 210;
            if (liBucket != siLastBucket)
            {
                siLastBucket = liBucket;
                BrnNetHarnessPC::Witness("update-out", "sent since=%u frame=%u car=%d pos=(%.1f, %.1f, %.1f)",
                                         static_cast<u32>(lUpdateData.mu16FramesSinceStart),
                                         static_cast<u32>(lu16CurrentFrame), static_cast<s32>(leActiveRaceCarIndex),
                                         lUpdateData.mMatrix.wAxis.x, lUpdateData.mMatrix.wAxis.y,
                                         lUpdateData.mMatrix.wAxis.z);
            }
        }
    }

    // Drive this player's network car from a received update: the transform, velocities and
    // catch-up time (from the update's frames-since-start to ours), a snap on the first update
    // or when the sender asked for one, the crash state and the crasher (us, when the sender
    // says we crashed it), and the driver's controls (none while on the start line). Then the
    // sender's camera and headset status, the driver event for the vehicle, and its
    // car-select state for the game side (network event 48).
    // The console also prints the update to its debug stream when the module asks for it
    // (IsRecvUpdateMessageToBeShown); not reproduced, as for the send side.
    void BrnNetworkPlayer::TellNetworkCarAboutRecievedUpdateMessage(u16 lu16CurrentFrame,
                                                                    UpdateMessage* lpUpdateMessage)
    {
        BrnPhysics::Vehicle::BrnNetworkDriverControls lNetworkDriverControls;

        CGS_ASSERT(GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ
                   || GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_60HZ,
                   "GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ || GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_60HZ");
        CGS_ASSERT(GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ
                   || GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_60HZ,
                   "GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ || GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_60HZ");

        const EActiveRaceCarIndex leActiveRaceCarIndex = mpNetworkModule->GetActiveRaceCarIndex(GetPlayerID());
        if (leActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            return;
        }

        mu16LastAnyUpdateAppliedFrame = lpUpdateMessage->mu16Frame;

        BrnPhysics::Vehicle::VehicleDriverInputInterface* lpVehicleDriverInputInterface =
            mpNetworkModule->GetVehicleDriverInputInterface();
        CGS_ASSERT(lpVehicleDriverInputInterface, "lpVehicleDriverInputInterface");

        const UpdateData* lpUpdateData = lpUpdateMessage->GetUpdateData();
        lNetworkDriverControls.Clear();

        EActiveRaceCarIndex leCrasherRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        if (lpUpdateData->mbReceivingPlayerCrashedUs)
        {
            CGS_ASSERT(GetPlayerManager(), "mpPlayerManager");
            CgsNetwork::NetworkPlayerID lLocalPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
            if (GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID))
            {
                CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
                leCrasherRaceCarIndex = mpNetworkModule->GetActiveRaceCarIndex(lLocalPlayerID);
            }
        }

        const bool lbRemoteIs50Hz = GetRemoteConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ;
        const bool lbLocalIs50Hz  = GetLocalConsoleFrameRate() == CgsSystem::E_FRAMERATE_50HZ;

        lNetworkDriverControls.miVehicleID            = leActiveRaceCarIndex;
        lNetworkDriverControls.mTransform             = lpUpdateData->mMatrix;
        lNetworkDriverControls.mfCatchupTime          =
            CgsNetwork::GetTimeDiffWrapped16(lu16CurrentFrame, lpUpdateData->mu16FramesSinceStart,
                                             mpTimeManager->GetFrameWrapCountSinceStart(),
                                             lbLocalIs50Hz, lbRemoteIs50Hz);
        lNetworkDriverControls.meCrasherRaceCarIndex  = leCrasherRaceCarIndex;
        lNetworkDriverControls.mbSnap                 = lpUpdateData->mbSnap || mbFirstUpdateMessage;
        lNetworkDriverControls.mbCrash                = lpUpdateData->mbIsCrashing;
        lNetworkDriverControls.mLinearVelocity        = lpUpdateData->mLinearVelocity;
        lNetworkDriverControls.mAngularVelocity       = lpUpdateData->mAngularVelocity;
        lNetworkDriverControls.mbIsOnStartLine        = mbIsOnStartLine;

        if (mbIsOnStartLine)
        {
            lNetworkDriverControls.mfBrake        = 0.0f;
            lNetworkDriverControls.mbBoost        = false;
            lNetworkDriverControls.mfSteering     = 0.0f;
            lNetworkDriverControls.mfGas          = 0.0f;
            lNetworkDriverControls.mfRequestedGas = 0.0f;
        }
        else
        {
            lNetworkDriverControls.mbBoost        = lpUpdateData->mbIsBoosting;
            lNetworkDriverControls.mfGas          = lpUpdateData->mfAcceleration;
            lNetworkDriverControls.mfRequestedGas = lpUpdateData->mfAcceleration;
            lNetworkDriverControls.mfBrake        = lpUpdateData->mfBraking;
            lNetworkDriverControls.mfSteering     = lpUpdateData->mfSteering;
        }

        lNetworkDriverControls.mfForwardSteering          = 0.0f;
        lNetworkDriverControls.mfSpin                     = 0.0f;
        lNetworkDriverControls.mfHandBrake                = 0.0f;
        lNetworkDriverControls.mbReset                    = false;
        lNetworkDriverControls.mbForceDrift               = false;
        lNetworkDriverControls.mbBoostBounce              = false;
        lNetworkDriverControls.mbIsInvulnerableToVehicles = false;
        lNetworkDriverControls.mbIsInvulnerableToWorld    = false;

        PlayerMenuData* lpMenuData = static_cast<PlayerMenuData*>(GetPlayerManager()->GetMenuDataByID(GetPlayerID()));
        CGS_ASSERT(lpMenuData, "lpMenuData");
        lpMenuData->meCameraStatus = lpUpdateData->meCameraStatus;

        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager(), "mpNetworkModule->GetNetworkManager()");
        CgsNetwork::VoIPManager* lpVoipManager = mpNetworkModule->GetNetworkManager()->GetVoIPManager();
        CGS_ASSERT(lpVoipManager, "lpVoipManager");
        lpVoipManager->HandleReceivedHeadsetStatus(
            GetPlayerID(), static_cast<CgsNetwork::ENetworkHeadsetPlayerStatus>(lpUpdateData->meHeadsetStatus));

        lpVehicleDriverInputInterface->GetUpdateDriverQueue()->AddEvent(&lNetworkDriverControls,
                                                                        BrnPhysics::Vehicle::E_DRIVER_TYPE_NETWORK);

        // [PC HARNESS, NOT X360] bounded witness: the first update applied in each 210-frame bucket
        // of the SENDER's round clock (the value its "update-out sent" line carries), which car it
        // drives and where to.
        {
            static s32 siLastBucket = -1;
            const s32 liBucket = static_cast<s32>(lpUpdateData->mu16FramesSinceStart) / 210;
            if (liBucket != siLastBucket)
            {
                siLastBucket = liBucket;
                BrnNetHarnessPC::Witness("update-in", "applied since=%u from=%d car=%d snap=%d pos=(%.1f, %.1f, %.1f)",
                                         static_cast<u32>(lpUpdateData->mu16FramesSinceStart),
                                         static_cast<s32>(GetPlayerID()),
                                         static_cast<s32>(leActiveRaceCarIndex),
                                         lNetworkDriverControls.mbSnap ? 1 : 0,
                                         lpUpdateData->mMatrix.wAxis.x, lpUpdateData->mMatrix.wAxis.y,
                                         lpUpdateData->mMatrix.wAxis.z);
            }
        }

        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetNetworkEventQueue(), "mpNetworkModule->GetNetworkEventQueue()");
        BrnNetworkModuleIO::NetworkOutPlayerCarSelectStatus lCarSelectStatusEvent;
        lCarSelectStatusEvent.meActiveRaceCarIndex = leActiveRaceCarIndex;
        lCarSelectStatusEvent.mbInCarSelect        = lpUpdateData->mbIsInCarSelect;
        mpNetworkModule->GetNetworkEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lCarSelectStatusEvent),
            lCarSelectStatusEvent.GetEventType(), sizeof(lCarSelectStatusEvent));   // console record: 8 bytes

        mbFirstUpdateMessage = false;
    }
} // namespace BrnNetwork
