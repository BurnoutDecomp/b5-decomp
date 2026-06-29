#include "types.hpp"

#include "GameSource/Network/BrnNetworkPlayer.h"
#include "GameSource/Network/BrnNetworkModule.h"          // BrnNetworkModule::GetNetworkManager / GetNetworkEventQueue
#include "GameSource/Network/BrnNetworkManager.h"         // BrnNetworkManager::GetLocalConsoleFrameRate
#include "GameSource/Network/Messages/BrnCheckpointTriggeredMessage.h"
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h" // PlayerManager::GetPlayerByID
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"      // TimeManager::GetFrameCount / GetFramesSinceStart
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // VariableEventQueue<14000,16>::AddEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"

// ===================================================================================
// BrnNetwork::BrnNetworkPlayer -- stunt-score / stunt-multiplier reliable-message slice.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   SendStuntScoreUpdatedMessage           @ 0x825826E8
//   SendStuntMultiplierMessage             @ 0x82582720
//   ProcessReceivedStuntMultiplier         @ 0x82593DF0
//   _StuntScoreUpdatedMessageArrivedCallback @ 0x825945C0
//   _StuntMultiplierMessageArrivedCallback   @ 0x82594668
//
// This message family is X360-only (absent from the DecFIGS DWARF). The class's full
// constructor + ~0x2230-byte layout (the buffered-update ring, the paired send/recv
// reliable-message subobjects, the latency window, and the remaining per-mode send helpers)
// live in this class's other reconstruction work and are NOT reproduced here.
// ===================================================================================

namespace BrnNetwork
{
    namespace
    {
        // The local-player network-frame id is the running frame counter folded into the
        // 16-bit reliable-message frame space (X360 takes the whole frame count modulo
        // KU16_INVALID_FRAME == 0xFFFF; see the magic-reciprocal divide-by-0xFFFF at e.g.
        // 0x82582700). 0xFFFF is the invalid-frame sentinel, so it is never produced here.
        const u32 KU_NETWORK_FRAME_MODULUS = 0xFFFFu;

        // Network-event ids the game side consumes (X360 immediates passed to AddEvent).
        const s32 KI_STUNT_MULTIPLIER_EVENT_TYPE   = 34;   // li r5, 0x22
        const s32 KI_STUNT_SCORE_UPDATED_EVENT_TYPE = 47;  // li r5, 0x2F

        // PAL/NTSC stunt rates differ by the 50:60 frame-rate ratio; a multiplier sampled on a
        // remote console at one rate is rescaled to the local rate when they disagree.
        const s32 KI_FRAMERATE_50HZ = CgsSystem::E_FRAMERATE_50HZ;   // 50
        const s32 KI_FRAMERATE_60HZ = CgsSystem::E_FRAMERATE_60HZ;   // 60

        // The X360 NetworkEventQueue accessor is typed against an incomplete forward; the
        // concrete queue is the 14000/16 variable-event queue the AddEvent call instantiates.
        typedef CgsModule::VariableEventQueue<14000, 16> NetworkEventQueueConcrete;

        inline NetworkEventQueueConcrete* AsConcreteQueue(BrnNetworkModuleIO::NetworkEventQueue* lpQueue)
        {
            return reinterpret_cast<NetworkEventQueueConcrete*>(lpQueue);
        }

        inline u16 GetLocalNetworkFrameId(CgsNetwork::TimeManager* lpTimeManager)
        {
            return static_cast<u16>(lpTimeManager->GetFrameCount() % KU_NETWORK_FRAME_MODULUS);
        }
    }

    // X360 0x825826E8 -- tail-calls StuntScoreUpdatedMessage::PrepareForSend on the send-slot.
    void BrnNetworkPlayer::SendStuntScoreUpdatedMessage(s32 liStuntScore)
    {
        mStuntScoreUpdatedMessageSend.PrepareForSend(GetLocalNetworkFrameId(mpTimeManager),
                                                     liStuntScore);
    }

    // X360 0x82582720 -- samples the frame count + frames-since-start, then stamps the
    // multiplier send-slot.
    void BrnNetworkPlayer::SendStuntMultiplierMessage(s32 liStuntMultiplier)
    {
        const u16 lu16FrameId = GetLocalNetworkFrameId(mpTimeManager);
        const s32 liFramesSinceStart = mpTimeManager->GetFramesSinceStart();
        mStuntMultiplierMessageSend.PrepareForSend(lu16FrameId, liFramesSinceStart,
                                                   liStuntMultiplier);
    }

    // X360 0x82593DF0 -- rescale the received multiplier from the sender's frame rate to ours
    // and queue it for the game side.
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

        // 16-byte event payload (matches the X360 stack record built at sp+var_60):
        //   +0  s64 multiplier data, +8 s32 converted multiplier, +12 s32 sending player id.
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
        AsConcreteQueue(mpNetworkModule->GetNetworkEventQueue())
            ->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                       KI_STUNT_MULTIPLIER_EVENT_TYPE, 16);
    }

    // X360 0x82594668 -- unpack the arrived multiplier message and forward it to the owning
    // BrnNetworkPlayer (handed in as the registered user-data).
    void BrnNetworkPlayer::_StuntMultiplierMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                 CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                 void* lpUserData)
    {
        CGS_ASSERT(lpMessage != 0, "lpStuntMultiplierMessage");

        s32 liStuntMultiplier = 0;
        s64 li64MultiplierData = 0;
        if (reinterpret_cast<StuntMultiplierMessage*>(lpMessage)->Retrieve(&liStuntMultiplier,
                                                                           &li64MultiplierData))
        {
            CGS_ASSERT(lpUserData != 0, "lpNetworkPlayer");
            static_cast<BrnNetworkPlayer*>(lpUserData)->ProcessReceivedStuntMultiplier(
                lNetworkPlayerID, liStuntMultiplier, li64MultiplierData);
        }
    }

    // X360 0x825945C0 -- unpack the arrived stunt-score-updated message (via the checkpoint
    // retrieval path) and queue a network event for the game side.
    void BrnNetworkPlayer::_StuntScoreUpdatedMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                                   CgsNetwork::NetworkPlayerID lNetworkPlayerID,
                                                                   void* lpUserData)
    {
        CGS_ASSERT(lpMessage != 0, "lpStuntScoreUpdatedMessage");

        s32 liStuntScore = 0;
        if (reinterpret_cast<CheckpointTriggeredMessage*>(lpMessage)->Retrieve(&liStuntScore))
        {
            CGS_ASSERT(lpUserData != 0, "lpPlayer");
            BrnNetworkPlayer* lpPlayer = static_cast<BrnNetworkPlayer*>(lpUserData);

            // 8-byte event payload: +0 s32 sending player id, +4 s32 retrieved score.
            struct StuntScoreUpdatedEvent
            {
                s32 miNetworkPlayerID;
                s32 miStuntScore;
            } lEvent;
            lEvent.miNetworkPlayerID = lNetworkPlayerID;
            lEvent.miStuntScore      = liStuntScore;

            AsConcreteQueue(lpPlayer->mpNetworkModule->GetNetworkEventQueue())
                ->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                           KI_STUNT_SCORE_UPDATED_EVENT_TYPE, 8);
        }
    }
} // namespace BrnNetwork
