#ifndef BRN_NETWORK_STATS_DEBUG_COMPONENT_H
#define BRN_NETWORK_STATS_DEBUG_COMPONENT_H

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent (real base)

// BrnNetwork::StatsDebugComponent - the in-game debug menu component for the network player-stats
// subsystem. Derives from the real CgsDev::DebugComponent. It owns three editable ELO values
// (Race / Burning Home Run / Road Rage) plumbed to the server-interface custom-commands component
// (Get/Set ELO menu actions), and on demand registers a per-player stats group (AddStats /
// RemoveStats), one variable per NetworkPlayerStats::EStatsValue.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct @ 0x825851E8   Prepare    @ 0x82585230   Release    @ 0x82593A28   Destruct @ 0x825852C0
//   GetName   @ 0x82585300   OnActivate @ 0x82593990   AddStats   @ 0x82585310   RemoveStats @ 0x82585398
//   GetELOFromServer @ 0x82591468   SetELOOnServer @ 0x82591560
//   _GetEloCallbackFunction @ 0x82585408   _SetEloCallbackFunction @ 0x82585548
//
// Member layout pinned from the X360 Construct/Prepare stores (the base DebugComponent sub-object
// occupies +0x00..+0x0B; word index N == a1[N]):
//   miRaceElo            +0x0C  (word 3, stw 0)
//   miBurningHomeRunElo  +0x10  (word 4, stw 0)
//   miRoadRageElo        +0x14  (word 5, stw 0)
//   mpServerInterface    +0x18  (word 6, stw 0 / lpServerInterface)
//   mpStatsManager       +0x1C  (word 7, stw lpStatsManager)
// (member names + types from the DecFIGS DWARF BrnNetworkStatsDebugComponent.h:122..127.)
//
// The three ELO fields are debug-menu variables; the menu API is s32*-typed, so the named fields
// are reinterpret_cast<s32*> at the call sites (same idiom as BrnNetworkServerInterfaceDebugComponent).

namespace CgsNetwork
{
    class ServerInterface;   // mpServerInterface back-pointer (pointer-only here)
}

namespace BrnNetwork
{
    class NetworkPlayerStatsManager;   // mpStatsManager back-pointer (pointer-only here)
    class NetworkPlayerStats;          // AddStats / RemoveStats parameter (Managers/BrnNetworkPlayerStats.h)

    class StatsDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct();                                                                   // @ 0x825851E8
        bool Prepare(CgsNetwork::ServerInterface* lpServerInterface,                         // @ 0x82585230
                     NetworkPlayerStatsManager* lpStatsManager);
        bool Release();                                                                      // @ 0x82593A28
        void Destruct();                                                                     // @ 0x825852C0

        void AddStats(NetworkPlayerStats* lpStats);      // @ 0x82585310 -- register one menu group of stats
        void RemoveStats(NetworkPlayerStats* lpStats);   // @ 0x82585398 -- unregister that group

    protected:
        const char* GetName() const override;   // @ 0x82585300
        const char* GetPath() const override;   // @ 0x82585300-adjacent (DWARF :200)
        void        OnActivate() override;       // @ 0x82593990

    private:
        // Static menu callbacks (the void* user-data IS this component).
        // GetELOFromServer / SetELOOnServer are RegisterFunction actions (void(*)(void*));
        // _GetEloCallbackFunction / _SetEloCallbackFunction are the custom-command completion
        // callbacks (CustomCommandCallback == void(*)(void* data, void* result, bool success)).
        static void GetELOFromServer(void* lpData);                                          // @ 0x82591468
        static void SetELOOnServer(void* lpData);                                            // @ 0x82591560
        static void _GetEloCallbackFunction(void* lpData, void* lpResult, bool lbSuccess);   // @ 0x82585408
        static void _SetEloCallbackFunction(void* lpData, void* lpResult, bool lbSuccess);   // @ 0x82585548

        // ---- member layout (see header comment) ----
        s32                        miRaceElo;           // +0x0C
        s32                        miBurningHomeRunElo; // +0x10
        s32                        miRoadRageElo;       // +0x14
        CgsNetwork::ServerInterface* mpServerInterface; // +0x18
        NetworkPlayerStatsManager* mpStatsManager;      // +0x1C
    };
}

#endif // BRN_NETWORK_STATS_DEBUG_COMPONENT_H
