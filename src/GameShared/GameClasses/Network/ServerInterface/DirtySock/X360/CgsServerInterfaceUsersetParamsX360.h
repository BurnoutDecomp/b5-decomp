#ifndef CGS_SERVER_INTERFACE_USERSET_PARAMS_X360_H
#define CGS_SERVER_INTERFACE_USERSET_PARAMS_X360_H

#include "types.hpp"
#include "../Components/CgsServerInterfaceUsersetParams.h"   // ServerInterfaceUsersetParamsBase

// ===========================================================================
// CgsNetwork::ServerInterfaceUsersetParamsX360
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/
//         CgsServerInterfaceUsersetParamsX360.{h,cpp}
//
// The X360 (and PC-remaster) platform leaf of ServerInterfaceUsersetParamsBase --
// the "userset" (lobby session) parameters payload. Unlike the GameParams / player
// leaves it adds NO platform-specific members; all three overrides are pure
// forwarders that occupy the leaf's vtable slots and chain straight to the shared
// base. (Sibling shape: CgsServerInterfaceGameParamsX360, whose SerialiseFromGame /
// SerialiseToString are the same trivial vtable-slot forwarders.)
//
// VTABLE (inherited slot order from ServerInterfaceUsersetParamsBase):
//   [+0x18] Prepare, [+0x1C] SerialiseToString, [+0x20] SerialiseFromUserset.
//
// No extra members => no leaf ctor is emitted (none is attested in the X360 ledger;
// the layout is identical to the base).
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfaceUsersetParamsX360 : public ServerInterfaceUsersetParamsBase
    {
    public:
        // @ X360 0x8287C790 -- chain to the base Prepare; the asm normalises the
        // result to 0/1 (clrlwi/cntlzw), which `bool` already is.
        virtual bool Prepare();

        // @ X360 0x82889E88 -- pure tail-call forward to the base (b instruction).
        virtual void SerialiseToString(char* lpcRecord, s32 liRecLen) const;

        // @ X360 0x82889E90 -- pure tail-call forward to the base (b instruction).
        virtual void SerialiseFromUserset(const void* lpUserset);
    };
}

#endif // CGS_SERVER_INTERFACE_USERSET_PARAMS_X360_H
