// ============================================================================
// CgsServerInterfaceUsersetParams.h
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceUsersetParams.{h,cpp}   (DirtySock component area)
//
// CgsNetwork::ServerInterfaceUsersetParamsBase -- the "userset" (lobby session)
// parameters payload that the DirtySock server-interface layer serialises to/from
// the lobby. Direct analog of ServerInterfaceGameParamsBase / PlayerParamsBase
// (same file/dir family, same string-setter + tagfield-serialise shape). Derives
// from ServerInterfaceStructureInterface (vptr-only polymorphic base at +0).
//
// LAYOUT -- from dwarfdump (CgsServerInterfaceUsersetParams.h:139..149) reconciled
// with the X360 ARTIST asm; where they disagree the asm is AUTHORITATIVE:
//   SetName        @ 0x82541598  strncpy(this+4 , src, 36)  -> macName[36]        (+4)
//   SetPassword    @ 0x8286F178  strncpy(this+40, src, 20)  -> macPassword[20]    (+40)
//   SetDescription @ 0x8286F278  strncpy(this+60, src, 68)  -> macDescription[68] (+60)
//   SerialiseFromUserset @ 0x82889BF8 strncpy(this+0x80, src, 16) -> macHostName[16] (+128)
//
// Offset map (this == ServerInterfaceStructureInterface vptr @ +0 on X360):
//   +4    macName[36]        (SetName limit 36 / 0x24)
//   +40   macPassword[20]    (SetPassword limit 20 / 0x14)   (+4 + 36 = +40)
//   +60   macDescription[68] (SetDescription limit 68 / 0x44)(+40 + 20 = +60)
//   +128  macHostName[16]    (SerialiseFromUserset strncpy count 0x10; the DWARF
//                             char[20] is drift -- the X360 build copies 16 and
//                             miUsersetId stores at 0x90 = +128+16, so the buffer
//                             is 16 bytes here)               (+60 + 68 = +128)
//   +144  miUsersetId        (0x90; Prepare seeds -1, from record[0])       stw -1,0x90
//   +148  miType             (0x94; from record[4], "TYPE")                 stw 0,0x94
//   +152  miNumPlayers       (0x98; from record[0x24])                      stw 0,0x98
//   +156  miMaxNumPlayers    (0x9C; Prepare seeds 8, from record[0x20], "SIZE") stw 8,0x9C
//   +160  muUsersetFlags     (0xA0; ConvertUsersetFlags(record[8]), "SYSFLAGS")  stw 0,0xA0
//   +164  muCustomFlags      (0xA4; from record[0xC], "CUSTFLAGS")          stw 0,0xA4
//
// The committed header previously placed macHostName as char[20] with the scalars
// at +148..+168; the asm store addresses (miUsersetId @ 0x90, ... muCustomFlags @
// 0xA4) prove the host-name occupies only 16 bytes and the scalars begin at 0x90.
//
// VTABLE order after the six StructureInterface entries (DWARF-attested; base slots
// +0 dtor, +4 GetPattern, +8 GetPatternLength, +0xC GetDataSize, +0x10 GetData,
// +0x14 GetData const):
//   [+0x18] Prepare, [+0x1C] SerialiseToString, [+0x20] SerialiseFromUserset.
//
// NOTE on absolute offsets: these are the X360 (32-bit pointer) layout. The lone
// pointer is the inherited vptr; on a 64-bit host it widens to 8 bytes, so the byte
// offsets above are NOT reproduced and are intentionally NOT static_asserted.
// Members are pinned BY NAME/ORDER; the char-buffer sizes are the load-bearing facts.
// ============================================================================
#ifndef CGS_SERVER_INTERFACE_USERSET_PARAMS_H
#define CGS_SERVER_INTERFACE_USERSET_PARAMS_H

#include "types.hpp"
#include "../CgsServerInterfaceStructureInterface.h"

namespace CgsNetwork
{
    // Buffer capacities from the strncpy limits (asm) reconciled with the DWARF
    // char[] member sizes. macHostName is 16 (asm strncpy count 0x10 / next store
    // at 0x90), not the DWARF-reported 20.
    const s32 KI_USERSETPARAMS_NAME_LENGTH        = 36;   // 0x24
    const s32 KI_USERSETPARAMS_PASSWORD_LENGTH    = 20;   // 0x14
    const s32 KI_USERSETPARAMS_DESCRIPTION_LENGTH = 68;   // 0x44
    const s32 KI_USERSETPARAMS_HOSTNAME_LENGTH    = 16;   // 0x10 (SerialiseFromUserset)

    struct ServerInterfaceUsersetParamsBase : public ServerInterfaceStructureInterface
    {
    public:
        ServerInterfaceUsersetParamsBase();
        virtual ~ServerInterfaceUsersetParamsBase();

        // CgsServerInterfaceUsersetParams.cpp:51 -- reset to defaults, returns true. [+0x18]
        virtual bool Prepare();

        // CgsServerInterfaceUsersetParams.cpp:80 -- emit into a tagfield record. [+0x1C]
        virtual void SerialiseToString(char* lpcRecord, s32 liRecLen) const;

        // CgsServerInterfaceUsersetParams.cpp:126 -- populate from a raw DirtySDK
        //   lobby userset record (LobbyApiUserSetT*); fields read by byte offset. [+0x20]
        virtual void SerialiseFromUserset(const void* lpUserset);

        // CgsServerInterfaceUsersetParams.h:192
        void SetName(const char* lpcName);
        // CgsServerInterfaceUsersetParams.h:199
        void SetPassword(const char* lpcPassword);
        // CgsServerInterfaceUsersetParams.h:206
        void SetDescription(const char* lpcDescription);

    protected:
        char macName[KI_USERSETPARAMS_NAME_LENGTH];               // +4   (X360)
        char macPassword[KI_USERSETPARAMS_PASSWORD_LENGTH];       // +40  (X360)
        char macDescription[KI_USERSETPARAMS_DESCRIPTION_LENGTH]; // +60  (X360)
        char macHostName[KI_USERSETPARAMS_HOSTNAME_LENGTH];       // +128 (X360)

        s32 miUsersetId;      // +144 (0x90)
        s32 miType;           // +148 (0x94)
        s32 miNumPlayers;     // +152 (0x98)
        s32 miMaxNumPlayers;  // +156 (0x9C)
        u32 muUsersetFlags;   // +160 (0xA0)
        u32 muCustomFlags;    // +164 (0xA4)
    };
}

#endif // CGS_SERVER_INTERFACE_USERSET_PARAMS_H
