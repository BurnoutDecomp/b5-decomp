#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"      // CgsSystem::Time (GetTime() return)
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"  // CgsSystem::EFrameRate (GetLocalConsoleFrameRate() return)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"   // BrnNetwork::NetworkPlayerID (committed typedef)
#include "GameSource/Network/BrnServerInterface.h"            // BrnNetwork::BrnServerInterface (embedded by value)
#include "GameShared/GameClasses/Network/Packeting/CgsNetworkAdapterBase.h" // CgsNetwork::NetworkAdapter (canonical home; embedded mNetworkAdapter)

namespace CgsNetwork
{
    // Pointer-only use below (the network-player-id pack/unpack helper takes a Message*);
    // forward-declared to avoid pulling the whole message hierarchy into this header.
    struct Message;
    // Pointer-only return of GetPlayerManager(); forward-declared to avoid pulling the whole
    // player registry into this header.
    struct PlayerManager;
    // Pointer-only return of GetPlayersConnectionManager() (BrnNetworkLaunchManager TU).
    class PlayersConnectionManager;
}

namespace CgsSystem
{
    // Pointer-only return of GetTimerStatus() (BrnNetworkLaunchManager TU): the published
    // per-frame timer snapshot.
    class TimerStatusInterface;
}

namespace BrnNetwork
{
    class NetworkServers;
    class SuspensionManager;          // pointer-only (own header)
    struct StateManager;              // pointer-only (own header; GetStateManager())
    struct LiveRevengeRelationship;   // pointer-only (own header)

    // The online live-revenge bookkeeping (per-rival relationship table). MINIMAL SLICE:
    // the BrnNetworkAggressiveDrivingManager TU only needs to look up the mutable relationship
    // for a given network player and read its current point-of-view score; the full layout and
    // the remaining API live in the LiveRevengeManager's own TU.
    // ADDITIVE GROW (BrnNetworkAggressiveDrivingManager TU): declared-only.
    //
    // Guard: when the full LiveRevengeManager header is included first (e.g. in the
    // implementing TU), the macro BRNETWORK_LIVEREVENGEMANAGER_DEFINED is set and this
    // minimal slice is suppressed to avoid a struct-redefinition ODR error.
#ifndef BRNETWORK_LIVEREVENGEMANAGER_DEFINED
    class LiveRevengeManager
    {
    public:
        // X360 BrnNetwork::LiveRevengeManager::GetNonConstRevengeRelation: returns the writable
        // relationship record for lNetworkPlayerID (or nullptr when there is none). Declared-only.
        LiveRevengeRelationship* GetNonConstRevengeRelation(NetworkPlayerID lNetworkPlayerID);

        // X360 BrnNetwork::LiveRevengeManager::GetNumberOfRivals: how many online rivals the
        // local player currently has a live-revenge relationship with. Read by
        // PostRoundManager::ProcessRaceResults to stamp the game-stats record. Declared-only;
        // body lands with the LiveRevengeManager TU. ADDITIVE GROW (PostRoundManager TU).
        s32 GetNumberOfRivals() const;
    };
#endif // BRNETWORK_LIVEREVENGEMANAGER_DEFINED
}

namespace CgsNetwork
{
    class VersionDisplay
    {
        friend class BrnNetwork::NetworkServers;

    private:
        const char* mpcVersion;
        EServerType meServerType;
    };

    // NetworkAdapter is the canonical struct homed in CgsNetworkAdapterBase.h (included above);
    // reused by name here as the embedded mNetworkAdapter member. (The earlier stale inline
    // class definition here was an ODR duplicate now that the canonical home is included.)
}

namespace BrnNetwork
{
    class NetworkGamerCardManagerX360;   // pointer-only (GetGamerCardManager return)

    class BrnNetworkManager
    {
        friend class NetworkServers;

    public:
        // CgsMessage.h:85 -- per-field (de)serialise status. 0 == success; callers OR the
        // per-field results together into the message's overall result (DWARF
        // BrnNetworkManager.h:373 returns this type).
        typedef u8 PackOrUnpackResult;

        // BrnNetworkManager.h:373 (X360 @ 0x82881xxx static helper): (de)serialise one
        // NetworkPlayerID field through a message's bitstream, mirroring the shared
        // CgsNetwork::PackOrUnpack* field primitives. The X360 call site passes only the
        // message and the field pointer (no manager `this`), so this is a static helper.
        // Returns the per-field status (0 == success).
        static PackOrUnpackResult PackOrUnpack(CgsNetwork::Message* lpMessage,
                                               NetworkPlayerID* lpNetworkPlayerID);

        BrnServerInterface* GetServerInterface()
        {
            return &mServerInterface;
        }

        const BrnServerInterface* GetServerInterface() const
        {
            return &mServerInterface;
        }

        // The embedded NetworkServers sub-object (X360: the debug component reaches it as
        // *(this+271820), i.e. an inlined accessor on the full manager). Declared-only here; the
        // minimal manager slice does not yet materialise the storage (body lands with the full
        // BrnNetworkManager TU).
        NetworkServers* GetNetworkServers();

        // The embedded session player registry (X360: reliable-message registrars take its
        // address as &GetNetworkManager()->mpPlayerManager, e.g.
        // BrnNetwork::SelectedRoutesManager::AddPlayer/RemovePlayer @ 0x8255BDA0/0x8255BEC8).
        // Declared-only here; the minimal manager slice does not yet materialise the storage
        // (body lands with the full BrnNetworkManager TU).
        CgsNetwork::PlayerManager* GetPlayerManager();

        // The active-race-car slot index a given network player currently occupies, or -1 when
        // the player has no race car (X360: BrnNetwork::BrnNetworkManager::GetActiveRaceCarIndex,
        // taken on `this` with a NetworkPlayerID; BrnNetwork::TeamSelectionManager's team-
        // assignment actions / _TeamSelectionMessageArrivedCallback index maActiveRaceCarTeam by
        // it -- see ActionAssignCoopStuntRunTeams @ 0x8254BC48). Declared-only here; the storage /
        // body materialise with the full BrnNetworkManager TU. ADDITIVE GROW
        // (BrnNetworkTeamSelectionManager TU).
        s32 GetActiveRaceCarIndex( NetworkPlayerID lPlayerID );

        // The signed-in local user's controller/pad index (X360: read whole as *(this+0x60);
        // BrnNetwork::BuddyManagerX360::DoInvite compares the invite's requesting controller port
        // against it to decide whether the invite targets the LOCAL user -- see DoInvite
        // @ 0x825700A8, `subf` of *(NetworkManager+0x60) from the params' miUserControllerPort).
        // Declared-only here; the storage materialises with the full BrnNetworkManager TU.
        // ADDITIVE GROW (BrnNetworkBuddyManagerX360 TU).
        s32 GetLocalUserControllerPort() const;

        // Clear the cached "local user is signed in" flag (X360: stb 0 at this+0x3D0CC). Called
        // by BrnNetwork::NetworkNotificationManagerX360::ProcessNotifications when a system
        // sign-in change affects the local user's controller port. Declared-only here; storage
        // materialises with the full BrnNetworkManager TU. ADDITIVE GROW
        // (BrnNetworkNotificationManagerX360 TU).
        void ClearLocalUserSignedInFlag();

        // The running network send-frame counter (X360: *(this+0x3658), read whole then taken
        // modulo 0xFFFF to derive a reliable-message frame id; see
        // BrnNetwork::MarkedManManager::SendMarkedManDataToAll @ 0x82548DCC). Declared-only here;
        // the storage materialises with the full BrnNetworkManager TU.
        u32 GetCurrentFrame() const;

        // The embedded live-revenge manager (X360: AddTakedownEvent reaches it as
        // &GetNetworkManager()->mpLiveRevengeManager). Declared-only; storage lands with the full
        // BrnNetworkManager TU. ADDITIVE GROW (BrnNetworkAggressiveDrivingManager TU).
        LiveRevengeManager* GetLiveRevengeManager();

        // The embedded X360 gamer-card manager (X360: reached as an embedded sub-object at
        // manager +0x41AB0; resolves a player's XUID by name). Read by
        // BrnNetwork::ScoreboardManager::HandleEvScoreTargetEvent. Declared-only here; storage
        // lands with the full BrnNetworkManager TU. ADDITIVE GROW (BrnNetworkScoreboardManager TU).
        NetworkGamerCardManagerX360* GetGamerCardManager();

        // The current online round number (X360: read whole as a u8 at *(this+613796) ==
        // *(this+0x95DA4); BrnNetwork::StandingsManager stamps the sent PlayerFinishedRoundMessage
        // with it and matches received messages against it -- see HandlePlayerFinishedMode @
        // 0x82550BB8 / _RoundFinishedMessageArrivedCallback @ 0x82545530). Declared-only here;
        // the storage materialises with the full BrnNetworkManager TU. ADDITIVE GROW
        // (BrnNetworkStandingsManager TU).
        u8 GetCurrentRoundNumber() const;

        // ---- ADDITIVE GROW (BrnNetworkLaunchManager TU) -----------------------------------
        // The launch state machine reaches four further embedded sub-objects of the full
        // manager (X360 byte offsets, from the LaunchManager asm against mpNetworkManager):
        //   GetPlayersConnectionManager() -> +0x78    (AreAllConnectionsSuccessful gate)
        //   GetSuspensionManager()        -> +0x6438  (driven each UpdateSuspending tick)
        //   GetStateManager()             -> +0x3DD90 (GameModeHasEnoughTeams gate)
        //   GetTimerStatus()              -> +0x95D8C (the per-frame timer snapshot)
        // Declared-only here; storage materialises with the full BrnNetworkManager TU.
        CgsNetwork::PlayersConnectionManager* GetPlayersConnectionManager();
        SuspensionManager*                    GetSuspensionManager();
        StateManager*                         GetStateManager();
        CgsSystem::TimerStatusInterface*      GetTimerStatus();

        // (GetSuspensionManager() is also reached by PostRoundManager::UpdateWaitResume as the
        // +25656 mpSuspensionManager sub-object; declared once above. ADDITIVE GROW shared by
        // the BrnNetworkLaunchManager / BrnNetworkPostRoundManager TUs.)

        // The current network/session time, read whole as the CgsSystem::Time value (an integer
        // second count + a sub-second fraction) living deep in the manager. BrnNetwork::Connection-
        // Manager reads it to stamp mLastKickTime and to measure the NAT-kick interval (X360
        // KickUnNATablePlayer @ 0x82566860 / UpdateNATData @ 0x8256CFF0 read the 8-byte time pair
        // *(this+613776)/*(this+613780)). Declared-only here; the storage materialises with the
        // full BrnNetworkManager TU. ADDITIVE GROW (BrnNetworkConnectionManager TU).
        CgsSystem::Time GetTime() const;

        // The local console's frame rate (X360: read whole as *(this+0x95DA0) and compared to
        // CgsSystem::E_FRAMERATE_50HZ/_60HZ; see BrnNetwork::BrnNetworkPlayer::
        // ProcessReceivedStuntMultiplier @ 0x82593E7C). Declared-only here; the storage
        // materialises with the full BrnNetworkManager TU. ADDITIVE GROW.
        CgsSystem::EFrameRate GetLocalConsoleFrameRate() const;

        // ---- ADDITIVE GROW (BrnNetworkAutoLoginManager TU) --------------------------------
        // The auto-login state machine reaches the manager's embedded login sub-machine. The
        // X360 (AutoLoginManager::UpdateWaitAutoLogin / ::UpdateConnecting) asserts the login
        // manager is present (manager + 0x4DB4 != 0) and reads its current state value
        // (lwz manager + 0x4DC0) to decide whether a sign-in is already in progress. The state
        // value is compared against the grounded literals 14 and 15 ("signing in" / "busy");
        // the full state enum is owned by the login manager's own TU, so the raw value is
        // exposed here and the comparison literals live (named) at the call site.
        // Declared-only; storage / body materialise with the full BrnNetworkManager TU.
        bool HasLoginManager() const;
        s32  GetNetworkLoginState() const;

        // The auto-login manager notifies the network manager that an auto-login connect has
        // completed (X360 AutoLoginManager::Connect -> BrnNetworkManager::OnAutoLogin).
        // Declared-only; body lands with the full BrnNetworkManager TU.
        void OnAutoLogin();

        // X360 @ 0x82581DB0 (called by BrnNetwork::StateManager::UpdateLogin /
        // ::HandleConnectEvent) -- begin an interactive sign-in: pure tail-call forwarder that
        // hands the login-connect arguments to the embedded login sub-object at this+0x41E90
        // (269968) Connect(...). X360 register shape is (a2..a6 : s32, a7 : s64); the exact
        // param types belong to that login sub-object's own un-homed TU, so the raw arg shape
        // is reproduced here (FLAG: re-type once the login sub-object is reconstructed).
        // DECLARATION-ONLY (FLAGGED: un-homed full layout).
        void OnLogIn( s32 liUserContext, s32 liArg2, s32 liArg3, s32 liArg4, s32 liArg5, s64 lliAccountId );

        // X360 @ 0x82581E28 (called by BrnNetwork::StateManager::ProcessNetworkEvents) --
        // latch the one-shot 'finished boot' flag (word at this+0x41E98 / 269976, the +8 field
        // of the embedded login sub-object at this+0x41E90): set it to 1 iff still 0.
        // DECLARATION-ONLY (FLAGGED: un-homed full layout / un-homed flag offset).
        void OnFinishedBoot();

        // X360 @ 0x82542668 (DWARF BrnNetworkManager.h:368, returns bool) -- true when the
        // local player is in a not-yet-started online game (the free-burn lobby wait). Reads
        // the embedded server-interface games component (this+0x38F4 / 14580): returns false
        // unless ServerInterfaceGames::IsLocalPlayerInGame() && !ServerInterfaceGames::IsGameStarted().
        // DECLARATION-ONLY (FLAGGED: un-homed full layout / un-homed games-component offset).
        bool IsDoingFreeBurnLobby();

        // ---- ADDITIVE GROW (BrnNetworkEventScoresManager TU) ------------------------------
        // EventScoresManager signals the manager that a stage of the auto-login process is done
        // (X360: every call site passes the literal 2 in r4 -- e.g. UpdateLoggedIn @ 0x82556BB8,
        // OnAutoLogin @ 0x8254B5C0, _UploadEventScoreCallback @ 0x825654A0 all call
        // OnAutoLoginProcessComplete(GetNetworkManager(), 2)). The argument is a process-stage
        // selector; its enum home is the auto-login manager's own TU, so it is exposed as a raw
        // s32 here (FLAG: re-type to the login-process-stage enum once that enum is reconstructed).
        // Declared-only; body lands with the full BrnNetworkManager TU.
        void OnAutoLoginProcessComplete( s32 liProcessStage );

        // ---- ADDITIVE GROW (BrnNetworkLoginManagerBase TU) --------------------------------
        // The login state machine raises a login-flow GUI/network event up to the rest of the
        // manager (X360: every BrnNetwork::LoginManagerBase answer/update path calls
        // TriggerEventFromLogin(GetNetworkManager(), <eventCode>, 0) -- e.g. AnswerAgreeTOS @
        // 0x82566478 passes r4 == 7 (agreed) / 4 (declined) with r5 == 0; UpdateLoggingIn passes
        // 1; UpdateDownloadingTOS passes 0). The first argument is the login-event code; the
        // second is a payload word every recovered call site leaves 0. The event-code enum's home
        // is the manager's own TU, so the raw s32 is exposed here (FLAG: re-type to the login-event
        // enum once it is reconstructed). Declared-only; body lands with the full
        // BrnNetworkManager TU.
        void TriggerEventFromLogin( s32 liEventCode, s32 liParam );

        // ====================================================================================
        // ---- ADDITIVE GROW (BrnNetworkManager.cpp TU) -------------------------------------
        // The top-level network manager's own lifecycle / per-frame / message-dispatch spine.
        // All but PackOrUnpack are DECLARATION-ONLY: their X360 bodies operate entirely over
        // the full ~613 KB BrnNetworkManager aggregate (28+ embedded manager sub-objects and
        // dozens of fields reached by raw byte offset), which is GENUINELY UN-HOMED in this
        // minimal slice. Materialising that layout would mean fabricating the whole class, so
        // per the anti-fabrication rule these are declared (exact reconstructed signatures
        // from the X360 asm) but left bodyless until the full layout is homed. FLAGGED.
        //
        // (TriggerEventFromLogin / GetLocalConsoleFrameRate / GetTime are already declared
        // above by earlier ADDITIVE GROWs and are NOT re-declared here.)

        // X360 @ 0x825885E8 (EXECUTED, called by Construct) -- returns the largest message size
        // across the static message-definition table, optionally filtered to reliable-only
        // messages. DECLARATION-ONLY (FLAGGED): the body stages a 41-entry {s32 miMessageSize;
        // bool mbReliable} MessageDef table from pure immediates on the stack then runs a
        // rw::math::vpu::Max<s32> reduction (BrnNetworkManager.cpp:1839..1897). Deferred rather
        // than hand-transcribed (41 immediate rows -> fabrication risk); no manager-`this` dep.
        s32 GetMaxMessageSize( bool lbReliableOnly );

        // X360 @ 0x8258E248 -- full teardown: zeroes the session-time/round bookkeeping then
        // tears down every embedded manager sub-object in reverse-construction order, finally
        // chaining the CgsNetwork::NetworkManager base Destruct. DECLARATION-ONLY (FLAGGED:
        // un-homed full layout).
        void Destruct();

        // X360 @ 0x8259AC20 -- the staged online-subsystem bring-up state machine (EPrepareStage
        // walk, one embedded manager Prepare()'d per stage). DECLARATION-ONLY (FLAGGED: un-homed
        // full layout + the multi-stage init pipeline).
        bool Prepare();

        // X360 @ 0x8258DC88 -- the staged online-subsystem tear-down state machine (EReleaseStage
        // walk, mirror of Prepare). DECLARATION-ONLY (FLAGGED: un-homed full layout).
        bool Release();

        // ---- per-frame dispatch -----------------------------------------------------------
        // X360 @ 0x825977D0 -- snapshots the published timer status, runs the HL-update-flag
        // handler, then ticks every embedded manager's ProcessBeforeSimulation and emits the
        // per-frame player status/results. DECLARATION-ONLY (FLAGGED: un-homed full layout).
        void ProcessBeforeSimulation( s32 liUpdateFlags, f32 lfDeltaTime, const void* lpTimerStatus );

        // X360 @ 0x825968C8 -- reacts to the high-level update flag bits (bit 0x200 == request
        // disconnect: drops the server connection and queues the disconnected GUI/network
        // events). DECLARATION-ONLY (FLAGGED: un-homed full layout).
        void ProcessHLUpdateFlags( s16 lu16UpdateFlags );

        // X360 @ 0x825967F0 -- on a network DXT-decode request event, copies the image header and
        // hands the compressed payload to the texture decompressor with DxtDecodeCallback.
        // DECLARATION-ONLY (FLAGGED: un-homed full layout).
        void ProcessNetworkTextureDecodeEvent( const void* lpDxtDecodeRequestEvent );

        // ---- session / online lifecycle hooks ---------------------------------------------
        // X360 @ 0x82581CB0 / 0x82581D00 -- entering / leaving an online game (chain base hook,
        // notify road-rules + launch/selected-routes/traffic/event-scores managers, reset the
        // session start frame). DECLARATION-ONLY (FLAGGED: un-homed full layout).
        void OnEnterGame();
        void OnLeaveGame();

        // X360 @ 0x82581E48 -- the session is about to launch: forward to the road-rules and
        // traffic managers. DECLARATION-ONLY (FLAGGED: un-homed full layout).
        void OnGameLaunching();

        // X360 @ 0x82582118 -- join the in-progress game session (delegate to the embedded
        // StateManager). DECLARATION-ONLY (FLAGGED: un-homed full layout).
        void JoinGameSession();

        // ---- player / stats queries -------------------------------------------------------
        // X360 @ 0x82593C70 -- true when lPlayerID is the local signed-in player (compares the
        // server-interface local-player-info id). DECLARATION-ONLY (FLAGGED: un-homed full
        // layout / un-homed ServerInterfacePlayerInfoDataX360 helper).
        bool IsLocalPlayer( NetworkPlayerID lPlayerID );

        // X360 @ 0x82588868 -- fills the per-frame player-results GUI buffer by walking the
        // session player list and copying each valid player's standings record.
        // DECLARATION-ONLY (FLAGGED: un-homed full layout).
        void OutputPlayerResultsInfo( void* lpOutput );

        // X360 @ 0x82596018 -- fills the per-frame player-status GUI buffer.
        // DECLARATION-ONLY (FLAGGED: un-homed full layout).
        void OutputPlayerStatusInfo( void* lpOutput );

        // X360 @ 0x82595CD0 -- pushes one player's stats record up to the GUI (resolving the
        // player by id, falling back to the local player's stats out of game). DECLARATION-ONLY
        // (FLAGGED: un-homed full layout).
        void OutputPlayerStatsToGui( NetworkPlayerID lPlayerID );

        // ---- telemetry --------------------------------------------------------------------
        // X360 @ 0x82582178 -- forward a telemetry event (with one parameter word) to the server
        // interface's telemetry component. DECLARATION-ONLY (FLAGGED: un-homed full layout /
        // un-homed ETelemetryHook enum). The X360 has further overloads (int / CgsID / Time&
        // parameter); only this one is in this TU's postmortem, so only it is declared.
        s32 CaptureTelemetryEvent( s32 leType, s32 liParameter );

        // ---- raised events ----------------------------------------------------------------
        // X360 @ 0x82598BE0 -- raise a server-interface-originated GUI/network event up to the
        // rest of the manager. DECLARATION-ONLY (FLAGGED: un-homed full layout).
        void TriggerEventFromServerInterface( s32 liEventCode, s32 liParam );

        // ---- callbacks (static; userdata is the manager) ----------------------------------
        // X360 @ 0x82581BA8 -- start-time-sync "client ready" callback: when the local player is
        // in a not-yet-started game, restart the network traffic. Static (takes the manager as
        // userdata). DECLARATION-ONLY (FLAGGED: un-homed full layout).
        static u32 SyncTimeClientReadyCallback( NetworkPlayerID lClientReadyID, void* lpUserData );

        // X360 @ 0x82593CD8 -- texture-decompressor completion callback: queues the decoded
        // pixels onto the network event queue. Static (takes the manager as userdata).
        // DECLARATION-ONLY (FLAGGED: un-homed full layout).
        static s32 DxtDecodeCallback( void* lpPixels, void* lpUserData );

        // X360 @ 0x82599430 -- the session player-manager event callback (player joined/left
        // etc.); routes the event to the relevant embedded managers. Static (takes the manager
        // as userdata). DECLARATION-ONLY (FLAGGED: un-homed full layout).
        static s32 PlayerManagerEventCallback( void* lpUserData, s32 a2, s32 a3 );

    private:
        CgsNetwork::VersionDisplay mVersionDisplay;
        CgsNetwork::NetworkAdapter mNetworkAdapter;
        BrnServerInterface mServerInterface;
    };
}
