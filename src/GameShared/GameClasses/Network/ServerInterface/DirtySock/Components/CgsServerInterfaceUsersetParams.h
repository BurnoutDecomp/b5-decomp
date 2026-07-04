// ============================================================================
// CgsServerInterfaceUsersetParams.h
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceUsersetParams.{h,cpp}   (DirtySock component area)
//
// CgsNetwork::ServerInterfaceUsersetParamsBase -- the "userset" (lobby session)
// parameters payload that the DirtySock server-interface layer serialises to/from
// the lobby. Direct analog of ServerInterfaceGameParamsBase (same file/dir family,
// same string-setter shape). Derives from ServerInterfaceStructureInterface (a
// vptr-only polymorphic base at +0).
//
// LAYOUT -- from dwarfdump (CgsServerInterfaceUsersetParams.h) reconciled with the
// X360 ARTIST asm; the two AGREE on buffer sizes and offsets:
//   SetName        @ 0x82541598  strncpy(this+4 , src, 36)  -> macName[36]        (+4)
//   SetPassword    @ 0x8286F178  strncpy(this+40, src, 20)  -> macPassword[20]    (+40)
//   SetDescription @ 0x8286F278  strncpy(this+60, src, 68)  -> macDescription[68] (+60)
//
// Offset map (this == ServerInterfaceStructureInterface vptr @ +0 on X360):
//   +4    macName[36]        (SetName limit 36 / 0x24)
//   +40   macPassword[20]    (SetPassword limit 20 / 0x14)   (+4 + 36 = +40)
//   +60   macDescription[68] (SetDescription limit 68 / 0x44)(+40 + 20 = +60)
//   +128  macHostName[20]    (DWARF member; +60 + 68 = +128)
//   +148  miUsersetId
//   +152  miType
//   +156  miNumPlayers
//   +160  miMaxNumPlayers
//   +164  muUsersetFlags
//   +168  muCustomFlags
//
// Member names/sizes/order are pinned BY NAME from DWARF
// (CgsServerInterfaceUsersetParams.h:139..149). The char-buffer sizes and member
// ORDER are the load-bearing parity facts.
//
// NOTE on absolute offsets: these are the X360 (32-bit pointer) layout. The lone
// pointer is the inherited vptr; on a 64-bit host it widens to 8 bytes, so the byte
// offsets above are NOT reproduced and are intentionally NOT static_asserted.
// ============================================================================
#ifndef CGS_SERVER_INTERFACE_USERSET_PARAMS_H
#define CGS_SERVER_INTERFACE_USERSET_PARAMS_H

#include "types.hpp"
#include "../CgsServerInterfaceStructureInterface.h"

namespace CgsNetwork
{
    // Buffer capacities from the strncpy limits in SetName/SetPassword/SetDescription
    // (asm) and the DWARF char[] member sizes (they agree).
    const s32 KI_USERSETPARAMS_NAME_LENGTH        = 36;   // 0x24
    const s32 KI_USERSETPARAMS_PASSWORD_LENGTH    = 20;   // 0x14
    const s32 KI_USERSETPARAMS_DESCRIPTION_LENGTH = 68;   // 0x44
    const s32 KI_USERSETPARAMS_HOSTNAME_LENGTH    = 20;

    struct ServerInterfaceUsersetParamsBase : public ServerInterfaceStructureInterface
    {
    public:
        ServerInterfaceUsersetParamsBase();

        // CgsServerInterfaceUsersetParams.h:192
        void SetName(const char* lpcName);
        // CgsServerInterfaceUsersetParams.h:199
        void SetPassword(const char* lpcPassword);
        // CgsServerInterfaceUsersetParams.h:206
        void SetDescription(const char* lpcDescription);

        virtual ~ServerInterfaceUsersetParamsBase();

    protected:
        char macName[KI_USERSETPARAMS_NAME_LENGTH];               // +4   (X360)
        char macPassword[KI_USERSETPARAMS_PASSWORD_LENGTH];       // +40  (X360)
        char macDescription[KI_USERSETPARAMS_DESCRIPTION_LENGTH]; // +60  (X360)
        char macHostName[KI_USERSETPARAMS_HOSTNAME_LENGTH];       // +128 (X360)

        s32 miUsersetId;      // +148
        s32 miType;           // +152
        s32 miNumPlayers;     // +156
        s32 miMaxNumPlayers;  // +160
        u32 muUsersetFlags;   // +164
        u32 muCustomFlags;    // +168
    };
}

#endif // CGS_SERVER_INTERFACE_USERSET_PARAMS_H
