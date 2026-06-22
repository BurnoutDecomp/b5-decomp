// ---- GameSource/Network/Managers/BrnNetworkSelectedRoutesManager.h ----
// Member names, ordering, return types, const-ness and the SelectedRoutesData inner
// layout recovered from the DecFIGS DWARF for this exact header
// (references/DecFIGS/dwarfdump/GameSource/Network/Managers/BrnNetworkSelectedRoutesManager.h),
// gated against the X360 binary (BrnNetwork::SelectedRoutesManager::Release @ 0x82548B98).
//
// Layout proof for Release (the only function homed in this TU):
//   X360:  *(a1 + 1964) = 0;  return 1;
//          0x82548BA4  stb r10, 0x7AC(r11)   ; 0x7AC == 1964, one byte, value 0
//          0x82548BA0  li  r3, 1             ; return true
//   mbRoutesChanged is the LAST member of the class (DWARF member order), so the X360
//   trailing-byte store at +1964 is exactly mbRoutesChanged = false. The absolute offset
//   1964 was produced with the X360 32-bit ABI (two 4-byte pointers, 32-bit-pointer
//   internals inside maEvents / maSelectedRoutesData); on the 64-bit host the absolute
//   offset differs, so it is NOT asserted here. The semantic -- "clear the trailing
//   routes-changed flag, return true" -- is what is reconstructed and gated.
#pragma once

#include "types.hpp"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"  // committed BrnNetwork::NetworkPlayerID (== s32)

namespace BrnNetwork
{
    // Full SelectedRoutesManager is reconstructed in its own class-sourced TU
    // (GameSource/Network/Managers/BrnNetworkSelectedRoutesManager.cpp -- Construct/Prepare/
    // Update/AddPlayer/.../SendRouteData/the arrived+delivered callbacks). This header is the
    // committed home: it declares the whole API surface (declared-only for the manager's own
    // TU) and homes the single trivial accessor the boot trace needs, Release().
    //
    // The leading member bulk (maEvents[10], maSelectedRoutesData[7]) is built from
    // uncommitted dependency types -- BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event,
    // BrnNetwork::SelectedRoutesMessage (: CgsNetwork::ReliableMessage), and
    // CgsContainers::BitArray<10>. Those types are NOT forked here; they are reconstructed in
    // their own homes. To keep this header self-contained and compilable for the per-TU gate
    // without dragging in (and risking divergent copies of) those uncommitted types, the
    // leading members are carried as a single named, honestly-flagged storage region. The
    // manager's own TU will GROW this header, replacing maLeadingStorage with the real typed
    // members once Event / SelectedRoutesMessage / BitArray land; the trailing scalars below
    // stay as named members so Release keeps accessing mbRoutesChanged by name.

    // Uncommitted dependency types this header only names in declared-only signatures
    // (never defined or dereferenced here). Promote to their real homes when those TUs land.
    class  BrnNetworkModule;

    namespace Detail
    {
        // PLACEHOLDER (honest): byte count of the leading member bulk is NOT recovered as
        // X360 fact -- it depends on uncommitted Event / SelectedRoutesMessage / BitArray
        // sizes. It is intentionally left as 0 here so this header makes NO false layout
        // claim; the manager's own TU replaces this region with the real typed members.
        // Release does not touch the leading bulk, so a 0-size placeholder is sufficient and
        // honest for everything this TU gates.
        const unsigned KU_SELECTED_ROUTES_LEADING_STORAGE_BYTES = 0u;
    }

    class SelectedRoutesManager
    {
    public:
        // NetworkPlayerID reuses the committed top-level typedef BrnNetwork::NetworkPlayerID
        // (== s32, 4 bytes; BrnNetworkSharedIO.h:39, shared by BrnNetworkPlayerStats and the
        // GameState flyby managers). The class is in namespace BrnNetwork so the unqualified
        // name resolves to it -- not forked here.

        // SpecificGameModeEventInterface::Event is uncommitted; declared-only signatures use
        // a forward-declared opaque type so this header needs no GameState include.
        struct GameModeEvent;            // -> BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event

        // === Declared-only API (bodies link from the manager's own TU) ===
        void Construct( BrnNetworkModule* lpNetworkModule, void* lpTimeManager );
        bool Prepare();
        void Destruct();
        void Update();
        void AddPlayer( NetworkPlayerID lPlayerID );
        void RemovePlayer( NetworkPlayerID lPlayerID );
        void Disconnected();
        void OnLeaveGame();
        bool HaveAllPlayersReceivedRoutes();
        void SendRouteData();
        void SetRouteData( s32 liNumRounds, const GameModeEvent* laEvents );
        void GetRouteData( GameModeEvent* laEvents ) const;
        bool HaveRoutesChanged() const;
        void SetRoutesChanged( bool lbChanged );

        // === Reconstructed in this TU ===
        // @ 0x82548B98 -- clear the trailing routes-changed flag and report success.
        bool Release();

    private:
        // --- leading member bulk (see header note above) -------------------------------
        // maEvents[10]            : SpecificGameModeEventInterface::Event[10]   (own TU)
        // maSelectedRoutesData[7] : SelectedRoutesData[7]                        (own TU)
        u8 maLeadingStorage[Detail::KU_SELECTED_ROUTES_LEADING_STORAGE_BYTES == 0u
                                ? 1u
                                : Detail::KU_SELECTED_ROUTES_LEADING_STORAGE_BYTES];

        // --- trailing members (named; X360-ordered) ------------------------------------
        BrnNetworkModule* mpNetworkModule;   // DWARF :139
        void*             mpTimeManager;     // DWARF :140  (CgsNetwork::TimeManager*, un-homed)
        s32               miNumRounds;       // DWARF :142
        bool              mbRoutesChanged;   // DWARF :144  <- X360 +1964 trailing byte

    public:
        // Inner per-player record. Layout (member order) is DWARF-recovered; element types of
        // mMessageSend/mMessageRecv (SelectedRoutesMessage) and the BitArrays are uncommitted,
        // so this nested type is declared-only here and fully reconstructed in the manager's
        // own TU alongside its dependency types.
        struct SelectedRoutesData;          // DWARF :125

    private:
        // Private helpers (bodies link from the manager's own TU).
        SelectedRoutesData* GetSelectedRoutesDataEntry( NetworkPlayerID lPlayerID );
        void ClearReceivedRounds();
    };

    // ---- Release @ 0x82548B98 -------------------------------------------------------------
    // X360: mbRoutesChanged = false; return true;  (single-byte store of 0 at the trailing
    // flag, then r3 = 1). Inlined here as the committed home for this leaf.
    inline bool SelectedRoutesManager::Release()
    {
        mbRoutesChanged = false;
        return true;
    }
}
