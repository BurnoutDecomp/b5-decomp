#ifndef CGS_SERVER_INTERFACE_DIRTY_SOCK_ERROR_HELPERS_H
#define CGS_SERVER_INTERFACE_DIRTY_SOCK_ERROR_HELPERS_H

#include "types.hpp"

#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"

// ===========================================================================
// CgsNetwork::DSErrorToServerInterfaceError / DSErrorToServerInterfaceErrorTable
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/
//         CgsServerInterfaceDirtySockErrorHelpers.h
//
// The two POD helper types used to map a raw DirtySock error code (a fourcc) to a
// game-side EServerInterfaceError. Each component owns a per-action table; ConvertError
// walks the entries of the table looked up for the current action.
//
// Layout recovered from the Feb-2007 partial source and confirmed against the X360
// ARTIST asm: the per-action lookup table is read as 8-byte entries
// { const DSErrorToServerInterfaceError* mpMappingTable; s32 miNumMappings } -- the
// EndAction / GetGameNewsCallback callees index it as
//   ConvertError(this, code, KA_DS_ERROR_TABLE_LOOKUP[action].mpMappingTable,
//                            KA_DS_ERROR_TABLE_LOOKUP[action].miNumMappings)
// (off_820E9380 holds the pointer word at +0, dword_820E9384 the count word at +4,
//  with the index scaled by 8). A DSErrorToServerInterfaceError is likewise 8 bytes
// { s32 miDSCode; EServerInterfaceError meError }.
// ===========================================================================

namespace CgsNetwork
{
    struct DSErrorToServerInterfaceError
    {
        s32                   miDSCode;
        EServerInterfaceError meError;
    };

    struct DSErrorToServerInterfaceErrorTable
    {
        const DSErrorToServerInterfaceError* mpMappingTable;
        s32                                  miNumMappings;
    };
}

#endif // CGS_SERVER_INTERFACE_DIRTY_SOCK_ERROR_HELPERS_H
