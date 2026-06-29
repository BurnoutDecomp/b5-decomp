// Bodies for the online "last-second challenge success" relay, reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   Construct                              @ 0x8255DCD8
//   Destruct                               @ 0x82555D48
//   ProcessBeforeSimulation                @ 0x82555DC0
//   ProcessAfterSimulation                 @ 0x8256FE78
//   AddPlayer                              @ 0x8256BF20
//   RemovePlayer                           @ 0x825569F8
//   Disconnected                           @ 0x8255DF28
//   GetChallengeSuccessDataEntry           @ 0x8254B350
//   HandleSendingUpdateMessage             @ 0x8254B068
//   HandleSendingChallengeSuccessMessage   @ 0x8254B1E0
//   CheckForNewUpdateMessages              @ 0x8256BE60
//   HandleReceivedUpdateMessage            @ 0x82565090
//   ProcessNetworkEvents                   @ 0x8255DDC8
//   TranslateSuccessUpdate60HzTo50Hz       @ 0x82555DD8
//   TranslateSuccessUpdate50HzTo60Hz       @ 0x825563F8
//   _ChallengeSuccessMessageArrivedCallback   @ 0x825652F8
//   _ChallengeSuccessMessageDeliveredCallback @ 0x8254B3D8
//
// The manager keeps a fixed table of 7 per-player slots, each with a send/receive
// FburnSuccessUpdateMessage pair (the per-frame round-robined "last-second success" bit set)
// and a send/receive FburnChallengeSuccessMessage pair (the reliable two-action result). The
// GameState raises three in-events (from PostSim's network-event queue) that drive the relay:
// the success-update bit set, the reliable challenge-success result, and the challenge
// lifecycle event that clears the "pending" gate. On the receive side the relay turns inbound
// messages back into network-out events for the GameState, translating the frame timing
// between 50 Hz and 60 Hz consoles via the FastBitArray<60> success bit sets.

#include "GameSource/Network/Managers/BrnChallengeSuccessManager.h"
#include "GameSource/Network/BrnNetworkModule.h"                       // BrnNetworkModule::GetNetworkManager / GetNetworkEventQueue
#include "GameSource/Network/BrnNetworkManager.h"                      // BrnNetworkManager::GetLocalConsoleFrameRate
#include "GameSource/Network/BrnNetworkModuleIO.h"                     // BrnNetworkModuleIO::PostSim, PostSimulationInputBuffer, NetworkEventQueue
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // GameStateToNetworkInterface::GetActiveRaceCarIndex
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"   // PlayerManager::GetPlayerByID / IsPlayerTurnToSendRoundRobinMessage
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"   // NetworkPlayer::RegisterMessageType / GetRemoteConsoleFrameRate, KI_INVALID_PLAYER_ID, EFrameRate
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"        // TimeManager::GetU16FrameCount / GetFramesSinceStart
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"       // VariableEventQueue<14000,16> (GetFirstEvent/GetNextEvent/AddEvent)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // CgsDev::Log::gpDebugPrint (nack warning)
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT

namespace BrnNetwork
{
    // The reliable-message wire ids the manager registers per player (X360 AddPlayer: li r4,0x24
    // / li r4,0x25). 36 == the unreliable per-frame update message; 37 == the reliable result.
    static const s32 KI_UPDATE_MESSAGE_TYPE            = 36;  // 0x24 -- FburnSuccessUpdateMessage
    static const s32 KI_CHALLENGE_SUCCESS_MESSAGE_TYPE = 37;  // 0x25 -- FburnChallengeSuccessMessage

    // The two console frame rates the relay translates between (X360 cmpwi 0x32 / 0x3C).
    static const s32 KI_FRAMERATE_50HZ = 50;
    static const s32 KI_FRAMERATE_60HZ = 60;

    // GameState -> network in-event type ids walked by ProcessNetworkEvents (X360: cmpwi 0x26 /
    // 0x28 / 0x29). They mirror BrnNetworkInEventTypeDefs.h's NetworkEvent<N> tags.
    static const s32 KI_INEVENT_FREEBURN_CHALLENGE  = 0x26;  // 38 -- challenge lifecycle (clears the gate on RESULTS_FINISHED)
    static const s32 KI_INEVENT_FBURN_SUCCESS_UPDATE = 0x28; // 40 -- arms the gate + caches the pending update
    static const s32 KI_INEVENT_FBURN_CHALLENGE_SUCCESS = 0x29; // 41 -- the reliable two-action result to send

    // Network-out event type ids the relay produces onto the module network-event queue (X360:
    // li r5,0x43 / li r5,0x44 to AddEvent). 67 == the received success-update event, 68 == the
    // received reliable challenge-success result.
    static const s32 KI_OUTEVENT_FBURN_SUCCESS_UPDATE  = 0x43;  // 67
    static const s32 KI_OUTEVENT_FBURN_CHALLENGE_SUCCESS = 0x44; // 68
    static const s32 KI_OUTEVENT_FBURN_SUCCESS_UPDATE_SIZE  = 24; // 0x18
    static const s32 KI_OUTEVENT_FBURN_CHALLENGE_SUCCESS_SIZE = 20; // 0x14

    namespace
    {
        // The BrnNetworkModuleIO::NetworkEventQueue accessor/PostSim return are typed against an
        // incomplete forward; the concrete queue is the 14000/16 variable-event queue (same
        // pattern as BrnEventScoresManager.cpp / BrnNetworkPlayer.cpp).
        typedef CgsModule::VariableEventQueue<14000, 16> NetworkEventQueueConcrete;

        inline const NetworkEventQueueConcrete* AsConcreteQueue(
            const BrnNetworkModuleIO::NetworkEventQueue* lpQueue)
        {
            return reinterpret_cast<const NetworkEventQueueConcrete*>(lpQueue);
        }

        inline NetworkEventQueueConcrete* AsConcreteQueue(
            BrnNetworkModuleIO::NetworkEventQueue* lpQueue)
        {
            return reinterpret_cast<NetworkEventQueueConcrete*>(lpQueue);
        }

        // ---- in-event payloads consumed from the GameState network-event queue --------------
        // Offsets are X360-AUTHORITATIVE from ProcessNetworkEvents @ 0x8255DDC8 (the raw event
        // reads). They mirror the DWARF NetworkInFreeburnChallengeEvent<38> /
        // NetworkInFburnSuccessUpdateEvent<40> / NetworkInFburnChallengeSuccessEvent<41> member
        // lists; only the fields this relay reads are named, the rest are pinned with padding.

        // NetworkEvent<38>: challenge lifecycle event. The relay reads only meEventType (@ +16)
        // and clears the pending gate when it is E_CHALLENGE_EVENT_RESULTS_FINISHED (== 5).
        struct InFreeburnChallengeEvent
        {
            NetworkPlayerID mPlayerID;     // +0x00
            u8              maPad4[4];      // +0x04 (CgsID is 8-aligned)
            u64             mChallengeID;   // +0x08 (CgsID)
            s32             meEventType;    // +0x10 (EChallengeEventType; lwz 0x10 -> cmpwi 5)
        };

        // NetworkEvent<40>: the cached success-update the relay re-broadcasts. The handler stores
        // the 8-byte bit set whole (ld 0 -> mLastSecondChallengeSuccess) and the action index
        // (lwz 8 -> miChallengeSuccessUpdateIndex).
        struct InFburnSuccessUpdateEvent
        {
            LastSecondChallengeSuccess mChallengeSuccessUpdate; // +0x00 (8 bytes)
            s32                        miActionIndex;           // +0x08
        };

        // NetworkEvent<41>: the reliable two-action result to send out. The relay forwards the
        // three field pointers (scores @ +0, successful @ +8, accumulation @ +10) straight into
        // HandleSendingChallengeSuccessMessage.
        struct InFburnChallengeSuccessEvent
        {
            f32  mafActionScores[2];          // +0x00
            bool mabSuccessfulActions[2];      // +0x08
            bool mabAccumulationThisFrame[2];  // +0x0A
        };

        // ---- network-out event payloads produced onto the module network-event queue ---------
        // X360-AUTHORITATIVE byte layouts from the AddEvent call sites (the relay builds the byte
        // image on the stack, then AddEvent copies the leading N bytes). These are the ARTIST-era
        // NetworkOut shapes; the PS3 DWARF NetworkOutFburnChallengeSuccess(Update)Event member
        // order drifted across the merge window, so the asm stack layout is authoritative here.

        // type 67 (0x43), 24 bytes -- built by HandleReceivedUpdateMessage @ 0x82565090.
        // Stack proof: std v20=success bitset @ var_60(+0); stw GetActiveRa @ var_58(+8);
        // stw frame index @ var_54(+0xC); stw action index @ var_50(+0x10).
        struct OutFburnSuccessUpdateEvent
        {
            LastSecondChallengeSuccess mChallengeSuccessUpdate; // +0x00 (8 bytes)
            s32                        meActiveRaceCarIndex;    // +0x08
            s32                        miChallengeUpdateFrame;  // +0x0C
            s32                        miActionIndex;           // +0x10
            // (AddEvent copies 24 bytes; +0x14..+0x17 trail as alignment.)
        };

        // type 68 (0x44), 20 bytes -- built by _ChallengeSuccessMessageArrivedCallback
        // @ 0x825652F8. Stack proof (buffer base = var_40 @ +0): Retrieve fills action scores
        // @ +0 (var_40), frames-since-start @ +8 (var_38), successful flags @ +0x10 (var_30),
        // accumulation flags @ +0x12 (var_2E); GetActiveRa is stw'd @ +0xC (var_34).
        struct OutFburnChallengeSuccessEvent
        {
            f32  mafActionScores[2];           // +0x00
            s32  miFramesSinceStart;           // +0x08
            s32  meActiveRaceCarIndex;         // +0x0C
            bool mabSuccessfulActions[2];       // +0x10
            bool mabAccumulationThisFrame[2];   // +0x12
        };
    } // anonymous namespace

    // -------------------------------------------------------------------------------------------
    // Construct  @ 0x8255DCD8
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::Construct(BrnNetworkModule* lpNetworkModule,
                                            CgsNetwork::PlayerManager* lpPlayerManager,
                                            CgsNetwork::TimeManager* lpTimeManager)
    {
        mpNetworkModule = lpNetworkModule;
        mpPlayerManager = lpPlayerManager;
        mpTimeManager   = lpTimeManager;

        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");
        CGS_ASSERT(mpTimeManager != nullptr, "mpTimeManager");

        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            ChallengeSuccessData& lrData = maChallengeSuccessData[liIndex];
            lrData.mPlayerID = CgsNetwork::KI_INVALID_PLAYER_ID;
            lrData.mUpdateMessageSend.Construct();
            lrData.mUpdateMessageRecv.Construct();
            lrData.mChallengeSuccessMessageSend.Construct();
            lrData.mChallengeSuccessMessageRecv.Construct();
        }

        mLastSecondChallengeSuccess.UnSetAll();
        miChallengeSuccessUpdateIndex = 0;
        mbSendUpdateSuccessMessage    = false;
    }

    // -------------------------------------------------------------------------------------------
    // Destruct  @ 0x82555D48
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::Destruct()
    {
        mLastSecondChallengeSuccess.UnSetAll();
        miChallengeSuccessUpdateIndex = 0;
        mbSendUpdateSuccessMessage    = false;

        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            ChallengeSuccessData& lrData = maChallengeSuccessData[liIndex];
            lrData.mPlayerID = CgsNetwork::KI_INVALID_PLAYER_ID;
            lrData.mUpdateMessageSend.Destruct();
            lrData.mUpdateMessageRecv.Destruct();
            lrData.mChallengeSuccessMessageSend.Destruct();
            lrData.mChallengeSuccessMessageRecv.Destruct();
        }

        // X360 @0x82555D48 zeroes all three pointer members (stw r28=0 to 0x5F4/0x5F8/0x5FC).
        mpNetworkModule = nullptr;   // +0x5F4
        mpPlayerManager = nullptr;   // +0x5F8
        mpTimeManager   = nullptr;   // +0x5FC
    }

    // -------------------------------------------------------------------------------------------
    // ProcessBeforeSimulation  @ 0x82555DC0
    // When an update is pending, round-robin the per-player success-update message out. (The
    // OutputBuffer arg is part of the module IO signature but unused by this relay.)
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::ProcessBeforeSimulation(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer)
    {
        (void)lpOutputBuffer;
        if (mbSendUpdateSuccessMessage)
        {
            HandleSendingUpdateMessage();
        }
    }

    // -------------------------------------------------------------------------------------------
    // ProcessAfterSimulation  @ 0x8256FE78
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::ProcessAfterSimulation(
        const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput)
    {
        CGS_ASSERT(lpInput != nullptr, "lpInput");
        ProcessNetworkEvents(BrnNetworkModuleIO::PostSim(lpInput));
        CheckForNewUpdateMessages();
    }

    // -------------------------------------------------------------------------------------------
    // GetChallengeSuccessDataEntry  @ 0x8254B350
    // Linear scan of the per-player table for the slot whose mPlayerID matches lPlayerID (called
    // with KI_INVALID_PLAYER_ID(-1) to claim the first free slot). Asserts (and returns null) when
    // no slot matches.
    // -------------------------------------------------------------------------------------------
    ChallengeSuccessManager::ChallengeSuccessData*
    ChallengeSuccessManager::GetChallengeSuccessDataEntry(NetworkPlayerID lPlayerID)
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            if (maChallengeSuccessData[liIndex].mPlayerID == lPlayerID)
            {
                return &maChallengeSuccessData[liIndex];
            }
        }

        CGS_ASSERT(false, "lpEntry");
        return nullptr;
    }

    // -------------------------------------------------------------------------------------------
    // AddPlayer  @ 0x8256BF20
    // Register the update + reliable success message types for a newly-joined player on its
    // NetworkPlayer, claiming a free table slot and stamping it with the player id.
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::AddPlayer(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer != nullptr)
        {
            ChallengeSuccessData* lpEntry = GetChallengeSuccessDataEntry(CgsNetwork::KI_INVALID_PLAYER_ID);
            CGS_ASSERT(lpEntry != nullptr, "lpEntry");

            // Unreliable per-frame update message pair (no arrival/delivery callbacks).
            lpNetworkPlayer->RegisterMessageType(KI_UPDATE_MESSAGE_TYPE,
                                                 sizeof(FburnSuccessUpdateMessage),
                                                 &lpEntry->mUpdateMessageSend,
                                                 &lpEntry->mUpdateMessageRecv,
                                                 nullptr, nullptr, nullptr);
            // Reliable challenge-success result pair, with the arrival/delivery callbacks and
            // this manager as their user data.
            lpNetworkPlayer->RegisterMessageType(KI_CHALLENGE_SUCCESS_MESSAGE_TYPE,
                                                 sizeof(FburnChallengeSuccessMessage),
                                                 &lpEntry->mChallengeSuccessMessageSend,
                                                 &lpEntry->mChallengeSuccessMessageRecv,
                                                 &ChallengeSuccessManager::_ChallengeSuccessMessageArrivedCallback,
                                                 &ChallengeSuccessManager::_ChallengeSuccessMessageDeliveredCallback,
                                                 this);

            lpEntry->mPlayerID = lPlayerID;
        }
    }

    // -------------------------------------------------------------------------------------------
    // RemovePlayer  @ 0x825569F8
    // Unregister the player's message types and release its table slot.
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::RemovePlayer(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer != nullptr)
        {
            lpNetworkPlayer->UnRegisterMessageType(KI_UPDATE_MESSAGE_TYPE);
            lpNetworkPlayer->UnRegisterMessageType(KI_CHALLENGE_SUCCESS_MESSAGE_TYPE);

            ChallengeSuccessData* lpEntry = GetChallengeSuccessDataEntry(lPlayerID);
            CGS_ASSERT(lpEntry != nullptr, "lpEntry");
            lpEntry->mPlayerID = CgsNetwork::KI_INVALID_PLAYER_ID;
        }
    }

    // -------------------------------------------------------------------------------------------
    // Disconnected  @ 0x8255DF28
    // Tear down every still-occupied slot and reset the pending-update bookkeeping.
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::Disconnected()
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            if (maChallengeSuccessData[liIndex].mPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID)
            {
                RemovePlayer(maChallengeSuccessData[liIndex].mPlayerID);
                CGS_ASSERT(maChallengeSuccessData[liIndex].mPlayerID == CgsNetwork::KI_INVALID_PLAYER_ID,
                           "maChallengeSuccessData[liIndex].mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID");
            }
        }

        mLastSecondChallengeSuccess.UnSetAll();
        miChallengeSuccessUpdateIndex = 0;
        mbSendUpdateSuccessMessage    = false;
    }

    // -------------------------------------------------------------------------------------------
    // HandleSendingUpdateMessage  @ 0x8254B068
    // Round-robin the cached "last-second success" update out: for each occupied slot whose turn
    // it is this frame, prepare its (still-empty) send update message with the current frame id,
    // the cached action index and the cached success bit set.
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::HandleSendingUpdateMessage()
    {
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");
        CGS_ASSERT(mpTimeManager != nullptr, "mpTimeManager");

        const u16 lu16CurrentFrame = mpTimeManager->GetU16FrameCount();

        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            ChallengeSuccessData* lpSuccessData = &maChallengeSuccessData[liIndex];
            CGS_ASSERT(lpSuccessData != nullptr, "lpSuccessData");

            const NetworkPlayerID lPlayerID = lpSuccessData->mPlayerID;
            if (lPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID
                && mpPlayerManager->IsPlayerTurnToSendRoundRobinMessage(lPlayerID, false, 0))
            {
                FburnSuccessUpdateMessage* lpUpdateMessage = &lpSuccessData->mUpdateMessageSend;
                CGS_ASSERT(lpUpdateMessage != nullptr, "lpUpdateMessage");
                CGS_ASSERT(!lpUpdateMessage->IsMessageValid(), "!lpUpdateMessage->IsMessageValid()");

                const s32 liFramesSinceStart = mpTimeManager->GetFramesSinceStart();
                lpUpdateMessage->PrepareForSend(lu16CurrentFrame,
                                                liFramesSinceStart,
                                                miChallengeSuccessUpdateIndex,
                                                &mLastSecondChallengeSuccess);
            }
        }
    }

    // -------------------------------------------------------------------------------------------
    // HandleSendingChallengeSuccessMessage  @ 0x8254B1E0
    // Push the reliable two-action challenge-success result to every occupied slot (its still-
    // empty send message), stamped with the current frame id.
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::HandleSendingChallengeSuccessMessage(const bool* lpaSuccessfulActions,
                                                                       const bool* lpaAccumulationThisFrame,
                                                                       const f32*  lpaActionScores)
    {
        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");
        CGS_ASSERT(mpTimeManager != nullptr, "mpTimeManager");

        const u16 lu16CurrentFrame = mpTimeManager->GetU16FrameCount();

        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            ChallengeSuccessData* lpSuccessData = &maChallengeSuccessData[liIndex];
            CGS_ASSERT(lpSuccessData != nullptr, "lpSuccessData");

            if (lpSuccessData->mPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID)
            {
                FburnChallengeSuccessMessage* lpChallengeSuccessMessage =
                    &lpSuccessData->mChallengeSuccessMessageSend;
                CGS_ASSERT(lpChallengeSuccessMessage != nullptr, "lpChallengeSuccessMessage");
                CGS_ASSERT(!lpChallengeSuccessMessage->IsMessageValid(),
                           "!lpChallengeSuccessMessage->IsMessageValid()");

                const s32 liFramesSinceStart = mpTimeManager->GetFramesSinceStart();
                lpChallengeSuccessMessage->PrepareForSend(lu16CurrentFrame,
                                                          liFramesSinceStart,
                                                          lpaSuccessfulActions,
                                                          lpaAccumulationThisFrame,
                                                          lpaActionScores);
            }
        }
    }

    // -------------------------------------------------------------------------------------------
    // CheckForNewUpdateMessages  @ 0x8256BE60
    // Drain each slot's received update message and forward it to HandleReceivedUpdateMessage.
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::CheckForNewUpdateMessages()
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            ChallengeSuccessData* lpSuccessData = &maChallengeSuccessData[liIndex];
            CGS_ASSERT(lpSuccessData != nullptr, "lpSuccessData");

            FburnSuccessUpdateMessage* lpReceivedMessage = &lpSuccessData->mUpdateMessageRecv;
            CGS_ASSERT(lpReceivedMessage != nullptr, "lpReceivedMessage");

            s32                        liFramesSinceStart = 0;
            s32                        liActionIndex      = 0;
            LastSecondChallengeSuccess lSuccessUpdate;
            if (lpReceivedMessage->Retrieve(&liFramesSinceStart, &liActionIndex, &lSuccessUpdate))
            {
                HandleReceivedUpdateMessage(lpSuccessData->mPlayerID,
                                            liFramesSinceStart,
                                            liActionIndex,
                                            &lSuccessUpdate);
            }
        }
    }

    // -------------------------------------------------------------------------------------------
    // HandleReceivedUpdateMessage  @ 0x82565090
    // Translate one inbound success-update from the sender's console frame rate to ours (50<->60
    // Hz), retime its frame index, look up the sender's active-race-car slot, and re-broadcast it
    // as a network-out success-update event for the GameState.
    // (ASM register order: r4 lPlayerID, r5 framesSinceNetworkStart, r6 actionIndex, r7 update.)
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::HandleReceivedUpdateMessage(
        NetworkPlayerID lPlayerID,
        s32 liFramesSinceNetworkStart,
        s32 liActionIndex,
        const LastSecondChallengeSuccess* lpSuccessUpdate)
    {
        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager() != nullptr,
                   "mpNetworkModule->GetNetworkManager()");

        const s32 leLocalConsoleFrameRate =
            static_cast<s32>(mpNetworkModule->GetNetworkManager()->GetLocalConsoleFrameRate());
        CGS_ASSERT(leLocalConsoleFrameRate == KI_FRAMERATE_50HZ
                   || leLocalConsoleFrameRate == KI_FRAMERATE_60HZ,
                   "( leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ ) || "
                   "( leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ )");

        CGS_ASSERT(mpPlayerManager != nullptr, "mpPlayerManager");
        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        CGS_ASSERT(lpNetworkPlayer != nullptr, "lpNetworkPlayer");

        const s32 leRemoteConsoleFrameRate =
            static_cast<s32>(lpNetworkPlayer->GetRemoteConsoleFrameRate());
        CGS_ASSERT(leRemoteConsoleFrameRate == KI_FRAMERATE_50HZ
                   || leRemoteConsoleFrameRate == KI_FRAMERATE_60HZ,
                   "( leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ ) || "
                   "( leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ )");

        LastSecondChallengeSuccess lTranslatedSuccess;
        s32                        liTranslatedFrame = liFramesSinceNetworkStart;

        if (leLocalConsoleFrameRate == leRemoteConsoleFrameRate)
        {
            // Same frame rate: no bit-set retiming, copy the update across unchanged.
            lTranslatedSuccess = *lpSuccessUpdate;
        }
        else if (leLocalConsoleFrameRate == KI_FRAMERATE_50HZ)
        {
            // Sender is 60 Hz, we are 50 Hz.
            TranslateSuccessUpdate60HzTo50Hz(&lTranslatedSuccess, lpSuccessUpdate);
            CGS_ASSERT(mpTimeManager != nullptr, "mpTimeManager");
            // Map a 60 Hz frame index onto the 50 Hz timeline (X360: 5 * frame / 6 - 50).
            liTranslatedFrame = 5 * liFramesSinceNetworkStart / 6 - KI_FRAMERATE_50HZ;
        }
        else
        {
            // Sender is 50 Hz, we are 60 Hz.
            TranslateSuccessUpdate50HzTo60Hz(&lTranslatedSuccess, lpSuccessUpdate);
            CGS_ASSERT(mpTimeManager != nullptr, "mpTimeManager");
            // Map a 50 Hz frame index onto the 60 Hz timeline (X360: 6 * frame / 5 - 60).
            liTranslatedFrame = 6 * liFramesSinceNetworkStart / 5 - KI_FRAMERATE_60HZ;
        }

        BrnNetworkModuleIO::GameStateToNetworkInterface* lpInterface =
            mpNetworkModule->GetGameStateToNetworkInterface();

        OutFburnSuccessUpdateEvent lOutEvent;
        lOutEvent.mChallengeSuccessUpdate = lTranslatedSuccess;
        lOutEvent.meActiveRaceCarIndex    = static_cast<s32>(lpInterface->GetActiveRaceCarIndex(lPlayerID));
        lOutEvent.miChallengeUpdateFrame  = liTranslatedFrame;
        lOutEvent.miActionIndex           = liActionIndex;

        CGS_ASSERT(mpNetworkModule->GetNetworkEventQueue() != nullptr,
                   "mpNetworkModule->GetNetworkEventQueue()");
        AsConcreteQueue(mpNetworkModule->GetNetworkEventQueue())->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lOutEvent),
            KI_OUTEVENT_FBURN_SUCCESS_UPDATE, KI_OUTEVENT_FBURN_SUCCESS_UPDATE_SIZE);
    }

    // -------------------------------------------------------------------------------------------
    // ProcessNetworkEvents  @ 0x8255DDC8
    // Walk the GameState's post-sim network-event queue: cache a fresh success-update (arming the
    // send gate), send the reliable challenge-success result out, and clear the send gate when the
    // challenge's results have finished.
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::ProcessNetworkEvents(
        const BrnNetworkModuleIO::NetworkEventQueue* lpInputNetworkEventQueue)
    {
        CGS_ASSERT(lpInputNetworkEventQueue != nullptr, "lpInputNetworkEventQueue");

        const NetworkEventQueueConcrete* lpQueue = AsConcreteQueue(lpInputNetworkEventQueue);

        const CgsModule::Event* lpEvent = nullptr;
        s32 liSize = 0;
        s32 leEventType = lpQueue->GetFirstEvent(&lpEvent, &liSize);

        while (lpEvent != nullptr)
        {
            if (leEventType == KI_INEVENT_FBURN_SUCCESS_UPDATE)
            {
                const InFburnSuccessUpdateEvent* lpSuccessEvent =
                    reinterpret_cast<const InFburnSuccessUpdateEvent*>(lpEvent);
                CGS_ASSERT(lpSuccessEvent != nullptr, "lpSuccessEvent");
                mbSendUpdateSuccessMessage    = true;
                mLastSecondChallengeSuccess   = lpSuccessEvent->mChallengeSuccessUpdate;
                miChallengeSuccessUpdateIndex = lpSuccessEvent->miActionIndex;
            }
            else if (leEventType == KI_INEVENT_FREEBURN_CHALLENGE)
            {
                const InFreeburnChallengeEvent* lpChallengeEvent =
                    reinterpret_cast<const InFreeburnChallengeEvent*>(lpEvent);
                CGS_ASSERT(lpChallengeEvent != nullptr, "lpChallengeEvent");
                if (lpChallengeEvent->meEventType == BrnNetworkModuleIO::E_CHALLENGE_EVENT_RESULTS_FINISHED)
                {
                    mbSendUpdateSuccessMessage = false;
                }
            }
            else if (leEventType == KI_INEVENT_FBURN_CHALLENGE_SUCCESS)
            {
                const InFburnChallengeSuccessEvent* lpChallengeSuccessEvent =
                    reinterpret_cast<const InFburnChallengeSuccessEvent*>(lpEvent);
                CGS_ASSERT(lpChallengeSuccessEvent != nullptr, "lpChallengeSuccessEvent");
                HandleSendingChallengeSuccessMessage(lpChallengeSuccessEvent->mabSuccessfulActions,
                                                     lpChallengeSuccessEvent->mabAccumulationThisFrame,
                                                     lpChallengeSuccessEvent->mafActionScores);
            }

            const CgsModule::Event* lpNextEvent = nullptr;
            leEventType = lpQueue->GetNextEvent(lpEvent, &lpNextEvent, &liSize);
            lpEvent = lpNextEvent;
        }
    }

    // -------------------------------------------------------------------------------------------
    // TranslateSuccessUpdate60HzTo50Hz  @ 0x82555DD8
    // Resample a 60-frame success bit set down to a 50-frame one: every 6th source-frame pair
    // collapses to one destination frame (its two source bits OR'd together), so 60 frames map to
    // 50. The destination bit set is cleared first; asserts confirm the source walk reaches 60.
    // (ASM register order: r4 destination, r5 source.)
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::TranslateSuccessUpdate60HzTo50Hz(
        LastSecondChallengeSuccess* lpTranslatedSuccess,
        const LastSecondChallengeSuccess* lpSuccessUpdateToTranslate)
    {
        lpTranslatedSuccess->UnSetAll();

        s32 liSrcIndex = 0;   // 60 Hz source frame
        for (s32 liDstIndex = 0; liDstIndex < KI_FRAMERATE_50HZ; ++liDstIndex)
        {
            if (liSrcIndex % 6 == 4)
            {
                // The collapsed pair: this destination frame carries either of the two source
                // frames' success bits.
                if (lpSuccessUpdateToTranslate->IsBitSet(liSrcIndex)
                    || lpSuccessUpdateToTranslate->IsBitSet(liSrcIndex + 1))
                {
                    lpTranslatedSuccess->SetBit(liDstIndex);
                }
                liSrcIndex += 2;   // skip the second source frame of the collapsed pair
            }
            else
            {
                if (lpSuccessUpdateToTranslate->IsBitSet(liSrcIndex))
                {
                    lpTranslatedSuccess->SetBit(liDstIndex);
                }
                ++liSrcIndex;
            }
        }

        CGS_ASSERT(liSrcIndex == KI_FRAMERATE_60HZ, "CgsSystem::E_FRAMERATE_60HZ == liSrcIndex");
    }

    // -------------------------------------------------------------------------------------------
    // TranslateSuccessUpdate50HzTo60Hz  @ 0x825563F8
    // Resample a 50-frame success bit set up to a 60-frame one: every 5th source frame duplicates
    // into two consecutive destination frames, so 50 frames map to 60. The destination bit set is
    // cleared first; asserts confirm the source walk reaches 50.
    // (ASM register order: r4 destination, r5 source.)
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::TranslateSuccessUpdate50HzTo60Hz(
        LastSecondChallengeSuccess* lpTranslatedSuccess,
        const LastSecondChallengeSuccess* lpSuccessUpdateToTranslate)
    {
        lpTranslatedSuccess->UnSetAll();

        s32 liDstIndex = 0;   // 60 Hz destination frame
        for (s32 liSrcIndex = 0; liSrcIndex < KI_FRAMERATE_50HZ; ++liSrcIndex)
        {
            if (liSrcIndex % 5 == 4)
            {
                // Duplicate this source frame into two consecutive destination frames.
                if (lpSuccessUpdateToTranslate->IsBitSet(liSrcIndex))
                {
                    lpTranslatedSuccess->SetBit(liDstIndex);
                    lpTranslatedSuccess->SetBit(liDstIndex + 1);
                }
                liDstIndex += 2;
            }
            else
            {
                if (lpSuccessUpdateToTranslate->IsBitSet(liSrcIndex))
                {
                    lpTranslatedSuccess->SetBit(liDstIndex);
                }
                ++liDstIndex;
            }
        }

        CGS_ASSERT(liDstIndex == KI_FRAMERATE_60HZ, "CgsSystem::E_FRAMERATE_50HZ == liSrcIndex");
    }

    // -------------------------------------------------------------------------------------------
    // _ChallengeSuccessMessageArrivedCallback  @ 0x825652F8
    // A reliable challenge-success result arrived from a peer: retrieve it, map the sender onto its
    // active-race-car slot, and re-broadcast it as a network-out challenge-success event.
    // (ASM register order: r3 message, r4 fromPlayerID, r5 userData == this manager.)
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::_ChallengeSuccessMessageArrivedCallback(
        CgsNetwork::ReliableMessage* lpMessage,
        NetworkPlayerID liFromPlayerID,
        void* lpUserData)
    {
        ChallengeSuccessManager* lpChallengeManager = static_cast<ChallengeSuccessManager*>(lpUserData);
        CGS_ASSERT(lpChallengeManager != nullptr, "lpChallengeManager");

        FburnChallengeSuccessMessage* lpChallengeSuccessMessage =
            reinterpret_cast<FburnChallengeSuccessMessage*>(lpMessage);
        CGS_ASSERT(lpChallengeSuccessMessage != nullptr, "lpChallengeSuccessMessage");

        OutFburnChallengeSuccessEvent lChallengeSuccessEvent;
        const bool lbRetrieved = lpChallengeSuccessMessage->Retrieve(
            &lChallengeSuccessEvent.miFramesSinceStart,
            lChallengeSuccessEvent.mabSuccessfulActions,
            lChallengeSuccessEvent.mabAccumulationThisFrame,
            lChallengeSuccessEvent.mafActionScores);
        CGS_ASSERT(lbRetrieved,
                   "lpChallengeSuccessMessage->Retrieve( &lChallengeSucessEvent.miFramesSinceNetworkStart, "
                   "lChallengeSucessEvent.mabSuccessfulActions, lChallengeSucessEvent.mabAccumulationThisFrame, "
                   "lChallengeSucessEvent.mafActionScores )");

        CGS_ASSERT(lpChallengeManager->mpNetworkModule != nullptr,
                   "lpChallengeManager->mpNetworkModule");

        BrnNetworkModuleIO::GameStateToNetworkInterface* lpInterface =
            lpChallengeManager->mpNetworkModule->GetGameStateToNetworkInterface();
        lChallengeSuccessEvent.meActiveRaceCarIndex =
            static_cast<s32>(lpInterface->GetActiveRaceCarIndex(liFromPlayerID));

        CGS_ASSERT(lpChallengeManager->mpNetworkModule->GetNetworkEventQueue() != nullptr,
                   "lpChallengeManager->mpNetworkModule->GetNetworkEventQueue()");
        AsConcreteQueue(lpChallengeManager->mpNetworkModule->GetNetworkEventQueue())->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lChallengeSuccessEvent),
            KI_OUTEVENT_FBURN_CHALLENGE_SUCCESS, KI_OUTEVENT_FBURN_CHALLENGE_SUCCESS_SIZE);
    }

    // -------------------------------------------------------------------------------------------
    // _ChallengeSuccessMessageDeliveredCallback  @ 0x8254B3D8
    // The reliable send completion trampoline. The X360 body only logs a warning when the message
    // was reliable (a "nack found" diagnostic); the delivery state and the rest of the args are
    // ignored. (ASM tests r4 == lbWasReliable, not r3.)
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::_ChallengeSuccessMessageDeliveredCallback(
        bool lbDelivered, bool lbWasReliable,
        CgsNetwork::SignalMessage* lpMessage,
        NetworkPlayerID liToPlayerID,
        void* lpUserData)
    {
        (void)lbDelivered;
        (void)lpMessage;
        (void)liToPlayerID;
        (void)lpUserData;

        if (lbWasReliable)
        {
            *CgsDev::Log::gpDebugPrint << "WARNING: Fack Nack found in ";
            *CgsDev::Log::gpDebugPrint
                << "BrnNetwork::ChallengeSuccessManager::_ChallengeSuccessMessageDeliveredCallback";
            *CgsDev::Log::gpDebugPrint << "\n";
        }
    }
} // namespace BrnNetwork
