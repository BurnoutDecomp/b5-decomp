// ===================================================================================
// BrnNetwork::ChallengeSuccessManager -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnChallengeSuccessManager.h
//
// The online "last-second challenge success" relay. It keeps a small fixed table of
// per-player slots (KI_MAX_NETWORK_PLAYERS == 7), each holding a send/receive
// FburnSuccessUpdateMessage pair (the per-frame "I had a last-second success" bit set the
// round-robin pump sends out) plus a send/receive FburnChallengeSuccessMessage pair (the
// reliable two-action challenge-success result). Each frame, when a freeburn-challenge
// success update is pending, it round-robins the update message out; on the receive side it
// turns inbound update/result messages back into NetworkOut events the GameState consumes,
// translating the frame timing between 50 Hz and 60 Hz consoles.
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../BrnChallengeSuccessManager.h:69/126), gated against
// the X360 ARTIST binary. The X360 manager-`this` member offsets pin the layout exactly
// (Construct @ 0x8255DCD8 / Destruct @ 0x82555D48 / AddPlayer @ 0x8256BF20):
//   maChallengeSuccessData[7]    @ +0      (7 * 216-byte ChallengeSuccessData stride == 1512)
//   mLastSecondChallengeSuccess  @ +1512   (8 bytes; std 0 in ctor/dtor; set from event 40 payload)
//   miChallengeSuccessUpdateIndex@ +1520   (s32; stw 0; set to event-40 miActionIndex)
//   mpNetworkModule              @ +1524
//   mpPlayerManager              @ +1528
//   mpTimeManager                @ +1532
//   mbSendUpdateSuccessMessage   @ +1536   (bool; stb 0; the "an update is pending" gate)
//
// X360 stride proof (Construct loop, r31 = this+0x38, stride 0xD8 == 216):
//   mPlayerID                    @ entry +0    (stw -1)
//   mUpdateMessageSend           @ entry +8    (FburnSuccessUpdateMessage, X360 size 0x30)
//   mUpdateMessageRecv           @ entry +0x38 (FburnSuccessUpdateMessage)
//   mChallengeSuccessMessageSend @ entry +0x68 (FburnChallengeSuccessMessage, X360 size 0x38)
//   mChallengeSuccessMessageRecv @ entry +0xA0 (FburnChallengeSuccessMessage)
// (4 + 4 pad + 0x30 + 0x30 + 0x38 + 0x38 == 0xD8.) AddPlayer registers message type 36 (0x24)
// for the update pair and type 37 (0x25) for the challenge-success pair against the
// NetworkPlayer; the message offsets the registrars take (+8 / +0x38 / +0x68 / +0xA0) confirm
// the member order.
//
// This project targets SEMANTIC parity on the host (x64, 8-byte vtable pointers), so the host
// sizeof of the embedded messages differs from the X360 32-bit halves; members are accessed
// strictly BY NAME, which is stride-correct on either target. The 0xD8/0x30/0x38 strides are
// documented from the X360 asm only and intentionally NOT static_asserted.
//
// FUNCTION OWNERSHIP: 13 functions are bodied in the sibling .cpp (the two reliable-message
// callbacks _ChallengeSuccessMessageArrived/DeliveredCallback are static; the delivered one is
// an empty X360 stub). Prepare/Release are not exported by the X360 build's ledger for this TU
// (the base ModuleManager handles them) -- the DWARF declares them but they are PS3-only here,
// so they are NOT modelled (see the DWARF-gating rule).
// ===================================================================================
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"            // BrnNetwork::NetworkPlayerID, EActiveRaceCarIndex
#include "GameSource/Network/Messages/BrnFburnSuccessUpdateMessage.h"  // FburnSuccessUpdateMessage, FburnChallengeSuccessUpdateAction::LastSecondChallengeSuccess
#include "GameSource/Network/Messages/BrnFburnChallengeSuccessMessage.h" // FburnChallengeSuccessMessage

namespace CgsNetwork
{
    struct PlayerManager;   // pointer-only (DWARF h:141)
    struct TimeManager;     // pointer-only (DWARF h:142)
    struct ReliableMessage; // arrived-callback param
    struct SignalMessage;   // delivered-callback param
}

namespace BrnNetwork
{
    class BrnNetworkModule; // pointer-only (DWARF h:140)

    namespace BrnNetworkModuleIO
    {
        struct OutputBuffer;               // ProcessBeforeSimulation param
        struct PostSimulationInputBuffer;  // ProcessAfterSimulation param
        class  NetworkEventQueue;          // ProcessNetworkEvents param (CgsModule::VariableEventQueue<14000,16>)
    }

    // The "last-second challenge success" 8-byte bit set the update message carries. Reuse the
    // committed home (BrnFburnSuccessUpdateMessage.h spells it
    // FburnChallengeSuccessUpdateAction::LastSecondChallengeSuccess); the manager stores/loads it
    // whole (8-byte std/ld), so the typedef keeps every call site reading it by name.
    typedef BrnGameState::FburnChallengeSuccessUpdateAction::LastSecondChallengeSuccess
        LastSecondChallengeSuccess;

    // The player-table width (the X360 loops run 7 times; the asserts spell KI_MAX_NETWORK_PLAYERS).
    // Modelled locally (no committed shared home), mirroring the AggressiveDriving / DirtyTrick
    // siblings.
    const s32 KI_MAX_NETWORK_PLAYERS = 7;

    struct ChallengeSuccessManager
    {
        // DWARF BrnChallengeSuccessManager.h:126 -- one player's send/receive update + challenge-
        // success message pairs. X360 216-byte (0xD8) stride (proof above).
        struct ChallengeSuccessData
        {
            NetworkPlayerID              mPlayerID;                    // entry +0    (DWARF :128; ctor -1)
            FburnSuccessUpdateMessage    mUpdateMessageSend;           // entry +8    (DWARF :129)
            FburnSuccessUpdateMessage    mUpdateMessageRecv;           // entry +0x38 (DWARF :130)
            FburnChallengeSuccessMessage mChallengeSuccessMessageSend; // entry +0x68 (DWARF :131)
            FburnChallengeSuccessMessage mChallengeSuccessMessageRecv; // entry +0xA0 (DWARF :132)
        };

        // ---- lifecycle / per-frame API (bodied in this TU) -------------------------
        void Construct(BrnNetworkModule* lpNetworkModule,
                       CgsNetwork::PlayerManager* lpPlayerManager,
                       CgsNetwork::TimeManager* lpTimeManager);
        void Destruct();
        void ProcessBeforeSimulation(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer);
        void ProcessAfterSimulation(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput);
        void AddPlayer(NetworkPlayerID lPlayerID);
        void RemovePlayer(NetworkPlayerID lPlayerID);
        void Disconnected();

    private:
        // ---- internals (bodied in this TU) -----------------------------------------
        ChallengeSuccessData* GetChallengeSuccessDataEntry(NetworkPlayerID lPlayerID);
        void HandleSendingUpdateMessage();
        void HandleSendingChallengeSuccessMessage(const bool* lpaSuccessfulActions,
                                                  const bool* lpaAccumulationThisFrame,
                                                  const f32*  lpaActionScores);
        void CheckForNewUpdateMessages();
        void HandleReceivedUpdateMessage(NetworkPlayerID lPlayerID,
                                         s32 liFramesSinceNetworkStart,
                                         s32 liActionIndex,
                                         const LastSecondChallengeSuccess* lpSuccessUpdate);
        void ProcessNetworkEvents(const BrnNetworkModuleIO::NetworkEventQueue* lpInputNetworkEventQueue);
        void TranslateSuccessUpdate60HzTo50Hz(LastSecondChallengeSuccess* lpTranslatedSuccess,
                                              const LastSecondChallengeSuccess* lpSuccessUpdateToTranslate);
        void TranslateSuccessUpdate50HzTo60Hz(LastSecondChallengeSuccess* lpTranslatedSuccess,
                                              const LastSecondChallengeSuccess* lpSuccessUpdateToTranslate);

        // Reliable-message arrival / delivery callbacks registered with the per-player message
        // table (type 37 / 0x25). Static (the X360 passes `this` through the lpUserData void* the
        // NetworkPlayer message-type registrars carry, not an implicit this). The delivered
        // callback is an empty X360 stub.
        static void _ChallengeSuccessMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                            NetworkPlayerID liFromPlayerID,
                                                            void* lpUserData);
        static void _ChallengeSuccessMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                             CgsNetwork::SignalMessage* lpMessage,
                                                             NetworkPlayerID liToPlayerID,
                                                             void* lpUserData);

        // ---- data layout (DWARF h:135-144; X360 byte offsets above) ----------------
        ChallengeSuccessData maChallengeSuccessData[KI_MAX_NETWORK_PLAYERS]; // +0      (7 x 0xD8 stride)
        LastSecondChallengeSuccess mLastSecondChallengeSuccess;              // X360 @ +1512 (8 bytes)
        s32                  miChallengeSuccessUpdateIndex;                  // X360 @ +1520
        BrnNetworkModule*          mpNetworkModule;                          // X360 @ +1524
        CgsNetwork::PlayerManager* mpPlayerManager;                          // X360 @ +1528
        CgsNetwork::TimeManager*   mpTimeManager;                            // X360 @ +1532
        bool                 mbSendUpdateSuccessMessage;                     // X360 @ +1536
    };
} // namespace BrnNetwork
