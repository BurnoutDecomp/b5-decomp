#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"      // CgsDev::DebugComponent (real base)
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h"              // BrnNetwork::CommonRelationship / LiveRevengeRelationship (Add/RemovePlayer params)

// BrnNetwork::LiveRevengeDebugComponent -- the in-game debug-menu component for the Live Revenge
// subsystem. Derives from the real CgsDev::DebugComponent. It holds a back-pointer to its owning
// LiveRevengeManager and, on demand, exposes one debug-menu group per live-revenge relationship:
// the relationship's two summary variables (current revenge status / total events), its 16 overall
// per-stat counters (Add* / RegisterRelationship), and three per-relationship action callbacks
// (reset timestamp / set timestamp old / clear relationship). OnActivate adds the three top-level
// "register all / unregister all / force upload" actions; RegisterAll / UnregisterAll walk the
// manager's revenge profile table and add/remove every relationship group; UploadToServer forces a
// server sync of the rival list.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct              @ 0x82584D48   (store manager, base Construct, Register)
//   RegisterRelationship   @ 0x82584D80   (16 overall-stat menu variables, grouped by rival name)
//   UnregisterRelationship @ 0x82584F18   (12 of those overall-stat variables; see note below)
//   AddPlayer              @ 0x82584FD8   (per-relationship group: 2 vars + 3 funcs + RegisterRelationship)
//   RemovePlayer           @ 0x825850C8   (tear down a relationship group)
//   GetName                @ 0x82585148 -> "Live Revenge"
//   UploadToServer         @ 0x82585158   (force LiveRevengeManager::SendLiveRevengeRivalsToServer)
//   UnregisterAll          @ 0x82591318   (walk the profile table, RemovePlayer-equivalent each)
//   RegisterAll            @ 0x82593880   (UnregisterAll, then AddPlayer each table entry)
//   OnActivate             @ 0x82594708   (register the three top-level menu actions)
//
// LAYOUT (X360-AUTHORITATIVE; the base CgsDev::DebugComponent sub-object occupies +0x00..+0x0B,
// word index N == a1[N]). Construct's `stw r4, 0xC(this)` pins the single own member:
//   mpLiveRevengeManager  +0x0C  LiveRevengeManager*  (the owning manager back-pointer; read as
//                                 a1[3] / this+12 by RegisterAll / UnregisterAll / UploadToServer).
// (member name + type from the DecFIGS DWARF BrnNetworkLiveRevengeDebugComponent.h:119.)
//
// X360 LEDGER: this TU's recovered function set is exactly the 10 bodies listed above. The DecFIGS
// DWARF also declares Prepare / Release / Destruct / GetPath for this class, but none has an X360
// body in this TU (they fold onto the shared base / empty thunks), so they are declared-only here to
// preserve the declaration surface; the base CgsDev::DebugComponent defaults apply at link time.

namespace BrnNetwork
{
    struct LiveRevengeManager;   // owning back-pointer member only (pointer-only use here; the
                                 // manager's own header embeds this component BY VALUE, so the
                                 // dependency is one-directional: manager.h includes this header).

    class LiveRevengeDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(LiveRevengeManager* lpLiveRevengeManager);   // @ 0x82584D48
        bool Prepare();                                             // declared-only (DWARF :63)
        bool Release();                                             // declared-only (DWARF :94)
        void Destruct();                                            // declared-only (DWARF :109)

        // Register / unregister one relationship's full debug-menu group (called by the manager when a
        // rival is added/removed, and by RegisterAll / UnregisterAll over the whole table).
        void AddPlayer(LiveRevengeRelationship* lpRelationship);     // @ 0x82584FD8
        void RemovePlayer(LiveRevengeRelationship* lpRelationship);  // @ 0x825850C8

    protected:
        const char* GetName() const override;   // @ 0x82585148 -> "Live Revenge"
        const char* GetPath() const override;   // declared-only (DWARF :231)
        void        OnActivate() override;       // @ 0x82594708

    private:
        // Register/unregister the 16 (register) / 12 (unregister) "overall stats" menu variables of a
        // relationship, grouped under the rival's name. lpStats points at the relationship's
        // mOverallStats block. NOTE the asymmetry is faithful to the X360: RegisterRelationship adds
        // all 16 (player+rival takedowns/streak/wins/scores-settled/marks/scalps/paybacks-dealt/
        // paybacks-scored), while UnregisterRelationship removes only the first 12 (it omits the four
        // paybacks variables) -- preserved exactly as the binary emits it.
        void RegisterRelationship(CommonRelationship* lpStats, const char* lpcRivalName);  // @ 0x82584D80
        void UnregisterRelationship(CommonRelationship* lpStats);                          // @ 0x82584F18

        // Top-level menu action callbacks (registered in OnActivate). The void* user-data IS this
        // component; each is a DebugUI::Function::DebugCallbackFunction (void(*)(void*)).
        static void RegisterAll(void* lpUserData);     // @ 0x82593880
        static void UnregisterAll(void* lpUserData);   // @ 0x82591318
        static void UploadToServer(void* lpUserData);  // @ 0x82585158

        // ---- member layout (see header comment) ----
        LiveRevengeManager* mpLiveRevengeManager;   // +0x0C
    };
}
