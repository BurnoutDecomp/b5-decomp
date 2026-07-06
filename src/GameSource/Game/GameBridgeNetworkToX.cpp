// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeNetworkToX.cpp
//
// The BrnGame::BrnGameModule network-bridge family. Each per-frame bridge reads the
// network module's OUTPUT buffer (BrnNetwork::BrnNetworkModuleIO::OutputBuffer /
// its NetworkToGuiInterface + NetworkEventQueue + InGamePlayerStatusInterface, the
// committed homes) and republishes its contents into the GUI + game-state subsystems --
// the mirror image of the controller bridges in GameBridgeControllerToX.cpp and the
// replay bridges in GameBridgeReplayToX.cpp.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   TranslateNetworkInterfaceToGuiEvents  0x823DF938
//   TranslateNetworkEventsToGameEvents    0x823DF9E0
//   TranslateNetworkEventsToGuiEvents     0x823E0900
//   BridgeNetworkToGui                    0x823E9518
//
// HONEST PLACEHOLDERS (FLAGGED):
//   * The CgsGui::GuiModule event sink + its AddGuiEvent<T>(event, buffer) template are
//     the committed by-name placeholders from GameBridgeControllerToX.h. The X360 forms
//     the sink as (this + 7252512) == mpCgsGuiModule (BrnGameModule.hpp @ +7252512);
//     reached here by that committed pointer member (GetGuiEventSink()). The placeholder
//     AddGuiEvent<T> returns void, so the translators return the queue-walk status
//     (GetNextEvent result), exactly as the X360 returns r3 from the final queue call.
//   * The ~50 GUI / game-state event payloads the translators synthesise are NOT homed.
//     Per HARD RULE 3 each is built store-for-store in a correctly-sized RAW local buffer
//     (the exact stack span the asm writes) and passed by name to the templated sink; the
//     event-type TAGS selecting each template instantiation are the real ones (attested by
//     the mangled X360 AddGuiEvent<...> / AddEvent symbols). No field names are invented.
//     The tag types are declared as opaque records in GameBridgeNetworkToX.h.
//   * The network OUTPUT buffer + its NetworkEventQueue (VariableEventQueue<14000,16>),
//     NetworkToGuiInterface, InGamePlayerStatusInterface, and OutputBuffer flag accessors
//     are the REAL committed types (BrnNetworkModuleIO.h), reached by their committed
//     member accessors. Where the Hex-Rays helper (sub_823BC450 / BrnNetworkMo / Net /
//     BrnNetwo / OutputB) has no committed name yet, it is reached by the committed
//     accessor it corresponds to (see inline FLAG) -- byte-offset-faithful, no fabrication.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeNetworkToX.h"
#include "GameSource/Game/GameBridgeControllerToX.h"   // CgsGui::GuiModule placeholder + AddGuiEvent<T>

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"    // VariableEventQueue<14000/1536/32768/4096,16>
#include "GameSource/Network/BrnNetworkModuleIO.h"                  // BrnNetwork::BrnNetworkModuleIO::OutputBuffer + interfaces
#include "GameSource/GameState/BrnGameStateModuleIO.h"              // BrnGameState::GameStateModuleIO::PreWorldInputBuffer + events

#include <cstring>   // std::memcpy (PASS bodies)

// -------------------------------------------------------------------------
// File-scope debug switches (X360 byte_82FB5090..92): default-off (retail path unchanged).
// -------------------------------------------------------------------------
namespace BrnGame
{
    bool byte_82FB5090 = false;
    bool byte_82FB5091 = false;
    u8   byte_82FB5092 = 0;
}

// -------------------------------------------------------------------------
// FLAGGED placeholder bodies for the un-homed by-name event setters + accessors the bridge
// references. Each corresponds to a de-inlined X360 helper / committed accessor whose real
// home is a not-yet-reconstructed TU; provided here as minimal stubs so this TU LINKS (the
// honest-placeholder pattern, mirroring GameBridgeControllerToX.cpp's placeholder tables).
// Promote each to its real home when that subsystem is reconstructed.
// -------------------------------------------------------------------------
namespace BrnGameState
{
    // De-inlined single-word network-player-id setters. FLAG: the within-record store offset
    // is owned by the real event type (unattested); modelled as a store into record word 0
    // (the X360 setters that write a non-zero slot are immediately overwritten there by the
    // bridge's explicit stores, so word-0 is the faithful de-inline for the parity contract).
    void OnlinePlayerAddedEvent::SetNetworkPlayerID(void* lpRecord, s32 liId)      { *reinterpret_cast<s32*>(lpRecord) = liId; }
    void OnlinePlayerRemovedEvent::SetNetworkPlayerID(void* lpRecord, s32 liId)    { *reinterpret_cast<s32*>(lpRecord) = liId; }
    void OnlinePlayerFinalisedEvent::SetNetworkPlayerID(void* lpRecord, s32 liId)  { *reinterpret_cast<s32*>(lpRecord) = liId; }
    void ChangeNetworkCarEvent::SetNetworkPlayerID(void* lpRecord, s32 liId)       { *reinterpret_cast<s32*>(lpRecord) = liId; }
    void RemotePlayerDisconnectedEvent::SetNetworkPlayerID(void* lpRecord, s32 liId){ *reinterpret_cast<s32*>(lpRecord) = liId; }
}

namespace BrnGui
{
    // GuiOverlayWaitFinishRequest::Construct -- real DWARF ctor (X360 0x823E0E98). FLAG: the
    // real body stores the request-name string into the overlay-request record; modelled here
    // as a by-name placeholder that zeroes the record (the request-name binding lands with
    // BrnGuiEventTypeDefs.h).
    void GuiOverlayWaitFinishRequest::Construct(void* lpDest, const char* /*lpcRequestName*/)
    {
        std::memset(lpDest, 0, sizeof(GuiOverlayWaitFinishRequest));
    }
}

namespace BrnNetwork
{
    namespace BrnNetworkModuleIO
    {
        // NetworkToGuiInterface live-revenge record accessor (Hex-Rays "Net"). FLAG: reaches the
        // index-th 16-byte record after the interface header (record array @ +16, stride 16);
        // exact base/stride land with the NetworkToGuiInterface TU.
        const s32* GetLiveRevengeRecord(const void* lpInterface, int liIndex)
        {
            return reinterpret_cast<const s32*>(
                reinterpret_cast<const unsigned char*>(lpInterface) + 16 + liIndex * 16);
        }
        // InGamePlayerStatusInterface player-status-data accessor (Hex-Rays "In"). FLAG: reaches
        // the index-th 312-byte status record (array @ +0, stride 312); exact base lands with the
        // status-interface TU.
        const void* In(const void* lpStatusInterface, int liIndex)
        {
            return reinterpret_cast<const unsigned char*>(lpStatusInterface) + liIndex * 312;
        }
    }

    // Scoreboard-response record heading-type accessor (Hex-Rays "BrnNetwo"). FLAG: the heading
    // type is a record field (exact offset lands with the network scoreboard TU); modelled as
    // the record's +4 word.
    unsigned int GetScoreboardResponseHeadingType(const void* lpRecord)
    {
        return *reinterpret_cast<const unsigned int*>(
            reinterpret_cast<const unsigned char*>(lpRecord) + 4);
    }

    // Online-lobby player-status interface base (Hex-Rays "BrnNetworkMo"). FLAG: exact sub-
    // interface offset within the OutputBuffer lands with the OutputBuffer TU.
    const void* GetOnlineLobbyPlayerStatusInterface(const void* lpOutputBuffer)
    {
        return lpOutputBuffer;
    }
}

namespace BrnGameState
{
    // GameStateModule PreWorld input buffer accessor (Hex-Rays "PreWorldInputB"). FLAG: exact
    // buffer offset within the module lands with the GameState module TU; modelled as the
    // module base (the committed PreWorldInputBuffer::GetGameEventQueue() is applied on top).
    GameStateModuleIO::PreWorldInputBuffer* GetPreWorldInputBuffer(GameStateModule* lpModule)
    {
        return reinterpret_cast<GameStateModuleIO::PreWorldInputBuffer*>(lpModule);
    }
}

namespace BrnGame
{
    using BrnNetwork::BrnNetworkModuleIO::OutputBuffer;

    // Reach the CgsGui::GuiModule event sink (X360 this + 7252512 == mpCgsGuiModule).
    static inline CgsGui::GuiModule* GetGuiEventSink(BrnGameModule* lpModule)
    {
        return *reinterpret_cast<CgsGui::GuiModule**>(
            reinterpret_cast<unsigned char*>(lpModule) + 7252512);
    }

    // GameState-bound network-event records land in the PreWorld input buffer's game-event
    // queue (VariableEventQueue<1536,16>).
    typedef CgsModule::VariableEventQueue<1536, 16>  GameStateEventQueue;
    // The network output's NetworkEventQueue (BrnNetworkModuleIO.h:106).
    typedef CgsModule::VariableEventQueue<14000, 16> NetworkEventQueue;

    // The network OUTPUT buffer's NetworkEventQueue accessor (Hex-Rays "OutputB",
    // X360 free-function shim). FLAG: named in BrnNetworkModuleIO.h as the mGameEventQueue
    // accessor (DWARF GetGameEventQueue); reached here as a const NetworkEventQueue* at the
    // committed +184080 offset until the OutputBuffer TU exposes the exact getter name.
    static inline const NetworkEventQueue* GetNetworkEventQueue(const OutputBuffer* lpOut)
    {
        return reinterpret_cast<const NetworkEventQueue*>(
            reinterpret_cast<const unsigned char*>(lpOut) + 184080);   // OutputBuffer @ +184080
    }

    // The PreWorld input buffer's game-event queue (Hex-Rays "PreWorldInputB" ->
    // GetGameEventQueue). FLAG: the committed GameStateModuleIO.h homes the PreWorldInputBuffer
    // + its GameEventQueue (== VariableEventQueue<1536,16>) but NOT the module-level PreWorld
    // accessor the X360 calls (GameStateModule::GetPreWorldInputBuffer). It is reached here by
    // that by-name accessor (declared in GameBridgeNetworkToX.h; body lands with the GameState
    // module TU) then the committed PreWorldInputBuffer::GetGameEventQueue(). The X360 also uses
    // this pointer's non-null-ness as the case-15 assertion.
    static inline GameStateEventQueue* GetGameEventQueue(BrnGameState::GameStateModule* lpModule)
    {
        BrnGameState::GameStateModuleIO::PreWorldInputBuffer* lpBuffer =
            BrnGameState::GetPreWorldInputBuffer(lpModule);
        return reinterpret_cast<GameStateEventQueue*>(lpBuffer->GetGameEventQueue());
    }

    // =========================================================================
    // TranslateNetworkInterfaceToGuiEvents  (X360 0x823DF938)
    // =========================================================================
    int BrnGameModule::TranslateNetworkInterfaceToGuiEvents(
        void* lpGuiBuffer, const void* lpNetworkToGuiInterface)
    {
        CgsGui::GuiModule* lpSink = GetGuiEventSink(this);

        // Live-revenge record count @ interface+8.
        const int liCount = *reinterpret_cast<const int*>(
            reinterpret_cast<const unsigned char*>(lpNetworkToGuiInterface) + 8);

        for (int liIndex = 0; liIndex < liCount; ++liIndex)
        {
            // BrnNetwork::Net(interface, index) -> const s32[4] record. FLAG: Hex-Rays "Net"
            // helper == NetworkToGuiInterface::GetLiveRevengeRecord(index); reached by name.
            const int* lpRecord = reinterpret_cast<const int*>(
                BrnNetwork::BrnNetworkModuleIO::GetLiveRevengeRecord(lpNetworkToGuiInterface, liIndex));

            // BrnGui::GuiLiveRevengeUpdateEvent (16-byte opaque record). The X360 shuffles the
            // four source words: out[0]=src[3], out[1]=src[2], out[2]=src[0], out[3]=src[1].
            alignas(16) BrnGui::GuiLiveRevengeUpdateEvent lEvent;
            int* lpOut = reinterpret_cast<int*>(&lEvent);
            lpOut[0] = lpRecord[3];
            lpOut[1] = lpRecord[2];
            lpOut[2] = lpRecord[0];
            lpOut[3] = lpRecord[1];

            lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
        }
        return liCount;
    }

    // =========================================================================
    // TranslateNetworkEventsToGameEvents  (X360 0x823DF9E0)
    // =========================================================================
    int BrnGameModule::TranslateNetworkEventsToGameEvents(
        BrnGameState::GameStateModule* lpGameStateModule, const OutputBuffer* lpNetworkOutput)
    {
        // GameBridgeNetworkToX.cpp:465 -- the network event queue.
        const NetworkEventQueue* lpNetworkEventQueue = GetNetworkEventQueue(lpNetworkOutput);
        CGS_ASSERT(lpNetworkEventQueue != 0, "lpNetworkEventQueue");

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        int result = lpNetworkEventQueue->GetFirstEvent(&lpEvent, &liSize);

        while (lpEvent)
        {
            const unsigned char* lpRecord = reinterpret_cast<const unsigned char*>(lpEvent);
            const int* lpW = reinterpret_cast<const int*>(lpRecord);

            alignas(16) unsigned char lOut[160];   // widest record built below is 156 bytes (case 6)
            s32 liOutSize = 0;
            s32 liOutType = 0;
            bool lbEmit = false;

            switch (result)
            {
                case 6:   // InviteRequest -> 156-byte record
                    CGS_ASSERT(lpRecord != 0, "lpInviteRequestEvent");
                    std::memcpy(lOut, lpRecord, 156);
                    liOutSize = 156; liOutType = 57; lbEmit = true;
                    break;
                case 12:  // InstantFreeburn -> 1-byte flag
                    CGS_ASSERT(lpRecord != 0, "lpInstantFreeburnEvent");
                    lOut[0] = lpRecord[0];
                    liOutSize = 1; liOutType = 151; lbEmit = true;
                    break;
                case 14:  // BuddyRemoved -> 16-byte record
                    CGS_ASSERT(lpRecord != 0, "lpBuddyRemovedEvent");
                    std::memcpy(lOut, lpRecord, 16);
                    liOutSize = 16; liOutType = 150; lbEmit = true;
                    break;
                case 15:  // player-car-select status -> 8-byte {status,flag}
                    CGS_ASSERT(lpRecord != 0, "lpEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[110];        // status word (+0x1B8)
                    lOut[4] = lpRecord[476];                           // flag byte (+0x1DC)
                    CGS_ASSERT(lpGameStateModule != 0, "lpGameStateInput");
                    CGS_ASSERT(GetGameEventQueue(lpGameStateModule) != 0,
                               "lpGameStateInput->GetGameEventQueue()");
                    liOutSize = 8; liOutType = 125; lbEmit = true;
                    break;
                case 16:  // OnlinePlayerAdded -> 40-byte record
                    BrnGameState::OnlinePlayerAddedEvent::SetNetworkPlayerID(lOut, lpW[4]);
                    reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[0];
                    reinterpret_cast<u64*>(lOut)[1] = reinterpret_cast<const u64*>(lpW)[1];
                    reinterpret_cast<s32*>(lOut)[5] = lpW[5];
                    *reinterpret_cast<float*>(lOut + 24) = *reinterpret_cast<const float*>(lpRecord + 24);
                    reinterpret_cast<s16*>(lOut)[14] = reinterpret_cast<const s16*>(lpW)[14];
                    reinterpret_cast<s16*>(lOut)[15] = reinterpret_cast<const s16*>(lpW)[15];
                    lOut[32] = lpRecord[32];
                    liOutSize = 40; liOutType = 127; lbEmit = true;
                    break;
                case 17:  // OnlinePlayerRemoved -> 8-byte record
                    BrnGameState::OnlinePlayerRemovedEvent::SetNetworkPlayerID(lOut, lpW[0]);
                    reinterpret_cast<s32*>(lOut)[1] = lpW[5];   // v51[4]=*(v6+20)
                    liOutSize = 8; liOutType = 129; lbEmit = true;
                    break;
                case 18:  // ChangeNetworkCar -> 32-byte record
                    BrnGameState::ChangeNetworkCarEvent::SetNetworkPlayerID(lOut, lpW[5]);
                    reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[0];
                    reinterpret_cast<u64*>(lOut)[1] = reinterpret_cast<const u64*>(lpW)[1];
                    *reinterpret_cast<float*>(lOut + 16) = *reinterpret_cast<const float*>(lpRecord + 16);
                    liOutSize = 32; liOutType = 7; lbEmit = true;
                    break;
                case 20:  // OnlinePlayerFinalised -> 4-byte record
                    BrnGameState::OnlinePlayerFinalisedEvent::SetNetworkPlayerID(lOut, lpW[0]);
                    liOutSize = 4; liOutType = 128; lbEmit = true;
                    break;
                case 21:  // TeamSelection -> 32-byte record
                    CGS_ASSERT(lpRecord != 0, "lpTeamSelectionEvent");
                    std::memcpy(lOut, lpRecord, 32);
                    liOutSize = 32; liOutType = 154; lbEmit = true;
                    break;
                case 23:  // PlayerDisconnected (only when subtype==2) -> 4-byte record
                    CGS_ASSERT(lpRecord != 0, "lpPlayerDisconnectedEvent");
                    if (lpW[1] != 2)
                        break;
                    BrnGameState::RemotePlayerDisconnectedEvent::SetNetworkPlayerID(lOut, lpW[0]);
                    liOutSize = 4; liOutType = 121; lbEmit = true;
                    break;
                case 24:  // empty 1-byte notify
                    liOutSize = 1; liOutType = 124; lbEmit = true;
                    break;
                case 27:  // Launched -> 8-byte {id, flagA, flagB}
                    CGS_ASSERT(lpRecord != 0, "lpLaunchedEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    lOut[4] = lpRecord[16];
                    lOut[5] = lpRecord[20];
                    liOutSize = 8; liOutType = 126; lbEmit = true;
                    break;
                case 34:  // StuntMultiplier -> 16-byte record
                    CGS_ASSERT(lpRecord != 0, "lpStuntMultiplierEvent");
                    reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[0];
                    reinterpret_cast<s32*>(lOut)[2] = lpW[2];
                    reinterpret_cast<s32*>(lOut)[3] = lpW[3];
                    liOutSize = 16; liOutType = 143; lbEmit = true;
                    break;
                case 35:  // RoadRulesPersonalBest -> 72-byte record
                    CGS_ASSERT(lpRecord != 0, "lpRoadRulesPersonalBest");
                    std::memcpy(lOut, lpRecord, 56);
                    reinterpret_cast<s32*>(lOut)[14] = lpW[14];
                    reinterpret_cast<s32*>(lOut)[15] = lpW[15];
                    lOut[64] = lpRecord[64];
                    liOutSize = 72; liOutType = 130; lbEmit = true;
                    break;
                case 36:  // RoadRulesUploaded -> 8-byte {w0,w1}
                    CGS_ASSERT(lpRecord != 0, "lpRoadRulesUploadedEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    reinterpret_cast<s32*>(lOut)[1] = lpW[1];
                    liOutSize = 8; liOutType = 131; lbEmit = true;
                    break;
                case 37:  // RoadRulesDownloaded -> 4-byte word
                    CGS_ASSERT(lpRecord != 0, "lpRoadRulesDownloadedEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    liOutSize = 4; liOutType = 132; lbEmit = true;
                    break;
                case 38:  // NetworkRRConnect -> 4-byte word
                    CGS_ASSERT(lpRecord != 0, "lpNetworkRRConnectEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    liOutSize = 4; liOutType = 133; lbEmit = true;
                    break;
                case 39:  // PlayerConnected -> 4-byte word
                    CGS_ASSERT(lpRecord != 0, "lpPlayerConnectedEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    liOutSize = 4; liOutType = 122; lbEmit = true;
                    break;
                case 44:  // Collectable -> 16-byte record
                    CGS_ASSERT(lpRecord != 0, "lpCollectableEvent");
                    reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[0];
                    reinterpret_cast<s32*>(lOut)[2] = lpW[2];
                    reinterpret_cast<s32*>(lOut)[3] = lpW[3];
                    liOutSize = 16; liOutType = 135; lbEmit = true;
                    break;
                case 45:  // NetworkNewHost -> 2-byte record
                    CGS_ASSERT(lpRecord != 0, "lpNetworkNewHostEvent");
                    lOut[0] = lpRecord[0];
                    lOut[1] = lpRecord[4];
                    liOutSize = 2; liOutType = 140; lbEmit = true;
                    break;
                case 46:  // NetworkCheckpoint -> 8-byte {w0,w1}
                    CGS_ASSERT(lpRecord != 0, "lpNetworkCheckpointEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    reinterpret_cast<s32*>(lOut)[1] = lpW[1];
                    liOutSize = 8; liOutType = 141; lbEmit = true;
                    break;
                case 47:  // NetworkStuntScoreUpdated -> 8-byte {w0,w1}
                    CGS_ASSERT(lpRecord != 0, "lpNetworkStuntScoreUpdatedEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    reinterpret_cast<s32*>(lOut)[1] = lpW[1];
                    liOutSize = 8; liOutType = 142; lbEmit = true;
                    break;
                case 49:  // NetworkRivalCount -> 4-byte word
                    CGS_ASSERT(lpRecord != 0, "lpNetworkRivalCountEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    liOutSize = 4; liOutType = 144; lbEmit = true;
                    break;
                case 50:  // empty 1-byte notify
                    liOutSize = 1; liOutType = 146; lbEmit = true;
                    break;
                case 53:  // TargetScore -> 40-byte record (shuffled vector + trailing words)
                    CGS_ASSERT(lpRecord != 0, "lpTargetScoreEvent");
                    reinterpret_cast<u32*>(lOut)[0] = lpW[1];
                    reinterpret_cast<u32*>(lOut)[1] = lpW[4];
                    reinterpret_cast<u32*>(lOut)[2] = lpW[2];
                    reinterpret_cast<u32*>(lOut)[3] = lpW[3];
                    reinterpret_cast<s32*>(lOut)[8] = lpW[8];
                    lOut[36] = lpRecord[36];
                    liOutSize = 40; liOutType = 152; lbEmit = true;
                    break;
                case 54:  // DldChallengeable -> 8-byte record
                    CGS_ASSERT(lpRecord != 0, "lpDldChallengeableEvent");
                    reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[0];
                    liOutSize = 8; liOutType = 153; lbEmit = true;
                    break;
                case 55:  // empty 1-byte notify
                    liOutSize = 1; liOutType = 79; lbEmit = true;
                    break;
                case 57:  // ImageReceived -> 16-byte record
                    CGS_ASSERT(lpRecord != 0, "lpImageReceivedEvent");
                    reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[0];
                    reinterpret_cast<s32*>(lOut)[2] = lpW[2];
                    reinterpret_cast<s32*>(lOut)[3] = lpW[3];
                    liOutSize = 16; liOutType = 136; lbEmit = true;
                    break;
                case 58:  // MugshotToSave -> 32-byte record (shuffled vector + trailing words)
                    CGS_ASSERT(lpRecord != 0, "lpMugshotToSaveEvent");
                    reinterpret_cast<u32*>(lOut)[0] = lpW[1];
                    reinterpret_cast<u32*>(lOut)[1] = lpW[4];
                    reinterpret_cast<u32*>(lOut)[2] = lpW[2];
                    reinterpret_cast<s32*>(lOut)[6] = lpW[6];
                    reinterpret_cast<s32*>(lOut)[7] = lpW[7];
                    liOutSize = 32; liOutType = 156; lbEmit = true;
                    break;
                case 59:  // AbortCapture -> 1-byte record
                    CGS_ASSERT(lpRecord != 0, "lpAbortCaptureEvent");
                    lOut[0] = lpRecord[0];
                    liOutSize = 1; liOutType = 138; lbEmit = true;
                    break;
                case 60:  // empty 1-byte notify
                    liOutSize = 1; liOutType = 147; lbEmit = true;
                    break;
                case 61:  // NetworkSwitchRunner -> 4-byte word
                    CGS_ASSERT(lpRecord != 0, "lpNetworkSwitchRunnerEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    liOutSize = 4; liOutType = 155; lbEmit = true;
                    break;
                case 62:  // BurnoutSkillz -> 64-byte record {word + 57-byte tail}
                    CGS_ASSERT(lpRecord != 0, "lpBurnoutSkillzNetworkEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    std::memcpy(lOut + 4, lpRecord + 4, 57);
                    liOutSize = 64; liOutType = 139; lbEmit = true;
                    break;
                case 63:  // ShowtimeUpdate -> 8-byte {w0,w1}
                    CGS_ASSERT(lpRecord != 0, "lpShowtimeUpdateNetworkEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    reinterpret_cast<s32*>(lOut)[1] = lpW[1];
                    liOutSize = 8; liOutType = 49; lbEmit = true;
                    break;
                case 64:  // ShowtimeModeSwitch -> 12-byte {w0,w1,flag}
                    CGS_ASSERT(lpRecord != 0, "lpShowtimeModeSwitchNetworkEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    reinterpret_cast<s32*>(lOut)[1] = lpW[1];
                    lOut[8] = lpRecord[8];
                    liOutSize = 12; liOutType = 50; lbEmit = true;
                    break;
                case 65:  // FreeburnChallenge -> variant sub-switch on record[4]
                {
                    CGS_ASSERT(lpRecord != 0, "lpFreeburnChallengeNetworkEvent");
                    const int liVariant = lpW[4];
                    if (liVariant == 0)
                    {
                        reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[1];
                        liOutSize = 8; liOutType = 163;
                    }
                    else if (liVariant == 1)
                    {
                        reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[1];
                        liOutSize = 8; liOutType = 164;
                    }
                    else if (liVariant == 2)
                    {
                        reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[1];
                        reinterpret_cast<s32*>(lOut)[2] = lpW[6];
                        liOutSize = 16; liOutType = 165;
                    }
                    else if (liVariant == 3)
                    {
                        reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[1];
                        reinterpret_cast<s32*>(lOut)[2] = lpW[6];
                        liOutSize = 16; liOutType = 166;
                    }
                    else if (liVariant == 4)
                    {
                        reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[1];
                        reinterpret_cast<s32*>(lOut)[2] = lpW[6];
                        liOutSize = 16; liOutType = 167;
                    }
                    else
                    {
                        reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[1];
                        reinterpret_cast<s32*>(lOut)[2] = lpW[5];
                        liOutSize = 16; liOutType = 168;
                    }
                    lbEmit = true;
                    break;
                }
                case 66:  // ActiveFreeburnChallenge -> 48-byte record
                    // v100[32] = XMemCpy(v6,28); v101(qword)@+32 = *(v6+4); v102(word)@+40 = v6[10].
                    CGS_ASSERT(lpRecord != 0, "lpActiveFreeburnChallengeEvent");
                    std::memcpy(lOut, lpRecord, 28);
                    reinterpret_cast<u64*>(lOut + 32)[0] = reinterpret_cast<const u64*>(lpRecord + 16)[0];
                    reinterpret_cast<s32*>(lOut + 40)[0] = lpW[10];
                    liOutSize = 48; liOutType = 173; lbEmit = true;
                    break;
                case 67:  // FburnChallengeSuccessUpdate -> 24-byte record
                    CGS_ASSERT(lpRecord != 0, "lpFburnChallengeSuccessUpdateEvent");
                    reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[0];
                    reinterpret_cast<s32*>(lOut)[2] = lpW[2];
                    reinterpret_cast<s32*>(lOut)[3] = lpW[3];
                    reinterpret_cast<s32*>(lOut)[4] = lpW[4];
                    liOutSize = 24; liOutType = 171; lbEmit = true;
                    break;
                case 68:  // FburnChallengeSuccess -> 20-byte record
                    CGS_ASSERT(lpRecord != 0, "lpFburnChallengeSuccessEvent");
                    std::memcpy(lOut, lpRecord, 8);
                    std::memcpy(lOut + 8, lpRecord + 16, 2);
                    std::memcpy(lOut + 10, lpRecord + 72, 2);
                    reinterpret_cast<s32*>(lOut)[3] = lpW[2];
                    reinterpret_cast<s32*>(lOut)[4] = lpW[3];
                    liOutSize = 20; liOutType = 172; lbEmit = true;
                    break;
                case 74:  // UploadedModeScores -> 128-byte record
                    CGS_ASSERT(lpRecord != 0, "lpUploadedModeScoresEvent");
                    std::memcpy(lOut, lpRecord, 128);
                    liOutSize = 128; liOutType = 175; lbEmit = true;
                    break;
                default:
                    break;
            }

            if (lbEmit)
            {
                GameStateEventQueue* lpGameEventQueue = GetGameEventQueue(lpGameStateModule);
                lpGameEventQueue->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(lOut), liOutType, liOutSize);
            }

            result = lpNetworkEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
        return result;
    }

    // ------------------------------------------------------------------------
    // TranslateNetworkEventsToGuiEvents (X360 0x823E0900), its inlined helper
    // TranslateScoreboardResponse, and BridgeNetworkToGui (X360 0x823E9518) are NOT
    // bodied here: their reconstructions did not pass verification this wave (case-15
    // NetworkGameParams tail dropped / case-52 dest offsets wrong; NetworkPlayerStatus +
    // NetworkLobbyPlayerList stack-base and header offsets wrong). They remain declared in
    // BrnGameModule.hpp so callers resolve, and land once re-verified.
    // ------------------------------------------------------------------------
}
