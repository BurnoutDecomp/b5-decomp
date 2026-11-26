#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySockErrorHelpers.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"   // mNetStreamLogChannelOutput
#include "GameShared/GameClasses/Development/CgsStrStream.h"

#define CGS_ARTIST_FILE \
    "..\\..\\..\\GameShared\\GameClasses\\Network\\ServerInterface\\DirtySock\\Components\\CgsServerInterfaceComponent.cpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceComponent::ClearLastError       @ 0x828766F0
//   CgsNetwork::ServerInterfaceComponent::Construct            @ 0x82876418
//   CgsNetwork::ServerInterfaceComponent::ConvertError         @ 0x82876728
//   CgsNetwork::ServerInterfaceComponent::GetAndClearLastError @ 0x82876708
//
// These are members of the canonical ServerInterfaceComponent declared in the
// sibling CgsServerInterfaceComponent.h (vptr@0, mpcCurrentAction@4, meStatus@8,
// miLastError@0xC -- see that header for the full layout note). Construct points
// mpcCurrentAction at the empty string and resets
// status/error; Clear/GetAndClear reset them the same way (mpcCurrentAction=0,
// meStatus=2 "no error", miLastError=0).
//
// ConvertError maps a raw DirtySock error fourcc to an EServerInterfaceError using a
// caller-supplied table of 8-byte { s32 miDSCode; EServerInterfaceError meError; }
// entries (DSErrorToServerInterfaceError, home: CgsServerInterfaceDirtySockErrorHelpers.h).
// A zero code maps to E_SERVER_INTERFACE_ERROR_NONE. If the code is absent from the
// supplied table (or no table is given) it falls back to the one-entry default table
// keyed on the 'nfnd' ("not found") fourcc (mapped to E_SERVER_INTERFACE_GAMES_ERROR_NO_GAMES_FOUND,
// the rodata word is 27), returning E_SERVER_INTERFACE_ERROR_UNHANDLED when even that misses.
// (The disassembly resolves the default-table address as a
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
        { KI_NOT_FOUND_CODE, E_SERVER_INTERFACE_GAMES_ERROR_NO_GAMES_FOUND }
    };
    const s32 KI_NUM_DEFAULT_ERROR_MAPPINGS = 1;

    // Out of line (DWARF: the CgsNetworkUnity CU defines it here, before ClearLastError). ARTIST's
    // callers in other units call the folded `lwz r3,0xC(r3); blr` at 0x82836DC8:
    // ScoreboardManager::ProcessBeforeSimulation 0x825688F8, BuddyManagerBase::
    // _GetFriendsListFromServerComplete 0x82549C58, NetworkGamerCardManagerX360::PlayerInfoIdle 0x8254CED0.
    s32 ServerInterfaceComponent::GetLastError() const
    {
        return miLastError;
    }

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
        mpcCurrentAction = "";
        miLastError = 0;
    }

    // Both action hooks report progress on the network log stream (ARTIST writes through
    // ServerInterfaceDirtySock::mNetStreamLogChannelOutput, 0x82F335C8): "StartAction: <name>"
    // (0x82876438) and "Finish Action: <name> successfully" / "... with error <n>" (0x828765D0).
    void ServerInterfaceComponent::StartActionCore(const char* lpcAction)
    {
        ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "StartAction: " << lpcAction << "\n";
        CGS_ASSERT_STREAM_AT(meStatus != 0,
            "Trying to start " << lpcAction << " when still doing " << mpcCurrentAction << "\n",
            CGS_ARTIST_FILE, 84 /* ARTIST 0x828765A8 */);

        mpcCurrentAction = lpcAction;
        meStatus         = 0;
        miLastError      = 0;
    }

    void ServerInterfaceComponent::EndActionCore(int liError)
    {
        if (liError == 0)
        {
            ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Finish Action: " << mpcCurrentAction << " successfully\n";
            mpcCurrentAction = 0;
            meStatus         = 2;
        }
        else
        {
            ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Finish Action: " << mpcCurrentAction << " with error " << liError << "\n";
            meStatus    = 1;
        }
        miLastError = liError;
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

#undef CGS_ARTIST_FILE
