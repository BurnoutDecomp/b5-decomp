#pragma once
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeNetworkToX.h
//
// Support types for the BrnGame::BrnGameModule network-bridge family
// (GameSource/Unity/../Game/GameBridgeNetworkToX.cpp). Each per-frame bridge reads the
// network module's OUTPUT buffer (BrnNetwork::BrnNetworkModuleIO::OutputBuffer, the
// committed home) and republishes its contents into the GUI + game-state subsystems --
// the mirror image of the controller/replay bridges in GameBridgeControllerToX.cpp /
// GameBridgeReplayToX.cpp.
//
// This header carries ONLY the bridge-local scaffolding the four reconstructed functions
// need that is not homed elsewhere:
//
//   * The un-homed GUI event payload family the ToGui translators synthesise from each
//     network-event record. Per HARD RULE 3 each is an opaque, X360-ATTESTED-stride byte
//     span: the translator fills the exact bytes the asm stores and passes the span by
//     name. The event-type TAGS (the AddGuiEvent<T> template argument) are the REAL ones
//     attested by the X360 mangled AddGuiEvent<...> symbols in the call graph; the payload
//     LAYOUTS are opaque until each GUI event TU is reconstructed. FLAGGED. Sizes are the
//     exact stack spans the asm writes (largest is the 2924-byte scoreboard table).
//
//   * The BrnGui::GuiOverlayWaitFinishRequest constructor + BrnGui::GuiLiveRevengeUpdateEvent
//     are referenced by name (real DWARF names, attested by the X360 call graph); modelled
//     as opaque records here until BrnGuiEventTypeDefs.h grows them.
//
//   * A small set of by-name network accessors (the Hex-Rays sub_*/BrnNetwo/Net helpers)
//     the bridge reaches on the network interfaces; declared here as free functions in
//     namespace BrnNetwork so the bridge references them by name. FLAGGED: these correspond
//     to committed OutputBuffer/interface accessors (see .cpp) reached by their byte offset.
//
//   * The three file-scope debug switches (X360 byte_82FB5090..92) that inject the
//     synthetic "Mole4Avril" test roster.
//
// The CgsGui::GuiModule placeholder + its AddGuiEvent<T>(event, buffer) template are the
// committed ones from GameBridgeControllerToX.h (#included by the .cpp); this header does
// not redefine them.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"   // CgsGui::GuiEvent<N>

// Forward declarations for the by-name game-state accessors below (the .cpp includes the
// real BrnGameStateModuleIO.h; this header only takes pointers / declares free functions).
namespace BrnGameState { class GameStateModule; namespace GameStateModuleIO { struct PreWorldInputBuffer; } }

// -------------------------------------------------------------------------
// File-scope debug switches (X360 byte_82FB5090 / _5091 / _5092). Inject the synthetic
// "Mole4Avril" roster into the player-list / player-status snapshots. FLAG: file-scope
// here (their real home is the network debug TU); default-zero (retail path unchanged).
// -------------------------------------------------------------------------
namespace BrnGame
{
    extern bool byte_82FB5090;
    extern bool byte_82FB5091;
    extern u8   byte_82FB5092;
}

// -------------------------------------------------------------------------
// Opaque GUI event payload tags (X360-attested strides). Each is the record the ToGui
// translator materialises store-for-store then hands to CgsGui::GuiModule::AddGuiEvent<T>.
// The TAG type is the real X360 template argument; the layout is an opaque, correctly
// sized byte span (HARD RULE 3) -- NO field names invented. Promote each to its real
// BrnGuiEventTypeDefs.h / CgsGuiEvent.h home when that GUI event TU is reconstructed.
// -------------------------------------------------------------------------
namespace BrnGui
{
    struct alignas(16) GuiLiveRevengeUpdateEvent          { u8 maOpaque[16]; };
    struct alignas(16) GuiEventOnlineNumFriendsCount      { u8 maOpaque[16]; };
    struct alignas(16) GuiEventOnlineReceiveFriendInfo    { u8 maOpaque[672]; };  // 660 + 3 trailing fields
    struct alignas(16) GuiEventInviteFailed               { u8 maOpaque[16]; };
    struct alignas(16) GuiEventBuddyNotification          { u8 maOpaque[32]; };   // qword + 16 bytes
    struct alignas(16) GuiEventNetworkGameParams          { u8 maOpaque[448]; };  // 440-byte record
    struct alignas(16) GuiEventNetworkPlayerLeftLobby     { u8 maOpaque[32]; };   // word + byte + 16
    struct alignas(16) GuiEventOnlinePostEventScalps      { u8 maOpaque[72]; };   // 64 + word
    struct alignas(16) GuiEventNetworkLeftGame            { u8 maOpaque[16]; };
    struct alignas(16) GuiEventNetworkPostGameProcessingFinished { u8 maOpaque[16]; };
    struct alignas(16) GuiEventLiveRevengeProfileData     { u8 maOpaque[16]; };
    struct alignas(16) GuiEventOnlineTimeout              { u8 maOpaque[16]; };   // single float
    struct alignas(16) GuiAutosaveRequestEvent            { u8 maOpaque[16]; };
    struct alignas(16) GuiOnlineCarStatusEvent            { u8 maOpaque[16]; };   // word + byte
    struct alignas(16) GuiMugshotControlEvent             { u8 maOpaque[32]; };   // qword + 3 words + byte
    struct alignas(16) GuiChallengeNotActiveStartEvent    { u8 maOpaque[48]; };
    struct alignas(16) GuiEventOnlineAccountSettings      { u8 maOpaque[16]; };   // 3 bytes
    struct alignas(16) GuiEventCamPicCompressed           { u8 maOpaque[16]; };   // 3 words
    // Scoreboard-response events: {count(4), N x 31-byte names[, 2 trailing bytes]}.
    struct alignas(16) GuiEventScoreboardResponseCategoryEvent  { u8 maOpaque[512]; };   // cap 16
    struct alignas(16) GuiEventScoreboardResponseIndexEvent     { u8 maOpaque[512]; };   // cap 11
    struct alignas(16) GuiEventScoreboardResponseVariationEvent { u8 maOpaque[2080]; };  // cap 67 + 2
    struct alignas(16) GuiEventScoreboardResponseTableEvent     { u8 maOpaque[2928]; };  // 2924-byte record

    // Player-snapshot events built by BridgeNetworkToGui:
    //   list   : {8 x {index(4), PlayerName(16)}} + 2 trailing count words  (v41[40]==160B + v42/v43)
    //   status : header(48) + 8 x InGamePlayerStatusData(312)               (v46 block + v48[2240])
    //   lobby  : {8 x 56-byte lobby record} + count word                    (v44[448] + v45)
    struct alignas(16) GuiEventNetworkPlayerList          { u8 maOpaque[176]; };
    struct alignas(16) GuiEventNetworkPlayerStatus        { u8 maOpaque[2624]; };  // 96 + 8*312 = 2592
    struct alignas(16) GuiEventNetworkLobbyPlayerList     { u8 maOpaque[464]; };   // 8*56 + count

    // GuiOverlayWaitFinishRequest -- real DWARF type (BrnGuiEventTypeDefs.h). The bridge
    // Constructs it from a request-name string then enqueues it. Opaque record + the
    // static Construct(void*, const char*) the X360 calls by name (0x823E0E98).
    struct alignas(16) GuiOverlayWaitFinishRequest
    {
        u8 maOpaque[24];
        static void Construct(void* lpDest, const char* lpcRequestName);
    };
}

namespace CgsGui
{
    // GuiEventNetworkLaunching -- the CgsGui-namespaced launch event (case 26). Opaque
    // 8-byte {w0,w1} record. Real X360 template argument CgsGui::GuiEventNetworkLaunching.
    struct alignas(16) GuiEventNetworkLaunching           { u8 maOpaque[16]; };
}

// -------------------------------------------------------------------------
// Game-state event tags the ToGameState translator (0x823DF9E0) synthesises. Each is the
// AddEvent record built store-for-store then queued into the PreWorld game-event queue.
// The X360 calls a de-inlined <Event>::SetNetworkPlayerID(record, id) single-word setter
// whose exact within-record offset is owned by the real event type (not attested here);
// modelled as a static setter on the opaque tag (HARD RULE 3 -- no field names invented).
// FLAG: real homes are BrnGameStateModuleIO.h event-type-defs; promote when reconstructed.
// -------------------------------------------------------------------------
namespace BrnGameState
{
    struct OnlinePlayerAddedEvent
    { u8 maOpaque[40]; static void SetNetworkPlayerID(void* lpRecord, s32 liId); };
    struct OnlinePlayerRemovedEvent
    { u8 maOpaque[8];  static void SetNetworkPlayerID(void* lpRecord, s32 liId); };
    struct OnlinePlayerFinalisedEvent
    { u8 maOpaque[4];  static void SetNetworkPlayerID(void* lpRecord, s32 liId); };
    struct ChangeNetworkCarEvent
    { u8 maOpaque[32]; static void SetNetworkPlayerID(void* lpRecord, s32 liId); };
    struct RemotePlayerDisconnectedEvent
    { u8 maOpaque[4];  static void SetNetworkPlayerID(void* lpRecord, s32 liId); };
}

// -------------------------------------------------------------------------
// By-name network accessors the bridge reaches on the network interfaces. FLAG: these are
// the committed OutputBuffer / interface accessors (BrnNetworkModuleIO.h) that the X360
// Hex-Rays surfaced as free-function shims (Net / In / BrnNetwo / BrnNetworkMo /
// GetOnlineLobbyPlayerStatusInterface). Declared here so the bridge references them by
// name; the .cpp notes the committed accessor + byte offset each corresponds to. Bodies
// land with the OutputBuffer / NetworkToGuiInterface / status-interface TUs.
// -------------------------------------------------------------------------
namespace BrnNetwork
{
    namespace BrnNetworkModuleIO
    {
        // NetworkToGuiInterface::GetLiveRevengeRecord(index) (Hex-Rays "Net").
        const s32* GetLiveRevengeRecord(const void* lpNetworkToGuiInterface, int liIndex);
        // InGamePlayerStatusInterface::GetPlayerStatusData(index) (Hex-Rays "In").
        const void* In(const void* lpStatusInterface, int liIndex);
    }
    // record-heading-type accessor (Hex-Rays "BrnNetwo") + online-lobby interface base
    // (Hex-Rays "BrnNetworkMo").
    unsigned int GetScoreboardResponseHeadingType(const void* lpRecord);
    const void*  GetOnlineLobbyPlayerStatusInterface(const void* lpOutputBuffer);
}

namespace BrnGameState
{
    // GameStateModule::GetPreWorldInputBuffer (Hex-Rays "PreWorldInputB"). FLAG: the
    // committed GameStateModuleIO.h homes PreWorldInputBuffer + its GetGameEventQueue(), but
    // not this module-level accessor; declared here (body lands with the GameState module TU).
    GameStateModuleIO::PreWorldInputBuffer* GetPreWorldInputBuffer(GameStateModule* lpModule);
}
