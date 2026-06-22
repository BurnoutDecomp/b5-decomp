#ifndef CGS_SERVER_INTERFACE_PLAYER_PARAMS_H
#define CGS_SERVER_INTERFACE_PLAYER_PARAMS_H

#include "types.hpp"
#include "../CgsServerInterfaceStructureInterface.h"

// ===========================================================================
// CgsNetwork::ServerInterfacePlayerParamsBase
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfacePlayerParams.{h,cpp}
//
// Per-player parameter block sent to the lobby (name, identity, presence, the
// internal/external addresses + firewall setting). Derives from
// ServerInterfaceStructureInterface (vptr-only polymorphic base).
//
// LAYOUT (dwarfdump + X360 asm @ 0x82879F28 SerialiseFromPlayer):
//   +0x00  vptr
//   +0x04  macMachineAddress[64] (char)
//   +0x44  mpcName               (const char*)
//   +0x48  miIdent               (s32)
//   +0x4C  miPresence            (s32)
//   +0x50  muExternalAddress     (u32)
//   +0x54  muInternalAddress     (u32)
//   +0x58  muFlags               (u32)
//   +0x5C  meFirewallSetting     (EFirewallSettings)
//
// VTABLE order after the StructureInterface six entries:
//   [+0x18] Prepare, [+0x1C] SerialiseFromPlayer.
// ===========================================================================

namespace CgsNetwork
{
    // CgsServerInterfacePlayerParams.h:54
    enum EFirewallSettings
    {
        E_FIREWALL_STRICT   = 0,
        E_FIREWALL_MODERATE = 1,
        E_FIREWALL_OPEN     = 2,
        E_FIREWALL_COUNT    = 3,
    };

    struct ServerInterfacePlayerParamsBase : public ServerInterfaceStructureInterface
    {
    public:
        ServerInterfacePlayerParamsBase();
        virtual ~ServerInterfacePlayerParamsBase();

        // CgsServerInterfacePlayerParams.cpp:53
        virtual bool Prepare();

        // CgsServerInterfacePlayerParams.cpp:77
        //   lpPlayer is a DirtySDK LobbyApiPlayerT* (raw lobby player record); its
        //   internals are read by documented byte offset in the .cpp.
        virtual void SerialiseFromPlayer(const void* lpPlayer);

        // CgsServerInterfacePlayerParams.cpp:106
        void SerialiseToString(char* lpcRecord, s32 liRecLen) const;

        // CgsServerInterfacePlayerParams.h:116
        const char* GetName() const { return mpcName; }
        // CgsServerInterfacePlayerParams.h:122
        s32 GetID() const { return miIdent; }
        // CgsServerInterfacePlayerParams.h:128
        EFirewallSettings GetFirewallSettings() const { return meFirewallSetting; }
        // CgsServerInterfacePlayerParams.h:134
        //   &macMachineAddress[0] reinterpreted as the LobbyApi DirtyAddrT.
        void* GetDirtyAddrT() { return macMachineAddress; }

    protected:
        // CgsServerInterfacePlayerParams.h:103
        char macMachineAddress[64];
        // CgsServerInterfacePlayerParams.h:104
        const char* mpcName;
        // CgsServerInterfacePlayerParams.h:105
        s32 miIdent;
        // CgsServerInterfacePlayerParams.h:106
        s32 miPresence;
        // CgsServerInterfacePlayerParams.h:107
        u32 muExternalAddress;
        // CgsServerInterfacePlayerParams.h:108
        u32 muInternalAddress;
        // CgsServerInterfacePlayerParams.h:109
        u32 muFlags;
        // CgsServerInterfacePlayerParams.h:110
        EFirewallSettings meFirewallSetting;
    };
}

#endif // CGS_SERVER_INTERFACE_PLAYER_PARAMS_H
