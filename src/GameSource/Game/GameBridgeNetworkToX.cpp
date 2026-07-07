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
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                 // CgsGui::GuiEvent<N> (tag-only GUI events)
#include "GameShared/GameClasses/Development/CgsStrStream.h"        // CgsDev::StrStreamBase (mugshot debug spew)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include "GameSource/Network/BrnNetworkModuleIO.h"                  // BrnNetwork::BrnNetworkModuleIO::OutputBuffer + interfaces
#include "GameSource/GameState/BrnGameStateModuleIO.h"              // BrnGameState::GameStateModuleIO::PreWorldInputBuffer + events
#include "GameSource/GameState/BrnCgsPlayerName.h"                  // CgsNetwork::PlayerName (Mole4Avril debug roster)

#include <cstring>   // std::memcpy / std::strncpy (PASS bodies)

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

    // The GUI module's input event queue the bridge bulk-appends the network GUI event queue into
    // (X360 sub_8284F238(lpGuiBuffer)). VariableEventQueue<32768,16>. FLAG: the exact queue offset
    // within the GUI IO buffer lands with the GUI module TU; reached here at the buffer base.
    typedef CgsModule::VariableEventQueue<32768, 16> GuiInputEventQueue;
    // The network output's small GUI event queue (OutputBuffer::GetGuiEventQueue() @ +16,
    // GuiEventQueueSmall == VariableEventQueue<4096,16>): the Append source.
    typedef CgsModule::VariableEventQueue<4096, 16>  NetworkGuiEventQueue;

    static inline GuiInputEventQueue* GetGuiInputEventQueue(void* lpGuiBuffer)
    {
        return reinterpret_cast<GuiInputEventQueue*>(lpGuiBuffer);   // FLAG: buffer base (see above)
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

    // -------------------------------------------------------------------------
    // Scoreboard-response string copy guard (X360 CgsStringUtils inline): scan the source
    // name to its NUL, assert length < 31 ("String too long: " verbatim, HARD RULE A), then
    // strncpy 31 bytes into the destination name slot.
    // -------------------------------------------------------------------------
    static void CopyScoreboardName(unsigned char* lpDst, const unsigned char* lpcName)
    {
        const char* lpcScan = reinterpret_cast<const char*>(lpcName);
        while (*lpcScan++)
        {
        }
        CGS_ASSERT((lpcScan - reinterpret_cast<const char*>(lpcName) - 1) < 31, "String too long: ");
        std::strncpy(reinterpret_cast<char*>(lpDst), reinterpret_cast<const char*>(lpcName), 31);
    }

    // =========================================================================
    // TranslateScoreboardResponse -- the case-52 heading-type sub-switch (inlined into
    // 0x823E0900). The scoreboard-response record carries a count word @ +0 and a packed
    // list of 31-byte names @ +8. The heading type (record accessor, Hex-Rays "BrnNetwo")
    // selects which scoreboard GUI event the names are copied into.
    // =========================================================================
    void BrnGameModule::TranslateScoreboardResponse(void* lpGuiBuffer, const unsigned char* lpRecord)
    {
        CgsGui::GuiModule* lpSink = GetGuiEventSink(this);
        const unsigned int luHeadingType = BrnNetwork::GetScoreboardResponseHeadingType(lpRecord);
        const int liCount = *reinterpret_cast<const int*>(lpRecord);

        if (luHeadingType == 0)
        {
            // category (cap KI_MAX_CATEGORIES == 15)
            BrnGui::GuiEventScoreboardResponseCategoryEvent lEvent;
            unsigned char* lpDst = lEvent.maOpaque;
            *reinterpret_cast<int*>(lpDst) = liCount;
            CGS_ASSERT(liCount <= 15,
                "lCategoryEvent.miNumberOfCategories <= BrnGui::GuiEventScoreboardResponseCategoryEvent::KI_MAX_CATEGORIES");
            for (int i = 0; i < liCount; ++i)
                CopyScoreboardName(lpDst + 4 + 31 * i, lpRecord + 8 + 31 * i);
            lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
        }
        else if (luHeadingType == 1)
        {
            // index (cap KI_MAX_INDEXES == 10)
            BrnGui::GuiEventScoreboardResponseIndexEvent lEvent;
            unsigned char* lpDst = lEvent.maOpaque;
            *reinterpret_cast<int*>(lpDst) = liCount;
            CGS_ASSERT(liCount <= 10,
                "lIndexEvent.miNumberOfIndexes <= BrnGui::GuiEventScoreboardResponseIndexEvent::KI_MAX_INDEXES");
            for (int i = 0; i < liCount; ++i)
                CopyScoreboardName(lpDst + 4 + 31 * i, lpRecord + 8 + 31 * i);
            lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
        }
        else if (luHeadingType < 3)
        {
            // variation (cap KI_MAX_VARIATIONS == 66); two trailing source bytes @ record +2054/+2055
            // land at event-buffer +2050/+2051 (X360: lbz r11,0x806/0x807(r26) -> stb var_C1E/var_C1D
            // == buffer 0x802/0x803), immediately after the last 31-byte name, NOT at +2054/+2055.
            BrnGui::GuiEventScoreboardResponseVariationEvent lEvent;
            unsigned char* lpDst = lEvent.maOpaque;
            *reinterpret_cast<int*>(lpDst) = liCount;
            CGS_ASSERT(liCount <= 66,
                "lVariationEvent.miNumberOfVariations <= BrnGui::GuiEventScoreboardResponseVariationEvent::KI_MAX_VARIATIONS");
            for (int i = 0; i < liCount; ++i)
                CopyScoreboardName(lpDst + 4 + 31 * i, lpRecord + 8 + 31 * i);
            lpDst[2050] = lpRecord[2054];
            lpDst[2051] = lpRecord[2055];
            lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
        }
        else
        {
            // Invalid heading type (X360 builds "Invalid heading type : <n>\n"; collapsed to the
            // rodata prefix per HARD RULE A).
            CGS_ASSERT(false, "Invalid heading type : ");
        }
    }

    // =========================================================================
    // TranslateNetworkEventsToGuiEvents  (X360 0x823E0900)
    // Drain the network event queue (VariableEventQueue<14000,16>) and translate each GUI-bound
    // network event into the matching CgsGui::GuiModule event (this + 7252512).
    // =========================================================================
    int BrnGameModule::TranslateNetworkEventsToGuiEvents(void* lpGuiBuffer, const OutputBuffer* lpNetworkOutput)
    {
        CgsGui::GuiModule* lpSink = GetGuiEventSink(this);

        const NetworkEventQueue* lpNetworkEventQueue = GetNetworkEventQueue(lpNetworkOutput);
        CGS_ASSERT(lpNetworkEventQueue != 0, "lpNetworkEventQueue");

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        int result = lpNetworkEventQueue->GetFirstEvent(&lpEvent, &liSize);

        while (lpEvent)
        {
            const unsigned char* lpRecord = reinterpret_cast<const unsigned char*>(lpEvent);
            const int* lpW = reinterpret_cast<const int*>(lpRecord);

            switch (result)
            {
                case 1:   // OnlineNumFriendsCount -> word0
                {
                    BrnGui::GuiEventOnlineNumFriendsCount lEvent;
                    *reinterpret_cast<int*>(lEvent.maOpaque) = lpW[0];
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 2:   // OnlineReceiveFriendInfo -> 660-byte copy + {word,word,byte} tail
                {
                    BrnGui::GuiEventOnlineReceiveFriendInfo lEvent;
                    unsigned char* lpDst = lEvent.maOpaque;
                    std::memcpy(lpDst, lpRecord, 660);
                    *reinterpret_cast<int*>(lpDst + 660) = *reinterpret_cast<const int*>(lpRecord + 660);
                    *reinterpret_cast<int*>(lpDst + 664) = *reinterpret_cast<const int*>(lpRecord + 664);
                    lpDst[668] = lpRecord[668];
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 8:   // InviteFailed -> word0
                {
                    BrnGui::GuiEventInviteFailed lEvent;
                    *reinterpret_cast<int*>(lEvent.maOpaque) = lpW[0];
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 9:   // BuddyNotification -> qword + 16 bytes (24 bytes total)
                {
                    BrnGui::GuiEventBuddyNotification lEvent;
                    std::memcpy(lEvent.maOpaque, lpRecord, 24);
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 10:  // GuiEvent<103>
                {
                    CgsGui::GuiEvent<103> lEvent;
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 11:  // GuiEvent<95>
                {
                    CgsGui::GuiEvent<95> lEvent;
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 13:  // FreeburnComplete -> conditional GuiEvent<285> then GuiEvent<95>
                {
                    CGS_ASSERT(lpRecord != 0, "lpFreeburnComplete");
                    if (lpRecord[0] == 0)
                    {
                        CgsGui::GuiEvent<285> lEventA;
                        lpSink->AddGuiEvent(&lEventA, lpGuiBuffer);
                    }
                    CgsGui::GuiEvent<95> lEventB;
                    lpSink->AddGuiEvent(&lEventB, lpGuiBuffer);
                    break;
                }
                case 15:  // NetworkGameParams -> 440-byte copy + reshuffled 40-byte tail
                {
                    CGS_ASSERT(lpRecord != 0, "lpParamsChangedEvent");
                    BrnGui::GuiEventNetworkGameParams lEvent;
                    unsigned char* lpDst = lEvent.maOpaque;
                    std::memcpy(lpDst, lpRecord, 440);
                    *reinterpret_cast<int*>(lpDst + 440) = *reinterpret_cast<const int*>(lpRecord + 440);
                    *reinterpret_cast<int*>(lpDst + 444) = *reinterpret_cast<const int*>(lpRecord + 444);
                    *reinterpret_cast<int*>(lpDst + 448) = *reinterpret_cast<const int*>(lpRecord + 448);
                    *reinterpret_cast<int*>(lpDst + 452) = *reinterpret_cast<const int*>(lpRecord + 452);
                    *reinterpret_cast<int*>(lpDst + 456) = *reinterpret_cast<const int*>(lpRecord + 456);
                    *reinterpret_cast<int*>(lpDst + 460) = *reinterpret_cast<const int*>(lpRecord + 472);
                    *reinterpret_cast<int*>(lpDst + 464) = *reinterpret_cast<const int*>(lpRecord + 460);
                    *reinterpret_cast<int*>(lpDst + 468) = *reinterpret_cast<const int*>(lpRecord + 464);
                    *reinterpret_cast<int*>(lpDst + 472) = *reinterpret_cast<const int*>(lpRecord + 468);
                    lpDst[476] = lpRecord[477];
                    lpDst[477] = lpRecord[478];
                    lpDst[478] = lpRecord[479];
                    lpDst[479] = lpRecord[476];
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 17:  // PlayerRemoved -> NetworkPlayerLeftLobby (only when record[21]==0)
                {
                    CGS_ASSERT(lpRecord != 0, "lpPlayerRemovedEvent");
                    if (lpRecord[21] == 0)
                    {
                        BrnGui::GuiEventNetworkPlayerLeftLobby lEvent;
                        unsigned char* lpDst = lEvent.maOpaque;
                        *reinterpret_cast<int*>(lpDst) = lpW[0];
                        std::memcpy(lpDst + 4, lpRecord + 4, 16);
                        lpDst[20] = lpRecord[20];
                        lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    }
                    break;
                }
                case 22:  // OnlinePostEventScalps -> 64-byte copy + word tail
                {
                    BrnGui::GuiEventOnlinePostEventScalps lEvent;
                    unsigned char* lpDst = lEvent.maOpaque;
                    std::memcpy(lpDst, lpRecord, 64);
                    *reinterpret_cast<int*>(lpDst + 64) = *reinterpret_cast<const int*>(lpRecord + 64);
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 24:  // NetworkLeftGame -> {word0, word1}
                {
                    CGS_ASSERT(lpRecord != 0, "lpEvent");
                    BrnGui::GuiEventNetworkLeftGame lEvent;
                    reinterpret_cast<int*>(lEvent.maOpaque)[0] = lpW[0];
                    reinterpret_cast<int*>(lEvent.maOpaque)[1] = lpW[1];
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 25:  // NetworkPostGameProcessingFinished -> byte0
                {
                    CGS_ASSERT(lpRecord != 0, "lpPostGameProcessingFinished");
                    BrnGui::GuiEventNetworkPostGameProcessingFinished lEvent;
                    lEvent.maOpaque[0] = lpRecord[0];
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 26:  // NetworkLaunching -> {word0, word1}
                {
                    CGS_ASSERT(lpRecord != 0, "lpLaunchEvent");
                    CgsGui::GuiEventNetworkLaunching lEvent;
                    reinterpret_cast<int*>(lEvent.maOpaque)[0] = lpW[0];
                    reinterpret_cast<int*>(lEvent.maOpaque)[1] = lpW[1];
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 27:  // Launch overlay wait-finish requests (two request names)
                {
                    BrnGui::GuiOverlayWaitFinishRequest lRequest;
                    BrnGui::GuiOverlayWaitFinishRequest::Construct(&lRequest, "CNOnlLchGame");
                    lpSink->AddGuiEvent(&lRequest, lpGuiBuffer);
                    BrnGui::GuiOverlayWaitFinishRequest::Construct(&lRequest, "CNOnlLchGmH");
                    lpSink->AddGuiEvent(&lRequest, lpGuiBuffer);
                    break;
                }
                case 28:  // GuiEvent<271>
                {
                    CgsGui::GuiEvent<271> lEvent;
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 29:  // LiveRevengeProfileData -> word0
                {
                    BrnGui::GuiEventLiveRevengeProfileData lEvent;
                    *reinterpret_cast<int*>(lEvent.maOpaque) = lpW[0];
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 30:  // GuiEvent<280>
                {
                    CgsGui::GuiEvent<280> lEvent;
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 31:  // OnlineTimeout -> single float: record[4](float) + (float)record[0](int)
                {
                    BrnGui::GuiEventOnlineTimeout lEvent;
                    const float lfBase = *reinterpret_cast<const float*>(lpRecord + 4);
                    *reinterpret_cast<float*>(lEvent.maOpaque) = lfBase + static_cast<float>(lpW[0]);
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 32:  // AutosaveRequest -> byte0 = 0
                {
                    BrnGui::GuiAutosaveRequestEvent lEvent;
                    lEvent.maOpaque[0] = 0;
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 42:  // GuiEvent<109>
                {
                    CgsGui::GuiEvent<109> lEvent;
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 48:  // OnlineCarStatus -> {word0, byte@+4}
                {
                    CGS_ASSERT(lpRecord != 0, "lpPlayerCarSelectStatus");
                    BrnGui::GuiOnlineCarStatusEvent lEvent;
                    *reinterpret_cast<int*>(lEvent.maOpaque) = lpW[0];
                    lEvent.maOpaque[4] = lpRecord[4];
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 51:  // ScoreboardResponseTable -> 2924-byte copy
                {
                    BrnGui::GuiEventScoreboardResponseTableEvent lEvent;
                    std::memcpy(lEvent.maOpaque, lpRecord, 2924);
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 52:  // ScoreboardResponse (category / index / variation heading sub-switch)
                    TranslateScoreboardResponse(lpGuiBuffer, lpRecord);
                    break;
                case 56:  // CapturingImage -> MugshotControlEvent (+ filter-gated debug spew)
                {
                    CGS_ASSERT(lpRecord != 0, "lpCapturingImageEvent");
                    BrnGui::GuiMugshotControlEvent lEvent;
                    unsigned char* lpDst = lEvent.maOpaque;
                    std::memcpy(lpDst, lpRecord, 8);                              // qword @ +0
                    *reinterpret_cast<int*>(lpDst + 8) = 1;
                    *reinterpret_cast<int*>(lpDst + 12) = lpW[2];                 // record[8]
                    *reinterpret_cast<int*>(lpDst + 16) = lpW[3];                 // record[12]
                    lpDst[20] = 1;
                    // FLAG: sub_821F0E50/sub_8229F668/sub_82203EE8 are un-homed StrStreamBase
                    // insertion overloads; modelled by their operand-typed committed overload.
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "MUGSHOT EVENT : BRIDGE NETWORK TO X (1)\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "IMAGE TYPE : " << lpW[2] << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "LOCAL AGG  : " << static_cast<s32>(1) << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "AGG ARCI   : " << lpW[3] << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "RESPONSE   : " << static_cast<s32>(1) << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "ROAD?      : " << *reinterpret_cast<const u64*>(lpRecord) << "\n";
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 59:  // AbortCapture -> MugshotControlEvent (+ filter-gated debug spew)
                {
                    CGS_ASSERT(lpRecord != 0, "lpEvent");
                    BrnGui::GuiMugshotControlEvent lEvent;
                    unsigned char* lpDst = lEvent.maOpaque;
                    const s32 liResponse = (lpRecord[0] == 0) ? 5 : 4;
                    *reinterpret_cast<u64*>(lpDst) = 0;                           // qword @ +0
                    *reinterpret_cast<int*>(lpDst + 8) = liResponse;
                    *reinterpret_cast<int*>(lpDst + 12) = 1;
                    *reinterpret_cast<int*>(lpDst + 16) = -1;
                    lpDst[20] = 0;
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "MUGSHOT EVENT : BRIDGE NETWORK TO X (2)\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "IMAGE TYPE : " << static_cast<s32>(1) << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "LOCAL AGG  : " << static_cast<s32>(0) << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "AGG ARCI   : " << static_cast<s32>(-1) << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "RESPONSE   : " << liResponse << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "ROAD?      : " << static_cast<u64>(0) << "\n";
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 66:  // ActiveFreeburnChallenge -> ChallengeNotActiveStartEvent (40-byte record)
                {
                    CGS_ASSERT(lpRecord != 0, "lpActiveFreeburnChallengeEvent");
                    BrnGui::GuiChallengeNotActiveStartEvent lEvent;
                    unsigned char* lpDst = lEvent.maOpaque;
                    std::memcpy(lpDst, lpRecord + 32, 8);                         // qword @ +0 = record[32..40)
                    std::memcpy(lpDst + 8, lpRecord, 28);                        // record[0..28)
                    *reinterpret_cast<int*>(lpDst + 36) = lpW[10];               // record[40]
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 70:  // OnlineAccountSettings -> 3 bytes
                {
                    CGS_ASSERT(lpRecord != 0, "lpAccountSettings");
                    BrnGui::GuiEventOnlineAccountSettings lEvent;
                    lEvent.maOpaque[0] = lpRecord[0];
                    lEvent.maOpaque[1] = lpRecord[1];
                    lEvent.maOpaque[2] = lpRecord[2];
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 71:  // GuiEvent<127>
                {
                    CgsGui::GuiEvent<127> lEvent;
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 72:  // CamPicCompressed -> 3 words
                {
                    CGS_ASSERT(lpRecord != 0, "lpCamPicCompressedEvent");
                    BrnGui::GuiEventCamPicCompressed lEvent;
                    int* lpDst = reinterpret_cast<int*>(lEvent.maOpaque);
                    lpDst[0] = lpW[0];
                    lpDst[1] = lpW[1];
                    lpDst[2] = lpW[2];
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                case 73:  // GuiEvent<571>
                {
                    CgsGui::GuiEvent<571> lEvent;
                    lpSink->AddGuiEvent(&lEvent, lpGuiBuffer);
                    break;
                }
                default:
                    break;
            }

            result = lpNetworkEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
        return result;
    }

    // =========================================================================
    // BridgeNetworkToGui  (X360 0x823E9518)
    // Top-level per-frame network->GUI bridge: run both translators, bulk-append the network
    // GUI event queue into the GUI input queue, then synthesise the three player-snapshot GUI
    // events (player-list / player-status / lobby-player-list). Called by DoUpdate_GUI /
    // LoadingScriptedState::Update.
    // =========================================================================
    int BrnGameModule::BridgeNetworkToGui(void* lpGuiBuffer, const OutputBuffer* lpNetworkOutput)
    {
        CgsGui::GuiModule* lpSink = GetGuiEventSink(this);

        const void* lpNetworkToGui = lpNetworkOutput->GetNetworkToGuiInterface();
        const unsigned char* lpStatus =
            reinterpret_cast<const unsigned char*>(lpNetworkOutput->GetInGamePlayerStatusInterface());

        CGS_ASSERT(lpStatus != 0, "lpNetworkOutput->GetInGamePlayerStatusInterface()");
        CGS_ASSERT(lpNetworkToGui != 0, "lpNetworkToGuiInterface");
        CGS_ASSERT(lpNetworkOutput->GetGuiEventQueue() != 0, "lpNetworkOutput->GetGuiEventQueue()");

        TranslateNetworkInterfaceToGuiEvents(lpGuiBuffer, lpNetworkToGui);
        TranslateNetworkEventsToGuiEvents(lpGuiBuffer, lpNetworkOutput);

        // Bulk-append the network output's GUI event queue into the GUI module input queue.
        GetGuiInputEventQueue(lpGuiBuffer)->Append(
            *reinterpret_cast<const NetworkGuiEventQueue*>(lpNetworkOutput->GetGuiEventQueue()));

        int liNumPlayers = *reinterpret_cast<const int*>(lpStatus + 2532);

        // -------- GuiEventNetworkPlayerList (8 x {index(4), PlayerName(16)} + two count words) -----
        {
            BrnGui::GuiEventNetworkPlayerList lListEvent;
            unsigned char* lpList = lListEvent.maOpaque;
            bool lbBuiltList = false;

            if (byte_82FB5090)
            {
                liNumPlayers = 8;
                CgsNetwork::PlayerName lName;
                lName.Construct("Mole4Avril");
                for (int i = 0; i < 8; ++i)
                {
                    unsigned char* lpEntry = lpList + 20 * i;
                    std::memcpy(lpEntry + 4, &lName, 16);
                    *reinterpret_cast<int*>(lpEntry) = i;
                }
                *reinterpret_cast<int*>(lpList + 160) = 8;
                *reinterpret_cast<int*>(lpList + 164) = 8;
                lbBuiltList = true;
            }
            else if (!lpNetworkOutput->IsPlaying() && lpNetworkOutput->IsConnected())
            {
                int i = 0;
                for (; i < liNumPlayers; ++i)
                {
                    const unsigned char* lpRec = lpStatus + 312 * i;   // In(statusInterface, i)
                    unsigned char* lpEntry = lpList + 20 * i;
                    std::memcpy(lpEntry + 4, lpRec + 256, 16);
                    *reinterpret_cast<int*>(lpEntry) = *reinterpret_cast<const int*>(lpRec + 272);
                }
                for (; i < 8; ++i)
                    *reinterpret_cast<int*>(lpList + 20 * i) = -1;
                *reinterpret_cast<int*>(lpList + 160) = liNumPlayers;
                *reinterpret_cast<int*>(lpList + 164) = *reinterpret_cast<const int*>(lpStatus + 2536);
                lbBuiltList = true;
            }

            if (lbBuiltList)
                lpSink->AddGuiEvent(&lListEvent, lpGuiBuffer);
        }

        // -------- GuiEventNetworkPlayerStatus (8 x InGamePlayerStatusData(312) + trailing) --------
        {
            BrnGui::GuiEventNetworkPlayerStatus lStatusEvent;
            unsigned char* lpBuf = lStatusEvent.maOpaque;

            // X360 float pre-init loop: per record, {int@+96, float@+100} = 0.
            for (int i = 0; i < 8; ++i)
            {
                *reinterpret_cast<int*>(lpBuf + 312 * i + 96) = 0;
                *reinterpret_cast<float*>(lpBuf + 312 * i + 100) = 0.0f;
            }
            *reinterpret_cast<int*>(lpBuf + 2496) = 0;
            lpBuf[2500] = 0;
            lpBuf[2536] = 0;
            for (int i = 0; i < 8; ++i)
                reinterpret_cast<BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData*>(lpBuf + 312 * i)->Clear();

            int liFilled;
            if (byte_82FB5091)
            {
                CgsNetwork::PlayerName lName;
                lName.Construct("Mole4Avril");
                int i = 0;
                for (; i < liNumPlayers; ++i)
                {
                    unsigned char* lpRec = lpBuf + 256 + 312 * i;
                    std::memcpy(lpRec, &lName, 16);
                    *reinterpret_cast<int*>(lpRec + 16) = i;
                    *reinterpret_cast<int*>(lpRec + 20) = i;
                    *reinterpret_cast<int*>(lpRec + 24) = byte_82FB5092;
                    *reinterpret_cast<int*>(lpRec + 28) = byte_82FB5092;
                    *reinterpret_cast<int*>(lpRec + 32) = -1;
                    *reinterpret_cast<int*>(lpRec + 36) = -1;
                    *reinterpret_cast<int*>(lpRec + 40) = 8;
                    lpRec[44] = 0;
                    lpRec[45] = 0;
                    lpRec[46] = 0;
                    lpRec[47] = 1;
                    if (i == static_cast<int>(lpNetworkOutput->GetPlayerActiveRaceCarIndex()))
                    {
                        lpRec[45] = 1;
                        lpRec[46] = 1;
                    }
                }
                liFilled = i;
            }
            else
            {
                int i = 0;
                for (; i < liNumPlayers; ++i)
                {
                    CGS_ASSERT(i >= 0, "liIndex >= 0");
                    CGS_ASSERT(i < *reinterpret_cast<const int*>(lpStatus + 2532), "liIndex < miNumPlayers");
                    std::memcpy(lpBuf + 312 * i, lpStatus + 312 * i, 312);
                }
                liFilled = i;
            }

            // Empty-slot marker: record[i]+276 = -1.
            for (int i = liFilled; i < 8; ++i)
                *reinterpret_cast<int*>(lpBuf + 312 * i + 276) = -1;

            // Interface name copy (CgsStringUtils length guard) + flag + count.
            const char* lpcName = reinterpret_cast<const char*>(lpStatus + 2496);
            const char* lpcScan = lpcName;
            while (*lpcScan++)
            {
            }
            CGS_ASSERT((lpcScan - lpcName - 1) < 0x24, "String too long: ");
            std::strncpy(reinterpret_cast<char*>(lpBuf + 2500), lpcName, 36);
            lpBuf[2536] = lpStatus[2540];
            *reinterpret_cast<int*>(lpBuf + 2496) = liNumPlayers;

            lpSink->AddGuiEvent(&lStatusEvent, lpGuiBuffer);
        }

        // -------- GuiEventNetworkLobbyPlayerList (8 x 56-byte lobby record + count word) ----------
        {
            BrnGui::GuiEventNetworkLobbyPlayerList lLobbyEvent;
            unsigned char* lpLobby = lLobbyEvent.maOpaque;
            const unsigned char* lpLobbyInterface = reinterpret_cast<const unsigned char*>(
                BrnNetwork::GetOnlineLobbyPlayerStatusInterface(lpNetworkOutput));

            int i = 0;
            for (; i < liNumPlayers; ++i)
            {
                CGS_ASSERT(i >= 0, "liPlayerIndex >= 0");
                CGS_ASSERT(i < 8, "liPlayerIndex < KI_MAX_PLAYERS");
                std::memcpy(lpLobby + 56 * i, lpLobbyInterface + 56 * i, 56);
            }
            for (; i < 8; ++i)
            {
                unsigned char* lpRec = lpLobby + 56 * i;
                *reinterpret_cast<int*>(lpRec + 16) = -1;
                *reinterpret_cast<int*>(lpRec + 20) = 4;
                *reinterpret_cast<int*>(lpRec + 24) = 0;
                *reinterpret_cast<int*>(lpRec + 28) = 0;
                *reinterpret_cast<int*>(lpRec + 32) = 0;
                *reinterpret_cast<int*>(lpRec + 36) = -1;
                lpRec[48] = 0;
                lpRec[49] = 0;
                lpRec[50] = 0;
                lpRec[51] = 0;
            }
            *reinterpret_cast<int*>(lpLobby + 448) = liNumPlayers;

            lpSink->AddGuiEvent(&lLobbyEvent, lpGuiBuffer);
        }

        // FLAG: the X360 returns r3 from the final AddGuiEvent<...LobbyPlayerList>; the placeholder
        // sink returns void, so the bridge returns 0 (the caller discards the result).
        return 0;
    }
}
