#ifndef CGS_SERVER_INTERFACE_HTTP_H
#define CGS_SERVER_INTERFACE_HTTP_H

#include "types.hpp"

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"
#include "protohttp.h"   // ProtoHttpRefT, the ProtoHttp* C API

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
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfaceDirtySock;      // forward; pointer member only
    struct DSErrorToServerInterfaceErrorTable;
    namespace DirtySock { using ::ProtoHttpRefT; }

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
        void StopHttpDownload();

        // The byte count the running download has received (the news / TOS copy sizes its
        // buffer from it).
        s32  GetHttpDownloadSize() const { return miAmountHttpDownloaded; }

        // --- component overrides and lifecycle ---
        virtual void Construct();
        virtual void OnEvent(EServerInterfaceEvent leEvent, void* lpData);
        void Destruct();
        bool Prepare(ServerInterfaceDirtySock* lpServerInterface);
        bool Release();
        void Update();
        void Suspend();
        void Resume();

    private:
        void EndAction(s32 liError);

        static const DSErrorToServerInterfaceErrorTable KA_DS_ERROR_TABLE_LOOKUP[E_ACTION_COUNT];
        static const char* KAPC_ACTION_NAMES[E_ACTION_COUNT];

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
