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
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // mNetStreamLogChannelOutput (0x82F335C8)
#include "GameSource/Network/BrnNetworkModule.h"                       // BrnNetworkModule::GetNetworkManager / GetNetworkEventQueue
#include "GameSource/Network/BrnNetworkManager.h"                      // BrnNetworkManager::GetLocalConsoleFrameRate
#include "GameSource/Network/BrnNetworkModuleIO.h"                     // BrnNetworkModuleIO::PostSim, PostSimulationInputBuffer, NetworkEventQueue (AddEvent<T>)
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"              // NetworkInFreeburnChallenge / FburnSuccessUpdate / FburnChallengeSuccess (tags 38 / 40 / 41)
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"             // NetworkOutFburnChallengeSuccess(Update)Event (tags 67 / 68)
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // GameStateToNetworkInterface::GetActiveRaceCarIndex
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"   // PlayerManager::GetPlayerByID / IsPlayerTurnToSendRoundRobinMessage
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"   // NetworkPlayer::RegisterMessageType / GetRemoteConsoleFrameRate, KI_INVALID_PLAYER_ID, EFrameRate
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"        // TimeManager::GetU16FrameCount / GetFramesSinceStart
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"       // VariableEventQueue<14000,16> (GetFirstEvent/GetNextEvent/AddEvent)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // CgsDev::Log::gpDebugPrint (nack warning)
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                     // CGS_ASSERT

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Network/Managers/BrnChallengeSuccessManager.cpp".

namespace BrnNetwork
{
    // The reliable-message wire ids the manager registers per player (X360 AddPlayer: li r4,0x24
    // / li r4,0x25). 36 == the unreliable per-frame update message; 37 == the reliable result.
    static const s32 KI_UPDATE_MESSAGE_TYPE            = 36;  // 0x24 -- FburnSuccessUpdateMessage
    static const s32 KI_CHALLENGE_SUCCESS_MESSAGE_TYPE = 37;  // 0x25 -- FburnChallengeSuccessMessage

    // The two console frame rates the relay translates between (X360 cmpwi 0x32 / 0x3C).
    static const s32 KI_FRAMERATE_50HZ = 50;
    static const s32 KI_FRAMERATE_60HZ = 60;

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

        CGS_ASSERT_AT(mpNetworkModule != 0,
            "mpNetworkModule", __FILE__, 68 /* ARTIST 0x8255DD0C */);
        CGS_ASSERT_AT(mpPlayerManager != 0,
            "mpPlayerManager", __FILE__, 69 /* ARTIST 0x8255DD34 */);
        CGS_ASSERT_AT(mpTimeManager != 0,
            "mpTimeManager", __FILE__, 70 /* ARTIST 0x8255DD5C */);

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
    // Prepare / Release: nothing to acquire or free, always ready.
    // -------------------------------------------------------------------------------------------
    bool ChallengeSuccessManager::Prepare()
    {
        return true;
    }

    bool ChallengeSuccessManager::Release()
    {
        return true;
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
        mpNetworkModule = 0;   // +0x5F4
        mpPlayerManager = 0;   // +0x5F8
        mpTimeManager   = 0;   // +0x5FC
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
        CGS_ASSERT_AT(lpInput != 0,
            "lpInput", __FILE__, 290 /* ARTIST 0x8256FEA4 */);
        ProcessNetworkEvents(lpInput->GetNetworkEventQueue());
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
        // DWARF locals cpp:720 liIndex / :721 lpEntry; the found entry leaves the loop and meets
        // the null assert (ARTIST 0x8254B38C tests the computed entry, line 733).
        s32                   liIndex;
        ChallengeSuccessData* lpEntry = 0;
        for (liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            if (maChallengeSuccessData[liIndex].mPlayerID == lPlayerID)
            {
                lpEntry = &maChallengeSuccessData[liIndex];
                break;
            }
        }

        CGS_ASSERT_AT(lpEntry != 0, "lpEntry", __FILE__, 733);
        return lpEntry;
    }

    // -------------------------------------------------------------------------------------------
    // AddPlayer  @ 0x8256BF20
    // Register the update + reliable success message types for a newly-joined player on its
    // NetworkPlayer, claiming a free table slot and stamping it with the player id.
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::AddPlayer(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT_AT(mpPlayerManager != 0,
            "mpPlayerManager", __FILE__, 607 /* ARTIST 0x8256BF50 */);

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer != 0)
        {
            ChallengeSuccessData* lpEntry = GetChallengeSuccessDataEntry(CgsNetwork::KI_INVALID_PLAYER_ID);
            CGS_ASSERT_AT(lpEntry != 0,
                "lpEntry", __FILE__, 619 /* ARTIST 0x8256BF9C */);

            // Unreliable per-frame update message pair (no arrival/delivery callbacks).
            lpNetworkPlayer->RegisterMessageType(KI_UPDATE_MESSAGE_TYPE,
                                                 sizeof(FburnSuccessUpdateMessage),
                                                 &lpEntry->mUpdateMessageSend,
                                                 &lpEntry->mUpdateMessageRecv,
                                                 0, 0, 0);
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
        CGS_ASSERT_AT(mpPlayerManager != 0,
            "mpPlayerManager", __FILE__, 654 /* ARTIST 0x82556A28 */);

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer != 0)
        {
            lpNetworkPlayer->UnRegisterMessageType(KI_UPDATE_MESSAGE_TYPE);
            lpNetworkPlayer->UnRegisterMessageType(KI_CHALLENGE_SUCCESS_MESSAGE_TYPE);

            ChallengeSuccessData* lpEntry = GetChallengeSuccessDataEntry(lPlayerID);
            CGS_ASSERT_AT(lpEntry != 0,
                "lpEntry", __FILE__, 669 /* ARTIST 0x82556A88 */);
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
                // ARTIST 0x8255DF74 reports line 695; the path is a literal (as __FILE__ is): a
                // named path array reorders the loop-hoisted message/file registers.
                CGS_ASSERT_AT(maChallengeSuccessData[liIndex].mPlayerID == CgsNetwork::KI_INVALID_PLAYER_ID,
                    "maChallengeSuccessData[liIndex].mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID",
                    "..\\..\\..\\GameSource\\Network/Managers/BrnChallengeSuccessManager.cpp", 695);
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
        CGS_ASSERT_AT(mpPlayerManager != 0, "mpPlayerManager", __FILE__, 191);
        CGS_ASSERT_AT(mpTimeManager != 0, "mpTimeManager", __FILE__, 192);

        const u16 lu16CurrentFrame = mpTimeManager->GetU16FrameCount();

        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            ChallengeSuccessData* lpSuccessData = &maChallengeSuccessData[liIndex];
            CGS_ASSERT_AT(lpSuccessData != 0, "lpSuccessData", __FILE__, 204);

            const NetworkPlayerID lPlayerID = lpSuccessData->mPlayerID;
            if (lPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID
                && mpPlayerManager->IsPlayerTurnToSendRoundRobinMessage(lPlayerID, false, 0))
            {
                FburnSuccessUpdateMessage* lpUpdateMessage = &lpSuccessData->mUpdateMessageSend;
                CGS_ASSERT_AT(lpUpdateMessage != 0, "lpUpdateMessage", __FILE__, 220);
                CGS_ASSERT_AT(!lpUpdateMessage->IsMessageValid(), "!lpUpdateMessage->IsMessageValid()", __FILE__, 221);

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
        CGS_ASSERT_AT(mpPlayerManager != 0, "mpPlayerManager", __FILE__, 247);
        CGS_ASSERT_AT(mpTimeManager != 0, "mpTimeManager", __FILE__, 248);

        const u16 lu16CurrentFrame = mpTimeManager->GetU16FrameCount();

        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            ChallengeSuccessData* lpSuccessData = &maChallengeSuccessData[liIndex];
            CGS_ASSERT_AT(lpSuccessData != 0, "lpSuccessData", __FILE__, 261);

            if (lpSuccessData->mPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID)
            {
                FburnChallengeSuccessMessage* lpChallengeSuccessMessage =
                    &lpSuccessData->mChallengeSuccessMessageSend;
                CGS_ASSERT_AT(lpChallengeSuccessMessage != 0, "lpChallengeSuccessMessage", __FILE__, 271);
                CGS_ASSERT_AT(!lpChallengeSuccessMessage->IsMessageValid(),
                           "!lpChallengeSuccessMessage->IsMessageValid()", __FILE__, 272);

                // DWARF has no frames local here (cpp:255..257): the call is the argument.
                lpChallengeSuccessMessage->PrepareForSend(lu16CurrentFrame,
                                                          mpTimeManager->GetFramesSinceStart(),
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
        // DWARF locals cpp:310 / :315..:319 (the two ints are not initialised: ARTIST stores
        // nothing before Retrieve fills them).
        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            LastSecondChallengeSuccess lLastSecondChallengeSuccess;
            s32                        liFramesSinceStart;
            s32                        liReceivedActionIndex;

            ChallengeSuccessData* lpSuccessData = &maChallengeSuccessData[liIndex];
            CGS_ASSERT_AT(lpSuccessData != 0, "lpSuccessData", __FILE__, 322);

            FburnSuccessUpdateMessage* lpReceivedMessage = &lpSuccessData->mUpdateMessageRecv;
            CGS_ASSERT_AT(lpReceivedMessage != 0, "lpReceivedMessage", __FILE__, 324);

            if (lpReceivedMessage->Retrieve(&liFramesSinceStart, &liReceivedActionIndex,
                                            &lLastSecondChallengeSuccess))
            {
                HandleReceivedUpdateMessage(lpSuccessData->mPlayerID,
                                            liFramesSinceStart,
                                            liReceivedActionIndex,
                                            &lLastSecondChallengeSuccess);
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
        CGS_ASSERT_AT(mpNetworkModule != 0, "mpNetworkModule", __FILE__, 359);
        CGS_ASSERT_AT(mpNetworkModule->GetNetworkManager() != 0,
                   "mpNetworkModule->GetNetworkManager()", __FILE__, 360);

        const s32 leLocalConsoleFrameRate =
            static_cast<s32>(mpNetworkModule->GetNetworkManager()->GetLocalConsoleFrameRate());
        CGS_ASSERT_AT(leLocalConsoleFrameRate == KI_FRAMERATE_50HZ
                   || leLocalConsoleFrameRate == KI_FRAMERATE_60HZ,
                   "( leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ ) || "
                   "( leLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ )", __FILE__, 363);

        CGS_ASSERT_AT(mpPlayerManager != 0, "mpPlayerManager", __FILE__, 365);
        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        CGS_ASSERT_AT(lpNetworkPlayer != 0, "lpNetworkPlayer", __FILE__, 367);

        const s32 leRemoteConsoleFrameRate =
            static_cast<s32>(lpNetworkPlayer->GetRemoteConsoleFrameRate());
        CGS_ASSERT_AT(leRemoteConsoleFrameRate == KI_FRAMERATE_50HZ
                   || leRemoteConsoleFrameRate == KI_FRAMERATE_60HZ,
                   "( leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ ) || "
                   "( leRemoteConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ )", __FILE__, 370);

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
            CGS_ASSERT_AT(mpTimeManager != 0, "mpTimeManager", __FILE__, 390);
            // Map a 60 Hz frame index onto the 50 Hz timeline (X360: 5 * frame / 6 - 50).
            liTranslatedFrame = 5 * liFramesSinceNetworkStart / 6 - KI_FRAMERATE_50HZ;
        }
        else
        {
            // Sender is 50 Hz, we are 60 Hz.
            TranslateSuccessUpdate50HzTo60Hz(&lTranslatedSuccess, lpSuccessUpdate);
            CGS_ASSERT_AT(mpTimeManager != 0, "mpTimeManager", __FILE__, 405);
            // Map a 50 Hz frame index onto the 60 Hz timeline (X360: 6 * frame / 5 - 60).
            liTranslatedFrame = 6 * liFramesSinceNetworkStart / 5 - KI_FRAMERATE_60HZ;
        }

        // DWARF (cpp:350) NetworkOutFburnChallengeSuccessUpdateEvent lSuccessUpdateEvent, filled
        // through the module's own GetActiveRaceCarIndex forward (DecFIGS BrnNetworkModule.h:545:
        // no mbIsUpdating assert, ARTIST 0x82565290 adds mGameStateToNetworkInterface's offset in
        // place) and posted with NetworkEventQueue::AddEvent<T> (BrnNetworkEvent.h:91; ARTIST's
        // li r6,0x18 / li r5,0x43 / addi r4 at 0x825652E0).
        BrnNetworkModuleIO::NetworkOutFburnChallengeSuccessUpdateEvent lSuccessUpdateEvent;
        lSuccessUpdateEvent.mChallengeSuccessUpdate = lTranslatedSuccess;
        lSuccessUpdateEvent.meActiveRaceCarIndex    = mpNetworkModule->GetActiveRaceCarIndex(lPlayerID);
        lSuccessUpdateEvent.miChallengeUpdateFrame  = liTranslatedFrame;
        lSuccessUpdateEvent.miActionIndex           = liActionIndex;

        CGS_ASSERT_AT(mpNetworkModule->GetNetworkEventQueue() != 0,
                   "mpNetworkModule->GetNetworkEventQueue()", __FILE__, 419);
        mpNetworkModule->GetNetworkEventQueue()->AddEvent(lSuccessUpdateEvent);
    }

    // -------------------------------------------------------------------------------------------
    // ProcessNetworkEvents  @ 0x8255DDC8
    // Walk the GameState's post-sim network-event queue: cache a fresh success-update (arming the
    // send gate), send the reliable challenge-success result out, and clear the send gate when the
    // challenge has ended.
    // -------------------------------------------------------------------------------------------
    void ChallengeSuccessManager::ProcessNetworkEvents(
        const BrnNetworkModuleIO::NetworkEventQueue* lpInputNetworkEventQueue)
    {
        // DWARF locals cpp:521..523; the cases in their DWARF line order (:536 success update,
        // :553 freeburn challenge, :569 challenge success). ARTIST 0x8255DE44 compares 0x26 /
        // 0x28 / 0x29 and GetNextEvent writes the next event back into lpEvent (var_60).
        const CgsModule::Event* lpEvent;
        s32                     liEventSize;
        s32                     liEventType;

        CGS_ASSERT_AT(lpInputNetworkEventQueue != 0, "lpInputNetworkEventQueue", __FILE__, 526);

        liEventType = lpInputNetworkEventQueue->GetFirstEvent(&lpEvent, &liEventSize);
        while (lpEvent != 0)
        {
            switch (liEventType)
            {
            case BrnNetworkModuleIO::NetworkInFburnSuccessUpdateEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInFburnSuccessUpdateEvent* lpSuccessEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInFburnSuccessUpdateEvent*>(lpEvent);
                CGS_ASSERT_AT(lpSuccessEvent != 0, "lpSuccessEvent", __FILE__, 540);
                mbSendUpdateSuccessMessage    = true;
                mLastSecondChallengeSuccess   = lpSuccessEvent->mChallengeSuccessUpdate;
                miChallengeSuccessUpdateIndex = lpSuccessEvent->miActionIndex;
                break;
            }

            case BrnNetworkModuleIO::NetworkInFreeburnChallengeEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInFreeburnChallengeEvent* lpChallengeEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInFreeburnChallengeEvent*>(lpEvent);
                CGS_ASSERT_AT(lpChallengeEvent != 0, "lpChallengeEvent", __FILE__, 557);
                if (lpChallengeEvent->meEventType == BrnNetworkModuleIO::E_CHALLENGE_EVENT_ENDED)
                {
                    mbSendUpdateSuccessMessage = false;
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInFburnChallengeSuccessEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInFburnChallengeSuccessEvent* lpChallengeSuccessEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInFburnChallengeSuccessEvent*>(lpEvent);
                CGS_ASSERT_AT(lpChallengeSuccessEvent != 0, "lpChallengeSuccessEvent", __FILE__, 573);
                HandleSendingChallengeSuccessMessage(lpChallengeSuccessEvent->mabSuccessfulActions,
                                                     lpChallengeSuccessEvent->mabAccumulationThisFrame,
                                                     lpChallengeSuccessEvent->mafActionScores);
                break;
            }

            default:
                break;
            }

            liEventType = lpInputNetworkEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
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
        // DWARF locals (cpp:752..754); the image's assert texts name lChallengeSucessEvent.
        BrnNetworkModuleIO::NetworkOutFburnChallengeSuccessEvent lChallengeSucessEvent;
        ChallengeSuccessManager* lpChallengeManager = static_cast<ChallengeSuccessManager*>(lpUserData);
        CGS_ASSERT_AT(lpChallengeManager != 0, "lpChallengeManager", __FILE__, 758);

        FburnChallengeSuccessMessage* lpChallengeSuccessMessage =
            reinterpret_cast<FburnChallengeSuccessMessage*>(lpMessage);
        CGS_ASSERT_AT(lpChallengeSuccessMessage != 0, "lpChallengeSuccessMessage", __FILE__, 762);

        // ARTIST's text names the frame member miFramesSinceNetworkStart; the shared event type
        // still calls it miChallengeUpdateFrame (+0x08).
        CGS_ASSERT_AT(lpChallengeSuccessMessage->Retrieve(&lChallengeSucessEvent.miChallengeUpdateFrame,
                                                          lChallengeSucessEvent.mabSuccessfulActions,
                                                          lChallengeSucessEvent.mabAccumulationThisFrame,
                                                          lChallengeSucessEvent.mafActionScores),
                   "lpChallengeSuccessMessage->Retrieve( &lChallengeSucessEvent.miFramesSinceNetworkStart, "
                   "lChallengeSucessEvent.mabSuccessfulActions, lChallengeSucessEvent.mabAccumulationThisFrame, "
                   "lChallengeSucessEvent.mafActionScores )", __FILE__, 768);

        CGS_ASSERT_AT(lpChallengeManager->mpNetworkModule != 0,
                   "lpChallengeManager->mpNetworkModule", __FILE__, 771);
        lChallengeSucessEvent.meActiveRaceCarIndex =
            lpChallengeManager->mpNetworkModule->GetActiveRaceCarIndex(liFromPlayerID);

        CGS_ASSERT_AT(lpChallengeManager->mpNetworkModule->GetNetworkEventQueue() != 0,
                   "lpChallengeManager->mpNetworkModule->GetNetworkEventQueue()", __FILE__, 775);
        lpChallengeManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(lChallengeSucessEvent);
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
            CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "WARNING: Fack Nack found in ";
            CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << __FUNCTION__;
            CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "\n";
        }
    }
} // namespace BrnNetwork
