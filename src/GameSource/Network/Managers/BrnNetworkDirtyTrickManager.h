// ===================================================================================
// BrnNetwork::NetworkDirtyTrickManager -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkDirtyTrickManager.h
//
// The online dirty-trick relay: holds a small fixed table of per-player send/receive
// dirty-trick message slots, keyed by network player id, and exchanges them over the
// reliable-message channel as players use paybacks on each other.
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../BrnNetworkDirtyTrickManager.h:62), gated against
// the X360 binary's GetDirtyTrickDataEntry @0x825486A8, whose linear search confirms the
// table layout exactly:
//   - it walks maDirtyTrickData with a 0x6C (108) byte stride, comparing the entry's
//     first word (mPlayerID @ +0) against the search key, for up to 7 entries, then
//     returns the matching entry pointer (asserting "lpEntry", at the manager's .cpp).
//
// DirtyTrickData (DWARF h:111-115): mPlayerID (s32) + two DirtyTrickMessage (send/recv).
//   The committed BrnNetwork::DirtyTrickMessage is a CgsNetwork::ReliableMessage subclass
//   (sizeof 0x28 base + 2 player ids + 2 u8 == 52 once 4-byte aligned), so the record is
//   4 + 52 + 52 == 108 (0x6C), matching the X360 search stride. A static_assert pins it.
//
// Only GetDirtyTrickDataEntry is binary-recovered in this TU (@0x825486A8 -- the X360
// export symbol is the shortened `Get`). All other methods are declared-only here; their
// bodies live in sibling TUs.
//
// The three manager pointers (BrnNetworkModule / CgsNetwork::PlayerManager /
// CgsNetwork::TimeManager) are forward-declared incomplete types -- this TU only needs
// their pointer slots at the DWARF offsets, never their layouts.
// ===================================================================================
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"      // BrnNetwork::NetworkPlayerID, EActiveRaceCarIndex
#include "GameSource/Network/Messages/BrnDirtyTrickMessage.h"    // BrnNetwork::DirtyTrickMessage (committed)

namespace CgsNetwork
{
    class PlayerManager;       // pointer-only (DWARF h:120)
    class TimeManager;         // pointer-only (DWARF h:121)
    class ReliableMessage;     // callback parameter (DWARF h:151)
    struct SignalMessage;      // callback parameter (DWARF h:160)
}

namespace BrnNetwork
{
    class BrnNetworkModule;    // pointer-only (DWARF h:119)

    namespace BrnNetworkModuleIO
    {
        struct OutputBuffer;                  // ProcessBeforeSimulation param (declared-only)
        struct PostSimulationInputBuffer;     // ProcessAfterSimulation param (declared-only)
    }

    struct NetworkDirtyTrickManager
    {
        // DWARF BrnNetworkDirtyTrickManager.h:111 -- one player's send/receive slot pair.
        struct DirtyTrickData
        {
            NetworkPlayerID   mPlayerID;                // +0x00 (DWARF h:113)
            DirtyTrickMessage mDirtyTrickMessageSend;   // +0x04 (DWARF h:114)
            DirtyTrickMessage mDirtyTrickMessageRecv;   // +0x38 (DWARF h:115)
        };

        // ---- lifecycle / API (declared-only; bodies are separate TUs) --------------
        void Construct(BrnNetworkModule* lpNetworkModule,
                       CgsNetwork::PlayerManager* lpPlayerManager,
                       CgsNetwork::TimeManager* lpTimeManager);
        bool Prepare();
        bool Release();
        void Destruct();
        void ProcessBeforeSimulation(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer);
        void ProcessAfterSimulation(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInputBuffer);
        void AddPlayer(NetworkPlayerID lNetworkPlayerID);
        void RemovePlayer(NetworkPlayerID lNetworkPlayerID);
        void Disconnected();

        // ---- owned by THIS TU ------------------------------------------------------
        // X360 0x825486A8 (export symbol `Get`): linear-search the table for the slot whose
        // mPlayerID matches lNetworkPlayerID; assert + return the entry (DWARF h:126).
        DirtyTrickData* GetDirtyTrickDataEntry(NetworkPlayerID lNetworkPlayerID);

    private:
        void ProcessDirtyTrickEvents();
        void SendDirtyTrickMessage(EActiveRaceCarIndex leAggressor, EActiveRaceCarIndex leVictim,
                                   u8 lu8DirtyTrickType, u8 lu8DirtyTrickStatus);
        void ReceiveDirtyTrickMessage(NetworkPlayerID lNetworkPlayerID, DirtyTrickMessage* lpMessage);
        void _DirtyTrickMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                               NetworkPlayerID lNetworkPlayerID, void* lpContext);
        void _DirtyTrickDeliveredCallback(bool lbA, bool lbB, CgsNetwork::SignalMessage* lpMessage,
                                          NetworkPlayerID lNetworkPlayerID, void* lpContext);

        // ---- data layout (DWARF h:118-121) -----------------------------------------
        static const s32 KI_MAX_DIRTY_TRICK_PLAYERS = 7;

        DirtyTrickData     maDirtyTrickData[KI_MAX_DIRTY_TRICK_PLAYERS];  // +0x000 (X360: 7 x 0x6C == 108B stride)
        BrnNetworkModule*          mpNetworkModule;                       // (DWARF h:119)
        CgsNetwork::PlayerManager* mpPlayerManager;                       // (DWARF h:120)
        CgsNetwork::TimeManager*   mpTimeManager;                         // (DWARF h:121)
    };

    // NOTE on the X360 record stride: GetDirtyTrickDataEntry @0x825486A8 walks this table
    // at a 0x6C (108) byte stride -- mPlayerID(4) + DirtyTrickMessage(52) + DirtyTrickMessage(52),
    // where DirtyTrickMessage is 52 bytes on the 32-bit X360 (its explicit mpVTable member +
    // ReliableMessage payload + 2 ids + 2 u8). This project targets SEMANTIC parity on the host
    // (x64, 8-byte pointers), so the host sizeof differs and is intentionally NOT pinned by a
    // static_assert; the search accesses maDirtyTrickData[i].mPlayerID strictly by name, which is
    // stride-correct on either target. The 0x6C stride is documented from the X360 asm only.
} // namespace BrnNetwork
