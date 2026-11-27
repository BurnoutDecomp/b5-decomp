#include "GameSource/Network/Managers/X360/BrnNetworkCameraX360.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // mNetStreamLogChannelOutput (0x82F335C8)
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Network/X360/BrnNetworkCameraX360.cpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"                       // streamed assertions
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"              // CgsNetwork::NetworkTexture
#include "GameShared/GameClasses/Network/Utilities/CgsNetworkImageConverter.h"     // NetworkImageConverter::Convert
#include "GameShared/Jobs/DXTCompress/CgsNetworkTextureDXTCompress.h"              // SetNewTextureToCompress
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"               // PlayerManager::GetPlayerByID / GetMenuDataByID
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"               // NetworkPlayer::(Un)RegisterMessageType
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"      // IsLocalPlayerInGame
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h" // GetLocalPlayerInfo
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceGamesX360.h"      // GetPlayerXUIDByID
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h"            // BrnNetwork::PlayerParams
#include "GameSource/Network/Parameters/BrnNetworkPlayerInfoData.h"               // BrnNetwork::PlayerInfoData
#include "GameSource/Network/BrnNetworkPlayerMenuData.h"                          // BrnNetwork::PlayerMenuData::meCameraStatus
#include "GameSource/Network/BrnNetworkManager.h"                                  // BrnNetworkManager accessors

#include "GameSource/Resource/BrnResourceAllocator.h"                              // PrintConsoleMemory
#include <cstring>
#include <xtl.h>
#include <xcam.h>

namespace BrnNetwork
{
    namespace
    {
        // --- pixel formats the camera textures are allocated at (X360 PrepareTextures
        //     immediates; passed straight to NetworkTexture). ---
        const s32 KI_CAMERA_WIDTH  = 160;
        const s32 KI_CAMERA_HEIGHT = 120;
        const s32 KI_REMOTE_FEED_PIXEL_FORMAT     = 405274691;  // maPlayers[].mCameraPicture
        const s32 KI_LOCAL_CAMERA_PIXEL_FORMAT    = 438304779;  // mLocalCameraPicture
        const s32 KI_RECEIVED_CAMERA_PIXEL_FORMAT = 673710214;  // mReceivedCameraPicture

        // The DXT1 pixel format the compressed destination texture must already be in
        // (GetCompressedLocalCameraPicture asserts lpDstTexture->GetFormat() against it, 0x825868E4).
        const s32 KI_PIXELFORMAT_LIN_DXT1 = 438304850;

        // How old (seconds) a machine's decode / data latch may be before GetCameraPicture stops
        // serving its picture (PS3 sibling BrnNetworkCameraPS3.cpp:30).
        const f32 KF_NO_DATA_RECEIVED_TIMEOUT = 1.0f;

        // The VDP port both ends bind and send to (OpenSocket 0x82586B5C, SendVideoData 0x8258BA58).
        const u16 KU16_VDP_PORT = 1001;

        // The stream-engine network-data chunk SendVideoData asks for (`li r22, 0x400`, 0x8258BA34).
        const s32 KI_NETWORK_DATA_SIZE = 0x400;

        // SendVideoData's VDP header word and the bytes it sends beyond the network data
        // (`li r11, 4` 0x8258BA24; `addi r29, r11, 0xC` 0x8258BAF4).
        const u16 KU16_VDP_GAME_DATA_SIZE  = 4;
        const s32 KI_VIDEO_PACKET_OVERHEAD = 12;
    }

    // =======================================================================
    // GetMachineEntry (PS3 sibling h:244, cpp:913; ARTIST inlines it at every walk of the table)
    // =======================================================================
    CameraX360::Machine* CameraX360::GetMachineEntry(s32 lPlayerID)
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_ALLOWABLE_PLAYERS; ++liPlayerIndex)
        {
            if (maPlayers[liPlayerIndex].mPlayerID == lPlayerID)
            {
                return &maPlayers[liPlayerIndex];
            }
        }
        return NULL;
    }

    // =======================================================================
    // GetRemoteCameraPictures @ 0x825866B0
    //
    // Drain every receiving machine's decoded frames into mReceivedCameraPicture; when a frame
    // arrived, advance its decode latch and convert it into the machine's picture.
    // =======================================================================
    void CameraX360::GetRemoteCameraPictures()
    {
        bool lbProcessedPicture;
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_ALLOWABLE_PLAYERS; ++liPlayerIndex)
        {
            lbProcessedPicture = false;
            if (maPlayers[liPlayerIndex].mPlayerID != -1 && maPlayers[liPlayerIndex].mbReceiveFromPlayer)
            {
                D3DLOCKED_RECT lLockedRect;
                lLockedRect.pBits = mReceivedCameraPicture.GetTexture();
                lLockedRect.Pitch = mReceivedCameraPicture.GetStride();
                DWORD luResult;
                do
                {
                    luResult = mpStreamEngine->GetNextRemoteConsoleFrameRGB(maPlayers[liPlayerIndex].mXNAddr, &lLockedRect);
                    lbProcessedPicture = lbProcessedPicture || luResult == ERROR_SUCCESS;
                }
                while (luResult == ERROR_SUCCESS);

                if (lbProcessedPicture)
                {
                    if (maPlayers[liPlayerIndex].meLastPictureDecodeState == Machine::E_WAIT_STATE_1)
                    {
                        maPlayers[liPlayerIndex].meLastPictureDecodeState = Machine::E_WAIT_STATE_2;
                    }
                    else
                    {
                        maPlayers[liPlayerIndex].mLastPictureDecodeTime   = mpNetworkManager->GetTime();
                        maPlayers[liPlayerIndex].meLastPictureDecodeState = Machine::E_STATE_TIME_SET;
                    }
                    CgsNetwork::NetworkImageConverter::Convert(&mReceivedCameraPicture, &maPlayers[liPlayerIndex].mCameraPicture);
                }
            }
        }
    }

    // =======================================================================
    // GetLocalCameraPicture @ 0x825867E8
    // =======================================================================
    void CameraX360::GetLocalCameraPicture()
    {
        bool lbProcessedPicture = false;
        if (XCamGetStatus() == XCAMDEVICESTATE_INITIALIZED)
        {
            D3DLOCKED_RECT lLockedRect;
            lLockedRect.pBits = mLocalCameraPicture.GetTexture();
            lLockedRect.Pitch = mLocalCameraPicture.GetStride();
            DWORD luResult;
            do
            {
                luResult = mpStreamEngine->GetNextLocalPreviewFrame(&lLockedRect);
                lbProcessedPicture = lbProcessedPicture || luResult == ERROR_SUCCESS;
            }
            while (luResult == ERROR_SUCCESS);
        }
    }

    // =======================================================================
    // GetCompressedLocalCameraPicture @ 0x82586890
    // =======================================================================
    void CameraX360::GetCompressedLocalCameraPicture(
        CgsNetwork::NetworkTexture* lpDstTexture,
        s32 liQualitySetting,
        CgsNetwork::NetworkTextureDXTCompress::CompressionCompleteCallback lCompleteCallback,
        void* lpCompleteData)
    {
        CGS_ASSERT_AT(lpDstTexture, "lpDstTexture", CGS_ARTIST_FILE, 448);
        CGS_ASSERT_AT(KI_PIXELFORMAT_LIN_DXT1 == lpDstTexture->GetFormat(),
            "rw::graphics::core::PIXELFORMAT_LIN_DXT1 == lpDstTexture->GetFormat()", CGS_ARTIST_FILE, 449);
        if (XCamGetStatus() == XCAMDEVICESTATE_INITIALIZED)
        {
            CGS_ASSERT_AT(mpTextureCompressor, "mpTextureCompressor", CGS_ARTIST_FILE, 453);
            mpTextureCompressor->SetNewTextureToCompress(mLocalCameraPicture.GetTexture(),
                                                         mLocalCameraPicture.GetTextureSize(),
                                                         mLocalCameraPicture.GetWidth(),
                                                         mLocalCameraPicture.GetHeight(),
                                                         mLocalCameraPicture.GetStride(),
                                                         lpDstTexture->GetStride(),
                                                         liQualitySetting,
                                                         mLocalCameraPicture.GetFormat(),
                                                         false,
                                                         lCompleteCallback,
                                                         lpCompleteData);
        }
    }

    // ARTIST 0x825869F8. The reset covers 0x400 payload bytes beginning at packet+0xA.
    void CameraX360::ResetVideoPackets()
    {
        for (s32 liPacket = 0; liPacket < KI_MAX_NUM_VIDEO_PACKETS; ++liPacket)
        {
            VideoPacket& lrPacket = maVideoPackets[liPacket];
            lrPacket.mOverlapped.Construct();
            std::memset(lrPacket.mPacket.macData, 0, KI_NETWORK_DATA_SIZE);
            lrPacket.mPacket.mu16DataSize = 0;
            lrPacket.mbInUse = false;
        }
    }

    // =======================================================================
    // ResetRemotePlayers @ 0x82586A48
    // =======================================================================
    void CameraX360::ResetRemotePlayers()
    {
        for (s32 liMachineIndex = 0; liMachineIndex < KI_MAX_ALLOWABLE_PLAYERS; ++liMachineIndex)
        {
            std::memset(&maPlayers[liMachineIndex].mXNAddr, 0, sizeof(maPlayers[liMachineIndex].mXNAddr));
            maPlayers[liMachineIndex].mPlayerID                 = -1;
            maPlayers[liMachineIndex].miIPAddress               = -1;
            maPlayers[liMachineIndex].meLastDataReceivedState   = Machine::E_WAIT_STATE_1;
            maPlayers[liMachineIndex].meLastPictureDecodeState  = Machine::E_WAIT_STATE_1;
            maPlayers[liMachineIndex].mXuid                     = 0;
            maPlayers[liMachineIndex].mbSendToPlayer            = false;
            maPlayers[liMachineIndex].mbReceiveFromPlayer       = false;
            maPlayers[liMachineIndex].mCameraPicture.Release();
            maPlayers[liMachineIndex].mStatusMessageSend.Construct();
            maPlayers[liMachineIndex].mStatusMessageRecv.Construct();
            maPlayers[liMachineIndex].mRequestMessageSend.Construct();
            maPlayers[liMachineIndex].mRequestMessageRecv.Construct();
            maPlayers[liMachineIndex].mRequestResponseMessageSend.Construct();
            maPlayers[liMachineIndex].mRequestResponseMessageRecv.Construct();
        }

        mbReceivingFromAnyPlayer = false;
    }

    // ARTIST 0x82586AF8.
    void CameraX360::OpenSocket()
    {
        sockaddr_in lAddress;
        XMemSet(&lAddress, 0, sizeof(lAddress));
        lAddress.sin_family = AF_INET;
        lAddress.sin_addr.s_addr = INADDR_ANY;
        lAddress.sin_port = KU16_VDP_PORT; // Xenon stores the port in network byte order.
        mVDPSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_VDP);
        CGS_ASSERT_AT(mVDPSocket != INVALID_SOCKET, "socket failed\n", CGS_ARTIST_FILE, 1021);
        const int liBindResult = bind(mVDPSocket, reinterpret_cast<const sockaddr*>(&lAddress), sizeof(lAddress));
        CGS_ASSERT_STREAM_AT(liBindResult == 0,
            "sendsock bind failed, error " << WSAGetLastError() << "\n", CGS_ARTIST_FILE, 1028);
        u_long luNonBlocking = 1;
        ioctlsocket(mVDPSocket, FIONBIO, &luNonBlocking);
    }

    // ARTIST 0x82586C50.
    void CameraX360::CloseSocket()
    {
        CGS_ASSERT_AT(mVDPSocket != INVALID_SOCKET, "mVDPSocket != INVALID_SOCKET", CGS_ARTIST_FILE, 1049);
        CGS_ASSERT_STREAM_AT(closesocket(mVDPSocket) == 0,
            "close socket bind failed, error " << WSAGetLastError() << "\n", CGS_ARTIST_FILE, 1051);
        mVDPSocket = INVALID_SOCKET;
    }

    // =======================================================================
    // SendTo (inlined into SendVideoData, ARTIST 0x8258BAFC..0x8258BB44)
    // =======================================================================
    void CameraX360::SendTo(const char* lpcData, s32 liDataSize, s32 liIPAddress)
    {
        CGS_ASSERT_AT(mVDPSocket != INVALID_SOCKET, "mVDPSocket != INVALID_SOCKET", CGS_ARTIST_FILE, 1076);
        sockaddr_in lAddress;
        lAddress.sin_family      = AF_INET;
        lAddress.sin_addr.s_addr = liIPAddress;
        lAddress.sin_port        = KU16_VDP_PORT;
        sendto(mVDPSocket, lpcData, liDataSize, 0,
               reinterpret_cast<const sockaddr*>(&lAddress), sizeof(lAddress));
    }

    // =======================================================================
    // ReceiveFrom @ 0x82586D50
    // =======================================================================
    s32 CameraX360::ReceiveFrom(char* lppData, s32 liDataSize, s32* lpnIPAddress)
    {
        s32 liBytesReceived = 0;
        CGS_ASSERT_AT(lppData, "lppData", CGS_ARTIST_FILE, 1114);
        CGS_ASSERT_AT(lpnIPAddress, "lpnIPAddress", CGS_ARTIST_FILE, 1115);

        timeval lTimeout = { 0 };
        fd_set lReadSet = { 0 };
        FD_SET(mVDPSocket, &lReadSet);
        s32 liReturnValue = select(0, &lReadSet, NULL, NULL, &lTimeout);
        CGS_ASSERT_AT(liReturnValue != SOCKET_ERROR, "liReturnValue != SOCKET_ERROR", CGS_ARTIST_FILE, 1123);
        if (liReturnValue == 1)
        {
            sockaddr_in lFromAddress;
            s32 liFromLength = sizeof(lFromAddress);
            std::memset(lppData, 0, liDataSize);
            liBytesReceived = recvfrom(mVDPSocket, lppData, liDataSize, 0,
                                       reinterpret_cast<sockaddr*>(&lFromAddress), &liFromLength);
            if (liBytesReceived < 0)
            {
                CGS_FAIL_STREAM_AT("recvfrom failed, error " << WSAGetLastError() << "\n", CGS_ARTIST_FILE, 1141);
                liBytesReceived = 0;
            }
            else if (liBytesReceived > 0)
            {
                *lpnIPAddress = lFromAddress.sin_addr.s_addr;
            }
        }
        return liBytesReceived;
    }

    // _RequestMessageDeliveredCallback @ 0x82586FD0 (PS3 sibling cpp:1064).
    void CameraX360::_RequestMessageDeliveredCallback(bool /*lbSuccess*/, bool lbFakeNack,
                                                      CgsNetwork::SignalMessage* /*lpAck*/,
                                                      s32 /*lRecvingPlayerID*/, void* /*lpUserData*/)
    {
        if (lbFakeNack)
        {
            CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "WARNING: Fack Nack found in ";
            CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << __FUNCTION__;
            CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "\n";
        }
    }

    // =======================================================================
    // _RequestResponseMessageArrivedCallback @ 0x82587060
    //
    // The player we requested a feed from accepted: receive from it and register its console
    // with the stream engine.
    // =======================================================================
    void CameraX360::_RequestResponseMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                            s32 lSendingPlayerID,
                                                            void* lpUserData)
    {
        Machine* lpMachine;
        CameraX360* lpCamera = static_cast<CameraX360*>(lpUserData);
        CameraRequestResponseMessage* lpCameraRequestResponseMessage = static_cast<CameraRequestResponseMessage*>(lpMessage);
        CameraRequestResponseMessage::EResponse leResponse;

        CGS_ASSERT_AT(lpCamera, "lpCamera", CGS_ARTIST_FILE, 1375);
        CGS_ASSERT_AT(lpCameraRequestResponseMessage, "lpCameraRequestResponseMessage", CGS_ARTIST_FILE, 1376);

        lpMachine = lpCamera->GetMachineEntry(lSendingPlayerID);
        lpCameraRequestResponseMessage->Retrieve(&leResponse);
        if (lpMachine && lSendingPlayerID == lpCamera->miRequestedPlayerID
            && leResponse == CameraRequestResponseMessage::E_RESPONSE_ACCEPT)
        {
            lpMachine->mbReceiveFromPlayer      = true;
            lpCamera->mbReceivingFromAnyPlayer = true;
            lpCamera->mpStreamEngine->RegisterRemoteConsole(lpMachine->mXNAddr, &lpMachine->mXuid, 1,
                                                            lpMachine->mbSendToPlayer,
                                                            lpMachine->mbReceiveFromPlayer);
        }
    }

    // _RequestResponseMessageDeliveredCallback (PS3 sibling h:303, cpp:1121). Empty; ARTIST's copy
    // is the ICF-folded empty body 0x8284CB38 AddPlayer registers (0x82592408).
    void CameraX360::_RequestResponseMessageDeliveredCallback(bool /*lbSuccess*/, bool /*lbFakeNack*/,
                                                              CgsNetwork::SignalMessage* /*lpAck*/,
                                                              s32 /*lRecvingPlayerID*/, void* /*lpUserData*/)
    {
    }

    // =======================================================================
    // UnregisterPlayer @ 0x825871A0
    //
    // A machine that we neither send to nor receive from leaves the stream engine; one that still
    // streams one way is registered again with the directions it keeps.
    // =======================================================================
    void CameraX360::UnregisterPlayer(s32 lPlayerID)
    {
        Machine* lpMachine = GetMachineEntry(lPlayerID);
        if (lpMachine)
        {
            if (!lpMachine->mbReceiveFromPlayer && !lpMachine->mbSendToPlayer)
            {
                mpStreamEngine->UnregisterRemoteConsole(lpMachine->mXNAddr);
            }
            else
            {
                mpStreamEngine->RegisterRemoteConsole(lpMachine->mXNAddr, &lpMachine->mXuid, 1,
                                                      lpMachine->mbSendToPlayer,
                                                      lpMachine->mbReceiveFromPlayer);
            }
        }
    }

    // =======================================================================
    // OnGameStart @ 0x825872A8
    // =======================================================================
    void CameraX360::OnGameStart()
    {
        Machine* lpMachine;
        for (s32 liMachineIndex = 0; liMachineIndex < KI_MAX_ALLOWABLE_PLAYERS; ++liMachineIndex)
        {
            lpMachine = &maPlayers[liMachineIndex];
            if (lpMachine->mbReceiveFromPlayer || lpMachine->mbSendToPlayer)
            {
                mpStreamEngine->UnregisterRemoteConsole(lpMachine->mXNAddr);
                lpMachine->mbReceiveFromPlayer = false;
                lpMachine->mbSendToPlayer      = false;
            }
        }

        mbReceivingFromAnyPlayer = false;
        miNumPlayersSendingTo    = 0;
        mRequestTimer.SetFloatVal(0.0f);
        miRequestedPlayerID      = -1;
    }

    // =======================================================================
    // SetUserSetting @ 0x82587370 / SetGameEnabled @ 0x82587380
    // =======================================================================
    void CameraX360::SetUserSetting(s32 liUserSetting)
    {
        miUserSetting = liUserSetting;
    }

    void CameraX360::SetGameEnabled(bool lbGameEnabled)
    {
        mbGameEnabled = lbGameEnabled;
    }

    // =======================================================================
    // GetLocalCameraStatus @ 0x82587390
    // =======================================================================
    ECameraStatus CameraX360::GetLocalCameraStatus()
    {
        if (!mbPrepared || XCamGetStatus() != XCAMDEVICESTATE_INITIALIZED || !miUserSetting)
        {
            return E_CAMERA_STATUS_NONE;
        }
        if (!mbGameEnabled)
        {
            return E_CAMERA_STATUS_DISABLED;
        }
        return (miNumPlayersSendingTo == KI_MAX_PLAYERS_TO_SEND_TO) ? E_CAMERA_STATUS_IN_USE
                                                                     : E_CAMERA_STATUS_AVAILABLE;
    }

    // =======================================================================
    // Construct @ 0x8258B410
    // =======================================================================
    void CameraX360::Construct(BrnNetworkManager* lpNetworkManager,
                               CgsNetwork::NetworkTextureDXTCompress* lpTextureCompressor)
    {
        mbPrepared       = false;
        mpNetworkManager = lpNetworkManager;
        mVDPSocket       = INVALID_SOCKET;

        mLocalCameraPicture.Construct();
        mReceivedCameraPicture.Construct();

        miNumPlayersSendingTo = 0;
        for (s32 liMachineIndex = 0; liMachineIndex < KI_MAX_ALLOWABLE_PLAYERS; ++liMachineIndex)
        {
            maPlayers[liMachineIndex].mCameraPicture.Construct();
        }

        ResetVideoPackets();
        ResetRemotePlayers();

        mRequestTimer.SetFloatVal(0.0f);
        miRequestedPlayerID = -1;

        CGS_ASSERT_AT(lpTextureCompressor, "lpTextureCompressor", CGS_ARTIST_FILE, 87);
        mpTextureCompressor = lpTextureCompressor;

        miUserSetting            = 1;
        mbGameEnabled            = false;
        mbReceivingFromAnyPlayer = false;
    }

    // ARTIST 0x8258B530.
    bool CameraX360::Release()
    {
        mLocalCameraPicture.Release();
        mReceivedCameraPicture.Release();
        CloseSocket();
        ResetVideoPackets();
        ResetRemotePlayers();
        mpStreamEngine->Release();

        miNumPlayersSendingTo = 0;
        mbPrepared = false;
        mRequestTimer.SetFloatVal(0.0f);
        miRequestedPlayerID = -1;
        miUserSetting = 0;
        mbGameEnabled = false;
        mbReceivingFromAnyPlayer = false;
        return true;
    }

    // =======================================================================
    // Destruct @ 0x8258B618
    // =======================================================================
    void CameraX360::Destruct()
    {
        mRequestTimer.SetFloatVal(0.0f);
        miRequestedPlayerID = -1;

        mLocalCameraPicture.Destruct();
        mReceivedCameraPicture.Destruct();
        mVDPSocket = INVALID_SOCKET;

        ResetVideoPackets();
        ResetRemotePlayers();

        mpNetworkManager         = NULL;
        mpTextureCompressor      = NULL;
        miNumPlayersSendingTo    = 0;
        mbPrepared               = false;
        miUserSetting            = 0;
        mbGameEnabled            = false;
        mbReceivingFromAnyPlayer = false;
    }

    // =======================================================================
    // UpdateCameraStatus @ 0x8258B6F8
    // =======================================================================
    void CameraX360::UpdateCameraStatus()
    {
        ECameraStatus leLocalCameraStatus = GetLocalCameraStatus();
        if (mpNetworkManager->GetPlayerManager()->GetNumberLocalPlayers() > 0)
        {
            NetworkPlayerID lLocalPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
            mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID);
            PlayerMenuData* lpMenuData =
                static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lLocalPlayerID));
            CGS_ASSERT_AT(lpMenuData, "lpMenuData", CGS_ARTIST_FILE, 351);
            lpMenuData->meCameraStatus = leLocalCameraStatus;
        }
    }

    // =======================================================================
    // RemovePlayer @ 0x8258B798
    // =======================================================================
    void CameraX360::RemovePlayer(s32 lPlayerID)
    {
        if (lPlayerID == miRequestedPlayerID)
        {
            miRequestedPlayerID = -1;
            mRequestTimer = 0.0f;
        }

        Machine* lpMachine = GetMachineEntry(lPlayerID);
        if (lpMachine)
        {
            CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpNetworkManager->GetPlayerManager()->GetPlayerByID(lPlayerID);
            CGS_ASSERT_AT(lpNetworkPlayer, "lpNetworkPlayer", CGS_ARTIST_FILE, 587);

            if (lpMachine->mbSendToPlayer)
            {
                --miNumPlayersSendingTo;
            }
            lpMachine->mbSendToPlayer      = false;
            lpMachine->mbReceiveFromPlayer = false;
            UnregisterPlayer(lPlayerID);

            lpMachine->mPlayerID                = -1;
            lpMachine->miIPAddress              = -1;
            lpMachine->meLastDataReceivedState  = Machine::E_WAIT_STATE_1;
            lpMachine->meLastPictureDecodeState = Machine::E_WAIT_STATE_1;
            lpMachine->mXuid                    = 0;
            std::memset(&lpMachine->mXNAddr, 0, sizeof(lpMachine->mXNAddr));

            lpNetworkPlayer->UnRegisterMessageType(20);
            lpNetworkPlayer->UnRegisterMessageType(21);
            lpNetworkPlayer->UnRegisterMessageType(22);

            for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_ALLOWABLE_PLAYERS; ++liPlayerIndex)
            {
                if (maPlayers[liPlayerIndex].mbReceiveFromPlayer)
                {
                    return;
                }
            }
            mbReceivingFromAnyPlayer = false;
        }
    }

    // =======================================================================
    // RequestFeed() @ 0x8258B928 -- resend the feed request while the requested player is
    // finalised; drop the request when it has no machine.
    // =======================================================================
    void CameraX360::RequestFeed()
    {
        Machine* lpMachine = GetMachineEntry(miRequestedPlayerID);
        if (lpMachine == NULL)
        {
            miRequestedPlayerID = -1;
            mRequestTimer = 0.0f;
        }
        else if (mpNetworkManager->GetPlayerManager()->IsPlayerFinalised(miRequestedPlayerID))
        {
            lpMachine->mRequestMessageSend.PrepareForSend(mpNetworkManager->GetTimeManager()->GetU16FrameCount(), false);
        }
    }

    // =======================================================================
    // SendVideoData @ 0x8258BA18
    // =======================================================================
    void CameraX360::SendVideoData()
    {
        VideoDataPacket lPacket;
        lPacket.mu16GameDataSize = KU16_VDP_GAME_DATA_SIZE;
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_ALLOWABLE_PLAYERS; ++liPlayerIndex)
        {
            bool lbSentData;
            do
            {
                lbSentData = false;
                if (maPlayers[liPlayerIndex].mPlayerID != -1 && maPlayers[liPlayerIndex].mbSendToPlayer
                    && maPlayers[liPlayerIndex].miIPAddress != -1)
                {
                    DWORD luDataSize = KI_NETWORK_DATA_SIZE;
                    std::memset(lPacket.macData, 0, KI_NETWORK_DATA_SIZE);
                    if (mpStreamEngine->GetNetworkData(maPlayers[liPlayerIndex].mXNAddr,
                                                       reinterpret_cast<PBYTE>(lPacket.macData),
                                                       &luDataSize) == ERROR_SUCCESS)
                    {
                        lPacket.mu16DataSize = static_cast<u16>(luDataSize);
                        SendTo(reinterpret_cast<const char*>(&lPacket), lPacket.mu16DataSize + KI_VIDEO_PACKET_OVERHEAD,
                               maPlayers[liPlayerIndex].miIPAddress);
                        lbSentData = true;
                    }
                }
            }
            while (lbSentData);
        }
    }

    // =======================================================================
    // ReceiveVideoData @ 0x8258BB70
    // =======================================================================
    void CameraX360::ReceiveVideoData()
    {
        for (s32 liPacket = 0; liPacket < KI_MAX_NUM_VIDEO_PACKETS; ++liPacket)
        {
            if (maVideoPackets[liPacket].mbInUse && !maVideoPackets[liPacket].mOverlapped.IsOperationInProgress())
            {
                maVideoPackets[liPacket].mbInUse = false;
            }
        }

        for (;;)
        {
            VideoPacket* lpPacket = NULL;
            for (s32 liPacket = 0; liPacket < KI_MAX_NUM_VIDEO_PACKETS; ++liPacket)
            {
                if (!maVideoPackets[liPacket].mbInUse)
                {
                    lpPacket = &maVideoPackets[liPacket];
                    break;
                }
            }
            CGS_ASSERT_AT(lpPacket, "We've run out of video packets", CGS_ARTIST_FILE, 944);

            s32 liIPAddress;
            if (ReceiveFrom(reinterpret_cast<char*>(&lpPacket->mPacket), sizeof(lpPacket->mPacket), &liIPAddress) <= 0)
            {
                break;
            }

            Machine* lpMachine = NULL;
            for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_ALLOWABLE_PLAYERS; ++liPlayerIndex)
            {
                if (maPlayers[liPlayerIndex].miIPAddress == liIPAddress)
                {
                    lpMachine = &maPlayers[liPlayerIndex];
                    break;
                }
            }

            if (lpMachine && lpMachine->mbReceiveFromPlayer)
            {
                if (lpMachine->meLastDataReceivedState == Machine::E_WAIT_STATE_1)
                {
                    lpMachine->meLastDataReceivedState = Machine::E_WAIT_STATE_2;
                }
                else
                {
                    lpMachine->mLastDataReceivedTime   = mpNetworkManager->GetTime();
                    lpMachine->meLastDataReceivedState = Machine::E_STATE_TIME_SET;
                }
                lpPacket->mbInUse = true;
                // CgsXOverlapped is the 28-byte XOVERLAPPED itself (CgsXOverlapped.h); ARTIST passes
                // the packet's +0x410 as the completion record (0x8258BD40).
                mpStreamEngine->SubmitNetworkData(lpMachine->mXNAddr,
                                                  reinterpret_cast<PBYTE>(lpPacket->mPacket.macData),
                                                  lpPacket->mPacket.mu16DataSize,
                                                  reinterpret_cast<PXOVERLAPPED>(&lpPacket->mOverlapped));
            }
        }
    }

    // =======================================================================
    // _RequestMessageArrivedCallback @ 0x8258BD60
    //
    // A player asks for our feed (or releases it). The X360 declines every request.
    // =======================================================================
    void CameraX360::_RequestMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                    s32 lSendingPlayerID,
                                                    void* lpUserData)
    {
        Machine* lpMachine;
        CameraX360* lpCamera = static_cast<CameraX360*>(lpUserData);
        CameraRequestMessage* lpCameraRequestMessage = static_cast<CameraRequestMessage*>(lpMessage);
        bool lbReleaseFeed = false;

        CGS_ASSERT_AT(lpCamera, "lpCamera", CGS_ARTIST_FILE, 1292);
        CGS_ASSERT_AT(lpCameraRequestMessage, "lpCameraRequestMessage", CGS_ARTIST_FILE, 1293);

        lpMachine = lpCamera->GetMachineEntry(lSendingPlayerID);
        lpCameraRequestMessage->Retrieve(&lbReleaseFeed);
        if (lpMachine)
        {
            if (lbReleaseFeed)
            {
                if (lpMachine->mbSendToPlayer)
                {
                    --lpCamera->miNumPlayersSendingTo;
                    lpMachine->mbSendToPlayer = false;
                    lpCamera->UnregisterPlayer(lSendingPlayerID);
                }
            }
            else
            {
                lpMachine->mRequestResponseMessageSend.PrepareForSend(
                    lpCamera->mpNetworkManager->GetTimeManager()->GetU16FrameCount(),
                    CameraRequestResponseMessage::E_RESPONSE_DECLINE);
            }
        }
    }

    // =======================================================================
    // PrepareTextures @ 0x82591FA0
    // =======================================================================
    void CameraX360::PrepareTextures(CgsMemory::HeapMalloc* lpHeapMalloc)
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_ALLOWABLE_PLAYERS; ++liPlayerIndex)
        {
            maPlayers[liPlayerIndex].mCameraPicture.Prepare(
                lpHeapMalloc, KI_CAMERA_WIDTH, KI_CAMERA_HEIGHT,
                static_cast<renderengine::PixelFormat>(KI_REMOTE_FEED_PIXEL_FORMAT));
        }
        mLocalCameraPicture.Prepare(
            lpHeapMalloc, KI_CAMERA_WIDTH, KI_CAMERA_HEIGHT,
            static_cast<renderengine::PixelFormat>(KI_LOCAL_CAMERA_PIXEL_FORMAT));
        mReceivedCameraPicture.Prepare(
            lpHeapMalloc, KI_CAMERA_WIDTH, KI_CAMERA_HEIGHT,
            static_cast<renderengine::PixelFormat>(KI_RECEIVED_CAMERA_PIXEL_FORMAT));
    }

    // =======================================================================
    // Disconnected @ 0x82592038
    // =======================================================================
    void CameraX360::Disconnected()
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_ALLOWABLE_PLAYERS; ++liPlayerIndex)
        {
            if (maPlayers[liPlayerIndex].mPlayerID != -1)
            {
                RemovePlayer(maPlayers[liPlayerIndex].mPlayerID);
            }
        }
        miRequestedPlayerID = -1;
        mRequestTimer = 0.0f;
    }

    // =======================================================================
    // Update @ 0x825920A8
    // =======================================================================
    void CameraX360::Update()
    {
        if (mRequestTimer.GetFloatVal() > 0.0f)
        {
            // ARTIST constructs the one-second delay first, at its own stack slot (0x82592108), and
            // compares the difference against that slot (0x8259213C).
            const CgsSystem::Time lFeedRequestDelay(1.0f);
            if (mpNetworkManager->GetTime() - mRequestTimer >= lFeedRequestDelay)
            {
                mRequestTimer.SetFloatVal(0.0f);
                RequestFeed();
            }
        }

        if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame())
        {
            if (mbPrepared && mbGameEnabled
                && !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
            {
                GetRemoteCameraPictures();
                if (miUserSetting)
                {
                    SendVideoData();
                }
                if (mbReceivingFromAnyPlayer)
                {
                    ReceiveVideoData();
                }
            }
            UpdateCameraStatus();
        }

        if (miUserSetting && mbGameEnabled)
        {
            GetLocalCameraPicture();
        }
    }

    // =======================================================================
    // AddPlayer @ 0x82592268
    // =======================================================================
    void CameraX360::AddPlayer(s32 lPlayerID)
    {
        Machine* lpMachine;
        XUID lXuid = 0;
        XNADDR lXNAddr = { 0 };

        CGS_ASSERT_AT(mpNetworkManager->GetPlayerManager(), "mpNetworkManager->GetPlayerManager()", CGS_ARTIST_FILE, 485);
        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpNetworkManager->GetPlayerManager()->GetPlayerByID(lPlayerID);
        CGS_ASSERT_AT(lpNetworkPlayer, "lpNetworkPlayer", CGS_ARTIST_FILE, 488);

        lpMachine = GetMachineEntry(-1);
        CGS_ASSERT_AT(lpMachine, "No free entry for this player", CGS_ARTIST_FILE, 491);

        static_cast<CgsNetwork::ServerInterfaceGamesX360*>(mpNetworkManager->GetServerInterface()->GetGameComponent())
            ->GetPlayerXUIDByID(lPlayerID, &lXuid);

        lpMachine->mPlayerID                = lPlayerID;
        lpMachine->mXNAddr                  = lXNAddr;
        lpMachine->meLastDataReceivedState  = Machine::E_WAIT_STATE_1;
        lpMachine->meLastPictureDecodeState = Machine::E_WAIT_STATE_1;
        lpMachine->mbSendToPlayer           = false;
        lpMachine->mXuid                    = lXuid;
        lpMachine->mbReceiveFromPlayer      = false;
        lpMachine->miIPAddress              = -1;

        lpNetworkPlayer->RegisterMessageType(20, 36, &lpMachine->mStatusMessageSend, &lpMachine->mStatusMessageRecv,
                                             NULL, NULL, NULL);
        lpNetworkPlayer->RegisterMessageType(21, 44, &lpMachine->mRequestMessageSend, &lpMachine->mRequestMessageRecv,
                                             _RequestMessageArrivedCallback, _RequestMessageDeliveredCallback, this);
        lpNetworkPlayer->RegisterMessageType(22, 44, &lpMachine->mRequestResponseMessageSend, &lpMachine->mRequestResponseMessageRecv,
                                             _RequestResponseMessageArrivedCallback, _RequestResponseMessageDeliveredCallback, this);
    }

    // ARTIST 0x82593B28.
    bool CameraX360::Prepare(CgsMemory::HeapMalloc* lpHeapMalloc)
    {
        BrnResource::PrintConsoleMemory(0);
        ResetVideoPackets();
        ResetRemotePlayers();
        PrepareTextures(lpHeapMalloc);

        // Every field written one by one (ARTIST stores all thirteen words, the three zeros
        // included, 0x82593B60..0x82593BC4; no aggregate zero-fill).
        XCAM_STREAM_ENGINE_INIT_PARAMS lParams;
        lParams.VideoResolution = XCAMRESOLUTION_160x120;
        lParams.InitialTargetBitrate = 128;
        lParams.MinimumBitrate = 64;
        lParams.MaximumBitrate = 1000;
        lParams.InitialTargetFramerate = XCAMFRAMERATE_30;
        lParams.MaxRemoteConsoles = KI_MAX_REMOTE_CONSOLES;
        lParams.NetworkDataPacketSize = 1024;
        lParams.ThreadProcessorID = 4;
        lParams.GenerateLocalPreview = TRUE;
        lParams.GetDataDirectFromCamera = TRUE;
        lParams.CameraDataReadyCallback = NULL;
        lParams.CameraDataReadyCallbackUserdata = 0;
        lParams.pD3DDevice = NULL;
        CGS_ASSERT_AT(XCamCreateStreamEngine(&lParams, &mpStreamEngine) == ERROR_SUCCESS,
            "Unable to create XVID Streaming Engine!\n", CGS_ARTIST_FILE, 137);
        OpenSocket();

        miNumPlayersSendingTo = 0;
        mbPrepared = true;
        BrnResource::PrintConsoleMemory(0);
        mRequestTimer.SetFloatVal(0.0f);
        miRequestedPlayerID = -1;
        miUserSetting = 1;
        mbGameEnabled = false;
        mbReceivingFromAnyPlayer = false;
        return true;
    }

    // =======================================================================
    // PlayerFinalised @ 0x82595188
    // =======================================================================
    void CameraX360::PlayerFinalised(s32 lPlayerID)
    {
        PlayerParams lPlayerParams;
        lPlayerParams.Prepare();
        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(lPlayerID, &lPlayerParams);
        // Assigned, not initialised: ARTIST builds the returned XNADDR in a stack temporary and
        // copies it into the local (0x825951D0..0x825951E4).
        XNADDR lXNAddr;
        lXNAddr = lPlayerParams.GetNetworkAddress();

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpNetworkManager->GetPlayerManager()->GetPlayerByID(lPlayerID);
        CGS_ASSERT_AT(lpNetworkPlayer, "lpNetworkPlayer", CGS_ARTIST_FILE, 551);
        Machine* lpMachine = GetMachineEntry(lPlayerID);
        CGS_ASSERT_AT(lpMachine, "This player has not been added", CGS_ARTIST_FILE, 554);

        lpMachine->miIPAddress = lpNetworkPlayer->GetConnectionData().miIPAddress;
        lpMachine->mXNAddr     = lXNAddr;
    }

    // =======================================================================
    // ReleaseFeed @ 0x825952C0
    // =======================================================================
    void CameraX360::ReleaseFeed(s32 lPlayerID)
    {
        miRequestedPlayerID = -1;
        mRequestTimer = 0.0f;

        // ARTIST inlines BrnNetworkManager::IsLocalPlayer here (its PlayerInfoData, 0x825952F8..
        // 0x82595334): both are BrnNetworkUnity members.
        if (!mpNetworkManager->IsLocalPlayer(lPlayerID))
        {
            Machine* lpMachine = GetMachineEntry(lPlayerID);
            if (lpMachine)
            {
                lpMachine->mbReceiveFromPlayer = false;
                UnregisterPlayer(lPlayerID);
                lpMachine->mRequestMessageSend.PrepareForSend(mpNetworkManager->GetTimeManager()->GetU16FrameCount(), true);
                lpMachine->meLastDataReceivedState  = Machine::E_WAIT_STATE_1;
                lpMachine->meLastPictureDecodeState = Machine::E_WAIT_STATE_1;

                for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_ALLOWABLE_PLAYERS; ++liPlayerIndex)
                {
                    if (maPlayers[liPlayerIndex].mbReceiveFromPlayer)
                    {
                        return;
                    }
                }
                mbReceivingFromAnyPlayer = false;
            }
        }
    }

    // =======================================================================
    // GetCameraPicture @ 0x82595410
    //
    // The local camera picture for the local player (or -1), while the user and the game have the
    // camera on and the device is initialised. A remote machine's picture only while both its decode
    // and data latches are set and at most KF_NO_DATA_RECEIVED_TIMEOUT old.
    // =======================================================================
    CgsNetwork::NetworkTexture* CameraX360::GetCameraPicture(s32 liPlayerID)
    {
        if (!mbPrepared)
        {
            return NULL;
        }

        if (mpNetworkManager->IsLocalPlayer(liPlayerID) || liPlayerID == -1)
        {
            if (miUserSetting != 0 && mbGameEnabled && XCamGetStatus() == XCAMDEVICESTATE_INITIALIZED)
            {
                return &mLocalCameraPicture;
            }
            return NULL;
        }

        Machine* lpMachine = GetMachineEntry(liPlayerID);
        if (lpMachine == NULL || lpMachine->meLastDataReceivedState != Machine::E_STATE_TIME_SET)
        {
            return NULL;
        }

        const CgsSystem::Time lMaxDataAge(KF_NO_DATA_RECEIVED_TIMEOUT);
        if (!(mpNetworkManager->GetTime() - lpMachine->mLastDataReceivedTime <= lMaxDataAge)
            || lpMachine->meLastPictureDecodeState != Machine::E_STATE_TIME_SET)
        {
            return NULL;
        }

        const CgsSystem::Time lMaxDecodeAge(KF_NO_DATA_RECEIVED_TIMEOUT);
        if (!(mpNetworkManager->GetTime() - lpMachine->mLastPictureDecodeTime <= lMaxDecodeAge))
        {
            return NULL;
        }

        return &lpMachine->mCameraPicture;
    }

    // =======================================================================
    // RequestFeed(lPlayerID) @ 0x825976A8
    //
    // Select whose feed to watch. Nothing happens when the player is already the one requested.
    // Otherwise the feed requested from the previous player is released and the request cleared,
    // then re-latched: a local player (or -1, "none") at once with the timer zeroed; a remote
    // player only when it owns a machine, with the request timer stamped from the manager's time.
    // =======================================================================
    void CameraX360::RequestFeed(s32 lPlayerID)
    {
        if (lPlayerID == miRequestedPlayerID)
        {
            return;
        }

        if (miRequestedPlayerID != -1)
        {
            ReleaseFeed(miRequestedPlayerID);
        }
        miRequestedPlayerID = -1;
        mRequestTimer = 0.0f;

        if (mpNetworkManager->IsLocalPlayer(lPlayerID) || lPlayerID == -1)
        {
            miRequestedPlayerID = lPlayerID;
            mRequestTimer = 0.0f;
        }
        else if (GetMachineEntry(lPlayerID) != NULL)
        {
            miRequestedPlayerID = lPlayerID;
            mRequestTimer = mpNetworkManager->GetTime();
        }
    }

    // =======================================================================
    // GetRequestedCameraPicture @ 0x825977C0
    // =======================================================================
    CgsNetwork::NetworkTexture* CameraX360::GetRequestedCameraPicture()
    {
        return GetCameraPicture(miRequestedPlayerID);
    }

    // =======================================================================
    // _AssertLayout -- never called; pins the console offsets in a 32-bit build.
    // =======================================================================
    void CameraX360::_AssertLayout()
    {
#define BRN_CAM_AT(type, member, off) \
        CGS_STATIC_ASSERT((sizeof(void*) != 4 || offsetof(type, member) == off), #member " @ " #off)
        BRN_CAM_AT(VideoDataPacket, mu16DataSize,         0x008);
        BRN_CAM_AT(VideoDataPacket, macData,              0x00A);
        CGS_STATIC_ASSERT((sizeof(VideoDataPacket) == KI_VDP_PACKET_SIZE), "sizeof(VideoDataPacket) == 1040");
        BRN_CAM_AT(VideoPacket, mOverlapped,              0x410);
        BRN_CAM_AT(VideoPacket, mbInUse,                  0x42C);
        CGS_STATIC_ASSERT((sizeof(VideoPacket) == 0x430), "sizeof(VideoPacket) == 0x430");
        BRN_CAM_AT(Machine, mXuid,                        0x28);
        BRN_CAM_AT(Machine, miIPAddress,                  0x30);
        BRN_CAM_AT(Machine, mPlayerID,                    0x34);
        BRN_CAM_AT(Machine, meLastPictureDecodeState,     0x38);
        BRN_CAM_AT(Machine, mLastDataReceivedTime,        0x40);
        BRN_CAM_AT(Machine, mLastPictureDecodeTime,       0x48);
        BRN_CAM_AT(Machine, mCameraPicture,               0x50);
        BRN_CAM_AT(Machine, mbSendToPlayer,               0x6C);
        BRN_CAM_AT(Machine, mbReceiveFromPlayer,          0x6D);
        BRN_CAM_AT(Machine, mStatusMessageSend,           0x70);
        BRN_CAM_AT(Machine, mStatusMessageRecv,           0x94);
        BRN_CAM_AT(Machine, mRequestMessageSend,          0xB8);
        BRN_CAM_AT(Machine, mRequestResponseMessageSend,  0x110);
        BRN_CAM_AT(Machine, mRequestResponseMessageRecv,  0x13C);
        CGS_STATIC_ASSERT((sizeof(void*) != 4 || sizeof(Machine) == 0x168), "sizeof(Machine) == 0x168");
        BRN_CAM_AT(CameraX360, mpStreamEngine,            0x43000);
        BRN_CAM_AT(CameraX360, maPlayers,                 0x43008);
        BRN_CAM_AT(CameraX360, mpNetworkManager,          0x43B48);
        BRN_CAM_AT(CameraX360, mVDPSocket,                0x43B4C);
        BRN_CAM_AT(CameraX360, mLocalCameraPicture,       0x43B50);
        BRN_CAM_AT(CameraX360, mReceivedCameraPicture,    0x43B6C);
        BRN_CAM_AT(CameraX360, miNumPlayersSendingTo,     0x43B88);
        BRN_CAM_AT(CameraX360, mRequestTimer,             0x43B8C);
        BRN_CAM_AT(CameraX360, miRequestedPlayerID,       0x43B94);
        BRN_CAM_AT(CameraX360, mpTextureCompressor,       0x43B98);
        BRN_CAM_AT(CameraX360, miUserSetting,             0x43B9C);
        BRN_CAM_AT(CameraX360, mbPrepared,                0x43BA0);
        BRN_CAM_AT(CameraX360, mbReceivingFromAnyPlayer,  0x43BA2);
        CGS_STATIC_ASSERT((sizeof(void*) != 4 || sizeof(CameraX360) == 0x43BA8), "sizeof(CameraX360) == 0x43BA8");
#undef BRN_CAM_AT
    }
}
