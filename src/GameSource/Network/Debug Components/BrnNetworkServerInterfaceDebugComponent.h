#ifndef BRN_NETWORK_SERVER_INTERFACE_DEBUG_COMPONENT_H
#define BRN_NETWORK_SERVER_INTERFACE_DEBUG_COMPONENT_H

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent (real base)
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"                     // CgsNetwork::EServerType

namespace CgsNetwork { class ServerInterfaceConnection; }

// BrnNetwork::ServerInterfaceDebugComponent - the in-game debug menu component for the network
// server interface. Derives from the real CgsDev::DebugComponent and is embedded BY VALUE in
// BrnServerInterfaceBase (mServerInterfaceDebugComponent), so this header must give the complete
// layout. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct   @ 0x82585700   Disconnect (static cb) @ 0x825857A8
//   GetName     @ 0x82585798   OnActivate @ 0x8258AD10   Update @ 0x8258AC70
//
// Member layout pinned from the X360 Construct stores (this+0xC..0x1E, the base DebugComponent
// sub-object occupies +0x00..+0x0B):
//   miServerType                 +0x0C  (stw 7 == E_SERVER_TYPE_COUNT sentinel)
//   miConnectionType             +0x10  (stw 3 == E_GAME_SERVER_CONNECTION_TYPE_FALLBACK_GAME_ONLY)
//   mpNetworkManager             +0x14  (stw 0)
//   mpServerInterfaceBase        +0x18  (stw lpServerInterfaceBase)
//   mbDisconnectNextUpdate       +0x1C  (stb 0)
//   mbApplyServerTypeNextUpdate  +0x1D  (stb 0)
//   mbDisplayConnectionStatus    +0x1E  (stb 0)
//
// The two enum fields are debug-menu variables; the menu API is s32*-typed, so the named fields
// are reinterpret_cast<s32*> at the call sites (same idiom as BrnPaybackDebugComponent).

namespace BrnNetwork
{
    class BrnServerInterfaceBase;   // back-pointer member only (would cycle: its header includes this one)
    class BrnNetworkManager;        // back-pointer member only

    // Recovered accessor for the connection component the debug component disconnects (X360 inlined
    // it as *(serverInterfaceBase+4) -> the stored CgsNetwork::ServerInterfaceConnection*). Declared
    // free here rather than as a BrnServerInterfaceBase method because that header is not yet
    // gate-compilable (its include chain references unreconstructed headers); pointer-only use of an
    // incomplete BrnServerInterfaceBase is the documented cascade-avoidance forward-decl exception.
    // Body lands with the BrnServerInterfaceBase reconstruction.
    CgsNetwork::ServerInterfaceConnection* GetConnectionComponent(BrnServerInterfaceBase* lpServerInterfaceBase);

    class ServerInterfaceDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(BrnServerInterfaceBase* lpServerInterfaceBase);   // @ 0x82585700
        void Update() override;                                          // @ 0x8258AC70

        // @0x82597690 -- gate the connection-status table on mbDisplayConnectionStatus, then
        // tail-call RenderConnectionStatus. Overrides CgsDev::DebugComponent::RenderHUD.
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay) override;

    protected:
        const char* GetName() const override;   // @ 0x82585798
        void        OnActivate() override;       // @ 0x8258AD10

        // @0x82594AC0 -- the connApi connection-status HUD table (called by RenderHUD).
        void RenderConnectionStatus(CgsDev::Debug2DImmediateRender* lpDisplay);

    private:
        // Static menu callbacks (registered with the debug menu; the void* user-data IS this
        // component). DebugCallbackFunction == void(*)(void*); VariableCallbackFunction ==
        // void(*)(void*, void*).
        static void Disconnect(void* lpData);                                   // @ 0x825857A8
        static void ServerTypeSelectCallback(void* lpData, void* lpUserData);   // own TU (declared-only)

        // ---- member layout (see header comment) ----
        s32                miServerType;                // +0x0C  CgsNetwork::EServerType
        s32                miConnectionType;            // +0x10  CgsNetwork::ServerInterfaceGames::EGameServerConnectionType
        BrnNetworkManager* mpNetworkManager;            // +0x14
        BrnServerInterfaceBase* mpServerInterfaceBase;  // +0x18
        bool               mbDisconnectNextUpdate;      // +0x1C
        bool               mbApplyServerTypeNextUpdate; // +0x1D
        bool               mbDisplayConnectionStatus;   // +0x1E
    };
}

#endif // BRN_NETWORK_SERVER_INTERFACE_DEBUG_COMPONENT_H
