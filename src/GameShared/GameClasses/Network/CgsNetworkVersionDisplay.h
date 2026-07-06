#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"   // CgsDev::DebugComponent base
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"                       // CgsNetwork::EServerType

// CgsNetwork::VersionDisplay - the debug-HUD overlay that prints the network build banner
// ("Server: <type>    Version:<ver>/<N>" plus an optional "Rebroadcast server available" line).
// A CgsDev::DebugComponent subclass; NetworkManager::Prepare drives Prepare() to seed the strings.
// Recovered from the X360 ARTIST build (GetName 0x8286FA60 / Prepare 0x8286F9F0 / RenderHUD
// 0x8286FA70) cross-checked against the DecFIGS DWARF (CgsNetworkVersionDisplay.h).
//
// LAYOUT: DebugComponent derives from Internal::DebugInternal (data-less, vptr only), then adds
// mbActive + mpDebugLinkedListNext, so the base occupies +0x00..+0x0C and this class's own members
// begin at +0x0C - matching the Prepare stores (stw 0xC/0x10/0x14, stb 0x18). The DWARF member list
// omits muField_04 (a known-incomplete hint) and the DWARF Prepare shape is 2-arg; the X360 asm is
// authoritative and carries a third value param stored at +0x10 (stw r28,0x10) that RenderHUD reads
// back (lwz r6,0x10; SnPrintf "/%d"), so muField_04 is materialised here.

namespace CgsNetwork
{
    // The 7 human-readable server-type names indexed by EServerType (X360 off_82F33478 /
    // KAPC_SERVER_TYPES). Definition lives in the .cpp; only element 0 ("Local Server") is
    // byte-attested in the X360 asm.
    extern const char* const KAPC_SERVER_TYPES[E_SERVER_TYPE_COUNT];

    struct VersionDisplay : public CgsDev::DebugComponent
    {
    public:
        void Construct();

        // X360 0x8286F9F0. Seed the banner from the network prepare params; range-checks the server
        // type. Returns true. (The asm carries three value params; the DWARF's 2-arg shape is an
        // incomplete hint -- muField_04 is attested by the +0x10 store + the RenderHUD "/%d" read.)
        bool Prepare(const char* lpcVersion, u32 luField_04, EServerType leServerType);

        bool Release();
        void Destruct();
        void SetGameServerGame(bool lbGameServerGame);

    protected:
        // X360 0x8286FA60. Debug-menu display name.
        virtual const char* GetName() const;
        virtual const char* GetPath() const;
        virtual void        OnActivate();
        // X360 0x8286FA70. Draw the network build banner along the bottom-right of the debug HUD.
        virtual void        RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay);
        virtual bool        IsSimple() const;

    private:
        const char* mpcVersion;         // +0x0C  server version string (null -> "Not Available")
        u32         muField_04;         // +0x10  associated count, drawn as "/%d" after the version
        EServerType meServerType;       // +0x14  index into KAPC_SERVER_TYPES
        bool        mbGameServerGame;   // +0x18  true -> also print the rebroadcast note
    };
}
