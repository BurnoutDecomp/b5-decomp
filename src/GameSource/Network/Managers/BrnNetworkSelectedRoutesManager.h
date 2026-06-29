// ---- GameSource/Network/Managers/BrnNetworkSelectedRoutesManager.h ----
// Member names, ordering, return types, const-ness and the SelectedRoutesData inner
// layout recovered from the DecFIGS DWARF for this exact header
// (references/DecFIGS/dwarfdump/GameSource/Network/Managers/BrnNetworkSelectedRoutesManager.h),
// gated against the X360 binary (the full manager TU @ 0x8255B... / 0x82548...).
//
// GROWN by the manager's own TU (BrnNetworkSelectedRoutesManager.cpp): the leading member
// bulk that the original commit carried as a 0-size honest placeholder is now materialised
// with the real typed members, because every dependency type has a committed home:
//   - SpecificGameModeEventInterface::Event   (BrnGameStateSharedIO.h)
//   - BrnNetwork::SelectedRoutesMessage        (BrnSelectedRoutesMessage.h)
//   - CgsContainers::BitArray<10>              (CgsBitArray.h)
// Members are accessed BY NAME; the C++ struct uses native x64 widths (8-byte pointers) and
// the compiler lays it out naturally -- semantic parity, not byte-exact X360 layout (the
// project rule; cf. CgsNetworkPlayer.h). The X360 byte offsets are kept as documentation.
//
// X360 LAYOUT (32-bit-pointer ABI, documentation only):
//   +0x000  maEvents[10]            (Event, 44B each            -> +0x1B8 == 440)
//   +0x1B8  maSelectedRoutesData[7] (SelectedRoutesData, 216B   -> +0x7A0 == 1952)
//   +0x7A0  mpNetworkModule         (BrnNetworkModule*)
//   +0x7A4  mpTimeManager           (CgsNetwork::TimeManager*)
//   +0x7A8  miNumRounds             (s32)
//   +0x7AC  mbRoutesChanged         (bool, trailing byte)  <- Release stb 0 @ +1964
//
//   SelectedRoutesData (216B X360 stride):
//     +0x00  mau16FrameNumRoundSent[10]   (u16[10], 20B)
//     +0x14  mPlayerID                    (NetworkPlayerID s32, sentinel -1)
//     +0x18  mMessageSend                 (SelectedRoutesMessage, 88B -> +0x70)
//     +0x70  mMessageRecv                 (SelectedRoutesMessage, 88B -> +0xC8)
//     +0xC8  mRoundsSent                  (BitArray<10>, 8B)
//     +0xD0  mRoundsReceived              (BitArray<10>, 8B)
#pragma once

#include "types.hpp"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"        // BrnNetwork::NetworkPlayerID (== s32)
#include "GameSource/GameState/BrnGameStateSharedIO.h"             // SpecificGameModeEventInterface::Event
#include "GameSource/Network/Messages/BrnSelectedRoutesMessage.h"  // BrnNetwork::SelectedRoutesMessage
#include "GameShared/GameClasses/Containers/CgsBitArray.h"         // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"    // CgsNetwork::TimeManager (frame-number source)

namespace CgsNetwork
{
    // Pointer-only params on the private reliable-message callbacks (no member access here);
    // forward-declared to avoid pulling the message hierarchy into this header.
    struct ReliableMessage;
    struct SignalMessage;
}

namespace BrnNetwork
{
    // Pointer-only members / params -- declared-only here.
    class BrnNetworkModule;

    class SelectedRoutesManager
    {
    public:
        // The committed per-event record type (home: BrnGameStateSharedIO.h), reused BY NAME.
        typedef BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event Event;

        // "No player" sentinel (== CgsNetwork::K_INVALID_PLAYER_ID, -1).
        static const NetworkPlayerID KI_INVALID_PLAYER_ID = -1;

        // Number of round slots == BitArray<10> capacity (the per-round message/bit storage).
        static const s32 KI_MAX_ROUNDS  = 10;
        // Number of remote-player slots tracked.
        static const s32 KI_MAX_PLAYERS = 7;

        // The wire EMessageType stamped onto a selected-routes reliable send (X360 li r4,0x17).
        static const s32 KI_SELECTED_ROUTES_MESSAGE_TYPE = 23;
        // Packed length passed to RegisterMessageType (X360 li r5,0x58).
        static const s32 KI_SELECTED_ROUTES_MESSAGE_LENGTH = 88;

        // === API ===
        void Construct( BrnNetworkModule* lpNetworkModule, CgsNetwork::TimeManager* lpTimeManager );
        bool Prepare();
        // @ 0x82548B98 -- clear the trailing routes-changed flag and report success (inline below).
        bool Release();
        void Destruct();
        void Update();
        void AddPlayer( NetworkPlayerID lPlayerID );
        void RemovePlayer( NetworkPlayerID lPlayerID );
        void Disconnected();
        void OnLeaveGame();
        bool HaveAllPlayersReceivedRoutes() const;
        void SendRouteData();
        // X360 takes a 3rd argument (the active game-mode type, r6) that the PS3 DWARF dropped;
        // the ASM is authoritative (the per-mode landmark-validity asserts switch on it).
        void SetRouteData( s32 liNumRounds, const Event* laEvents,
                           BrnGameState::GameStateModuleIO::EGameModeType leGameModeType );
        void GetRouteData( Event* laEvents ) const;
        bool HaveRoutesChanged() const;
        void SetRoutesChanged( bool lbChanged );

        // Inner per-player record (DWARF :125). Round-send frame stamps, the player it tracks,
        // the send/recv reliable-message slots, and the sent/received round bit sets.
        struct SelectedRoutesData
        {
            u16                          mau16FrameNumRoundSent[KI_MAX_ROUNDS]; // +0x00
            NetworkPlayerID              mPlayerID;                             // +0x14 (sentinel -1)
            SelectedRoutesMessage        mMessageSend;                          // +0x18 (88B)
            SelectedRoutesMessage        mMessageRecv;                          // +0x70 (88B)
            CgsContainers::BitArray<KI_MAX_ROUNDS> mRoundsSent;                 // +0xC8
            CgsContainers::BitArray<KI_MAX_ROUNDS> mRoundsReceived;             // +0xD0
        };

    private:
        SelectedRoutesData* GetSelectedRoutesDataEntry( NetworkPlayerID lPlayerID );
        void ClearReceivedRounds();

        // The two reliable-message callbacks registered per player (DWARF :158/:167). The
        // userData is the manager `this`.
        static void _SelectedRoutesMessageArrivedCallback( CgsNetwork::ReliableMessage* lpMessage,
                                                           NetworkPlayerID lSendingPlayerID,
                                                           void* lpUserData );
        static void _SelectedRoutesMessageDeliveredCallback( bool lbDelivered, bool lbWasReliable,
                                                            CgsNetwork::SignalMessage* lpMessage,
                                                            NetworkPlayerID lToPlayerID,
                                                            void* lpUserData );

        // --- members (DWARF-ordered) ---------------------------------------------------
        Event              maEvents[KI_MAX_ROUNDS];                 // +0x000  DWARF :136
        SelectedRoutesData maSelectedRoutesData[KI_MAX_PLAYERS];    // +0x1B8  DWARF :137
        BrnNetworkModule*  mpNetworkModule;                         // +0x7A0  DWARF :139
        CgsNetwork::TimeManager* mpTimeManager;                     // +0x7A4  DWARF :140
        s32                miNumRounds;                             // +0x7A8  DWARF :142
        bool               mbRoutesChanged;                         // +0x7AC  DWARF :144
    };

    // ---- Release @ 0x82548B98 -------------------------------------------------------------
    // X360: mbRoutesChanged = false; return true;  (single-byte store of 0 at the trailing
    // flag, then r3 = 1).
    inline bool SelectedRoutesManager::Release()
    {
        mbRoutesChanged = false;
        return true;
    }

    inline bool SelectedRoutesManager::HaveRoutesChanged() const
    {
        return mbRoutesChanged;
    }

    inline void SelectedRoutesManager::SetRoutesChanged( bool lbChanged )
    {
        mbRoutesChanged = lbChanged;
    }
}
