#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySockErrorHelpers.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceComponent::ClearLastError       @ 0x828766F0
//   CgsNetwork::ServerInterfaceComponent::Construct            @ 0x82876418
//   CgsNetwork::ServerInterfaceComponent::ConvertError         @ 0x82876728
//   CgsNetwork::ServerInterfaceComponent::GetAndClearLastError @ 0x82876708
//
// These are members of the canonical ServerInterfaceComponent declared in the
// sibling CgsServerInterfaceComponent.h (vptr@0, mpcCurrentAction@4, meStatus@8,
// miLastError@0xC -- see that header for the full layout note). Construct points
// mpcCurrentAction at the static error-string data (unk_820046A7) and resets
// status/error; Clear/GetAndClear reset them the same way (mpcCurrentAction=0,
// meStatus=2 "no error", miLastError=0).
//
// ConvertError maps a raw DirtySock error fourcc to an EServerInterfaceError using a
// caller-supplied table of 8-byte { s32 miDSCode; EServerInterfaceError meError; }
// entries (DSErrorToServerInterfaceError, home: CgsServerInterfaceDirtySockErrorHelpers.h).
// A zero code maps to E_SERVER_INTERFACE_ERROR_NONE. If the code is absent from the
// supplied table (or no table is given) it falls back to the one-entry default table
// keyed on the 'nfnd' ("not found") fourcc, returning E_SERVER_INTERFACE_ERROR_UNHANDLED
// when even that misses. (The X360 disassembly resolves the default-table address as a
// "nfnd" string literal purely because its bytes are ASCII-printable -- it is actually
// &kaDefaultDSServerInterfaceErrorMapping[0], per the DWARF-attested private static.)

namespace CgsNetwork
{
    // 'nfnd' packed as the X360 build reads it (big-endian byte order).
    const s32 KI_NOT_FOUND_CODE =
        (static_cast<s32>('n') << 24) | (static_cast<s32>('f') << 16) |
        (static_cast<s32>('n') << 8)  |  static_cast<s32>('d');

    // CgsServerInterfaceComponent.cpp:43 / :49 (DWARF)
    const DSErrorToServerInterfaceError kaDefaultDSServerInterfaceErrorMapping[1] =
    {
        { KI_NOT_FOUND_CODE, E_SERVER_INTERFACE_ERROR_UNHANDLED }
    };
    const s32 KI_NUM_DEFAULT_ERROR_MAPPINGS = 1;

    // Static error-string data, defined in another (not-yet-reconstructed) TU.
    extern const u8 gServerInterfaceErrorData[];

    void* ServerInterfaceComponent::ClearLastError()
    {
        miLastError = 0;
        meStatus = 2;
        mpcCurrentAction = 0;
        return this;
    }

    void ServerInterfaceComponent::Construct()
    {
        meStatus = 2;
        mpcCurrentAction = reinterpret_cast<const char*>(&gServerInterfaceErrorData);
        miLastError = 0;
    }

    int ServerInterfaceComponent::GetAndClearLastError()
    {
        int liError = miLastError;
        miLastError = 0;
        meStatus = 2;
        mpcCurrentAction = 0;
        return liError;
    }

    EServerInterfaceError ServerInterfaceComponent::ConvertError(int liError,
                                                                  const DSErrorToServerInterfaceError* lpTable,
                                                                  int liCount) const
    {
        int liResult = E_SERVER_INTERFACE_ERROR_UNHANDLED;
        if (!liError)
            return E_SERVER_INTERFACE_ERROR_NONE;

        for (;;)
        {
            const DSErrorToServerInterfaceError* lpEffectiveTable = lpTable;
            int liEntries = liCount;
            if (!lpTable || !liCount)
            {
                liEntries = KI_NUM_DEFAULT_ERROR_MAPPINGS;
                lpEffectiveTable = kaDefaultDSServerInterfaceErrorMapping;
            }

            int liIdx = 0;
            if (liEntries > 0)
            {
                while (lpEffectiveTable[liIdx].miDSCode != liError)
                {
                    if (++liIdx >= liEntries)
                        break;
                }
                if (liIdx < liEntries)
                    liResult = lpEffectiveTable[liIdx].meError;
            }

            if (liIdx != liEntries || lpEffectiveTable == kaDefaultDSServerInterfaceErrorMapping)
                return static_cast<EServerInterfaceError>(liResult);

            // Not found in the supplied table: retry against the default table.
            lpTable = kaDefaultDSServerInterfaceErrorMapping;
            liCount = KI_NUM_DEFAULT_ERROR_MAPPINGS;
            liResult = E_SERVER_INTERFACE_ERROR_UNHANDLED;
        }
    }
}
