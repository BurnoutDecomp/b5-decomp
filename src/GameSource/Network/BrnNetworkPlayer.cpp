#include "types.hpp"

#include "GameSource/Network/BrnNetworkPlayer.h"
#include "GameSource/Network/BrnNetworkModule.h"          // BrnNetworkModule::GetNetworkManager / GetNetworkEventQueue
#include "GameSource/Network/BrnNetworkManager.h"         // BrnNetworkManager::GetLocalConsoleFrameRate
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h" // PlayerManager::GetPlayerByID
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"      // TimeManager::GetFrameCount / GetFramesSinceStart
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // VariableEventQueue<14000,16>::AddEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"

// ===================================================================================
// BrnNetwork::BrnNetworkPlayer -- stunt-score / stunt-multiplier reliable-message slice
// and the delivered-callback set.
//
// Bodied here:
//   SendStuntScoreUpdatedMessage, SendStuntMultiplierMessage,
//   ProcessReceivedStuntMultiplier, _StuntMultiplierMessageArrivedCallback,
//   the eleven *DeliveredCallback functions (one shared empty body).
//
// _StuntScoreUpdatedMessageArrivedCallback is declared only: its retrieval call lands
// on the folded CheckpointTriggeredMessage::Retrieve address, and the message's own
// StuntScoreUpdatedMessage::Retrieve has no declaration in its header yet.
// ===================================================================================

namespace BrnNetwork
{
    namespace
    {
        // The local-player network-frame id is the running frame counter folded into the
        // 16-bit reliable-message frame space (the whole frame count modulo 0xFFFF, the
        // invalid-frame sentinel, so the sentinel itself is never produced here).
        const u32 KU_NETWORK_FRAME_MODULUS = 0xFFFFu;

        // Network-event ids the game side consumes.
        const s32 KI_STUNT_MULTIPLIER_EVENT_TYPE = 34;

        // PAL/NTSC stunt rates differ by the 50:60 frame-rate ratio; a multiplier sampled on a
        // remote console at one rate is rescaled to the local rate when they disagree.
        const s32 KI_FRAMERATE_50HZ = CgsSystem::E_FRAMERATE_50HZ;   // 50
        const s32 KI_FRAMERATE_60HZ = CgsSystem::E_FRAMERATE_60HZ;   // 60

        inline u16 GetLocalNetworkFrameId(CgsNetwork::TimeManager* lpTimeManager)
        {
            return static_cast<u16>(lpTimeManager->GetFrameCount() % KU_NETWORK_FRAME_MODULUS);
        }
    }

    // Tail-calls StuntScoreUpdatedMessage::PrepareForSend on the send slot.
    void BrnNetworkPlayer::SendStuntScoreUpdatedMessage(s32 liStuntScore)
    {
        mStuntScoreUpdatedMessageSend.PrepareForSend(GetLocalNetworkFrameId(mpTimeManager),
                                                     liStuntScore);
    }

    // Samples the frame count + frames-since-start, then stamps the multiplier send slot.
    void BrnNetworkPlayer::SendStuntMultiplierMessage(s32 liStuntMultiplier)
    {
        const u16 lu16FrameId = GetLocalNetworkFrameId(mpTimeManager);
        const s32 liFramesSinceStart = mpTimeManager->GetFramesSinceStart();
        mStuntMultiplierMessageSend.PrepareForSend(lu16FrameId, liFramesSinceStart,
                                                   liStuntMultiplier);
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

        // 16-byte event payload: +0 s64 multiplier data, +8 s32 converted multiplier,
        // +12 s32 sending player id.
        struct StuntMultiplierEvent
        {
            s64 mi64MultiplierData;
            s32 miStuntMultiplier;
            s32 miNetworkPlayerID;
        } lEvent;
        lEvent.mi64MultiplierData = li64MultiplierData;
        lEvent.miStuntMultiplier  = liConvertedMultiplier;
        lEvent.miNetworkPlayerID  = lNetworkPlayerID;

        CGS_ASSERT(mpNetworkModule != 0, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetNetworkEventQueue() != 0,
                   "mpNetworkModule->GetNetworkEventQueue()");
        mpNetworkModule->GetNetworkEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent),
            KI_STUNT_MULTIPLIER_EVENT_TYPE, sizeof(lEvent));   // console record: 16 bytes
    }

    // Unpack the arrived stunt score and queue it, with the sender's id, as network event 47
    // on the owning player's module (the player is the registered user data).
    void BrnNetworkPlayer::_StuntScoreUpdatedMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                   CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                   void* lpUserData)
    {
        const s32 KI_STUNT_SCORE_UPDATED_EVENT_TYPE = 47;

        StuntScoreUpdatedMessage* lpStuntScoreUpdatedMessage =
            static_cast<StuntScoreUpdatedMessage*>(lpMessage);
        CGS_ASSERT(lpStuntScoreUpdatedMessage != 0, "lpStuntScoreUpdatedMessage");

        // 8-byte event record: +0 sending player id, +4 the score.
        struct StuntScoreUpdatedEvent
        {
            s32 miNetworkPlayerID;
            s32 miStuntScore;
        } lEvent;

        if (lpStuntScoreUpdatedMessage->Retrieve(&lEvent.miStuntScore))
        {
            BrnNetworkPlayer* lpPlayer = static_cast<BrnNetworkPlayer*>(lpUserData);
            CGS_ASSERT(lpPlayer != 0, "lpPlayer");

            lEvent.miNetworkPlayerID = lNetworkPlayerID;
            lpPlayer->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent),
                KI_STUNT_SCORE_UPDATED_EVENT_TYPE, sizeof(lEvent));   // console record: 8 bytes
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
} // namespace BrnNetwork
