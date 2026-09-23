// ============================================================================
// b5-decomp/src/GameSource/Network/Managers/BrnNetworkTrafficManager.cpp
// ============================================================================
// BrnNetwork::TrafficManager -- lifecycle, session hooks, restart-traffic handshake,
// hull-sync send/receive, crashing-traffic buffering and traffic-hash divergence
// detection. Layout and member map: BrnNetworkTrafficManager.h.
//
// Lines the console build writes to the shared network debug stream (the "NWTRAF:",
// "DIVERGENCE:" and "WARNING:" progress lines) have no sink in this tree; they are
// dropped at their call sites, which keep every value computation and call the
// console performs around them.
// ============================================================================

#include "GameSource/Network/Managers/BrnNetworkTrafficManager.h"

#include <cstring>                                                                        // std::memcpy / std::memset

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/Numeric/CgsBranchlessOperations.h"                       // CgsNumeric::Max
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"   // CgsDev::DebugInterface
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"                 // UInt16IsLarger(OrEqual)Wrapped
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h"    // GetSendingPlayerID
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                      // CgsNetwork::NetworkPlayer
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                      // CgsNetwork::PlayerManager
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"                           // CgsNetwork::TimeManager
#include "GameSource/Network/BrnNetworkModule.h"                                          // BrnNetworkModule accessors
#include "GameSource/Network/BrnNetworkManager.h"                                         // GetLocalConsoleFrameRate / GetPlayerManager
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h"  // traffic network IO
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h"        // BrnTraffic::KU_INVALID_HULL
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleNetworkIOInterfaces.h"      // crash network IO
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                                // NetworkOutRestartTrafficEvent
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                    // CgsCore::SnPrintf
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"          // Draw2DText
#include "GameShared/GameClasses/Network/StartTime/CgsStartTimeManager.h"                   // AreWeSyncingTime
#include "GameShared/GameClasses/Development/CgsStrStream.h"                             // CgsDev::StrStream (streamed assert)

namespace BrnNetwork
{
    namespace
    {
        // Message-type slots the traffic manager registers with every network player.
        const s32 KI_HULL_SYNC_MESSAGE_TYPE         = 13;
        const s32 KI_TRAFFIC_HASH_MESSAGE_TYPE      = 14;
        const s32 KI_CRASHING_TRAFFIC_MESSAGE_TYPE  = 16;
        const s32 KI_RESTART_TRAFFIC_MESSAGE_TYPE   = 28;

        // A restart is scheduled this many seconds ahead of the local frame (the frame-rate
        // enumerators are the literal Hz values, so the product is a frame count).
        const s32 KI_RESTART_TRAFFIC_DELAY_SECONDS  = 5;

        // Frames after a traffic reset during which messages are still compared against it.
        const u16 KU16_RESET_FRAME_LIFETIME         = 2000;

        // Per-hash decay of the "running in the past" amount.
        const f32 KF_IN_THE_PAST_DECAY              = 0.8f;

        // The debug overlay: text size, the three lines' screen positions and colours, and the
        // print buffer. The lag is reported (in seconds, one traffic update per tenth of a
        // second) only once it exceeds two updates.
        const f32          KF_TRAFFIC_DEBUG_TEXT_SIZE           = 22.0f;
        const s32          KI_TRAFFIC_DEBUG_TEXT_LENGTH         = 1024;
        const s32          KI_IN_THE_PAST_REPORT_THRESHOLD      = 2;
        const f32          KF_TRAFFIC_UPDATE_PERIOD             = 0.1f;
        const CgsDev::RGBA KU_TRAFFIC_DIVERGENCE_TEXT_COLOUR    = 0xFF0000E6u;
        const CgsDev::RGBA KU_IN_THE_PAST_TEXT_COLOUR           = 0xFF0064C8u;
        const CgsDev::RGBA KU_RESTART_PENDING_TEXT_COLOUR       = 0xFFC86400u;
        const Vector2      KV2_TRAFFIC_DIVERGENCE_TEXT_POSITION = { 251.3f, 158.0f, 0.0f, 0.0f };
        const Vector2      KV2_IN_THE_PAST_TEXT_POSITION        = { 586.9f, 158.0f, 0.0f, 0.0f };
        const Vector2      KV2_RESTART_PENDING_TEXT_POSITION    = { 536.85f, 133.0f, 0.0f, 0.0f };
    }

    // =========================================================================
    // BufferedMessage::operator=
    // A plain whole-object copy: the head (frame, owner, count) then all 24 records.
    // =========================================================================
    TrafficManager::BufferedMessage& TrafficManager::BufferedMessage::operator=(const BufferedMessage& lOther)
    {
        mu16FramesSinceRoundStart  = lOther.mu16FramesSinceRoundStart;
        mOwningNetworkPlayerID     = lOther.mOwningNetworkPlayerID;
        miCrashingTrafficDataCount = lOther.miCrashingTrafficDataCount;

        for (s32 liIndex = 0; liIndex < KI_MAX_CRASHING_TRAFFIC_IN_MESSAGE; ++liIndex)
        {
            maCrashingTrafficData[liIndex].mu16VehicleID = lOther.maCrashingTrafficData[liIndex].mu16VehicleID;
            maCrashingTrafficData[liIndex].mMatrix       = lOther.maCrashingTrafficData[liIndex].mMatrix;
        }
        return *this;
    }

    // =========================================================================
    // Constructor
    // Per slot the console installs the eight message vtables (carried by the messages'
    // explicit vtable member in this tree, not by a C++ constructor), marks the two
    // hull-sync arrays and the slot's own buffered-hull array unconstructed (-1), and
    // default-constructs the two 24-record CrashingTrafficData arrays (empty bodies).
    // =========================================================================
    TrafficManager::TrafficManager()
    {
        for (s32 liIndex = 0; liIndex < ::KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            maTrafficData[liIndex].mHullSyncMessageSend.mBufferedHullActivates.MarkUnconstructed();
            maTrafficData[liIndex].mHullSyncMessageRecv.mBufferedHullActivates.MarkUnconstructed();
            maTrafficData[liIndex].mBufferedHullActivates.MarkUnconstructed();
        }
    }

    // =========================================================================
    // Construct
    // =========================================================================
    void TrafficManager::Construct(BrnNetworkModule* lpNetworkModule,
                                   CgsNetwork::PlayerManager* lpPlayerManager,
                                   CgsNetwork::TimeManager* lpTimeManager)
    {
        mpNetworkModule = lpNetworkModule;
        mpPlayerManager = lpPlayerManager;
        mpTimeManager   = lpTimeManager;

        miNumMessagesBuffered   = 0;
        mu16LastBufferReadFrame = CgsNetwork::KU16_INVALID_FRAME;

        mbHasRoundStarted        = false;
        mbSuppressTrafficRestart = false;
        mbRestartNetworkTraffic  = false;
        // The console clears the single 64-bit field twice back to back.
        mPendingTrafficResetBitArray.UnSetAll();
        mbSuppressingHullSyncsUntilReset = false;

        ClearCrashingTraffic();
        ResetData();

        for (s32 liIndex = 0; liIndex < ::KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            TrafficSyncData& lrData = maTrafficData[liIndex];

            lrData.mPlayerID = CgsNetwork::KI_INVALID_PLAYER_ID;
            lrData.mHullSyncMessageSend.Construct();
            lrData.mHullSyncMessageRecv.Construct();
            lrData.mCrashingTrafficMessageSend.Construct();
            lrData.mCrashingTrafficMessageRecv.Construct();
            lrData.mRestartTrafficMessageSend.Construct();
            lrData.mRestartTrafficMessageRecv.Construct();
            lrData.mBufferedHullActivates.Construct();
            lrData.mTrafficHashMessageSend.Construct();
            lrData.mTrafficHashMessageRecv.Construct();
            lrData.mbIsThereCrashingTrafficToSend = false;
        }

        mbLastHashDataValid      = false;
        mbShowTrafficDivergence  = true;
        mbHasTrafficDiverged     = false;
        miInThePastAmount        = 0;
        mbIsThisMachineInThePast = false;
        maStoredTrafficHashes.Construct();
        muCurrentTrafficUpdateFrame = 0;
        mbSyncingTime = false;
    }

    // =========================================================================
    // Prepare -- registers the divergence overlay toggle with the debug menu.
    // =========================================================================
    bool TrafficManager::Prepare()
    {
        CgsDev::DebugInterface lDebugInterface;
        lDebugInterface.RegisterVariable(&mbShowTrafficDivergence, "Network", "Show Traffic Divergence");
        return true;
    }

    // =========================================================================
    // Release -- nothing to release (the console body is a shared `return true` leaf).
    // =========================================================================
    bool TrafficManager::Release()
    {
        return true;
    }

    // =========================================================================
    // Destruct -- the Construct reset without re-binding the collaborators.
    // =========================================================================
    void TrafficManager::Destruct()
    {
        miNumMessagesBuffered   = 0;
        mu16LastBufferReadFrame = CgsNetwork::KU16_INVALID_FRAME;
        mbHasRoundStarted        = false;
        mbSuppressTrafficRestart = false;
        // The console clears the single 64-bit field twice back to back.
        mPendingTrafficResetBitArray.UnSetAll();

        ClearCrashingTraffic();
        ResetData();

        for (s32 liIndex = 0; liIndex < ::KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            TrafficSyncData& lrData = maTrafficData[liIndex];

            lrData.mPlayerID = CgsNetwork::KI_INVALID_PLAYER_ID;
            lrData.mHullSyncMessageSend.Construct();
            lrData.mHullSyncMessageRecv.Construct();
            lrData.mCrashingTrafficMessageSend.Construct();
            lrData.mCrashingTrafficMessageRecv.Construct();
            lrData.mRestartTrafficMessageSend.Construct();
            lrData.mRestartTrafficMessageRecv.Construct();
            lrData.mBufferedHullActivates.Construct();
            lrData.mTrafficHashMessageSend.Construct();
            lrData.mTrafficHashMessageRecv.Construct();
            lrData.mbIsThereCrashingTrafficToSend = false;
        }

        mbSyncingTime = false;
    }

    // =========================================================================
    // ClearCrashingTraffic -- empties the buffered crashing-traffic messages.
    // =========================================================================
    void TrafficManager::ClearCrashingTraffic()
    {
        miNumMessagesBuffered   = 0;
        mu16LastBufferReadFrame = CgsNetwork::KU16_INVALID_FRAME;

        for (s32 liIndex = 0; liIndex < KI_MAX_BUFFERED_CRASHING_TRAFFIC_MESSAGES; ++liIndex)
        {
            BufferedMessage& lrMessage = maBufferedCrashingTrafficMessages[liIndex];
            lrMessage.mu16FramesSinceRoundStart  = CgsNetwork::KU16_INVALID_FRAME;
            lrMessage.mOwningNetworkPlayerID     = CgsNetwork::KI_INVALID_PLAYER_ID;
            lrMessage.miCrashingTrafficDataCount = -1;
            std::memset(lrMessage.maCrashingTrafficData, 0, sizeof(lrMessage.maCrashingTrafficData));
        }
    }

    // =========================================================================
    // ResetData -- forgets the pending restart and the stored traffic hashes.
    // =========================================================================
    void TrafficManager::ResetData()
    {
        mu16LastTrafficResetFrame   = CgsNetwork::KU16_INVALID_FRAME;
        mu16NumFramesSinceLastReset = CgsNetwork::KU16_INVALID_FRAME;
        mBufferedRestartTrafficMessage.mu16RestartFrame = CgsNetwork::KU16_INVALID_FRAME;
        miNumRestartMessagesBuffered = 0;
        std::memset(mBufferedRestartTrafficMessage.mau16ActiveHulls, 0xFF,
                    sizeof(mBufferedRestartTrafficMessage.mau16ActiveHulls));

        mbLastHashDataValid      = false;
        mbHasTrafficDiverged     = false;
        mbIsThisMachineInThePast = false;
        miInThePastAmount        = 0;
        maStoredTrafficHashes.Clear();
        muCurrentTrafficUpdateFrame = 0;
    }

    // =========================================================================
    // GetTrafficSyncDataEntry -- the slot owned by lPlayerID (asserts when none is).
    // =========================================================================
    TrafficManager::TrafficSyncData* TrafficManager::GetTrafficSyncDataEntry(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(lPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID,
                   "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        TrafficSyncData* lpDataEntry = nullptr;
        for (s32 liIndex = 0; liIndex < ::KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            if (maTrafficData[liIndex].mPlayerID == lPlayerID)
            {
                lpDataEntry = &maTrafficData[liIndex];
                break;
            }
        }

        // The console streams the player id after this prefix.
        CGS_ASSERT(lpDataEntry != nullptr, "No entry for this player: ");
        return lpDataEntry;
    }

    // =========================================================================
    // AddPlayer -- claims a free slot and registers the four traffic message types.
    // =========================================================================
    void TrafficManager::AddPlayer(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer == nullptr)
        {
            return;
        }

        TrafficSyncData* lpDataEntry = nullptr;
        for (s32 liIndex = 0; liIndex < ::KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            if (maTrafficData[liIndex].mPlayerID == CgsNetwork::KI_INVALID_PLAYER_ID)
            {
                lpDataEntry = &maTrafficData[liIndex];
                break;
            }
        }
        CGS_ASSERT(lpDataEntry != nullptr, "lpDataEntry");

        lpDataEntry->mPlayerID = lPlayerID;
        lpDataEntry->mBufferedHullActivates.Clear();
        lpDataEntry->mHullSyncMessageRecv.Construct();
        lpDataEntry->mHullSyncMessageSend.Construct();

        RegisterMessages(lpNetworkPlayer, lpDataEntry);
    }

    // =========================================================================
    // RemovePlayer -- unregisters the message types and frees the player's slot.
    // =========================================================================
    void TrafficManager::RemovePlayer(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer == nullptr)
        {
            return;
        }

        lpNetworkPlayer->UnRegisterMessageType(KI_HULL_SYNC_MESSAGE_TYPE);
        lpNetworkPlayer->UnRegisterMessageType(KI_CRASHING_TRAFFIC_MESSAGE_TYPE);
        lpNetworkPlayer->UnRegisterMessageType(KI_RESTART_TRAFFIC_MESSAGE_TYPE);
        lpNetworkPlayer->UnRegisterMessageType(KI_TRAFFIC_HASH_MESSAGE_TYPE);

        TrafficSyncData* lpDataEntry = GetTrafficSyncDataEntry(lPlayerID);
        lpDataEntry->mPlayerID = CgsNetwork::KI_INVALID_PLAYER_ID;
        lpDataEntry->mBufferedHullActivates.Clear();
        lpDataEntry->mHullSyncMessageRecv.Release();
        lpDataEntry->mHullSyncMessageSend.Release();
    }

    // =========================================================================
    // RegisterMessages -- hull sync and restart traffic are reliable (callbacks with this
    // manager as user data); crashing traffic and the traffic hash are unreliable.
    // =========================================================================
    void TrafficManager::RegisterMessages(CgsNetwork::NetworkPlayer* lpNetworkPlayer, TrafficSyncData* lpDataEntry)
    {
        lpNetworkPlayer->RegisterMessageType(KI_HULL_SYNC_MESSAGE_TYPE,
                                             sizeof(HullSyncMessage),
                                             &lpDataEntry->mHullSyncMessageSend,
                                             &lpDataEntry->mHullSyncMessageRecv,
                                             &TrafficManager::_HullSyncMessageArrivedCallback,
                                             &TrafficManager::_HullSyncMessageDeliveredCallback,
                                             this);
        lpNetworkPlayer->RegisterMessageType(KI_CRASHING_TRAFFIC_MESSAGE_TYPE,
                                             sizeof(CrashingTrafficMessage),
                                             &lpDataEntry->mCrashingTrafficMessageSend,
                                             &lpDataEntry->mCrashingTrafficMessageRecv,
                                             nullptr, nullptr, nullptr);
        lpNetworkPlayer->RegisterMessageType(KI_RESTART_TRAFFIC_MESSAGE_TYPE,
                                             sizeof(RestartTrafficMessage),
                                             &lpDataEntry->mRestartTrafficMessageSend,
                                             &lpDataEntry->mRestartTrafficMessageRecv,
                                             &TrafficManager::_RestartTrafficMessageArrivedCallback,
                                             &TrafficManager::_RestartTrafficMessageDeliveredCallback,
                                             this);
        lpNetworkPlayer->RegisterMessageType(KI_TRAFFIC_HASH_MESSAGE_TYPE,
                                             sizeof(TrafficHashMessage),
                                             &lpDataEntry->mTrafficHashMessageSend,
                                             &lpDataEntry->mTrafficHashMessageRecv,
                                             nullptr, nullptr, nullptr);
    }

    // =========================================================================
    // Disconnected -- ends the round and removes every remaining player.
    // =========================================================================
    void TrafficManager::Disconnected()
    {
        OnRoundFinish();

        for (s32 liIndex = 0; liIndex < ::KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            if (maTrafficData[liIndex].mPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID)
            {
                RemovePlayer(maTrafficData[liIndex].mPlayerID);
                CGS_ASSERT(maTrafficData[liIndex].mPlayerID == CgsNetwork::KI_INVALID_PLAYER_ID,
                           "maTrafficData[liIndex].mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID");
            }
        }
    }

    // =========================================================================
    // Session hooks
    // =========================================================================
    void TrafficManager::OnGameLaunching()
    {
        ClearCrashingTraffic();
        ResetData();
    }

    void TrafficManager::OnGameStart()
    {
        ClearCrashingTraffic();
        mu16LastTrafficResetFrame   = CgsNetwork::KU16_INVALID_FRAME;
        mu16NumFramesSinceLastReset = CgsNetwork::KU16_INVALID_FRAME;
    }

    void TrafficManager::OnRoundStart(bool lbSuppressHullSyncsUntilReset)
    {
        mbHasRoundStarted        = true;
        mbLastHashDataValid      = false;
        mbHasTrafficDiverged     = false;
        mbIsThisMachineInThePast = false;
        miInThePastAmount        = 0;
        maStoredTrafficHashes.Clear();
        muCurrentTrafficUpdateFrame      = 0;
        mbSuppressingHullSyncsUntilReset = false;

        ClearCrashingTraffic();

        if (lbSuppressHullSyncsUntilReset)
        {
            mbSuppressingHullSyncsUntilReset = true;
        }

        mu16LastTrafficResetFrame   = CgsNetwork::KU16_INVALID_FRAME;
        mu16NumFramesSinceLastReset = CgsNetwork::KU16_INVALID_FRAME;
        mPendingTrafficResetBitArray.UnSetAll();
    }

    void TrafficManager::OnRoundFinish()
    {
        mbHasRoundStarted = false;
        ClearCrashingTraffic();
        ResetData();
        mPendingTrafficResetBitArray.UnSetAll();
    }

    void TrafficManager::OnLeaveGame()
    {
        OnRoundFinish();
    }

    // =========================================================================
    // SuppressTrafficRestart
    // =========================================================================
    void TrafficManager::SuppressTrafficRestart(bool lbSuppress)
    {
        // The console logs "ONLINE TRAFFIC MANAGER: suppressing allowed to " + true/false.
        mbSuppressTrafficRestart = lbSuppress;
    }

    // =========================================================================
    // RestartNetworkTraffic -- flags lPlayerID's slot as waiting for a traffic reset.
    // =========================================================================
    void TrafficManager::RestartNetworkTraffic(NetworkPlayerID lPlayerID)
    {
        const s32 liTrafficDataIndex = GetIndexOfPlayerInTrafficData(lPlayerID);
        CGS_ASSERT(liTrafficDataIndex >= 0, "liTrafficDataIndex >= 0");
        CGS_ASSERT(liTrafficDataIndex < ::KI_MAX_NETWORK_PLAYERS, "liTrafficDataIndex < KI_MAX_NETWORK_PLAYERS");

        // BitArray::SetBit's inlined range check; the console streams the index and the
        // bit count after this prefix.
        CGS_ASSERT(static_cast<u32>(liTrafficDataIndex) < mPendingTrafficResetBitArray.GetCapacity(), "Index: ");
        mPendingTrafficResetBitArray.SetBit(static_cast<u32>(liTrafficDataIndex));
    }

    // =========================================================================
    // IsTrafficSystemResetPending
    // =========================================================================
    bool TrafficManager::IsTrafficSystemResetPending()
    {
        return miNumRestartMessagesBuffered > 0;
    }

    // =========================================================================
    // OnTrafficRestarted -- the traffic system has reset: drop hash and crash state.
    // =========================================================================
    void TrafficManager::OnTrafficRestarted()
    {
        mbLastHashDataValid      = false;
        mbHasTrafficDiverged     = false;
        mbIsThisMachineInThePast = false;
        miInThePastAmount        = 0;
        maStoredTrafficHashes.Clear();
        muCurrentTrafficUpdateFrame      = 0;
        mbSuppressingHullSyncsUntilReset = false;
        ClearCrashingTraffic();
    }

    // =========================================================================
    // UpdateRestartTraffic -- runs the restart handshake and ages the last reset frame.
    // =========================================================================
    void TrafficManager::UpdateRestartTraffic()
    {
        HandleSendingRestartTrafficMessages();
        ProcessBufferedRestartTrafficMessages();

        if (mu16LastTrafficResetFrame != CgsNetwork::KU16_INVALID_FRAME)
        {
            ++mu16NumFramesSinceLastReset;
            if (mu16NumFramesSinceLastReset >= KU16_RESET_FRAME_LIFETIME)
            {
                mu16LastTrafficResetFrame   = CgsNetwork::KU16_INVALID_FRAME;
                mu16NumFramesSinceLastReset = CgsNetwork::KU16_INVALID_FRAME;
            }
        }
    }

    // =========================================================================
    // HandleSendingRestartTrafficMessages
    // When a restart is wanted (flagged locally or requested for any slot) and the round
    // is running, snapshot the active traffic hulls per active race car and broadcast a
    // restart a few seconds ahead, then buffer the same restart locally.
    // =========================================================================
    void TrafficManager::HandleSendingRestartTrafficMessages()
    {
        mbRestartNetworkTraffic = mbRestartNetworkTraffic || !mPendingTrafficResetBitArray.IsZero();
        if (!mbRestartNetworkTraffic || !mbHasRoundStarted)
        {
            return;
        }

        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetTrafficOutputInterface() != nullptr,
                   "mpNetworkModule->GetTrafficOutputInterface()");
        const BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface* lpTrafficOutput =
            mpNetworkModule->GetTrafficOutputInterface();
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");
        CGS_ASSERT(mpTimeManager != nullptr, "mpTimeManager");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager() != nullptr, "mpNetworkModule->GetNetworkManager()");

        const u16 lu16FramesSinceStart = mpTimeManager->GetU16FrameCountSinceStart();
        const s32 liLocalFrameRate =
            static_cast<s32>(mpNetworkModule->GetNetworkManager()->GetLocalConsoleFrameRate());
        u16 lu16RestartFrame = static_cast<u16>(lu16FramesSinceStart + KI_RESTART_TRAFFIC_DELAY_SECONDS * liLocalFrameRate);
        if (lu16RestartFrame == CgsNetwork::KU16_INVALID_FRAME)
        {
            lu16RestartFrame = 0;
        }

        u16 lau16ActiveHulls[E_ACTIVE_RACE_CAR_INDEX_COUNT];
        std::memcpy(lau16ActiveHulls, lpTrafficOutput->GetActiveHulls(), sizeof(lau16ActiveHulls));

        NetworkPlayerID laPlayerIDsForActiveHulls[E_ACTIVE_RACE_CAR_INDEX_COUNT];
        for (EActiveRaceCarIndex leIndex = E_ACTIVE_RACE_CAR_INDEX_0; leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; leIndex++)
        {
            const NetworkPlayerID lPlayerID = mpNetworkModule->GetNetworkPlayerID(leIndex);
            if (lPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID)
            {
                // A car with a player but no active hull yet: try again next frame.
                if (lpTrafficOutput->GetActiveHulls()[leIndex] == BrnTraffic::KU_INVALID_HULL)
                {
                    return;
                }
            }
            laPlayerIDsForActiveHulls[leIndex] = lPlayerID;
        }

        NetworkPlayerID lPlayerID = CgsNetwork::KI_INVALID_PLAYER_ID;
        if (mpPlayerManager->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            do
            {
                CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
                if (lpNetworkPlayer != nullptr)
                {
                    RestartTrafficMessage* lpRestartTrafficMessage = static_cast<RestartTrafficMessage*>(
                        lpNetworkPlayer->GetRegisteredSendMessage(KI_RESTART_TRAFFIC_MESSAGE_TYPE));
                    CGS_ASSERT(!lpRestartTrafficMessage->IsMessageValid(), "!lpRestartTrafficMessage->IsMessageValid()");
                    lpRestartTrafficMessage->PrepareForSend(mpTimeManager->GetU16FrameCount(), lu16RestartFrame,
                                                            lau16ActiveHulls, laPlayerIDsForActiveHulls);
                }
            }
            while (mpPlayerManager->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED));
        }

        BufferedRestartMessage lRestartMessage;
        lRestartMessage.mu16RestartFrame = lu16RestartFrame;
        std::memcpy(lRestartMessage.mau16ActiveHulls, lpTrafficOutput->GetActiveHulls(),
                    sizeof(lRestartMessage.mau16ActiveHulls));
        BufferRestartTrafficMessage(lRestartMessage);

        mbRestartNetworkTraffic = false;
        mPendingTrafficResetBitArray.UnSetAll();
    }

    // =========================================================================
    // BufferRestartTrafficMessage -- keeps the latest-frame restart request.
    // =========================================================================
    void TrafficManager::BufferRestartTrafficMessage(BufferedRestartMessage lRestartMessage)
    {
        const u16 lu16RestartFrame = lRestartMessage.mu16RestartFrame;

        if (miNumRestartMessagesBuffered == 0
            || CgsNetwork::UInt16IsLargerWrapped(lu16RestartFrame, mBufferedRestartTrafficMessage.mu16RestartFrame))
        {
            // The console logs "NWTRAF: Setting reset message to fr=..., hulls=...".
            mBufferedRestartTrafficMessage = lRestartMessage;
            miNumRestartMessagesBuffered   = 1;
            return;
        }

        // The console logs "NWTRAF: IGNORING reset message fr=... in favour of reset message fr=...".
        CGS_ASSERT(mBufferedRestartTrafficMessage.mu16RestartFrame != CgsNetwork::KU16_INVALID_FRAME,
                   "mBufferedRestartTrafficMessage.mu16RestartFrame != KU16_INVALID_FRAME");

        if (lu16RestartFrame == mBufferedRestartTrafficMessage.mu16RestartFrame)
        {
            for (s32 liIndex = 0; liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liIndex)
            {
                CGS_ASSERT(mBufferedRestartTrafficMessage.mau16ActiveHulls[liIndex] == lRestartMessage.mau16ActiveHulls[liIndex],
                           "We've been told to restart the traffic twice on the same frame, but the hulls to activate are different");
            }
        }
    }

    // =========================================================================
    // IsTrafficRestartRequired -- hands out the buffered restart once its frame arrives.
    // =========================================================================
    bool TrafficManager::IsTrafficRestartRequired(u16* lpauOutActiveHulls, u16* lpuOutRestartFrame)
    {
        CGS_ASSERT(mpTimeManager != nullptr, "mpTimeManager");
        CGS_ASSERT(lpauOutActiveHulls != nullptr, "lpauOutActiveHulls");
        CGS_ASSERT(lpuOutRestartFrame != nullptr, "lpuOutRestartFrame");

        if (!mpTimeManager->HasStartFrameBeenSetValid() || !IsTrafficSystemResetPending())
        {
            return false;
        }

        const u16 lu16FramesSinceStart = mpTimeManager->GetU16FrameCountSinceStart();
        if (!CgsNetwork::UInt16IsLargerOrEqualWrapped(lu16FramesSinceStart, mBufferedRestartTrafficMessage.mu16RestartFrame))
        {
            return false;
        }

        // The console logs "NWTRAF: Telling traffic system to reset on frame: ... we wanted
        // to restart on ...".
        std::memcpy(lpauOutActiveHulls, mBufferedRestartTrafficMessage.mau16ActiveHulls,
                    sizeof(mBufferedRestartTrafficMessage.mau16ActiveHulls));
        *lpuOutRestartFrame = mBufferedRestartTrafficMessage.mu16RestartFrame;
        mBufferedRestartTrafficMessage.mu16RestartFrame = CgsNetwork::KU16_INVALID_FRAME;
        miNumRestartMessagesBuffered = 0;
        return true;
    }

    // =========================================================================
    // Delivery callbacks -- both only log a failed delivery ("WARNING: Fack Nack found in
    // <function>") on the console; there is no state to update.
    // =========================================================================
    void TrafficManager::_HullSyncMessageDeliveredCallback(bool lbDelivered, bool /*lbWasReliable*/,
                                                           CgsNetwork::SignalMessage* /*lpMessage*/,
                                                           NetworkPlayerID /*lPlayerID*/, void* /*lpUserData*/)
    {
        (void)lbDelivered;
    }

    void TrafficManager::_RestartTrafficMessageDeliveredCallback(bool lbDelivered, bool /*lbWasReliable*/,
                                                                 CgsNetwork::SignalMessage* /*lpMessage*/,
                                                                 NetworkPlayerID /*lPlayerID*/, void* /*lpUserData*/)
    {
        (void)lbDelivered;
    }

    // =========================================================================
    // _HullSyncMessageArrivedCallback -- forwards a peer's hull activations to the local
    // traffic system, ditching ones sent before our last traffic reset.
    // =========================================================================
    void TrafficManager::_HullSyncMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                         NetworkPlayerID lPlayerID, void* lpUserData)
    {
        TrafficManager*   lpTrafficManager  = static_cast<TrafficManager*>(lpUserData);
        HullSyncMessage*  lpHullSyncMessage = static_cast<HullSyncMessage*>(lpMessage);
        BrnNetworkModule* lpNetworkModule   = lpTrafficManager->mpNetworkModule;

        BufferedHullsToActivate lBufferedHullActivates;
        lBufferedHullActivates.MarkUnconstructed();

        CGS_ASSERT(lpHullSyncMessage != nullptr, "lpHullSyncMessage");
        CGS_ASSERT(lpNetworkModule != nullptr, "lpNetworkModule");

        lBufferedHullActivates.Construct();
        const bool lbRetrieved = lpHullSyncMessage->Retrieve(&lBufferedHullActivates);
        CGS_ASSERT(lbRetrieved, "lpHullSyncMessage->Retrieve(&lBufferedHullActivates)");

        if (lpTrafficManager->mbSyncingTime
            || lpTrafficManager->IsTrafficSystemResetPending()
            || lpTrafficManager->mbSuppressingHullSyncsUntilReset)
        {
            return;
        }

        for (u32 luIndex = 0; luIndex < lBufferedHullActivates.GetLength(); ++luIndex)
        {
            const HullToActivateInfo* lpActivate = &lBufferedHullActivates.Ge(luIndex);
            CGS_ASSERT(lpActivate->muHull < BrnTraffic::KU_MAX_HULLS, "lpActivate->muHull < BrnTraffic::KU_MAX_HULLS");

            if (lpTrafficManager->IsMessageFromBeforeTrafficReset(lpActivate->muFramesSinceStart, lPlayerID))
            {
                // The console logs "NWTRAF: Ditching hull sync message from before reset: arc=..."
                // with the sender's active race car index, which it looks up for the line.
                const EActiveRaceCarIndex leDitchedActiveRaceCarIndex =
                    lpNetworkModule->GetActiveRaceCarIndex(lpHullSyncMessage->GetSendingPlayerID());
                (void)leDitchedActiveRaceCarIndex;
                continue;
            }

            const EActiveRaceCarIndex lePlayerActiveRaceCarIndex =
                lpNetworkModule->GetActiveRaceCarIndex(lpHullSyncMessage->GetSendingPlayerID());
            if (lePlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
            {
                // The console logs "WARNING: We are going to lose a Hull Activate Message.".
                continue;
            }

            CGS_ASSERT(lePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                       "lePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

            lpNetworkModule->GetTrafficInputInterface()->ActivateHull(lePlayerActiveRaceCarIndex,
                                                                      lpActivate->muHull,
                                                                      lpActivate->muTrafUpdateToActivate);
        }
    }

    // =========================================================================
    // _RestartTrafficMessageArrivedCallback -- maps a peer's restart (hulls per peer player)
    // onto the local active-race-car slots and buffers it in local frames.
    // =========================================================================
    void TrafficManager::_RestartTrafficMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                               NetworkPlayerID lPlayerID, void* lpUserData)
    {
        TrafficManager*        lpTrafficManager        = static_cast<TrafficManager*>(lpUserData);
        RestartTrafficMessage* lpRestartTrafficMessage = static_cast<RestartTrafficMessage*>(lpMessage);

        CGS_ASSERT(lpTrafficManager != nullptr, "lpTrafficManager");
        CGS_ASSERT(lpRestartTrafficMessage != nullptr, "lpRestartTrafficMessage");
        CGS_ASSERT(lpTrafficManager->mpPlayerManager != nullptr, "lpTrafficManager->mpPlayerManager");
        CGS_ASSERT(lpTrafficManager->mpNetworkModule != nullptr, "lpTrafficManager->mpNetworkModule");
        CGS_ASSERT(lpTrafficManager->mpNetworkModule->GetNetworkManager() != nullptr,
                   "lpTrafficManager->mpNetworkModule->GetNetworkManager()");

        u16             lu16RestartFrame;
        u16             lau16ActiveTrafficHulls[E_ACTIVE_RACE_CAR_INDEX_COUNT];
        NetworkPlayerID laPlayerIDsForActiveHulls[E_ACTIVE_RACE_CAR_INDEX_COUNT];
        lpRestartTrafficMessage->Retrieve(&lu16RestartFrame, lau16ActiveTrafficHulls, laPlayerIDsForActiveHulls);

        BufferedRestartMessage lRestartMessage;
        for (EActiveRaceCarIndex leIndex = E_ACTIVE_RACE_CAR_INDEX_0; leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; leIndex++)
        {
            lRestartMessage.mau16ActiveHulls[leIndex] = BrnTraffic::KU_INVALID_HULL;
        }

        for (EActiveRaceCarIndex leIndex = E_ACTIVE_RACE_CAR_INDEX_0; leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; leIndex++)
        {
            if (laPlayerIDsForActiveHulls[leIndex] != CgsNetwork::KI_INVALID_PLAYER_ID)
            {
                const EActiveRaceCarIndex leLocalIndex =
                    lpTrafficManager->mpNetworkModule->GetActiveRaceCarIndex(laPlayerIDsForActiveHulls[leIndex]);
                lRestartMessage.mau16ActiveHulls[leLocalIndex] = lau16ActiveTrafficHulls[leIndex];
            }
        }

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = lpTrafficManager->mpPlayerManager->GetPlayerByID(lPlayerID);
        const CgsSystem::EFrameRate leRemoteConsoleFrameRate = lpNetworkPlayer->GetRemoteConsoleFrameRate();
        const CgsSystem::EFrameRate leLocalConsoleFrameRate =
            lpTrafficManager->mpNetworkModule->GetNetworkManager()->GetLocalConsoleFrameRate();
        lRestartMessage.mu16RestartFrame = lpTrafficManager->ConvertReceivedMessageFrameToLocalFrame(
            lu16RestartFrame, leLocalConsoleFrameRate, leRemoteConsoleFrameRate);

        lpTrafficManager->BufferRestartTrafficMessage(lRestartMessage);
    }

    // =========================================================================
    // SendHullSyncMessages -- each finalised player whose round-robin turn it is gets the
    // hull activations buffered for it since the last send.
    // =========================================================================
    void TrafficManager::SendHullSyncMessages()
    {
        NetworkPlayerID lPlayerID = CgsNetwork::KI_INVALID_PLAYER_ID;
        if (!mpPlayerManager->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            return;
        }

        do
        {
            if (mpPlayerManager->IsPlayerTurnToSendRoundRobinMessage(lPlayerID, true, 0))
            {
                TrafficSyncData& lrData = maTrafficData[GetIndexOfPlayerInTrafficData(lPlayerID)];
                if (lrData.mBufferedHullActivates.GetLength() > 0)
                {
                    lrData.mHullSyncMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount(),
                                                               &lrData.mBufferedHullActivates);
                    lrData.mBufferedHullActivates.Clear();
                }
            }
        }
        while (mpPlayerManager->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED));
    }

    // =========================================================================
    // HandleHullSyncMessage -- queues a local hull activation for every finalised peer.
    // =========================================================================
    void TrafficManager::HandleHullSyncMessage(EActiveRaceCarIndex /*leActiveRaceCarIndex*/,
                                               u16 lu16TrafficUpdateToActivate, s32 liHull)
    {
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");

        for (s32 liIndex = 0; liIndex < ::KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            TrafficSyncData& lrData = maTrafficData[liIndex];
            if (lrData.mPlayerID == CgsNetwork::KI_INVALID_PLAYER_ID
                || !mpPlayerManager->IsPlayerFinalised(lrData.mPlayerID))
            {
                continue;
            }

            HullToActivateInfo* lpBufferedMessage = lrData.mBufferedHullActivates.Grow();
            CGS_ASSERT(lpBufferedMessage != nullptr, "lpBufferedMessage");
            lpBufferedMessage->muHull                 = static_cast<u16>(liHull);
            lpBufferedMessage->muTrafUpdateToActivate = lu16TrafficUpdateToActivate;
            lpBufferedMessage->muFramesSinceStart     = mpTimeManager->GetU16FrameCountSinceStart();
        }
    }

    // =========================================================================
    // GetCrashingTrafficData -- copies the locally owned crashing traffic out of the crash
    // module's network output queue.
    // =========================================================================
    bool TrafficManager::GetCrashingTrafficData(CrashingTrafficData* lpaCrashingTrafficData, s32* lpiCount)
    {
        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");

        const BrnWorld::CrashIO::NetworkOutputInterface::CrashingTrafficUpdateQueue* lpOwnedCrashingTrafficQueue =
            mpNetworkModule->GetCrashOutputInterface()->GetCrashingTrafficUpdateQueue();
        CGS_ASSERT(lpOwnedCrashingTrafficQueue->GetLength() < KI_MAX_CRASHING_TRAFFIC_IN_MESSAGE,
                   "lpOwnedCrashingTrafficQueue->GetLength() < KI_MAX_CRASHING_TRAFFIC_IN_MESSAGE");

        const s32 liCount = lpOwnedCrashingTrafficQueue->GetLength();
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            const BrnWorld::CrashIO::CrashingTrafficUpdateEvent& lrEvent = lpOwnedCrashingTrafficQueue->GetEvent(liIndex);
            lpaCrashingTrafficData[liIndex].mMatrix       = lrEvent.mTransform;
            lpaCrashingTrafficData[liIndex].mu16VehicleID = lrEvent.muVehicleId;
        }

        *lpiCount = liCount;
        return true;
    }

    // =========================================================================
    // SendCrashingTrafficMessages -- each remote player whose round-robin turn it is gets
    // the locally owned crashing traffic; one empty message is sent after traffic stops.
    // =========================================================================
    void TrafficManager::SendCrashingTrafficMessages(bool lbInGame)
    {
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");

        const u16 lu16Frame            = mpTimeManager->GetU16FrameCount();
        const u16 lu16FramesSinceStart = mpTimeManager->GetU16FrameCountSinceStart();
        // Queried and unused by the console body.
        (void)mpPlayerManager->GetNumberNetworkPlayers(CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED);

        for (s32 liIndex = 0; liIndex < ::KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            TrafficSyncData& lrData    = maTrafficData[liIndex];
            const NetworkPlayerID lPlayerID = lrData.mPlayerID;
            if (lPlayerID == CgsNetwork::KI_INVALID_PLAYER_ID)
            {
                continue;
            }

            NetworkPlayerID lLocalNetworkPlayerID = CgsNetwork::KI_INVALID_PLAYER_ID;
            const bool lbHaveLocalPlayer =
                mpNetworkModule->GetNetworkManager()->GetPlayerManager()->GetNextLocalPlayerID(&lLocalNetworkPlayerID);
            CGS_ASSERT(lbHaveLocalPlayer,
                       "mpNetworkModule->GetNetworkManager()->GetPlayerManager()->GetNextLocalPlayerID( &lLocalNetworkPlayerID )");
            CGS_ASSERT(lPlayerID != lLocalNetworkPlayerID, "lPlayerID != lLocalNetworkPlayerID");

            if (!mpPlayerManager->IsPlayerTurnToSendRoundRobinMessage(lPlayerID, lbInGame, 0))
            {
                continue;
            }

            CrashingTrafficData laCrashingTrafficData[KI_MAX_CRASHING_TRAFFIC_IN_MESSAGE];
            s32 liCount;
            GetCrashingTrafficData(laCrashingTrafficData, &liCount);

            if (liCount > 0)
            {
                lrData.mbIsThereCrashingTrafficToSend = true;
            }

            if (lrData.mbIsThereCrashingTrafficToSend)
            {
                lrData.mCrashingTrafficMessageSend.PrepareForSend(lu16Frame, lu16FramesSinceStart,
                                                                  liCount, laCrashingTrafficData);
                if (liCount == 0)
                {
                    lrData.mbIsThereCrashingTrafficToSend = false;
                }
            }
        }
    }

    // =========================================================================
    // StoreBufferedMessage -- queues a received crashing-traffic message (in local frames)
    // unless it is older than the last frame already consumed.
    // =========================================================================
    void TrafficManager::StoreBufferedMessage(NetworkPlayerID lNetworkPlayerID, CrashingTrafficMessage* lpMsg,
                                              CgsSystem::EFrameRate leLocalConsoleFrameRate,
                                              CgsSystem::EFrameRate leRemoteConsoleFrameRate)
    {
        CGS_ASSERT(lpMsg != nullptr, "lpMsg");

        const u16 lu16MessageFrameToCompare = ConvertReceivedMessageFrameToLocalFrame(
            lpMsg->GetFramesSinceStart(), leLocalConsoleFrameRate, leRemoteConsoleFrameRate);

        if (mu16LastBufferReadFrame != CgsNetwork::KU16_INVALID_FRAME)
        {
            CGS_ASSERT(lu16MessageFrameToCompare != CgsNetwork::KU16_INVALID_FRAME,
                       "lu16MessageFrameToCompare != CgsNetwork::KU16_INVALID_FRAME");
            if (!CgsNetwork::UInt16IsLargerWrapped(lu16MessageFrameToCompare, mu16LastBufferReadFrame))
            {
                lpMsg->SetMessageInvalid();
                return;
            }
        }

        BufferedMessage& lrBufferedMessage = maBufferedCrashingTrafficMessages[miNumMessagesBuffered];
        lpMsg->Retrieve(&lrBufferedMessage.miCrashingTrafficDataCount, lrBufferedMessage.maCrashingTrafficData);
        lrBufferedMessage.mOwningNetworkPlayerID     = lNetworkPlayerID;
        lrBufferedMessage.mu16FramesSinceRoundStart  = lu16MessageFrameToCompare;

        CGS_ASSERT(lNetworkPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID,
                   "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        NetworkPlayerID lLocalNetworkPlayerID = CgsNetwork::KI_INVALID_PLAYER_ID;
        const bool lbHaveLocalPlayer =
            mpNetworkModule->GetNetworkManager()->GetPlayerManager()->GetNextLocalPlayerID(&lLocalNetworkPlayerID);
        CGS_ASSERT(lbHaveLocalPlayer,
                   "mpNetworkModule->GetNetworkManager()->GetPlayerManager()->GetNextLocalPlayerID( &lLocalNetworkPlayerID )");
        CGS_ASSERT(lNetworkPlayerID != lLocalNetworkPlayerID, "lNetworkPlayerID != lLocalNetworkPlayerID");

        ++miNumMessagesBuffered;
        if (miNumMessagesBuffered >= KI_MAX_BUFFERED_CRASHING_TRAFFIC_MESSAGES)
        {
            // The console logs "WARNING: Filled up crashing traffic message data buffer".
            miNumMessagesBuffered = KI_MAX_BUFFERED_CRASHING_TRAFFIC_MESSAGES - 1;
        }
    }

    // =========================================================================
    // RetrieveBufferedMessage -- the first buffered message due at or before lu16Frame.
    // =========================================================================
    TrafficManager::BufferedMessage* TrafficManager::RetrieveBufferedMessage(u16 lu16Frame)
    {
        for (s32 liIndex = 0; liIndex < miNumMessagesBuffered; ++liIndex)
        {
            BufferedMessage& lrMessage = maBufferedCrashingTrafficMessages[liIndex];
            if (lrMessage.mOwningNetworkPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID
                && CgsNetwork::UInt16IsLargerOrEqualWrapped(lu16Frame, lrMessage.mu16FramesSinceRoundStart))
            {
                return &lrMessage;
            }
        }
        return nullptr;
    }

    // =========================================================================
    // IsMessageFromBeforeTrafficReset -- true when lu16MessageFrame (in lPlayerID's frames)
    // precedes the last local traffic reset.
    // =========================================================================
    bool TrafficManager::IsMessageFromBeforeTrafficReset(u16 lu16MessageFrame, NetworkPlayerID lPlayerID)
    {
        if (mu16LastTrafficResetFrame == CgsNetwork::KU16_INVALID_FRAME)
        {
            return false;
        }

        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");
        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        CGS_ASSERT(lpNetworkPlayer != nullptr, "lpNetworkPlayer");

        const CgsSystem::EFrameRate leRemoteConsoleFrameRate = lpNetworkPlayer->GetRemoteConsoleFrameRate();
        const CgsSystem::EFrameRate leLocalConsoleFrameRate  =
            mpNetworkModule->GetNetworkManager()->GetLocalConsoleFrameRate();
        const u16 lu16ResetFrameInSenderFrames = ConvertLocalFrameToReceivedMessageFrame(
            mu16LastTrafficResetFrame, leLocalConsoleFrameRate, leRemoteConsoleFrameRate);

        return CgsNetwork::UInt16IsLargerWrapped(lu16ResetFrameInSenderFrames, lu16MessageFrame);
    }

    // =========================================================================
    // RemoveBufferedMessage -- swap-removes with the last buffered message.
    // =========================================================================
    void TrafficManager::RemoveBufferedMessage(BufferedMessage* lpBufferedMessage)
    {
        CGS_ASSERT(miNumMessagesBuffered > 0, "miNumMessagesBuffered > 0");

        *lpBufferedMessage = maBufferedCrashingTrafficMessages[miNumMessagesBuffered - 1];

        BufferedMessage& lrLast = maBufferedCrashingTrafficMessages[miNumMessagesBuffered - 1];
        lrLast.miCrashingTrafficDataCount = -1;
        lrLast.mOwningNetworkPlayerID     = CgsNetwork::KI_INVALID_PLAYER_ID;
        lrLast.mu16FramesSinceRoundStart  = CgsNetwork::KU16_INVALID_FRAME;

        --miNumMessagesBuffered;
    }

    // =========================================================================
    // DeleteOutOfDateBufferedMesssages -- drops every buffered message older than lu16Frame.
    // =========================================================================
    void TrafficManager::DeleteOutOfDateBufferedMesssages(u16 lu16Frame)
    {
        if (lu16Frame == CgsNetwork::KU16_INVALID_FRAME)
        {
            return;
        }

        for (s32 liIndex = 0; liIndex < miNumMessagesBuffered; ++liIndex)
        {
            BufferedMessage* lpMessage = &maBufferedCrashingTrafficMessages[liIndex];
            if (lpMessage->mOwningNetworkPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID
                && !CgsNetwork::UInt16IsLargerOrEqualWrapped(lpMessage->mu16FramesSinceRoundStart, lu16Frame))
            {
                RemoveBufferedMessage(lpMessage);
                --liIndex;
            }
        }
    }

    // =========================================================================
    // UpdateTrafficHashing -- stores this frame's traffic hash, exchanges hashes with the
    // peers, and tells the traffic system whether it has diverged.
    // =========================================================================
    void TrafficManager::UpdateTrafficHashing(bool lbInGame)
    {
        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");

        if (!IsTrafficSystemResetPending() && !mbSuppressingHullSyncsUntilReset)
        {
            const BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface* lpTrafficOutput =
                mpNetworkModule->GetTrafficOutputInterface();
            if (lpTrafficOutput->HasHashBeenSet())
            {
                const u16 lu16TrafficHash = lpTrafficOutput->GetDataHash();
                StoreTrafficHash(static_cast<u16>(lpTrafficOutput->GetUpdateFrameForHash()), lu16TrafficHash);
            }

            SendTrafficHashingMessage(lbInGame);
            ReceiveTrafficHashingMessages();
        }

        const bool lbHasTrafficDiverged = mbHasTrafficDiverged;
        mpNetworkModule->GetTrafficInputInterface()->SetDiverged(lbHasTrafficDiverged);
    }

    // =========================================================================
    // StoreTrafficHash
    // =========================================================================
    void TrafficManager::StoreTrafficHash(u16 lu16Update10HzFrame, u16 lu16TrafficHash)
    {
        TrafficHash lTrafficHash;
        lTrafficHash.muUpdate10HzFrame = lu16Update10HzFrame;
        lTrafficHash.muTrafficHash     = lu16TrafficHash;
        maStoredTrafficHashes.Push(&lTrafficHash);

        muCurrentTrafficUpdateFrame = lu16Update10HzFrame;
        muLastHashUpdate10HzFrame   = lu16Update10HzFrame;
        muLastTrafficHash           = lu16TrafficHash;
        mbLastHashDataValid         = true;
    }

    // =========================================================================
    // SendTrafficHashingMessage -- sends our latest traffic hash to every finalised peer
    // whose round-robin turn it is.
    // =========================================================================
    void TrafficManager::SendTrafficHashingMessage(bool /*lbInGame*/)
    {
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");

        if (!mbLastHashDataValid)
        {
            return;
        }

        const s32 liNumNetworkPlayers =
            mpPlayerManager->GetNumberNetworkPlayers(CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED);
        if (liNumNetworkPlayers <= 0)
        {
            return;
        }

        const u16 lu16Frame = mpTimeManager->GetU16FrameCount();
        for (s32 liIndex = 0; liIndex < liNumNetworkPlayers; ++liIndex)
        {
            TrafficSyncData& lrData = maTrafficData[liIndex];
            if (lrData.mPlayerID == CgsNetwork::KI_INVALID_PLAYER_ID
                || !mpPlayerManager->IsPlayerFinalised(lrData.mPlayerID)
                || !mpPlayerManager->IsPlayerTurnToSendRoundRobinMessage(lrData.mPlayerID, false, 0))
            {
                continue;
            }

            lrData.mTrafficHashMessageSend.PrepareForSend(lu16Frame, mpTimeManager->GetU16FrameCountSinceStart(),
                                                          muLastHashUpdate10HzFrame, muLastTrafficHash);
        }
    }

    // =========================================================================
    // ReceiveTrafficHashingMessages -- compares each peer's hash with ours for the same
    // update frame; a hash from our future means this machine is running behind.
    // =========================================================================
    void TrafficManager::ReceiveTrafficHashingMessages()
    {
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");

        for (s32 liIndex = 0; liIndex < ::KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            TrafficSyncData& lrData = maTrafficData[liIndex];

            u16 lu16SyncedFrameSinceStart;
            u16 lu16Update10HzFrame;
            u16 lu16TheirHash;
            const bool lbReceived = lrData.mTrafficHashMessageRecv.Retrieve(&lu16SyncedFrameSinceStart,
                                                                            &lu16Update10HzFrame,
                                                                            &lu16TheirHash);

            if (mbSyncingTime
                || !lbReceived
                || liIndex >= mpPlayerManager->GetNumberNetworkPlayers(CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED)
                || IsMessageFromBeforeTrafficReset(lu16SyncedFrameSinceStart, lrData.mPlayerID)
                || IsTrafficSystemResetPending()
                || mbSuppressingHullSyncsUntilReset
                || mbHasTrafficDiverged)
            {
                continue;
            }

            const s32 liFrameDifference =
                CalcWrappedUpdateFrameDifference(muCurrentTrafficUpdateFrame, lu16Update10HzFrame);

            if (liFrameDifference < 0)
            {
                // The console logs "DIVERGENCE: Warning: Received traffic hash from the future on
                // our frame ... with update frame ..., when our update frame is ...".
                mbIsThisMachineInThePast = true;
                miInThePastAmount = static_cast<s32>(static_cast<f32>(miInThePastAmount) * KF_IN_THE_PAST_DECAY);
                miInThePastAmount = CgsNumeric::Max(miInThePastAmount, -liFrameDifference);
            }
            else
            {
                u16 lu16OurHash;
                if (FindHashForFrame(lu16Update10HzFrame, &lu16OurHash) && lu16OurHash != lu16TheirHash)
                {
                    // The console logs "DIVERGENCE: Traffic diverged on frame ... with update
                    // frame ..., our hash=..., their hash=...".
                    mbHasTrafficDiverged = true;
                }
            }
        }
    }

    // =========================================================================
    // FindHashForFrame -- our stored hash for lu16Update10HzFrame, if still in the ring.
    // =========================================================================
    bool TrafficManager::FindHashForFrame(u16 lu16Update10HzFrame, u16* lpu16OutHash) const
    {
        CGS_ASSERT(lpu16OutHash != nullptr, "lpu16OutHash");

        for (s32 liIndex = 0; liIndex < maStoredTrafficHashes.GetLength(); ++liIndex)
        {
            if (maStoredTrafficHashes[static_cast<u32>(liIndex)].muUpdate10HzFrame == lu16Update10HzFrame)
            {
                *lpu16OutHash = maStoredTrafficHashes[static_cast<u32>(liIndex)].muTrafficHash;
                return true;
            }
        }
        return false;
    }

    // =========================================================================
    // Update -- the per-frame tick. While the start time is being synced or a traffic
    // restart is pending, every crash / hull / hash record is dropped; otherwise the hull
    // syncs, crashing traffic and traffic hashes are exchanged. The debug overlay then
    // reports divergence, a lagging simulation and a pending restart.
    // =========================================================================
    void TrafficManager::Update(bool lbInGame)
    {
        mbSyncingTime = mpNetworkModule->GetNetworkManager()->GetStartTimeManager()->AreWeSyncingTime();
        UpdateRestartTraffic();

        if (mbSyncingTime || IsTrafficSystemResetPending())
        {
            ClearCrashingTraffic();
            for (s32 liIndex = 0; liIndex < ::KI_MAX_NETWORK_PLAYERS; ++liIndex)
            {
                maTrafficData[liIndex].mBufferedHullActivates.Clear();
            }
            mbLastHashDataValid = false;
            maStoredTrafficHashes.Clear();
        }
        else
        {
            UpdateHullSync();
            if (mbHasRoundStarted)
            {
                SendCrashingTrafficMessages(lbInGame);
                ReceiveCrashingTrafficMessages();
            }
            UpdateTrafficHashing(lbInGame);
        }

        if (mbShowTrafficDivergence && !IsTrafficSystemResetPending() && !mbSuppressingHullSyncsUntilReset)
        {
            if (mbHasTrafficDiverged)
            {
                CgsDev::DebugInterface lDebugInterface;
                lDebugInterface.Get2dRender().Draw2DText("Traffic Divergence", KV2_TRAFFIC_DIVERGENCE_TEXT_POSITION,
                                                         KF_TRAFFIC_DEBUG_TEXT_SIZE, KU_TRAFFIC_DIVERGENCE_TEXT_COLOUR);
            }

            if (mbIsThisMachineInThePast && miInThePastAmount > KI_IN_THE_PAST_REPORT_THRESHOLD)
            {
                char lacText[KI_TRAFFIC_DEBUG_TEXT_LENGTH];
                CgsCore::SnPrintf(lacText, KI_TRAFFIC_DEBUG_TEXT_LENGTH, "Traffic running %.01fs in the past",
                                  static_cast<f32>(miInThePastAmount) * KF_TRAFFIC_UPDATE_PERIOD);

                CgsDev::DebugInterface lDebugInterface;
                lDebugInterface.Get2dRender().Draw2DText(lacText, KV2_IN_THE_PAST_TEXT_POSITION,
                                                         KF_TRAFFIC_DEBUG_TEXT_SIZE, KU_IN_THE_PAST_TEXT_COLOUR);
            }
        }

        if (IsTrafficSystemResetPending())
        {
            CgsDev::DebugInterface lDebugInterface;

            char lacText[KI_TRAFFIC_DEBUG_TEXT_LENGTH];
            CgsCore::SnPrintf(lacText, KI_TRAFFIC_DEBUG_TEXT_LENGTH, "Network Traffic restart pending, frame %u",
                              static_cast<u32>(mBufferedRestartTrafficMessage.mu16RestartFrame));

            lDebugInterface.Get2dRender().Draw2DText(lacText, KV2_RESTART_PENDING_TEXT_POSITION,
                                                     KF_TRAFFIC_DEBUG_TEXT_SIZE, KU_RESTART_PENDING_TEXT_COLOUR);
        }
    }

    // =========================================================================
    // ProcessBufferedRestartTrafficMessages -- once a buffered restart is due, tell the
    // game which hulls to reactivate and treat the traffic system as restarted (unless
    // restarts are suppressed).
    // =========================================================================
    void TrafficManager::ProcessBufferedRestartTrafficMessages()
    {
        BrnNetworkModuleIO::NetworkOutRestartTrafficEvent lRestartEvent;
        u16 lu16RestartFrame;

        if (IsTrafficRestartRequired(lRestartEvent.mau16ActveHulls, &lu16RestartFrame))
        {
            CGS_ASSERT(mpNetworkModule, "mpNetworkModule");

            if (!mbSuppressTrafficRestart)
            {
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRestartEvent),
                                                                  lRestartEvent.GetEventType(), sizeof(lRestartEvent));
                mu16LastTrafficResetFrame   = lu16RestartFrame;
                mu16NumFramesSinceLastReset = 0;
                OnTrafficRestarted();
            }
        }
    }

    // =========================================================================
    // ConvertReceivedMessageFrameToLocalFrame -- a frame stamped by a console running at
    // the other frame rate, re-expressed in our own frame count.
    // =========================================================================
    u16 TrafficManager::ConvertReceivedMessageFrameToLocalFrame(u16 lu16Frame,
                                                                CgsSystem::EFrameRate leLocalConsoleFrameRate,
                                                                CgsSystem::EFrameRate leRemoteConsoleFrameRate)
    {
        CGS_ASSERT(leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ || leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ,
                   "leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ || leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ");
        CGS_ASSERT(leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ || leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ,
                   "leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ || leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ");

        if (leLocalConsoleFrameRate == leRemoteConsoleFrameRate)
        {
            return lu16Frame;
        }

        const u16 lu16NumWraps     = static_cast<u16>(mpTimeManager->GetFrameWrapCountSinceStart());
        const u16 lu16CurrentFrame = mpTimeManager->GetU16FrameCountSinceStart();
        if (leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ)
        {
            return CgsNetwork::TranslateFrame60HzTo50Hz(lu16Frame, lu16CurrentFrame, lu16NumWraps);
        }
        return CgsNetwork::TranslateFrame50HzTo60Hz(lu16Frame, lu16CurrentFrame, lu16NumWraps);
    }

    // =========================================================================
    // ConvertLocalFrameToReceivedMessageFrame -- one of our frames, re-expressed in the
    // frame count of a console running at the other frame rate.
    // =========================================================================
    u16 TrafficManager::ConvertLocalFrameToReceivedMessageFrame(u16 lu16Frame,
                                                                CgsSystem::EFrameRate leLocalConsoleFrameRate,
                                                                CgsSystem::EFrameRate leRemoteConsoleFrameRate)
    {
        CGS_ASSERT(mpTimeManager, "mpTimeManager");
        CGS_ASSERT(leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ || leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ,
                   "leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ || leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ");
        CGS_ASSERT(leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ || leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ,
                   "leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ || leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ");

        if (leLocalConsoleFrameRate == leRemoteConsoleFrameRate)
        {
            return lu16Frame;
        }

        const u16 lu16NumWraps     = static_cast<u16>(mpTimeManager->GetFrameWrapCountSinceStart());
        const u16 lu16CurrentFrame = mpTimeManager->GetU16FrameCountSinceStart();
        if (leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ)
        {
            return CgsNetwork::TranslateFrame50HzTo60Hz(lu16Frame, lu16CurrentFrame, lu16NumWraps);
        }
        return CgsNetwork::TranslateFrame60HzTo50Hz(lu16Frame, lu16CurrentFrame, lu16NumWraps);
    }

    // =========================================================================
    // UpdateHullSync -- relay the hull activations the local traffic produced this
    // frame to every peer (unless hull syncs are suppressed until the next reset).
    // =========================================================================
    void TrafficManager::UpdateHullSync()
    {
        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");

        if (mbSuppressingHullSyncsUntilReset)
        {
            return;
        }

        const BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface* lpTrafficOutput =
            mpNetworkModule->GetTrafficOutputInterface();

        BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface::ActivateHullQueue lActivateHullQueue;
        lActivateHullQueue.Construct();
        lActivateHullQueue.Clear();
        lActivateHullQueue.Append(lpTrafficOutput->GetActivateHullQueue());

        for (s32 liIndex = 0; liIndex < lActivateHullQueue.GetLength(); ++liIndex)
        {
            const BrnTraffic::BrnTrafficIO::ActivateHullEvent lActivateHullEvent = lActivateHullQueue.GetEvent(liIndex);

            CGS_ASSERT(lActivateHullEvent.muNewActiveHull <= KU16_MAX_HULL_NUMBER,
                       "lActivateHullEvent.muNewActiveHull <= BrnNetwork::KU16_MAX_HULL_NUMBER");
            CGS_ASSERT(lActivateHullEvent.meActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                       "lActivateHullEvent.meActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(lActivateHullEvent.meActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "lActivateHullEvent.meActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

            HandleHullSyncMessage(lActivateHullEvent.meActiveRaceCarIndex,
                                  static_cast<u16>(lActivateHullEvent.muUpdateFrame),
                                  lActivateHullEvent.muNewActiveHull);
        }

        SendHullSyncMessages();
    }

    // =========================================================================
    // ReceiveCrashingTrafficMessages -- buffer every peer's freshly arrived crashing
    // traffic (dropping it while syncing time, for unmapped peers, from before a
    // traffic reset or while a reset is pending), then hand the crash module every
    // buffered update due one second ago.
    // =========================================================================
    void TrafficManager::ReceiveCrashingTrafficMessages()
    {
        CGS_ASSERT(mpTimeManager, "mpTimeManager");
        CGS_ASSERT(mpPlayerManager, "mpPlayerManager");
        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager()->GetServerInterface()->GetGameComponent(),
                   "lpServerInterfaceGames");

        const CgsSystem::EFrameRate leLocalConsoleFrameRate =
            mpNetworkModule->GetNetworkManager()->GetLocalConsoleFrameRate();

        for (s32 liIndex = 0; liIndex < ::KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            TrafficSyncData& lrData = maTrafficData[liIndex];
            CrashingTrafficMessage* lpMessage = &lrData.mCrashingTrafficMessageRecv;
            if (!lpMessage->IsMessageValid())
            {
                continue;
            }

            bool lbBuffered = false;
            if (!mbSyncingTime
                && liIndex < mpPlayerManager->GetNumberNetworkPlayers(CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
            {
                CGS_ASSERT(lrData.mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                           "maTrafficData[liIndex].mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

                if (mpNetworkModule->GetActiveRaceCarIndex(lrData.mPlayerID) != -1)
                {
                    CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lrData.mPlayerID);
                    const NetworkPlayerID lPlayerID = lrData.mPlayerID;
                    if (!IsMessageFromBeforeTrafficReset(lpMessage->GetFramesSinceStart(), lPlayerID)
                        && !IsTrafficSystemResetPending()
                        && !mbSuppressingHullSyncsUntilReset)
                    {
                        CGS_ASSERT(leLocalConsoleFrameRate == lpNetworkPlayer->GetLocalConsoleFrameRate(),
                                   "leLocalConsoleFrameRate == lpNetworkPlayer->GetLocalConsoleFrameRate()");
                        StoreBufferedMessage(lPlayerID, lpMessage, leLocalConsoleFrameRate,
                                             lpNetworkPlayer->GetRemoteConsoleFrameRate());
                        lbBuffered = true;
                    }
                }
            }

            if (!lbBuffered)
            {
                lpMessage->SetMessageInvalid();
            }
        }

        // Updates are applied one second behind the local frame.
        u16 lu16Frame;
        if (leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ)
        {
            lu16Frame = static_cast<u16>(mpTimeManager->GetU16FrameCountSinceStart() - KI_REPORT_CRASHING_TRAFFIC_DELAY_50HZ);
        }
        else if (leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ)
        {
            lu16Frame = static_cast<u16>(mpTimeManager->GetU16FrameCountSinceStart() - KI_REPORT_CRASHING_TRAFFIC_DELAY_60HZ);
        }
        else
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Invalid frame rate " << static_cast<s32>(leLocalConsoleFrameRate);
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            lu16Frame = CgsNetwork::KU16_INVALID_FRAME;
        }

        if (lu16Frame == CgsNetwork::KU16_INVALID_FRAME)
        {
            return;
        }

        DeleteOutOfDateBufferedMesssages(lu16Frame);
        BufferedMessage* lpBufferedMessage = RetrieveBufferedMessage(lu16Frame);
        BrnWorld::CrashIO::NetworkInputInterface* lpCrashInput = mpNetworkModule->GetCrashInputInterface();

        while (lpBufferedMessage != nullptr)
        {
            const EActiveRaceCarIndex leActiveRaceCarIndex =
                mpNetworkModule->GetActiveRaceCarIndex(lpBufferedMessage->mOwningNetworkPlayerID);
            if (leActiveRaceCarIndex != -1 && !lpCrashInput->IsRaceCarMarkedForUpdate(leActiveRaceCarIndex))
            {
                lpCrashInput->MarkRaceCarForUpdate(leActiveRaceCarIndex);
                for (s32 liData = 0; liData < lpBufferedMessage->miCrashingTrafficDataCount; ++liData)
                {
                    const CrashingTrafficData& lrCrashingTraffic = lpBufferedMessage->maCrashingTrafficData[liData];
                    lpCrashInput->AddTrafficUpdate(lrCrashingTraffic.mu16VehicleID,
                                                   static_cast<u32>(leActiveRaceCarIndex),
                                                   lrCrashingTraffic.mMatrix);
                }
            }

            RemoveBufferedMessage(lpBufferedMessage);
            mu16LastBufferReadFrame = lu16Frame;
            lpBufferedMessage = RetrieveBufferedMessage(lu16Frame);
        }
    }
}
