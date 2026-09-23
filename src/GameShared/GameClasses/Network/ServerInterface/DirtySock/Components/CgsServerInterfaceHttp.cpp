#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceHttp.h"

#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceEvents.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"   // MemAlloc / MemFree
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySockErrorHelpers.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <string.h>   // strlen, _strnicmp

// Case-insensitive bounded compare (console strnicmp -> MSVC _strnicmp).
#if defined(_MSC_VER)
#  define strnicmp _strnicmp
#endif

// The HTTPS download component over DirtySDK ProtoHttp. StartHttpsDownload issues the
// GET; Update pumps the transfer, sizes the buffer from the body length, receives the
// whole body and rejects an HTML error page, then completes the action. The buffer is
// owned by the component until DestroyHttpsDownload (or a disconnect) frees it.

namespace CgsNetwork
{
    namespace
    {
        // ProtoHttp receive-buffer size handed to ProtoHttpCreate.
        const s32 KI_PROTO_HTTP_BUFFER_SIZE = 0x2000;

        // ProtoHttpStatus selectors.
        const s32 KI_HTTP_STATUS_TIMEOUT = 0x74696D65;   // 'time'
        const s32 KI_HTTP_STATUS_BODY    = 0x626F6479;   // 'body'

        // ProtoHttpStatus('body') result while the header is still pending.
        const s32 KI_HTTP_BODY_PENDING = -2;

        // Download results EndAction maps through the error table.
        const s32 KI_HTTP_RESULT_OK           = 0;
        const s32 KI_HTTP_RESULT_TIMEOUT      = 1;
        const s32 KI_HTTP_RESULT_FAILURE      = 2;
        const s32 KI_HTTP_RESULT_INVALID_DATA = 3;

        const DSErrorToServerInterfaceError KA_HTTP_DS_SERVER_INTERFACE_ERROR_MAPPING[4] =
        {
            { KI_HTTP_RESULT_OK,           E_SERVER_INTERFACE_ERROR_NONE },
            { KI_HTTP_RESULT_TIMEOUT,      E_SERVER_INTERFACE_HTTP_ERROR_CONNECTION_TIMEOUT },
            { KI_HTTP_RESULT_FAILURE,      E_SERVER_INTERFACE_HTTP_ERROR_DOWNLOAD_FAILURE },
            { KI_HTTP_RESULT_INVALID_DATA, E_SERVER_INTERFACE_HTTP_ERROR_INVALID_DOWNLOAD_DATA },
        };
    }

    const DSErrorToServerInterfaceErrorTable
    ServerInterfaceHttp::KA_DS_ERROR_TABLE_LOOKUP[ServerInterfaceHttp::E_ACTION_COUNT] =
    {
        { KA_HTTP_DS_SERVER_INTERFACE_ERROR_MAPPING, 4 },
    };

    const char* ServerInterfaceHttp::KAPC_ACTION_NAMES[ServerInterfaceHttp::E_ACTION_COUNT] =
    {
        "Download Http",
    };

    // The console object is laid out by Construct; the constructor only installs the vtable.
    ServerInterfaceHttp::ServerInterfaceHttp()
    {
    }

    ServerInterfaceHttp::~ServerInterfaceHttp()
    {
    }

    void ServerInterfaceHttp::Construct()
    {
        meStatus               = 2;
        mpcCurrentAction       = "";
        miLastError            = 0;
        mpServerInterface      = 0;
        mpHttp                 = 0;
        mpcDownloadBuffer      = 0;
        meCurrentAction        = E_ACTION_COUNT;
        miAmountHttpDownloaded = 0;
        miHttpBufferSize       = 0;
        mi16Timeout            = 0;
    }

    void ServerInterfaceHttp::Destruct()
    {
        mpServerInterface      = 0;
        mpHttp                 = 0;
        mpcDownloadBuffer      = 0;
        meCurrentAction        = E_ACTION_COUNT;
        miAmountHttpDownloaded = 0;
        miHttpBufferSize       = 0;
        mi16Timeout            = 0;
    }

    bool ServerInterfaceHttp::Prepare(ServerInterfaceDirtySock* lpServerInterface)
    {
        mpServerInterface = lpServerInterface;
        meCurrentAction   = E_ACTION_COUNT;
        mpHttp            = ProtoHttpCreate(KI_PROTO_HTTP_BUFFER_SIZE);
        CGS_ASSERT(mpHttp != 0, "NULL != mpHttp");
        return true;
    }

    bool ServerInterfaceHttp::Release()
    {
        mpServerInterface = 0;
        meCurrentAction   = E_ACTION_COUNT;
        if (mpcDownloadBuffer != 0)
        {
            ServerInterfaceDirtySock::MemFree(mpcDownloadBuffer, 0, 0);
            mpcDownloadBuffer = 0;
        }
        if (mpHttp != 0)
        {
            ProtoHttpDestroy(mpHttp);
            mpHttp = 0;
        }
        return true;
    }

    void ServerInterfaceHttp::Update()
    {
        if (mpHttp == 0 || meCurrentAction != E_ACTION_DOWNLOADING)
        {
            return;
        }

        ProtoHttpUpdate(mpHttp);

        if (ProtoHttpStatus(mpHttp, KI_HTTP_STATUS_TIMEOUT, 0, 0) == 1)
        {
            EndAction(KI_HTTP_RESULT_TIMEOUT);
            return;
        }

        if (miHttpBufferSize > 0)
        {
            if (mpcDownloadBuffer == 0)
            {
                mpcDownloadBuffer = static_cast<char*>(
                    ServerInterfaceDirtySock::MemAlloc(miHttpBufferSize + 1, 0, 0));
            }

            const s32 liReceived = ProtoHttpRecvAll(mpHttp, mpcDownloadBuffer, miHttpBufferSize);
            if (liReceived > 0)
            {
                miAmountHttpDownloaded = liReceived;

                // A server error page instead of the requested document.
                if (strnicmp(mpcDownloadBuffer, "<!DOCTYPE", strlen("<!DOCTYPE")) == 0)
                {
                    EndAction(KI_HTTP_RESULT_FAILURE);
                    return;
                }
                if (strnicmp(mpcDownloadBuffer, "<html>", strlen("<html>")) == 0)
                {
                    EndAction(KI_HTTP_RESULT_FAILURE);
                    return;
                }

                mpcDownloadBuffer[miAmountHttpDownloaded] = 0;
                EndAction(KI_HTTP_RESULT_OK);
            }
            else if (liReceived < 0)
            {
                EndAction(KI_HTTP_RESULT_INVALID_DATA);
            }
            return;
        }

        miHttpBufferSize = ProtoHttpStatus(mpHttp, KI_HTTP_STATUS_BODY, 0, 0);
        if (miHttpBufferSize < 0 && miHttpBufferSize != KI_HTTP_BODY_PENDING)
        {
            EndAction(KI_HTTP_RESULT_INVALID_DATA);
        }
    }

    // Nothing to pause or restart: the server interface's suspend / resume fan-out reaches the
    // shared empty body for this component.
    void ServerInterfaceHttp::Suspend()
    {
    }

    void ServerInterfaceHttp::Resume()
    {
    }

    void ServerInterfaceHttp::OnEvent(EServerInterfaceEvent leEvent, void* /*lpData*/)
    {
        if (leEvent == E_SERVER_INTERFACE_CONNECTION_EVENT_DISCONNECTED)
        {
            miAmountHttpDownloaded = 0;
            meCurrentAction        = E_ACTION_COUNT;
            mi16Timeout            = 0;
            if (mpcDownloadBuffer != 0)
            {
                ServerInterfaceDirtySock::MemFree(mpcDownloadBuffer, 0, 0);
                mpcDownloadBuffer = 0;
            }
            ClearLastError();
        }
    }

    void ServerInterfaceHttp::EndAction(s32 liError)
    {
        CGS_ASSERT(meCurrentAction != E_ACTION_COUNT, "Trying to end an action when not doing anything\n");

        const DSErrorToServerInterfaceErrorTable& lrTable = KA_DS_ERROR_TABLE_LOOKUP[meCurrentAction];
        ServerInterfaceComponent::EndActionCore(
            ConvertError(liError, lrTable.mpMappingTable, lrTable.miNumMappings));

        miHttpBufferSize = 0;
        meCurrentAction  = E_ACTION_COUNT;
        mi16Timeout      = 0;
    }

    void ServerInterfaceHttp::StartHttpsDownload(const char* lpcUrl, s32 liBufferSize, s16 li16Timeout)
    {
        CGS_ASSERT(mpHttp != 0, "mpHttp != NULL");

        miHttpBufferSize       = liBufferSize;
        miAmountHttpDownloaded = 0;

        CGS_ASSERT(li16Timeout > 0, "li16Timeout > 0");

        mi16Timeout = li16Timeout;
        ProtoHttpGet(mpHttp, lpcUrl, 0);

        meCurrentAction = E_ACTION_DOWNLOADING;
        ServerInterfaceComponent::StartActionCore(KAPC_ACTION_NAMES[E_ACTION_DOWNLOADING]);
    }

    void ServerInterfaceHttp::StopHttpDownload()
    {
        if (meCurrentAction == E_ACTION_DOWNLOADING)
        {
            ProtoHttpAbort(mpHttp);
            EndAction(KI_HTTP_RESULT_OK);
        }
    }

    void ServerInterfaceHttp::DestroyHttpsDownload()
    {
        if (mpcDownloadBuffer != 0)
        {
            ServerInterfaceDirtySock::MemFree(mpcDownloadBuffer, 0, 0);
            mpcDownloadBuffer = 0;
        }
    }

    u8* ServerInterfaceHttp::GetHttpsDownloadBuffer()
    {
        return reinterpret_cast<u8*>(mpcDownloadBuffer);
    }
}
