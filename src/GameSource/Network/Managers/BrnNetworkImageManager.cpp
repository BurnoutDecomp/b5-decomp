// ===================================================================================
// BrnNetwork::NetworkImageManager -- sibling .cpp
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkImageManager.cpp
//
// The online mugshot / photo-finish image relay. See BrnNetworkImageManager.h for the full
// subsystem note + the X360 member-offset map.
//
// SOURCE-OF-TRUTH: X360 ARTIST pseudocode + ASM (postmortem packet) is the spine; the DecFIGS
// DWARF supplies the declaration shape; committed siblings (BrnNetworkAggressiveDrivingManager,
// CgsNetworkTexture, BrnImageMessage, the debug component) supply style.
//
// FUNCTION OWNERSHIP for this TU:
//   BODIED: Construct, Destruct, Prepare, Release, OnRoundStart, AddPlayer, RemovePlayer,
//           Disconnected, GetMugshotImageByAggressor, GetMugshotVictimID, the two message
//           callbacks, the two table look-ups, CheckMugshotPrivilege, ProcessAfterSimulation,
//           HandleRoundResults, ProcessNetworkEvents, OutputMugshotData, AbortMugshotCapture,
//           AbortMugshotShow and HandleReceivedCameraPic (HandlePlayerStoppedMode and
//           EnableMugshotOutput are header inlines).
//   DECLARATION-ONLY: the rest (listed at the end of this file); no stub bodies.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkImageManager.h"

#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"   // CgsNetwork::PlayerManager
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"   // CgsNetwork::NetworkPlayer + RegisterMessageType
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"               // CgsMemory::HeapMalloc
#include "GameSource/Network/BrnNetworkModule.h"                        // BrnNetworkModule::GetNetworkManager
#include "GameSource/Network/BrnNetworkManager.h"                       // BrnNetworkManager::GetLocalUserControllerPort
#include "GameShared/GameClasses/System/Input/CgsInputTypes.h"         // CgsInput::KU_NUMBER_OF_PADS
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"       // GetFirstEvent / GetNextEvent / AddEvent
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h" // IsLoggedIn
#include "GameSource/Network/BrnServerInterface.h"                      // GetConnectionComponent
#include "GameSource/Network/BrnNetworkModuleIO.h"                      // Output / PostSimulation buffers, NetworkEventQueue
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"               // NetworkInPaybackMugshotEvent / NetworkInAbortMugshotCaptureEvent
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"              // NetworkOutPostEventScalps / NetworkOutAbortImageCaptureEvent
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // GameStateToNetworkInterface::GetActiveRaceCarIndex
#include "GameSource/Network/Managers/X360/BrnNetworkCameraX360.h"      // CameraX360::GetUserSetting
#include "GameSource/Network/Managers/X360/BrnNetworkGamerPictureManagerX360.h" // GetCompressedGamerPictureTexture
#include "GameSource/GameState/BrnGameActions.h"                        // OnlineRoundResults::GetWinner

// Vendor XDK privilege query used by CheckMugshotPrivilege. Declared extern "C" at file
// scope (the real prototype lives in the Xbox 360 XDK), mirroring the corpus convention.
extern "C" { int XUserCheckPrivilege(u32 luUserIndex, u32 luPrivilegeType, u32* lpbResult); }

namespace BrnNetwork
{
    // The reliable-message channel id for the ImageMessage type (X360 RegisterMessageType leType = 19).
    static const s32 KI_IMAGE_MESSAGE_TYPE = 19;
    // The X360 packed ImageMessage byte length passed to RegisterMessageType (liLength = 568).
    static const s32 KI_IMAGE_MESSAGE_PACKED_LENGTH = 568;

    // Xbox communications privilege ids (asm 0xF7 / 0xF6).
    static const u32 KU_XPRIVILEGE_COMMUNICATIONS              = 247; // 0xF7
    static const u32 KU_XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY = 246; // 0xF6
    // The camera user setting that restricts the feed to friends (CAMERA_USER_FRIENDS_ONLY).
    static const s32 KI_CAMERA_USER_FRIENDS_ONLY                = 2;

    // The inbound network event that arms the mugshot-data output (tag 21; the event type has
    // no named home).
    static const s32 KI_INEVENT_OUTPUT_MUGSHOT_DATA             = 21;

    // ----------------------------------------------------------------------------------
    // Construct  @ 0x8255D7D0  (EXECUTED in goal trace)
    //   Stash the three collaborators, Construct the 7 ImageMessage send/recv pairs (clearing each
    //   slot's mPlayerID to -1), Construct the 8 mugshot NetworkTextures and clear their bookkeeping,
    //   then reset the manager scalars to the idle (E_..._STATE_COUNT) state and Construct the debug
    //   component. Mugshots default enabled.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::Construct(BrnNetworkModule* lpNetworkModule,
                                        CgsNetwork::PlayerManager* lpPlayerManager,
                                        CgsNetwork::TimeManager* lpTimeManager,
                                        CgsNetwork::NetworkTextureDXTCompress* lpTextureCompressor)
    {
        mpNetworkModule = lpNetworkModule;
        mpPlayerManager = lpPlayerManager;
        mpTimeManager   = lpTimeManager;

        for ( s32 liIndex = 0; liIndex < KI_MAX_IMAGE_PLAYERS; ++liIndex )
        {
            maImageData[liIndex].mPlayerID = -1;
            maImageData[liIndex].mImageMessageSend.Construct();
            maImageData[liIndex].mImageMessageRecv.Construct();
        }

        for ( s32 liIndex = 0; liIndex < KI_MAX_MUGSHOT_PLAYERS; ++liIndex )
        {
            maMugshotData[liIndex].mPicture.Construct();
            maMugshotData[liIndex].mReceivedPhotoPackets.UnSetAll();
            maMugshotData[liIndex].mTakedownAggressorPlayerID = -1;
            maMugshotData[liIndex].mTakedownVictimPlayerID    = -1;
            maMugshotData[liIndex].miLastPacketSent           = 0;
            maMugshotData[liIndex].miNumberOfPacketsToSend    = 0;
            maMugshotData[liIndex].mbPictureValid             = false;
            maMugshotData[liIndex].mbPhotoFinishValid         = false;
        }

        mTakedownAggressorPlayerID    = -1;
        mTakedownVictimPlayerID       = -1;
        mbOutputMugshotData           = false;
        mbBroadcastCurrentImage       = false;
        mbAbortCaptureThisFrame       = false;
        mbAbortShowThisFrame          = false;
        mRoadRuleBeatenID             = 0;
        meState                       = E_IMAGE_MANAGER_STATE_COUNT;
        meImageTypeToSend             = BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_COUNT;
        meImageTypeOfGamerPicToSave   = BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_COUNT;
        mbMugshotsEnabled             = true;

        mDebugComponent.Construct(this, lpTextureCompressor);
    }

    // ----------------------------------------------------------------------------------
    // Destruct  @ 0x8255D8B0
    //   Mirror of Construct: tear down the debug component, re-Construct the message pairs and
    //   Destruct the mugshot textures (clearing the same bookkeeping), reset the scalars.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::Destruct()
    {
        mDebugComponent.Destruct();

        for ( s32 liIndex = 0; liIndex < KI_MAX_IMAGE_PLAYERS; ++liIndex )
        {
            maImageData[liIndex].mPlayerID = -1;
            maImageData[liIndex].mImageMessageSend.Construct();
            maImageData[liIndex].mImageMessageRecv.Construct();
        }

        for ( s32 liIndex = 0; liIndex < KI_MAX_MUGSHOT_PLAYERS; ++liIndex )
        {
            maMugshotData[liIndex].mPicture.Destruct();
            maMugshotData[liIndex].mReceivedPhotoPackets.UnSetAll();
            maMugshotData[liIndex].mTakedownAggressorPlayerID = -1;
            maMugshotData[liIndex].mTakedownVictimPlayerID    = -1;
            maMugshotData[liIndex].miLastPacketSent           = 0;
            maMugshotData[liIndex].miNumberOfPacketsToSend    = 0;
            maMugshotData[liIndex].mbPictureValid             = false;
            maMugshotData[liIndex].mbPhotoFinishValid         = false;
        }

        mTakedownAggressorPlayerID    = -1;
        mTakedownVictimPlayerID       = -1;
        mbOutputMugshotData           = false;
        mRoadRuleBeatenID             = 0;
        meState                       = E_IMAGE_MANAGER_STATE_COUNT;
        meImageTypeToSend             = BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_COUNT;
        meImageTypeOfGamerPicToSave   = BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_COUNT;
        mbMugshotsEnabled             = true;
    }

    // ----------------------------------------------------------------------------------
    // Prepare  @ 0x8255D978
    //   Prepare the debug component first; if it fails, bail false. Otherwise allocate the 8 mugshot
    //   textures (160x120, the X360 pixel-format constant 438304850) from the supplied heap, clear
    //   their state, reset the takedown ids + output flag, return true.
    //   (DebugComponent::Prepare is the CgsSound::Playback::Content::DoOnPostLoad the X360 inlined.)
    // ----------------------------------------------------------------------------------
    bool NetworkImageManager::Prepare(CgsMemory::HeapMalloc* lpHeapMalloc)
    {
        if ( !mDebugComponent.Prepare(lpHeapMalloc) )
            return false;

        for ( s32 liIndex = 0; liIndex < KI_MAX_MUGSHOT_PLAYERS; ++liIndex )
        {
            maMugshotData[liIndex].mPicture.Prepare(lpHeapMalloc, 160, 120,
                                                    static_cast<renderengine::PixelFormat>(438304850));
            maMugshotData[liIndex].mReceivedPhotoPackets.UnSetAll();
            maMugshotData[liIndex].mTakedownAggressorPlayerID = -1;
            maMugshotData[liIndex].mTakedownVictimPlayerID    = -1;
            maMugshotData[liIndex].mbPictureValid             = false;
            maMugshotData[liIndex].mbPhotoFinishValid         = false;   // stb r30,0x11(r31)
            maMugshotData[liIndex].miLastPacketSent           = 0;
            maMugshotData[liIndex].miNumberOfPacketsToSend    = 0;       // stw r30,0xC(r31)
        }

        mTakedownAggressorPlayerID = -1;
        mTakedownVictimPlayerID    = -1;
        mbOutputMugshotData        = false;
        return true;
    }

    // ----------------------------------------------------------------------------------
    // Release  @ 0x8255DA20
    //   Release the debug component; if it fails, bail false. Otherwise Release the 8 mugshot
    //   textures, clear their state and the takedown ids + output flag, return true.
    // ----------------------------------------------------------------------------------
    bool NetworkImageManager::Release()
    {
        if ( !mDebugComponent.Release() )
            return false;

        for ( s32 liIndex = 0; liIndex < KI_MAX_MUGSHOT_PLAYERS; ++liIndex )
        {
            maMugshotData[liIndex].mPicture.Release();
            maMugshotData[liIndex].mReceivedPhotoPackets.UnSetAll();
            maMugshotData[liIndex].mTakedownAggressorPlayerID = -1;
            maMugshotData[liIndex].mTakedownVictimPlayerID    = -1;
            maMugshotData[liIndex].mbPictureValid             = false;
            maMugshotData[liIndex].mbPhotoFinishValid         = false;   // stb r30,0x11
            maMugshotData[liIndex].miLastPacketSent           = 0;
            maMugshotData[liIndex].miNumberOfPacketsToSend    = 0;       // stw r30,0xC
        }

        mTakedownAggressorPlayerID = -1;
        mTakedownVictimPlayerID    = -1;
        mbOutputMugshotData        = false;
        return true;
    }

    // ----------------------------------------------------------------------------------
    // OnRoundStart  @ 0x8255DC98
    //   Reset every mugshot slot's takedown ids + valid flags at the start of a round (the
    //   reassembled pictures themselves are left allocated).
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::OnRoundStart()
    {
        for ( s32 liIndex = 0; liIndex < KI_MAX_MUGSHOT_PLAYERS; ++liIndex )
        {
            // asm @0x8255DC98: the aggressor field (+0x28) is NOT touched here.
            maMugshotData[liIndex].mReceivedPhotoPackets.UnSetAll();  // std 0,-0xC(r11) -> bitset +0x20
            maMugshotData[liIndex].mTakedownVictimPlayerID    = -1;   // stw -1,0(r11)   -> +0x2C
            maMugshotData[liIndex].mbPictureValid             = false;// stb 0,0xC       -> +0x38
            maMugshotData[liIndex].mbPhotoFinishValid         = false;// stb 0,0xD       -> +0x39
            maMugshotData[liIndex].miLastPacketSent           = 0;    // stw 0,4         -> +0x30
            maMugshotData[liIndex].miNumberOfPacketsToSend    = 0;    // stw 0,8         -> +0x34
        }
    }

    // ----------------------------------------------------------------------------------
    // AddPlayer  @ 0x82576110
    //   Initialise the joining player's mugshot slot (cleared), then -- if the PlayerManager knows
    //   the player -- register the ImageMessage reliable-message type on that NetworkPlayer (binding
    //   the slot's send/recv ImageMessage + the arrival/delivery callbacks) and record the id.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::AddPlayer(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(mpPlayerManager, "mpPlayerManager");

        MugshotData* lpMugshotDataEntry = GetMugshotDataEntry(-1);
        CGS_ASSERT(lpMugshotDataEntry, "lpMugshotDataEntry");

        lpMugshotDataEntry->mReceivedPhotoPackets.UnSetAll();
        lpMugshotDataEntry->mTakedownAggressorPlayerID = lPlayerID;  // stw r28,0x28 -- slot KEY
        lpMugshotDataEntry->mTakedownVictimPlayerID    = -1;         // stw r10,0x2C
        lpMugshotDataEntry->mbPictureValid             = false;      // +0x38
        lpMugshotDataEntry->mbPhotoFinishValid         = false;      // stb r11,0x39
        lpMugshotDataEntry->miLastPacketSent           = 0;          // +0x30
        lpMugshotDataEntry->miNumberOfPacketsToSend    = 0;          // stw 0,0x34

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        if ( lpNetworkPlayer )
        {
            ImageMessageData* lpDataEntry = GetImageMessageDataEntry(-1);
            CGS_ASSERT(lpDataEntry, "lpDataEntry");

            lpNetworkPlayer->RegisterMessageType(
                KI_IMAGE_MESSAGE_TYPE,
                KI_IMAGE_MESSAGE_PACKED_LENGTH,
                reinterpret_cast<CgsNetwork::Message*>(&lpDataEntry->mImageMessageSend),
                reinterpret_cast<CgsNetwork::Message*>(&lpDataEntry->mImageMessageRecv),
                &NetworkImageManager::_ImageMessageArrivedCallback,
                &NetworkImageManager::_ImageMessageDeliveredCallback,
                this);
            lpDataEntry->mPlayerID = lPlayerID;
        }
    }

    // ----------------------------------------------------------------------------------
    // RemovePlayer  @ 0x8255DB40
    //   Unregister the ImageMessage type from the leaving player's NetworkPlayer (and clear that
    //   ImageMessageData slot's id), then clear the player's mugshot slot. If the player was the one
    //   we were mid-capture for and we are not idle, request a capture abort next frame.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::RemovePlayer(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(mpPlayerManager, "mpPlayerManager");

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        if ( lpNetworkPlayer )
        {
            lpNetworkPlayer->UnRegisterMessageType(KI_IMAGE_MESSAGE_TYPE);
            GetImageMessageDataEntry(lPlayerID)->mPlayerID = -1;
        }

        MugshotData* lpMugshotDataEntry = GetMugshotDataEntry(lPlayerID);
        lpMugshotDataEntry->mReceivedPhotoPackets.UnSetAll();
        lpMugshotDataEntry->mTakedownAggressorPlayerID = -1;
        lpMugshotDataEntry->mTakedownVictimPlayerID    = -1;
        lpMugshotDataEntry->mbPictureValid             = false;
        lpMugshotDataEntry->mbPhotoFinishValid         = false;
        lpMugshotDataEntry->miLastPacketSent           = 0;
        lpMugshotDataEntry->miNumberOfPacketsToSend    = 0;

        if ( lPlayerID == mTakedownAggressorPlayerID && meState != E_IMAGE_MANAGER_STATE_COUNT )
            mbAbortCaptureThisFrame = true;
    }

    // ----------------------------------------------------------------------------------
    // Disconnected  @ 0x8255DC08
    //   On a full disconnect, RemovePlayer every still-registered mugshot slot (asserting it cleared),
    //   then -- if we were not idle -- request a show abort next frame.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::Disconnected()
    {
        for ( s32 liIndex = 0; liIndex < KI_MAX_MUGSHOT_PLAYERS; ++liIndex )
        {
            if ( maMugshotData[liIndex].mTakedownAggressorPlayerID != -1 )
            {
                RemovePlayer(maMugshotData[liIndex].mTakedownAggressorPlayerID);
                CGS_ASSERT(maMugshotData[liIndex].mTakedownAggressorPlayerID == -1,
                           "maMugshotData[liIndex].mTakedownAggressorPlayerID == CgsNetwork::K_INVALID_PLAYER_ID");
            }
        }

        if ( meState != E_IMAGE_MANAGER_STATE_COUNT )
            mbAbortCaptureThisFrame = true;
    }

    // ----------------------------------------------------------------------------------
    // GetMugshotImageByAggressor  @ 0x8254AC28
    //   Look up the mugshot slot keyed by aggressor id; return the reassembled picture only if the
    //   slot has a known victim (mTakedownVictimPlayerID != -1) and a valid picture, else null.
    // ----------------------------------------------------------------------------------
    CgsNetwork::NetworkTexture* NetworkImageManager::GetMugshotImageByAggressor(NetworkPlayerID lTakedownAggressorID)
    {
        MugshotData* lpMugshotDataEntry = GetMugshotDataEntry(lTakedownAggressorID);
        if ( !lpMugshotDataEntry )
        {
            CGS_ASSERT(lpMugshotDataEntry, "lpMugshotDataEntry");
            return nullptr;
        }

        if ( lpMugshotDataEntry->mTakedownVictimPlayerID == -1 || !lpMugshotDataEntry->mbPictureValid )
            return nullptr;

        return &lpMugshotDataEntry->mPicture;
    }

    // ----------------------------------------------------------------------------------
    // GetMugshotVictimID  @ 0x8254AE10
    //   Return the victim id recorded in the mugshot slot keyed by aggressor id.
    // ----------------------------------------------------------------------------------
    NetworkPlayerID NetworkImageManager::GetMugshotVictimID(NetworkPlayerID lTakedownAggressorID)
    {
        MugshotData* lpMugshotDataEntry = GetMugshotDataEntry(lTakedownAggressorID);
        CGS_ASSERT(lpMugshotDataEntry, "lpMugshotDataEntry");
        return lpMugshotDataEntry->mTakedownVictimPlayerID;
    }

    // ----------------------------------------------------------------------------------
    // _ImageMessageArrivedCallback  @ 0x82573700
    //   Reliable-message arrival hook: assert the message, then forward to ReceiveImageMessage on the
    //   manager carried in the user-data. (The X360 asserts the message pointer twice.)
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::_ImageMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                           NetworkPlayerID liFromPlayerID, void* lpUserData)
    {
        CGS_ASSERT(lpMessage, "lpImageMessage");
        CGS_ASSERT(lpMessage, "lpImageMessage");

        NetworkImageManager* lpThis = static_cast<NetworkImageManager*>(lpUserData);
        lpThis->ReceiveImageMessage(liFromPlayerID, reinterpret_cast<ImageMessage*>(lpMessage));
    }

    // ----------------------------------------------------------------------------------
    // ProcessAfterSimulation
    //   While no capture is in progress the local player's received-packet set is kept clear.
    //   Service the abort requests raised last frame, then drain the inbound network events,
    //   the dirty-trick events and the next outgoing picture segment.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::ProcessAfterSimulation(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput)
    {
        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");

        if ( mTakedownVictimPlayerID == -1 )
        {
            CGS_ASSERT(mpPlayerManager, "mpPlayerManager");
            const NetworkPlayerID lLocalPlayerID = mpPlayerManager->GetLocalPlayerID();
            if ( lLocalPlayerID != -1 )
            {
                MugshotData* lpLocalMugshotData = GetMugshotDataEntry(lLocalPlayerID);
                if ( lpLocalMugshotData )
                    lpLocalMugshotData->mReceivedPhotoPackets.UnSetAll();
            }
        }

        if ( mbAbortCaptureThisFrame )
        {
            AbortMugshotCapture();
            mbAbortCaptureThisFrame = false;
        }

        if ( mbAbortShowThisFrame )
        {
            AbortMugshotShow();
            mbAbortShowThisFrame = false;
        }

        ProcessNetworkEvents(lpInput->GetNetworkEventQueue());
        ProcessDirtyTrickEvents();
        SendNextSegment();
    }

    // ----------------------------------------------------------------------------------
    // HandleRoundResults
    //   Online and with a round winner: when the winner's slot holds a photo-finish picture,
    //   save it as the victory mugshot; otherwise fetch the winner's gamer picture (compressed
    //   into the winner's slot texture) to stand in for it, recording the winner as the local
    //   slot's victim.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::HandleRoundResults(const BrnGameState::GameStateModuleIO::OnlineRoundResults* lpRoundResults)
    {
        if ( !mpNetworkModule->GetNetworkManager()->GetServerInterface()->GetConnectionComponent()->IsLoggedIn() )
            return;

        CGS_ASSERT(lpRoundResults, "lpRoundResults");

        const NetworkPlayerID lRoundWinnerID = lpRoundResults->GetWinner();
        if ( lRoundWinnerID == -1 )
            return;

        MugshotData* lpMugshotDataEntry = GetMugshotDataEntry(lRoundWinnerID);
        CGS_ASSERT(lpMugshotDataEntry, "lpMugshotDataEntry");

        if ( lpMugshotDataEntry->mbPhotoFinishValid )
        {
            RequestMugshotSave(lpMugshotDataEntry, lRoundWinnerID,
                               BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_VICTORY_MUGSHOT);
            return;
        }

        CGS_ASSERT(mpNetworkModule->GetNetworkManager(), "mpNetworkModule->GetNetworkManager()");
        GamerPictureManagerX360* lpGamerPicManager = mpNetworkModule->GetNetworkManager()->GetGamerPictureManager();
        CGS_ASSERT(lpGamerPicManager, "lpGamerPicManager");

        CGS_ASSERT(mpPlayerManager, "mpPlayerManager");
        const NetworkPlayerID lLocalPlayerID = mpPlayerManager->GetLocalPlayerID();
        if ( lLocalPlayerID == -1 )
            return;

        MugshotData* lpLocalMugshotData = GetMugshotDataEntry(lLocalPlayerID);
        CGS_ASSERT(lpLocalMugshotData, "lpLocalMugshotData");

        lpLocalMugshotData->mTakedownVictimPlayerID = lRoundWinnerID;
        meImageTypeOfGamerPicToSave = BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_VICTORY_MUGSHOT;
        lpGamerPicManager->GetCompressedGamerPictureTexture(lRoundWinnerID, &lpMugshotDataEntry->mPicture,
                                                            &NetworkImageManager::_GetCompressedGamerPicCallback, this);
    }

    // ----------------------------------------------------------------------------------
    // ProcessNetworkEvents
    //   Walk the inbound network events: the output request arms the mugshot-data output, a
    //   payback mugshot starts a capture, an abort request aborts the capture next frame.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::ProcessNetworkEvents(const BrnNetworkModuleIO::NetworkEventQueue* lpQueue)
    {
        const CgsModule::Event* lpEvent = nullptr;
        s32 liSize = 0;
        s32 leEventType = lpQueue->GetFirstEvent(&lpEvent, &liSize);

        while ( lpEvent != nullptr )
        {
            switch ( leEventType )
            {
            case KI_INEVENT_OUTPUT_MUGSHOT_DATA:
                mbOutputMugshotData = true;
                break;

            case BrnNetworkModuleIO::NetworkInPaybackMugshotEvent::KI_EVENT_TYPE:
                HandleMugshotEvent(reinterpret_cast<const BrnNetworkModuleIO::NetworkInPaybackMugshotEvent*>(lpEvent));
                break;

            case BrnNetworkModuleIO::NetworkInAbortMugshotCaptureEvent::KI_EVENT_TYPE:
                mbAbortCaptureThisFrame = true;
                break;

            default:
                break;
            }

            const CgsModule::Event* lpNextEvent = nullptr;
            leEventType = lpQueue->GetNextEvent(lpEvent, &lpNextEvent, &liSize);
            lpEvent = lpNextEvent;
        }
    }

    // ----------------------------------------------------------------------------------
    // OutputMugshotData
    //   When armed, publish every slot that holds a takedown (aggressor -> victim) as the
    //   post-event scalps list, each player given as its active-race-car index.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::OutputMugshotData(BrnNetworkModuleIO::OutputBuffer* lpOutput)
    {
        if ( !mbOutputMugshotData )
            return;

        BrnNetworkModuleIO::GameStateToNetworkInterface* lpGameStateToNetworkInterface =
            mpNetworkModule->GetGameStateToNetworkInterface();

        BrnNetworkModuleIO::NetworkOutPostEventScalps lScalpsEvent;
        lScalpsEvent.miNumScalpsWon = 0;
        for ( s32 liIndex = 0; liIndex < KI_MAX_MUGSHOT_PLAYERS; ++liIndex )
        {
            const MugshotData& lrMugshotData = maMugshotData[liIndex];
            if ( lrMugshotData.mTakedownVictimPlayerID != -1 )
            {
                BrnNetworkModuleIO::NetworkOutPostEventScalps::OnlineScalp& lrScalp =
                    lScalpsEvent.maOnlineScalps[lScalpsEvent.miNumScalpsWon];
                lrScalp.miAggressorIndex =
                    lpGameStateToNetworkInterface->GetActiveRaceCarIndex(lrMugshotData.mTakedownAggressorPlayerID);
                lrScalp.miVictimIndex =
                    lpGameStateToNetworkInterface->GetActiveRaceCarIndex(lrMugshotData.mTakedownVictimPlayerID);
                ++lScalpsEvent.miNumScalpsWon;
            }
        }

        lpOutput->GetNetworkEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lScalpsEvent),
                                                   lScalpsEvent.GetEventType(), sizeof(lScalpsEvent));
    }

    // ----------------------------------------------------------------------------------
    // AbortMugshotCapture / AbortMugshotShow
    //   Stop sending every slot's picture, tell the game the capture (or show) was aborted,
    //   clear the displayed image and drop back to the idle state.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::AbortMugshotCapture()
    {
        for ( s32 liIndex = 0; liIndex < KI_MAX_MUGSHOT_PLAYERS; ++liIndex )
            maMugshotData[liIndex].miNumberOfPacketsToSend = 0;

        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetNetworkEventQueue(), "mpNetworkModule->GetNetworkEventQueue()");

        BrnNetworkModuleIO::NetworkOutAbortImageCaptureEvent lAbortEvent;
        lAbortEvent.mbCapture = true;
        mpNetworkModule->GetNetworkEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAbortEvent),
                                                          lAbortEvent.GetEventType(), sizeof(lAbortEvent));

        mpNetworkModule->GetNetworkManager()->PackTextureAndSendDisplayEventToGui(nullptr, -1);

        mbBroadcastCurrentImage    = false;
        mRoadRuleBeatenID          = 0;
        mTakedownAggressorPlayerID = -1;
        mTakedownVictimPlayerID    = -1;
        meState                    = E_IMAGE_MANAGER_STATE_COUNT;
        meImageTypeToSend          = BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_COUNT;
    }

    void NetworkImageManager::AbortMugshotShow()
    {
        for ( s32 liIndex = 0; liIndex < KI_MAX_MUGSHOT_PLAYERS; ++liIndex )
            maMugshotData[liIndex].miNumberOfPacketsToSend = 0;

        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetNetworkEventQueue(), "mpNetworkModule->GetNetworkEventQueue()");

        BrnNetworkModuleIO::NetworkOutAbortImageCaptureEvent lAbortEvent;
        lAbortEvent.mbCapture = false;
        mpNetworkModule->GetNetworkEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAbortEvent),
                                                          lAbortEvent.GetEventType(), sizeof(lAbortEvent));

        mpNetworkModule->GetNetworkManager()->PackTextureAndSendDisplayEventToGui(nullptr, -1);

        mbBroadcastCurrentImage    = false;
        mRoadRuleBeatenID          = 0;
        mTakedownAggressorPlayerID = -1;
        mTakedownVictimPlayerID    = -1;
        meState                    = E_IMAGE_MANAGER_STATE_COUNT;
        meImageTypeToSend          = BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_COUNT;
    }

    // ----------------------------------------------------------------------------------
    // HandleReceivedCameraPic
    //   A picture from lSenderID finished arriving in lpMugshotData: record the sender, then mark
    //   the picture valid (and a photo finish when it is a victory mugshot) unless the sender's
    //   mugshots are disabled, in which case both flags are cleared.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::HandleReceivedCameraPic(NetworkPlayerID lSenderID, MugshotData* lpMugshotData,
                                                      BrnGameState::GameStateModuleIO::EImageType leImageType)
    {
        const bool lbMugshotsDisabled = AreMugshotsDisabledForPlayer(lSenderID);
        lpMugshotData->mTakedownVictimPlayerID = lSenderID;

        if ( lbMugshotsDisabled )
        {
            lpMugshotData->mbPictureValid     = false;
            lpMugshotData->mbPhotoFinishValid = false;
        }
        else
        {
            lpMugshotData->mbPictureValid     = true;
            lpMugshotData->mbPhotoFinishValid =
                (leImageType == BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_VICTORY_MUGSHOT);
        }
    }

    // ----------------------------------------------------------------------------------
    // _ImageMessageDeliveredCallback
    //   Delivery of an ImageMessage needs no bookkeeping. When the second flag is raised the
    //   console writes "WARNING: Fack Nack found in
    //   BrnNetwork::NetworkImageManager::_ImageMessageDeliveredCallback" to the network dev-log
    //   stream, which has no home in this tree.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::_ImageMessageDeliveredCallback(bool /*lbDelivered*/, bool /*lbWasReliable*/,
                                                             CgsNetwork::SignalMessage* /*lpMessage*/,
                                                             NetworkPlayerID /*liToPlayerID*/,
                                                             void* /*lpUserData*/)
    {
    }

    // ==================================================================================
    // Not bodied here (declared in the header, FLAG: declaration-only): ProcessBeforeSimulation,
    // GetPhotoFinishImageByRoundWinner, SendNextSegment, ProcessDirtyTrickEvents,
    // HandleMugshotEvent, SendMugshotPicture, BroadcastImage, ReceiveImageMessage,
    // HandleShowingMugshot, GetCompressedTexture, RequestMugshotSave,
    // AreMugshotsDisabledForPlayer and the two compress callbacks. Each walks the camera /
    // gamer-picture / manager internals or the compress-segment-send pipeline and needs its own
    // reconstruction pass.
    // ==================================================================================

    // ----------------------------------------------------------------------------------
    // GetImageMessageDataEntry  @ 0x8254A8B8  (DWARF spells the X360 callee GetImageMes)
    //   Linear-scan the in-flight ImageMessage send/recv table for the slot keyed by lPlayerID
    //   (mPlayerID, the entry's first field). Passing -1 finds the first free slot. Asserts +
    //   returns the (possibly null) entry on miss.
    // ----------------------------------------------------------------------------------
    NetworkImageManager::ImageMessageData* NetworkImageManager::GetImageMessageDataEntry(NetworkPlayerID lPlayerID)
    {
        ImageMessageData* lpEntry = nullptr;

        for ( s32 liIndex = 0; liIndex < KI_MAX_IMAGE_PLAYERS; ++liIndex )
        {
            if ( maImageData[liIndex].mPlayerID == lPlayerID )
            {
                lpEntry = &maImageData[liIndex];
                if ( lpEntry )
                    return lpEntry;
                break;
            }
        }

        CGS_ASSERT(lpEntry, "lpEntry");
        return lpEntry;
    }

    // ----------------------------------------------------------------------------------
    // GetMugshotDataEntry  @ 0x8254A940  (DWARF spells the X360 callee GetMugshotDataEn)
    //   Linear-scan the per-player mugshot table for the slot keyed by aggressor id
    //   (mTakedownAggressorPlayerID). Passing -1 finds the first free slot. Asserts + returns the
    //   (possibly null) entry on miss.
    // ----------------------------------------------------------------------------------
    NetworkImageManager::MugshotData* NetworkImageManager::GetMugshotDataEntry(NetworkPlayerID lPlayerID)
    {
        MugshotData* lpEntry = nullptr;

        for ( s32 liIndex = 0; liIndex < KI_MAX_MUGSHOT_PLAYERS; ++liIndex )
        {
            if ( maMugshotData[liIndex].mTakedownAggressorPlayerID == lPlayerID )
            {
                lpEntry = &maMugshotData[liIndex];
                if ( lpEntry )
                    return lpEntry;
                break;
            }
        }

        CGS_ASSERT(lpEntry, "lpEntry");
        return lpEntry;
    }

    // ----------------------------------------------------------------------------------
    // CheckMugshotPrivilege  @ 0x8254A9C8
    //   Resolve the local player's communications privilege for mugshot exchange. Reads the
    //   signed-in local controller index off the network manager, then queries the Xbox-LIVE
    //   communications privilege (XPRIVILEGE_COMMUNICATIONS == 247, then the friends-only variant
    //   246). If communications are allowed at all, the answer is taken from the camera manager's
    //   published privilege field (E_MUGSHOT_PRIVILEGE_FRIENDS when that field reads 2, else
    //   E_MUGSHOT_PRIVILEGE_ANYONE). Otherwise it is FRIENDS when the restricted query resolved a
    //   friends-only grant (pfResult == 1) and NOONE when it is fully blocked.
    // ----------------------------------------------------------------------------------
    NetworkImageManager::EMugshotPrivilege NetworkImageManager::CheckMugshotPrivilege()
    {
        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager(), "mpNetworkModule->GetNetworkManager()");

        const s32 liActiveUserIndex = mpNetworkModule->GetNetworkManager()->GetLocalUserControllerPort();
        CGS_ASSERT(liActiveUserIndex >= 0, "liActiveUserIndex >= 0");
        CGS_ASSERT(liActiveUserIndex < static_cast<s32>(CgsInput::KU_NUMBER_OF_PADS),
                   "liActiveUserIndex < CgsInput::KU_NUMBER_OF_PADS");

        u32 lu32PrivilegeResult[16];   // X360 [sp+50h] pfResult BYREF (only [0] is read)

        EMugshotPrivilege lePrivilege = E_MUGSHOT_PRIVILEGE_ANYONE;
        if ( XUserCheckPrivilege(liActiveUserIndex, KU_XPRIVILEGE_COMMUNICATIONS, lu32PrivilegeResult)
             || lu32PrivilegeResult[0] == 1
             || XUserCheckPrivilege(liActiveUserIndex, KU_XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY, lu32PrivilegeResult) )
        {
            CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
            CGS_ASSERT(mpNetworkModule->GetNetworkManager(), "mpNetworkModule->GetNetworkManager()");

            // The camera's user setting decides: friends-only restricts mugshots to friends.
            CameraX360* lpCamera = mpNetworkModule->GetNetworkManager()->GetCamera();
            CGS_ASSERT(lpCamera, "lpCamera");

            lePrivilege = (lpCamera->GetUserSetting() == KI_CAMERA_USER_FRIENDS_ONLY)
                              ? E_MUGSHOT_PRIVILEGE_FRIENDS : E_MUGSHOT_PRIVILEGE_ANYONE;
        }
        else
        {
            // X360 0x8254AB78: ((cntlzw(pfResult[0] - 1) & 0x20) == 0) + 1
            //   pfResult[0] == 1 -> FRIENDS (1); anything else -> NOONE (2).
            lePrivilege = static_cast<EMugshotPrivilege>((lu32PrivilegeResult[0] != 1) + 1);
        }

        return lePrivilege;
    }
} // namespace BrnNetwork
