#ifndef CGS_SERVER_INTERFACE_HTTP_H
#define CGS_SERVER_INTERFACE_HTTP_H

#include "types.hpp"

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

// ===========================================================================
// CgsNetwork::ServerInterfaceHttp
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceHttp.{h,cpp}
//
// The HTTPS-download server-interface component. Derives from
// CgsNetwork::ServerInterfaceComponent (CgsServerInterfaceHttp.h DWARF:
// `ServerInterfaceHttp : public CgsNetwork::ServerInterfaceComponent`).
//
// Layout (CgsServerInterfaceHttp.h DWARF), after the 4-word Component base:
//     +0x10  mpServerInterface       (ServerInterfaceDirtySock*)
//     +0x14  meCurrentAction         (EAction)
//     +0x18  mpHttp                  (DirtySock::ProtoHttpRefT*)
//     +0x1C  miAmountHttpDownloaded  (s32)
//     +0x20  miHttpBufferSize        (s32)
//     +0x24  mpcDownloadBuffer       (char*)
//     +0x28  mi16Timeout             (s16)
//
// This TU owns only the X360 vector-deleting destructor @ 0x827DE1F0 (it stores the
// component vtable off_820CDBF8 at this+0 and conditionally frees). The component's
// behavioural methods (Construct / Prepare / Update / StartHttpsDownload / ...) are
// declared for layout fidelity but bodied in their own TUs; only the destructor is
// reconstructed here.
// ===========================================================================

namespace CgsNetwork
{
    class ServerInterfaceDirtySock;       // forward; pointer member only
    namespace DirtySock { struct ProtoHttpRefT; }

    class ServerInterfaceHttp : public ServerInterfaceComponent
    {
    public:
        // CgsServerInterfaceHttp.h:75
        enum EAction
        {
            E_ACTION_DOWNLOADING = 0,
            E_ACTION_COUNT       = 1,
        };

        // CgsServerInterfaceHttp.h:73
        static const s32 KI_SERVER_INTERFACE_HTTP_DOWNLOAD_TIMEOUT = 2100;

        ServerInterfaceHttp();

        // CgsServerInterfaceHttp.h:69 -- vector deleting destructor @ 0x827DE1F0.
        virtual ~ServerInterfaceHttp();

        // ---- ADDITIVE GROW (BrnNetworkLoginManagerBase TU) --------------------------------
        // The login state machine downloads the terms-of-service over HTTPS through this
        // component (X360 reaches it through *(mpNetworkManager+0x38EC) + HTTP slot):
        //   StartHttpsDownload    -- LoginManagerBase::PrepareDownloadingTOS @ 0x825438A8
        //                            (StartHttpsDownload(http, url, bufferSize, 2100)). The
        //                            timeout literal 2100 matches KI_SERVER_INTERFACE_HTTP_DOWNLOAD_TIMEOUT.
        //   GetHttpsDownloadBuffer -- LoginManagerBase::UpdateDownloadingTOS @ 0x82566320
        //                            (returns the downloaded bytes; the BOM / leading-newline scan
        //                            walks the returned pointer).
        //   DestroyHttpsDownload  -- LoginManagerBase::UpdateDownloadingTOS (tear the download down).
        // Declared-only here; the bodies live in this component's own (DirtySock) TU.
        void StartHttpsDownload(const char* lpcUrl, s32 liBufferSize, s16 li16TimeoutMs);
        u8*  GetHttpsDownloadBuffer();
        void DestroyHttpsDownload();

    private:
        ServerInterfaceDirtySock*    mpServerInterface;       // +0x10
        EAction                      meCurrentAction;          // +0x14
        DirtySock::ProtoHttpRefT*    mpHttp;                   // +0x18
        s32                          miAmountHttpDownloaded;   // +0x1C
        s32                          miHttpBufferSize;         // +0x20
        char*                        mpcDownloadBuffer;        // +0x24
        s16                          mi16Timeout;              // +0x28
    };
}

#endif // CGS_SERVER_INTERFACE_HTTP_H
