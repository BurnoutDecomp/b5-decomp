// ============================================================================
// b5-decomp/src/GameSource/Network/Managers/BrnNetworkTrafficManager.h
// ============================================================================
// BrnNetwork::TrafficManager -- the online traffic synchroniser embedded in
// BrnNetworkManager at +0x6B00 (console sizeof 0x184C0 == 99,520 bytes).
//
// It keeps one TrafficSyncData slot per remote network player (7 slots, each with a
// send/receive copy of the four traffic messages: hull sync, crashing traffic,
// restart traffic and traffic hash), a 35-deep buffer of crashing-traffic messages
// received ahead of the local frame, the pending traffic-restart request, and a
// 128-deep ring of the local traffic hashes used to detect a diverged simulation.
//
// LAYOUT
// ------
// Class key, member names and member order follow the reference type dump
// (struct BrnNetwork::TrafficManager). Every console offset below is attested by the
// constructor, Construct, Destruct, ResetData, ClearCrashingTraffic and the per-frame
// functions; _AssertLayout() pins them in a 32-bit build, where the committed message
// headers reproduce their console widths. On the x64 host pointers widen, so code
// reaches every member by name only.
//
// The per-slot CrashingTrafficData arrays are default-constructed by the constructor
// (the console runs a vector-constructor loop over each 24-record array whose element
// constructor is an empty, identical-code-folded body); the aggregate is homed in
// BrnCrashingTrafficMessage.h next to the message that carries it.
// ============================================================================
#pragma once

#include <cstddef>                                                                // offsetof (_AssertLayout)

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                                          // EActiveRaceCarIndex
#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"                      // CgsContainers::FixedRingBuffer<T,N>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                        // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"                     // CgsSystem::EFrameRate
#include "GameSource/CompilerDefines/gameshared_network_defines.h"                // ::KI_MAX_NETWORK_PLAYERS
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                       // BrnNetwork::NetworkPlayerID
#include "GameSource/Network/Messages/BrnHullSyncMessage.h"                       // HullSyncMessage, BufferedHullsToActivate
#include "GameSource/Network/Messages/BrnCrashingTrafficMessage.h"                // CrashingTrafficMessage, CrashingTrafficData
#include "GameSource/Network/Messages/BrnRestartTrafficMessage.h"                 // RestartTrafficMessage
#include "GameSource/Network/Messages/BrnTrafficHashMessage.h"                    // TrafficHashMessage

namespace CgsNetwork
{
    struct PlayerManager;       // pointer-only (mpPlayerManager)
    struct TimeManager;         // pointer-only (mpTimeManager)
    struct NetworkPlayer;       // RegisterMessages parameter
    struct ReliableMessage;     // arrived-callback parameter
    struct SignalMessage;       // delivered-callback parameter
}

namespace BrnNetwork
{
    class BrnNetworkModule;     // pointer-only (mpNetworkModule)

    struct TrafficManager
    {
    public:
        // One per-player synchronisation slot. Console stride 0x1160.
        struct TrafficSyncData
        {
            NetworkPlayerID         mPlayerID;                        // +0x0000
            HullSyncMessage         mHullSyncMessageSend;             // +0x0004
            HullSyncMessage         mHullSyncMessageRecv;             // +0x005C
            CrashingTrafficMessage  mCrashingTrafficMessageSend;      // +0x00C0
            CrashingTrafficMessage  mCrashingTrafficMessageRecv;      // +0x0870
            RestartTrafficMessage   mRestartTrafficMessageSend;       // +0x1020
            RestartTrafficMessage   mRestartTrafficMessageRecv;       // +0x107C
            TrafficHashMessage      mTrafficHashMessageSend;          // +0x10D8
            TrafficHashMessage      mTrafficHashMessageRecv;          // +0x1100
            BufferedHullsToActivate mBufferedHullActivates;           // +0x1128
            bool                    mbIsThereCrashingTrafficToSend;   // +0x1158
        };

        // A crashing-traffic message received for a frame the local simulation has not
        // reached yet. Console stride 0x790.
        struct BufferedMessage
        {
            u16                 mu16FramesSinceRoundStart;                              // +0x00
            NetworkPlayerID     mOwningNetworkPlayerID;                                 // +0x04
            s32                 miCrashingTrafficDataCount;                             // +0x08
            CrashingTrafficData maCrashingTrafficData[KI_MAX_CRASHING_TRAFFIC_IN_MESSAGE]; // +0x10

            BufferedMessage& operator=(const BufferedMessage& lOther);
        };

        // The pending traffic-restart request (console 18 bytes).
        struct BufferedRestartMessage
        {
            u16 mau16ActiveHulls[8];                                  // +0x00
            u16 mu16RestartFrame;                                     // +0x10
        };

        // One stored local traffic hash (console 4 bytes).
        struct TrafficHash
        {
            u16 muTrafficHash;                                        // +0x00
            u16 muUpdate10HzFrame;                                    // +0x02
        };

        TrafficManager();

        // ---- lifecycle ----------------------------------------------------------------
        void Construct(BrnNetworkModule* lpNetworkModule,
                       CgsNetwork::PlayerManager* lpPlayerManager,
                       CgsNetwork::TimeManager* lpTimeManager);
        bool Prepare();
        bool Release();
        void Destruct();

        // ---- per-frame -------------------------------------------------------------------
        void Update(bool lbInGame);

        // ---- session hooks ---------------------------------------------------------------
        void AddPlayer(NetworkPlayerID lPlayerID);
        void RemovePlayer(NetworkPlayerID lPlayerID);
        void Disconnected();
        void ClearCrashingTraffic();
        void OnGameLaunching();
        void OnGameStart();
        void OnRoundStart(bool lbSuppressHullSyncsUntilReset);
        void OnRoundFinish();
        void OnLeaveGame();
        void SuppressTrafficRestart(bool lbSuppress);
        void RestartNetworkTraffic(NetworkPlayerID lPlayerID);

    private:
        static const s32 KI_REPORT_CRASHING_TRAFFIC_DELAY_50HZ     = 50;
        static const s32 KI_REPORT_CRASHING_TRAFFIC_DELAY_60HZ     = 60;
        static const s32 KI_MAX_BUFFERED_CRASHING_TRAFFIC_MESSAGES = 35;
        static const s32 KI_NUM_TRAFFIC_HASH_TO_STORE              = 128;
        static const s32 KI_UPDATE_FRAME_WRAP_MARGIN               = 10000;

        void ResetData();
        TrafficSyncData* GetTrafficSyncDataEntry(NetworkPlayerID lPlayerID);

        // Inlined at every call site: the index of the slot owned by lPlayerID, or -1.
        s32 GetIndexOfPlayerInTrafficData(NetworkPlayerID lPlayerID)
        {
            for (s32 liIndex = 0; liIndex < ::KI_MAX_NETWORK_PLAYERS; ++liIndex)
            {
                if (maTrafficData[liIndex].mPlayerID == lPlayerID)
                {
                    return liIndex;
                }
            }
            return -1;
        }

        void RegisterMessages(CgsNetwork::NetworkPlayer* lpNetworkPlayer, TrafficSyncData* lpDataEntry);

        void UpdateHullSync();
        void HandleHullSyncMessage(EActiveRaceCarIndex leActiveRaceCarIndex,
                                   u16 lu16TrafficUpdateToActivate, s32 liHull);
        void SendHullSyncMessages();

        void SendCrashingTrafficMessages(bool lbInGame);
        bool GetCrashingTrafficData(CrashingTrafficData* lpaCrashingTrafficData, s32* lpiCount);
        void ReceiveCrashingTrafficMessages();
        void StoreBufferedMessage(NetworkPlayerID lPlayerID, CrashingTrafficMessage* lpMessage,
                                  CgsSystem::EFrameRate leLocalConsoleFrameRate,
                                  CgsSystem::EFrameRate leRemoteConsoleFrameRate);
        BufferedMessage* RetrieveBufferedMessage(u16 lu16Frame);
        u16  ConvertReceivedMessageFrameToLocalFrame(u16 lu16Frame,
                                                     CgsSystem::EFrameRate leLocalConsoleFrameRate,
                                                     CgsSystem::EFrameRate leRemoteConsoleFrameRate);
        u16  ConvertLocalFrameToReceivedMessageFrame(u16 lu16Frame,
                                                     CgsSystem::EFrameRate leLocalConsoleFrameRate,
                                                     CgsSystem::EFrameRate leRemoteConsoleFrameRate);
        bool IsMessageFromBeforeTrafficReset(u16 lu16MessageFrame, NetworkPlayerID lPlayerID);
        bool IsTrafficSystemResetPending();
        void RemoveBufferedMessage(BufferedMessage* lpBufferedMessage);
        void DeleteOutOfDateBufferedMesssages(u16 lu16Frame);

        void UpdateRestartTraffic();
        void OnTrafficRestarted();
        void HandleSendingRestartTrafficMessages();
        void ProcessBufferedRestartTrafficMessages();
        void BufferRestartTrafficMessage(BufferedRestartMessage lRestartMessage);
        bool IsTrafficRestartRequired(u16* lpauOutActiveHulls, u16* lpuOutRestartFrame);

        static void _HullSyncMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                    NetworkPlayerID lPlayerID, void* lpUserData);
        static void _HullSyncMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                      CgsNetwork::SignalMessage* lpMessage,
                                                      NetworkPlayerID lPlayerID, void* lpUserData);
        static void _RestartTrafficMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                          NetworkPlayerID lPlayerID, void* lpUserData);
        static void _RestartTrafficMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                            CgsNetwork::SignalMessage* lpMessage,
                                                            NetworkPlayerID lPlayerID, void* lpUserData);

        void UpdateTrafficHashing(bool lbInGame);
        void StoreTrafficHash(u16 lu16Update10HzFrame, u16 lu16TrafficHash);
        void SendTrafficHashingMessage(bool lbInGame);
        void ReceiveTrafficHashingMessages();
        bool FindHashForFrame(u16 lu16Update10HzFrame, u16* lpu16OutHash) const;

        // Inlined at its call site: the signed distance between two 16-bit update frames,
        // unwrapping whichever one lags the other by more than the wrap margin.
        s32 CalcWrappedUpdateFrameDifference(s32 liFrameA, s32 liFrameB) const
        {
            if (liFrameA + KI_UPDATE_FRAME_WRAP_MARGIN < liFrameB)
            {
                liFrameA += 0x10000;
            }
            else if (liFrameB + KI_UPDATE_FRAME_WRAP_MARGIN < liFrameA)
            {
                liFrameB += 0x10000;
            }
            return liFrameA - liFrameB;
        }

        // Console offsets pinned in a 32-bit build; inert on the x64 host.
        static void _AssertLayout();

        BufferedMessage             maBufferedCrashingTrafficMessages[KI_MAX_BUFFERED_CRASHING_TRAFFIC_MESSAGES]; // +0x00000
        TrafficSyncData             maTrafficData[::KI_MAX_NETWORK_PLAYERS];      // +0x108B0
        BufferedRestartMessage      mBufferedRestartTrafficMessage;             // +0x18250
        s32                         miNumMessagesBuffered;                      // +0x18264
        s32                         miNumRestartMessagesBuffered;               // +0x18268
        u16                         mu16LastBufferReadFrame;                    // +0x1826C
        u16                         mu16LastTrafficResetFrame;                  // +0x1826E
        u16                         mu16NumFramesSinceLastReset;                // +0x18270
        CgsContainers::BitArray<7>  mPendingTrafficResetBitArray;               // +0x18278
        bool                        mbHasRoundStarted;                          // +0x18280
        bool                        mbRestartNetworkTraffic;                    // +0x18281
        bool                        mbSuppressTrafficRestart;                   // +0x18282
        bool                        mbSuppressingHullSyncsUntilReset;           // +0x18283
        BrnNetworkModule*           mpNetworkModule;                            // +0x18284
        CgsNetwork::PlayerManager*  mpPlayerManager;                            // +0x18288
        CgsNetwork::TimeManager*    mpTimeManager;                              // +0x1828C
        bool                        mbSyncingTime;                              // +0x18290
        u16                         muLastHashUpdate10HzFrame;                  // +0x18292
        u16                         muLastTrafficHash;                          // +0x18294
        bool                        mbLastHashDataValid;                        // +0x18296
        CgsContainers::FixedRingBuffer<TrafficHash, KI_NUM_TRAFFIC_HASH_TO_STORE> maStoredTrafficHashes; // +0x18298
        u16                         muCurrentTrafficUpdateFrame;                // +0x184AC
        bool                        mbHasTrafficDiverged;                       // +0x184AE
        bool                        mbIsThisMachineInThePast;                   // +0x184AF
        bool                        mbShowTrafficDivergence;                    // +0x184B0
        s32                         miInThePastAmount;                          // +0x184B4
    };

    inline void TrafficManager::_AssertLayout()
    {
        // The embedded message types must match their console widths first; a failure here
        // points at the message header, not at this class.
        static_assert(sizeof(void*) != 4 || sizeof(HullSyncMessage)        == 0x58,  "HullSyncMessage console size");
        static_assert(sizeof(void*) != 4 || sizeof(CrashingTrafficMessage) == 0x7B0, "CrashingTrafficMessage console size");
        static_assert(sizeof(void*) != 4 || sizeof(RestartTrafficMessage)  == 0x5C,  "RestartTrafficMessage console size");
        static_assert(sizeof(void*) != 4 || sizeof(TrafficHashMessage)     == 0x28,  "TrafficHashMessage console size");
        static_assert(sizeof(void*) != 4 || sizeof(CrashingTrafficData)    == 0x50,  "CrashingTrafficData console size");

#define BRN_TM_SLOT_AT(member, off) \
        static_assert(sizeof(void*) != 4 || offsetof(TrafficSyncData, member) == off, "TrafficSyncData::" #member " @ " #off)
        BRN_TM_SLOT_AT(mPlayerID,                     0x0000);
        BRN_TM_SLOT_AT(mHullSyncMessageSend,          0x0004);
        BRN_TM_SLOT_AT(mHullSyncMessageRecv,          0x005C);
        BRN_TM_SLOT_AT(mCrashingTrafficMessageSend,   0x00C0);
        BRN_TM_SLOT_AT(mCrashingTrafficMessageRecv,   0x0870);
        BRN_TM_SLOT_AT(mRestartTrafficMessageSend,    0x1020);
        BRN_TM_SLOT_AT(mRestartTrafficMessageRecv,    0x107C);
        BRN_TM_SLOT_AT(mTrafficHashMessageSend,       0x10D8);
        BRN_TM_SLOT_AT(mTrafficHashMessageRecv,       0x1100);
        BRN_TM_SLOT_AT(mBufferedHullActivates,        0x1128);
        BRN_TM_SLOT_AT(mbIsThereCrashingTrafficToSend, 0x1158);
#undef BRN_TM_SLOT_AT
        static_assert(sizeof(void*) != 4 || sizeof(TrafficSyncData) == 0x1160, "TrafficSyncData stride 0x1160");

        static_assert(offsetof(BufferedMessage, mu16FramesSinceRoundStart)  == 0x00, "BufferedMessage +0x00");
        static_assert(offsetof(BufferedMessage, mOwningNetworkPlayerID)     == 0x04, "BufferedMessage +0x04");
        static_assert(offsetof(BufferedMessage, miCrashingTrafficDataCount) == 0x08, "BufferedMessage +0x08");
        static_assert(offsetof(BufferedMessage, maCrashingTrafficData)      == 0x10, "BufferedMessage +0x10");
        static_assert(sizeof(void*) != 4 || sizeof(BufferedMessage) == 0x790, "BufferedMessage stride 0x790");
        static_assert(offsetof(BufferedRestartMessage, mu16RestartFrame) == 0x10, "BufferedRestartMessage +0x10");
        static_assert(sizeof(TrafficHash) == 4, "TrafficHash stride 4");

#define BRN_TM_AT(member, off) \
        static_assert(sizeof(void*) != 4 || offsetof(TrafficManager, member) == off, "TrafficManager::" #member " @ " #off)
        BRN_TM_AT(maBufferedCrashingTrafficMessages, 0x00000);
        BRN_TM_AT(maTrafficData,                     0x108B0);
        BRN_TM_AT(mBufferedRestartTrafficMessage,    0x18250);
        BRN_TM_AT(miNumMessagesBuffered,             0x18264);
        BRN_TM_AT(miNumRestartMessagesBuffered,      0x18268);
        BRN_TM_AT(mu16LastBufferReadFrame,           0x1826C);
        BRN_TM_AT(mu16LastTrafficResetFrame,         0x1826E);
        BRN_TM_AT(mu16NumFramesSinceLastReset,       0x18270);
        BRN_TM_AT(mPendingTrafficResetBitArray,      0x18278);
        BRN_TM_AT(mbHasRoundStarted,                 0x18280);
        BRN_TM_AT(mbRestartNetworkTraffic,           0x18281);
        BRN_TM_AT(mbSuppressTrafficRestart,          0x18282);
        BRN_TM_AT(mbSuppressingHullSyncsUntilReset,  0x18283);
        BRN_TM_AT(mpNetworkModule,                   0x18284);
        BRN_TM_AT(mpPlayerManager,                   0x18288);
        BRN_TM_AT(mpTimeManager,                     0x1828C);
        BRN_TM_AT(mbSyncingTime,                     0x18290);
        BRN_TM_AT(muLastHashUpdate10HzFrame,         0x18292);
        BRN_TM_AT(muLastTrafficHash,                 0x18294);
        BRN_TM_AT(mbLastHashDataValid,               0x18296);
        BRN_TM_AT(maStoredTrafficHashes,             0x18298);
        BRN_TM_AT(muCurrentTrafficUpdateFrame,       0x184AC);
        BRN_TM_AT(mbHasTrafficDiverged,              0x184AE);
        BRN_TM_AT(mbIsThisMachineInThePast,          0x184AF);
        BRN_TM_AT(mbShowTrafficDivergence,           0x184B0);
        BRN_TM_AT(miInThePastAmount,                 0x184B4);
#undef BRN_TM_AT
        static_assert(sizeof(void*) != 4 || sizeof(TrafficManager) == 0x184C0, "TrafficManager console size 0x184C0");
    }
}
