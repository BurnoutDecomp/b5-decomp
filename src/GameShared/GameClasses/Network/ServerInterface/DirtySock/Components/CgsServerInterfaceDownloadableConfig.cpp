#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceDownloadableConfig.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySockErrorHelpers.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "lobbyapi.h"                                 // LobbyApiRequestCB / LobbyApiMsgT / LobbyApiCallbackT
#include "lobbytagfield.h"                            // TagFieldFind / Get* / SetNumber

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceDownloadableConfig::Construct                    @ 0x8287ACC0
//   CgsNetwork::ServerInterfaceDownloadableConfig::Destruct                     @ 0x8287ACF0
//   CgsNetwork::ServerInterfaceDownloadableConfig::Prepare                      @ 0x8287AD08
//   CgsNetwork::ServerInterfaceDownloadableConfig::Release                      @ 0x8287AD30
//   CgsNetwork::ServerInterfaceDownloadableConfig::Suspend                      @ 0x8287AD50
//   CgsNetwork::ServerInterfaceDownloadableConfig::OnEvent                      @ 0x8287AD68
//   CgsNetwork::ServerInterfaceDownloadableConfig::StartAction                  @ 0x8287AD88
//   CgsNetwork::ServerInterfaceDownloadableConfig::EndAction                    @ 0x8287AE18
//   CgsNetwork::ServerInterfaceDownloadableConfig::ParseConfigFile              @ 0x8287AE78
//   CgsNetwork::ServerInterfaceDownloadableConfig::GetGameNewsCallback          @ 0x82888CB0
//   CgsNetwork::ServerInterfaceDownloadableConfig::GetConfigurationDataFromNews @ 0x828916C8
//
// The component drives a single action -- "download configuration from game news" --
// over the DirtySock lobby. GetConfigurationDataFromNews stashes the caller's parse
// table + requested news index, writes the index into the shared message buffer as the
// "NAME" tagfield, then StartAction fires a LobbyApiRequestCB. When the response lands,
// GetGameNewsCallback checks the result fourcc ('new0' + index on success), parses the
// returned config record into the registered memory addresses (ParseConfigFile), and
// EndAction records the outcome.
//
// FLAGGED (wire/data constants not recoverable from the available exports -- see the
// per-table notes below): the request fourcc KAI_ACTION_CODE_MAPPING, and the DirtySock
// error-code mapping KA_CONFIGDOWNLOAD_NEWS_SERVER_INTERFACE_ERROR_MAPPING.

namespace CgsNetwork
{
    // The shared empty/error string the components point their base error-data slot at
    // (X360 &unk_820046A7). Also used as the default value for TagFieldGetString. Its
    // bytes live in unrecovered .rdata; declared extern as an honest placeholder.
    extern const char gpcEmptyErrorString[];

    namespace
    {
        // Pack a 4-character code the way the X360 build reads it (big-endian byte order),
        // so the literal matches the immediates in the disassembly.
        inline int FourCC(char a, char b, char c, char d)
        {
            return (static_cast<int>(static_cast<unsigned char>(a)) << 24) |
                   (static_cast<int>(static_cast<unsigned char>(b)) << 16) |
                   (static_cast<int>(static_cast<unsigned char>(c)) << 8)  |
                    static_cast<int>(static_cast<unsigned char>(d));
        }

        // The "game news" lobby request fourcc. GetGameNewsCallback (@0x82888CB0) accepts a
        // response whose code == miRequestedNewsIndex + 'new0' (the asm adds 0x6E657730),
        // and StartAction sends this fixed kind plus the index as the "NAME" field.
        // FLAGGED: the exact request kind word is not present in the recovered exports;
        // 'new0' is inferred from the success-code base the callback compares against.
        const int KI_REQUEST_GAME_NEWS = FourCC('n', 'e', 'w', '0');   // 0x6E657730

        // Result codes the callback / parser produce explicitly (X360 immediates).
        const int KI_RESULT_NEWS_BASE  = FourCC('n', 'e', 'w', '0');   // 0x6E657730 ('new0'+idx)
        const int KI_RESULT_NOT_FOUND  = FourCC('n', 'f', 'n', 'd');   // 0x6E666E64 ('nfnd')

        // The "NAME" tagfield carries the requested news index in the outgoing request.
        const int KI_NAME_FIELD_RECLEN = 2048;   // == KI_MESSAGE_BUFFER_SIZE

        // OnEvent reset trigger (the asm compares the event id against 5).
        const s32 KI_EVENT_RESET = 5;

        // DirtySock-error -> EServerInterfaceError map for the game-news download.
        // (CgsServerInterfaceDownloadableConfig.cpp:45 -- 2 entries.)
        // FLAGGED: only the 'nfnd' -> MISSING_CONFIG_FILE pairing is grounded (the empty-
        // config path returns 'nfnd'); the second entry's DirtySock code is inferred --
        // a timeout maps to the config TIMEOUT error in this sub-range.
        const DSErrorToServerInterfaceError KA_CONFIGDOWNLOAD_NEWS_SERVER_INTERFACE_ERROR_MAPPING[2] =
        {
            { FourCC('n', 'f', 'n', 'd'), E_SERVER_INTERFACE_DOWNLOADABLE_CONFIG_ERROR_MISSING_CONFIG_FILE },
            { FourCC('t', 'i', 'm', 'e'), E_SERVER_INTERFACE_DOWNLOADABLE_CONFIG_ERROR_TIMEOUT },
        };
    }

    // ---- Static lookup tables (indexed by EAction) --------------------------------

    // The lobby request kind sent for each action (StartAction: dword_820E936C[action]).
    const int ServerInterfaceDownloadableConfig::KAI_ACTION_CODE_MAPPING[E_ACTION_COUNT] =
    {
        KI_REQUEST_GAME_NEWS,
    };

    // The human-readable action name (StartActionCore: off_82F33598[action]).
    const char* ServerInterfaceDownloadableConfig::KAPC_ACTION_NAMES[E_ACTION_COUNT] =
    {
        "Download Configuration From Game News",
    };

    // Per-action { error mapping table, count } (EndAction/GetGameNewsCallback:
    // off_820E9380[action] / dword_820E9384[action]).
    const DSErrorToServerInterfaceErrorTable
    ServerInterfaceDownloadableConfig::KA_DS_ERROR_TABLE_LOOKUP[E_ACTION_COUNT] =
    {
        { KA_CONFIGDOWNLOAD_NEWS_SERVER_INTERFACE_ERROR_MAPPING, 2 },
    };

    // ===========================================================================
    // Lifecycle
    // ===========================================================================

    // @ 0x8287ACC0 -- initialise the base component error slots and our own state.
    void ServerInterfaceDownloadableConfig::Construct()
    {
        meStatus             = 2;                          // +0x08 (E_STATUS_IDLE)
        mpcCurrentAction     = gpcEmptyErrorString;        // +0x04 (base error-data slot)
        miLastError          = 0;                          // +0x0C
        mpaDataIDToMemAddrs  = 0;                           // +0x14
        miMemAddrTableSize   = 0;                           // +0x1C
        meCurrentAction      = E_ACTION_COUNT;             // +0x18 (== 1)
    }

    // @ 0x8287ACF0 -- reset the in-flight action.
    void ServerInterfaceDownloadableConfig::Destruct()
    {
        mpaDataIDToMemAddrs = 0;                            // +0x14
        miMemAddrTableSize  = 0;                            // +0x1C
        meCurrentAction     = E_ACTION_COUNT;              // +0x18
    }

    // @ 0x8287AD08 -- bind the server interface and clear the action state.
    bool ServerInterfaceDownloadableConfig::Prepare(ServerInterfaceDirtySock* lpServerInterface)
    {
        mpServerInterface   = lpServerInterface;           // +0x10
        mpaDataIDToMemAddrs = 0;                            // +0x14
        meCurrentAction     = E_ACTION_COUNT;              // +0x18
        miMemAddrTableSize  = 0;                            // +0x1C
        return true;
    }

    // @ 0x8287AD30 -- clear the action state.
    bool ServerInterfaceDownloadableConfig::Release()
    {
        mpaDataIDToMemAddrs = 0;                            // +0x14
        meCurrentAction     = E_ACTION_COUNT;              // +0x18
        miMemAddrTableSize  = 0;                            // +0x1C
        return true;
    }

    // @ 0x8287AD50 -- pause: drop any in-flight action.
    void ServerInterfaceDownloadableConfig::Suspend()
    {
        mpaDataIDToMemAddrs = 0;                            // +0x14
        meCurrentAction     = E_ACTION_COUNT;              // +0x18
        miMemAddrTableSize  = 0;                            // +0x1C
    }

    // No standalone X360 body in the milestone func set (the build inlines/omits it);
    // reconstructed from the Feb-2007 declaration. Resume re-enables the component without
    // an in-flight action, mirroring Suspend's reset. NOT asm-verified.
    void ServerInterfaceDownloadableConfig::Resume()
    {
        mpaDataIDToMemAddrs = 0;
        meCurrentAction     = E_ACTION_COUNT;
        miMemAddrTableSize  = 0;
    }

    // @ 0x8287AD68 -- on the reset event (id 5) drop the in-flight action.
    void ServerInterfaceDownloadableConfig::OnEvent(EServerInterfaceEvent leEvent, void* /*lpData*/)
    {
        if (static_cast<s32>(leEvent) == KI_EVENT_RESET)
        {
            mpaDataIDToMemAddrs = 0;                        // +0x14
            meCurrentAction     = E_ACTION_COUNT;          // +0x18
            miMemAddrTableSize  = 0;                        // +0x1C
        }
    }

    // ===========================================================================
    // Action plumbing
    // ===========================================================================

    // @ 0x8287AD88 -- record the action, tell the base it has started, then fire the
    // lobby request with the assembled message buffer + completion callback.
    void ServerInterfaceDownloadableConfig::StartAction(EAction leAction, LobbyApiCallbackT* lpCallback)
    {
        meCurrentAction = leAction;                        // +0x18
        StartActionCore(KAPC_ACTION_NAMES[leAction]);

        CGS_ASSERT(mpServerInterface->GetMessageBuffer() != 0, "mpacMessageBuffer");

        LobbyApiRequestCB(mpServerInterface->GetLobbyAPIRef(),
                          KAI_ACTION_CODE_MAPPING[meCurrentAction],
                          mpServerInterface->GetMessageBuffer(),
                          lpCallback,
                          this);
    }

    // @ 0x8287AE18 -- convert the DirtySock error for the current action, hand it to the
    // base to record, then return to idle.
    void ServerInterfaceDownloadableConfig::EndAction(int32_t liErrorCode)
    {
        const DSErrorToServerInterfaceErrorTable& lrTable = KA_DS_ERROR_TABLE_LOOKUP[meCurrentAction];
        EndActionCore(ServerInterfaceComponent::ConvertError(liErrorCode,
                                                             lrTable.mpMappingTable,
                                                             lrTable.miNumMappings));
        meCurrentAction = E_ACTION_COUNT;                  // +0x18 (== 1)
    }

    // The derived ConvertError wrapper (declared in the header; the X360 build inlines it
    // into EndAction / GetGameNewsCallback). Looks up this action's mapping table and
    // forwards to the base. Behaviour matches the inlined call sites.
    EServerInterfaceError ServerInterfaceDownloadableConfig::ConvertError(int32_t liErrorCode,
                                                                          EAction leAction)
    {
        const DSErrorToServerInterfaceErrorTable& lrTable = KA_DS_ERROR_TABLE_LOOKUP[leAction];
        return ServerInterfaceComponent::ConvertError(liErrorCode,
                                                       lrTable.mpMappingTable,
                                                       lrTable.miNumMappings);
    }

    // ===========================================================================
    // Public API
    // ===========================================================================

    // @ 0x828916C8 -- begin a game-news config download for liRequestIndex, registering
    // lpaDataIDToMemAddrs[liSize] as the destination for the parsed fields.
    void ServerInterfaceDownloadableConfig::GetConfigurationDataFromNews(
        int32_t liRequestIndex,
        DataIDToMemoryAddr* lpaDataIDToMemAddrs,
        int32_t liSize)
    {
        CGS_ASSERT(lpaDataIDToMemAddrs != 0, "lpaDataIDToMemAddrs");
        CGS_ASSERT(mpaDataIDToMemAddrs == 0, "mpaDataIDToMemAddrs == NULL");
        CGS_ASSERT(mpServerInterface != 0, "mpServerInterface");

        miMemAddrTableSize   = liSize;                     // +0x1C  (a4)
        miRequestedNewsIndex = liRequestIndex;             // +0x20  (a2)
        mpaDataIDToMemAddrs  = lpaDataIDToMemAddrs;         // +0x14  (a3)

        // Start the request from a freshly-emptied shared message buffer, then write the
        // requested index into it as the "NAME" field. (The X360 asserts the buffer twice
        // -- once before zeroing, once before TagFieldSetNumber; CGS_ASSERT folds both.)
        char* lpacMessageBuffer = mpServerInterface->GetMessageBuffer();
        lpacMessageBuffer[0] = 0;
        TagFieldSetNumber(lpacMessageBuffer, KI_NAME_FIELD_RECLEN, "NAME", liRequestIndex);

        StartAction(E_ACTION_DOWNLOADING_GAME_NEWS, GetGameNewsCallback);
    }

    // No standalone X360 body in the milestone func set; reconstructed from the Feb-2007
    // declaration. Parses the already-downloaded "conf" config record directly (no lobby
    // round-trip) into the registered memory addresses. NOT asm-verified.
    void ServerInterfaceDownloadableConfig::GetConfigurationDataFromConf(
        DataIDToMemoryAddr* lpaDataIDToMemAddrs,
        int32_t liSize)
    {
        CGS_ASSERT(lpaDataIDToMemAddrs != 0, "lpaDataIDToMemAddrs");
        CGS_ASSERT(mpServerInterface != 0, "mpServerInterface");

        mpaDataIDToMemAddrs = lpaDataIDToMemAddrs;
        miMemAddrTableSize  = liSize;

        const char* lpacConf = static_cast<const char*>(
            LobbyApiInfo(mpServerInterface->GetLobbyAPIRef(), FourCC('c', 'o', 'n', 'f')));
        ParseConfigFile(lpacConf);
    }

    // ===========================================================================
    // Completion / parsing
    // ===========================================================================

    // @ 0x82888CB0 -- the LobbyApiRequestCB completion for the game-news download.
    void ServerInterfaceDownloadableConfig::GetGameNewsCallback(LobbyApiRefT* /*lpApiRef*/,
                                                                LobbyApiMsgT* lpMsg,
                                                                void* lpData)
    {
        ServerInterfaceDownloadableConfig* lpThis =
            static_cast<ServerInterfaceDownloadableConfig*>(lpData);

        if (lpMsg->code == lpThis->miRequestedNewsIndex + KI_RESULT_NEWS_BASE)
        {
            // Success: the response carries the config record. An empty record means the
            // config file was not present on the server.
            if (lpMsg->pData[0] != 0)
            {
                lpThis->ParseConfigFile(lpMsg->pData);
                lpThis->EndAction(0);
            }
            else
            {
                lpThis->EndAction(KI_RESULT_NOT_FOUND);
            }
        }
        else
        {
            // The lobby reported an error fourcc instead of our news response.
            const DSErrorToServerInterfaceErrorTable& lrTable =
                KA_DS_ERROR_TABLE_LOOKUP[lpThis->meCurrentAction];
            lpThis->EndActionCore(lpThis->ServerInterfaceComponent::ConvertError(lpMsg->code,
                                                                                 lrTable.mpMappingTable,
                                                                                 lrTable.miNumMappings));
            lpThis->meCurrentAction = E_ACTION_COUNT;
        }
    }

    // @ 0x8287AE78 -- walk the registered field table, pulling each named field out of the
    // downloaded record and writing it into its destination memory address.
    void ServerInterfaceDownloadableConfig::ParseConfigFile(const char* lpacConfigData)
    {
        char lacBool[6] = { 0, 0, 0, 0, 0, 0 };

        CGS_ASSERT(lpacConfigData != 0, "NULL != lpacConfigData");
        CGS_ASSERT(mpaDataIDToMemAddrs != 0, "mpaDataIDToMemAddrs != NULL");

        char* lpacBase = reinterpret_cast<char*>(this);

        for (int32_t liIndex = 0; liIndex < miMemAddrTableSize; ++liIndex)
        {
            const DataIDToMemoryAddr& lrEntry = mpaDataIDToMemAddrs[liIndex];

            // Skip fields that are absent from the record.
            if (!TagFieldFind(lpacConfigData, lrEntry.mpacName))
                continue;

            char* lpacDest = lpacBase + lrEntry.miOffset;

            switch (lrEntry.meDataType)
            {
                case E_TYPE_INTEGER:
                {
                    *reinterpret_cast<int*>(lpacDest) =
                        TagFieldGetNumber(TagFieldFind(lpacConfigData, lrEntry.mpacName), 0);
                    break;
                }

                case E_TYPE_STRING:
                {
                    // Copy the field text into the destination buffer; its capacity is the
                    // field's own length (the X360 measures strlen of the located value).
                    const char* lpacValue = TagFieldFind(lpacConfigData, lrEntry.mpacName);
                    const char* lpacScan  = lpacValue;
                    while (*lpacScan)
                        ++lpacScan;
                    const s32 liLength = static_cast<s32>(lpacScan - lpacValue);
                    TagFieldGetString(lpacValue, lpacDest, liLength, gpcEmptyErrorString);
                    break;
                }

                case E_TYPE_FLOAT:
                {
                    *reinterpret_cast<float*>(lpacDest) =
                        TagFieldGetFloat(TagFieldFind(lpacConfigData, lrEntry.mpacName), 0.0f);
                    break;
                }

                case E_TYPE_BOOL:
                {
                    TagFieldGetString(TagFieldFind(lpacConfigData, lrEntry.mpacName),
                                      lacBool, 6, gpcEmptyErrorString);
                    // True iff the value equals "TRUE" (byte-wise compare, stopping at the
                    // first mismatch or the terminating NUL of either string).
                    const char* lpacValue = lacBool;
                    const char* lpacTrue  = "TRUE";
                    int liDiff = *lpacValue - *lpacTrue;
                    while (*lpacValue != 0 && liDiff == 0)
                    {
                        ++lpacValue;
                        ++lpacTrue;
                        liDiff = *lpacValue - *lpacTrue;
                    }
                    *lpacDest = (liDiff == 0) ? 1 : 0;
                    break;
                }

                default:
                {
                    CGS_ASSERT(false, "Invalid Data Type");
                    break;
                }
            }
        }
    }
}
