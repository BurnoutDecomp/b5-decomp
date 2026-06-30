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

        // The running network send-frame counter (X360: *(this+0x3658), read whole then taken
        // modulo 0xFFFF to derive a reliable-message frame id; see
        // BrnNetwork::MarkedManManager::SendMarkedManDataToAll @ 0x82548DCC). Declared-only here;
        // the storage materialises with the full BrnNetworkManager TU.
        u32 GetCurrentFrame() const;

        // The embedded live-revenge manager (X360: AddTakedownEvent reaches it as
        // &GetNetworkManager()->mpLiveRevengeManager). Declared-only; storage lands with the full
        // BrnNetworkManager TU. ADDITIVE GROW (BrnNetworkAggressiveDrivingManager TU).
        LiveRevengeManager* GetLiveRevengeManager();

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

        // ---- ADDITIVE GROW (BrnNetworkEventScoresManager TU) ------------------------------
        // EventScoresManager signals the manager that a stage of the auto-login process is done
        // (X360: every call site passes the literal 2 in r4 -- e.g. UpdateLoggedIn @ 0x82556BB8,
        // OnAutoLogin @ 0x8254B5C0, _UploadEventScoreCallback @ 0x825654A0 all call
        // OnAutoLoginProcessComplete(GetNetworkManager(), 2)). The argument is a process-stage
        // selector; its enum home is the auto-login manager's own TU, so it is exposed as a raw
        // s32 here (FLAG: re-type to the login-process-stage enum once that enum is reconstructed).
        // Declared-only; body lands with the full BrnNetworkManager TU.
        void OnAutoLoginProcessComplete( s32 liProcessStage );

    private:
        CgsNetwork::VersionDisplay mVersionDisplay;
        CgsNetwork::NetworkAdapter mNetworkAdapter;
        BrnServerInterface mServerInterface;
    };
}
