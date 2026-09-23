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
//   BODIED: every console function of the class
//           (HandlePlayerStoppedMode, EnableMugshotOutput and GetImageDataToSend are header
//           inlines; IsThereEnoughBandwidthToSend is an inline in this file).
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
#include "GameSource/GameState/BrnCgsPlayerName.h"                      // CgsNetwork::PlayerName
#include "GameShared/GameClasses/Development/CgsStrStream.h"           // CgsDev::StrStream (streamed assert messages)
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"        // TimeManager::GetU16FrameCount
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceGamesX360.h" // GetPlayerXUIDByID
#include "GameShared/GameClasses/Network/Players/X360/CgsUniquePlayerIDX360.h" // NetworkOutMugshotToSaveEvent::mUniquePlayerID
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h"           // PlayerParams (GetUniqueIDByPlayerID)

#include <cstring>                                                      // std::memcpy

// Vendor XDK privilege query used by CheckMugshotPrivilege. Declared extern "C" at file
// scope (the real prototype lives in the Xbox 360 XDK), mirroring the corpus convention.
extern "C" { int XUserCheckPrivilege(u32 luUserIndex, u32 luPrivilegeType, u32* lpbResult); }
// Vendor XDK mute-list query used by AreMugshotsDisabledForPlayer (same convention).
extern "C" { u32 XUserMuteListQuery(u32 luUserIndex, u64 luXuidRemoteTalker, s32* lpbOnMuteList); }

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

    // Image types that fall back to the gamer picture when the camera has no picture (only
    // the road-rule time mugshot does).
    static const bool KAB_IMAGE_TRANSMITS_GAMERPIC[BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_COUNT] =
    {
        false,  // E_IMAGE_TYPE_FREEBURN_MUGSHOT
        false,  // E_IMAGE_TYPE_MUGSHOT
        false,  // E_IMAGE_TYPE_PAYBACK_MUGSHOT
        true,   // E_IMAGE_TYPE_ROAD_RULE_TIME_MUGSHOT
        false,  // E_IMAGE_TYPE_ROAD_RULE_CRASH_MUGSHOT
        false,  // E_IMAGE_TYPE_VICTORY_MUGSHOT
    };

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
                static_cast<s32>(sizeof(ImageMessage)),
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

    // ----------------------------------------------------------------------------------
    // ProcessBeforeSimulation
    //   Run one step of the capture/show state machine, publish the takedown list when it was
    //   asked for, then give the debug component its per-frame hook.
    //     CAPTURE_MUGSHOT:       turn the game camera on and preview the local camera picture
    //                            (or, for image types that fall back to it, the gamer picture).
    //     TAKE_MUGSHOT:          compress the camera picture into the aggressor's slot, or send
    //                            the gamer picture straight away when there is no camera.
    //     WAIT_COMPRESS_MUGSHOT: keep previewing the camera picture until the compress lands.
    //     SHOW_MY_MUGSHOT / SHOW_MUGSHOT: HandleShowingMugshot.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::ProcessBeforeSimulation(BrnNetworkModuleIO::OutputBuffer* lpOutput)
    {
        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");

        switch ( meState )
        {
        case E_IMAGE_MANAGER_STATE_CAPTURE_MUGSHOT:
        {
            CGS_ASSERT(mpNetworkModule->GetNetworkManager(), "mpNetworkModule->GetNetworkManager()");
            CameraX360* lpCamera = mpNetworkModule->GetNetworkManager()->GetCamera();
            GamerPictureManagerX360* lpGamerPicManager = mpNetworkModule->GetNetworkManager()->GetGamerPictureManager();
            CGS_ASSERT(lpCamera, "lpCamera");
            CGS_ASSERT(lpGamerPicManager, "lpGamerPicManager");

            lpCamera->SetGameEnabled(true);

            CGS_ASSERT(mpPlayerManager, "mpPlayerManager");
            NetworkPlayerID lLocalPlayerID;
            if ( mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID) )
            {
                const CgsNetwork::NetworkTexture* lpTexture = lpCamera->GetCameraPicture(lLocalPlayerID);
                if ( lpTexture == nullptr )
                {
                    CGS_ASSERT(meImageTypeToSend != BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_COUNT,
                               "meImageTypeToSend != GsmIO::E_IMAGE_TYPE_COUNT");
                    if ( KAB_IMAGE_TRANSMITS_GAMERPIC[meImageTypeToSend] )
                    {
                        lpTexture = lpGamerPicManager->GetGamerPictureTexture(lLocalPlayerID);
                    }
                    else
                    {
                        mbAbortCaptureThisFrame = true;
                    }
                }
                mpNetworkModule->GetNetworkManager()->PackTextureAndSendDisplayEventToGui(lpTexture, 0);
            }
            else
            {
                mbAbortCaptureThisFrame = true;
            }
            break;
        }

        case E_IMAGE_MANAGER_STATE_TAKE_MUGSHOT:
        {
            CGS_ASSERT(mpNetworkModule->GetNetworkManager(), "mpNetworkModule->GetNetworkManager()");
            CameraX360* lpCamera = mpNetworkModule->GetNetworkManager()->GetCamera();
            GamerPictureManagerX360* lpGamerPicManager = mpNetworkModule->GetNetworkManager()->GetGamerPictureManager();
            CGS_ASSERT(lpCamera, "lpCamera");
            CGS_ASSERT(lpGamerPicManager, "lpGamerPicManager");

            lpCamera->SetGameEnabled(true);

            CGS_ASSERT(mpPlayerManager, "mpPlayerManager");
            NetworkPlayerID lLocalPlayerID;
            mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);
            CGS_ASSERT(CgsNetwork::K_INVALID_PLAYER_ID != lLocalPlayerID,
                       "CgsNetwork::K_INVALID_PLAYER_ID != lLocalPlayerID");
            CGS_ASSERT(meImageTypeToSend != BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_COUNT,
                       "meImageTypeToSend != GsmIO::E_IMAGE_TYPE_COUNT");

            const CgsNetwork::NetworkTexture* lpTexture = lpCamera->GetCameraPicture(lLocalPlayerID);
            if ( lpTexture != nullptr )
            {
                CGS_ASSERT(mTakedownAggressorPlayerID != -1,
                           "mTakedownAggressorPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
                MugshotData* lpMugshotData = GetMugshotDataEntry(mTakedownAggressorPlayerID);
                CGS_ASSERT(lpMugshotData, "lpMugshotData");

                GetCompressedTexture(&lpMugshotData->mPicture);
                meState = E_IMAGE_MANAGER_STATE_WAIT_COMPRESS_MUGSHOT;
            }
            else if ( KAB_IMAGE_TRANSMITS_GAMERPIC[meImageTypeToSend] && !mbBroadcastCurrentImage )
            {
                CGS_ASSERT(mTakedownAggressorPlayerID != -1,
                           "mTakedownAggressorPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
                SendMugshotPicture(mTakedownAggressorPlayerID, mTakedownAggressorPlayerID, lLocalPlayerID);
                meState = E_IMAGE_MANAGER_STATE_SHOW_MY_MUGSHOT;
            }
            else
            {
                mbAbortCaptureThisFrame = true;
            }
            break;
        }

        case E_IMAGE_MANAGER_STATE_WAIT_COMPRESS_MUGSHOT:
        {
            CGS_ASSERT(mpNetworkModule->GetNetworkManager(), "mpNetworkModule->GetNetworkManager()");
            CameraX360* lpCamera = mpNetworkModule->GetNetworkManager()->GetCamera();
            CGS_ASSERT(lpCamera, "lpCamera");

            CGS_ASSERT(mpPlayerManager, "mpPlayerManager");
            NetworkPlayerID lLocalPlayerID;
            mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);
            CGS_ASSERT(CgsNetwork::K_INVALID_PLAYER_ID != lLocalPlayerID,
                       "CgsNetwork::K_INVALID_PLAYER_ID != lLocalPlayerID");

            const CgsNetwork::NetworkTexture* lpTexture = lpCamera->GetCameraPicture(lLocalPlayerID);
            mpNetworkModule->GetNetworkManager()->PackTextureAndSendDisplayEventToGui(lpTexture, 0);
            break;
        }

        case E_IMAGE_MANAGER_STATE_SHOW_MY_MUGSHOT:
            HandleShowingMugshot(true);
            break;

        case E_IMAGE_MANAGER_STATE_SHOW_MUGSHOT:
            HandleShowingMugshot(false);
            break;

        default:
            break;
        }

        if ( mbOutputMugshotData )
        {
            OutputMugshotData(lpOutput);
            mbOutputMugshotData = false;
        }

        mDebugComponent.PreWorldUpdate();
    }

    // ----------------------------------------------------------------------------------
    // GetPhotoFinishImageByRoundWinner
    //   Online only. Hand back the round winner's photo-finish picture when that slot holds
    //   one. While a capture or show is running it is checked against the photo finish: a
    //   capture that is not the local winner's victory mugshot, or any mugshot being shown, is
    //   aborted next frame. *lpbAlreadyShowingPhotoFinish reports that the machine is busy.
    //   The console also writes "[IMAGE MGR]: Aborting photo finish capture ..." / "... show
    //   ..." to the network dev-log stream on each abort; that stream has no home in this tree.
    // ----------------------------------------------------------------------------------
    CgsNetwork::NetworkTexture* NetworkImageManager::GetPhotoFinishImageByRoundWinner(NetworkPlayerID lRoundWinnerID,
                                                                                      bool* lpbAlreadyShowingPhotoFinish)
    {
        if ( !mpNetworkModule->GetNetworkManager()->GetServerInterface()->GetConnectionComponent()->IsLoggedIn() )
            return nullptr;

        MugshotData* lpMugshotDataEntry = GetMugshotDataEntry(lRoundWinnerID);
        CGS_ASSERT(lpMugshotDataEntry, "lpMugshotDataEntry");

        if ( meState != E_IMAGE_MANAGER_STATE_COUNT )
        {
            CGS_ASSERT(mpPlayerManager, "mpPlayerManager");
            NetworkPlayerID lLocalPlayerID;
            mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);
            CGS_ASSERT(lLocalPlayerID != -1, "lLocalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

            if ( lLocalPlayerID != lRoundWinnerID ||
                 meImageTypeToSend != BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_VICTORY_MUGSHOT )
            {
                mbAbortCaptureThisFrame = true;
            }

            if ( meState == E_IMAGE_MANAGER_STATE_SHOW_MUGSHOT )
            {
                mbAbortShowThisFrame = true;
            }
        }

        *lpbAlreadyShowingPhotoFinish = ( meState != E_IMAGE_MANAGER_STATE_COUNT );

        if ( lpMugshotDataEntry == nullptr ||
             lpMugshotDataEntry->mTakedownVictimPlayerID == -1 ||
             !lpMugshotDataEntry->mbPhotoFinishValid )
        {
            return nullptr;
        }

        return &lpMugshotDataEntry->mPicture;
    }

    // ----------------------------------------------------------------------------------
    // AreMugshotsDisabledForPlayer
    //   A remote player's mugshots are hidden when the local user has muted or blocked them;
    //   otherwise the local communications privilege decides: anyone, friends only (the
    //   player must be a full buddy) or no one.
    // ----------------------------------------------------------------------------------
    bool NetworkImageManager::AreMugshotsDisabledForPlayer(NetworkPlayerID lRemotePlayerID)
    {
        bool lbDisabled = false;

        const s32 liControllerPort = mpNetworkModule->GetNetworkManager()->GetLocalUserControllerPort();
        u64 lu64XUID;
        static_cast<CgsNetwork::ServerInterfaceGamesX360*>(
            mpNetworkModule->GetNetworkManager()->GetServerInterface()->GetGameComponent())
            ->GetPlayerXUIDByID(lRemotePlayerID, &lu64XUID);

        s32 liRestricted;
        XUserMuteListQuery(static_cast<u32>(liControllerPort), lu64XUID, &liRestricted);

        if ( liRestricted == 1 )
        {
            lbDisabled = true;
        }
        else
        {
            CGS_ASSERT(lRemotePlayerID != -1, "lRemotePlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
            CGS_ASSERT(mpPlayerManager, "mpPlayerManager");

            CgsNetwork::PlayerMenuData* lpPlayerMenuData = mpPlayerManager->GetMenuDataByID(lRemotePlayerID);
            CGS_ASSERT(lpPlayerMenuData, "lpPlayerMenuData");

            CgsNetwork::PlayerName lPlayerName;
            lPlayerName.Construct(lpPlayerMenuData->macName);

            BuddyManagerX360* lpBuddyManager = mpNetworkModule->GetNetworkManager()->GetBuddyManager();
            CGS_ASSERT(lpBuddyManager, "lpBuddyManager");

            if ( lpBuddyManager->IsBuddyBlocked(&lPlayerName) )
            {
                lbDisabled = true;
            }
            else
            {
                const EMugshotPrivilege leMugshotPrivilege = CheckMugshotPrivilege();
                switch ( leMugshotPrivilege )
                {
                case E_MUGSHOT_PRIVILEGE_ANYONE:
                    lbDisabled = false;
                    break;

                case E_MUGSHOT_PRIVILEGE_FRIENDS:
                    lbDisabled = !lpBuddyManager->IsFullBuddy(&lPlayerName);
                    break;

                case E_MUGSHOT_PRIVILEGE_NOONE:
                    lbDisabled = true;
                    break;

                default:
                {
                    CgsDev::Assert::BeginAssert();
                    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                    CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                    lStrStream << "Unknown mugshot privilege: " << static_cast<s32>(leMugshotPrivilege);
                    CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                    CgsDev::Assert::EndAssert();
                    break;
                }
                }
            }
        }

        return lbDisabled;
    }

    // ----------------------------------------------------------------------------------
    // HandleMugshotEvent
    //   The game side drives the capture/show state machine with one response per event. The
    //   game camera is only wanted while a capture or take is in progress.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::HandleMugshotEvent(const BrnNetworkModuleIO::NetworkInPaybackMugshotEvent* lpMugshotEvent)
    {
        CGS_ASSERT(lpMugshotEvent, "lpMugshotEvent");

        const bool lbGameCameraEnabled =
            lpMugshotEvent->meMugshotResponse == BrnGameState::GameStateModuleIO::E_MUGSHOT_RESPONSE_CAPTURE ||
            lpMugshotEvent->meMugshotResponse == BrnGameState::GameStateModuleIO::E_MUGSHOT_RESPONSE_TAKE;
        mpNetworkModule->GetNetworkManager()->GetCamera()->SetGameEnabled(lbGameCameraEnabled);

        switch ( lpMugshotEvent->meMugshotResponse )
        {
        case BrnGameState::GameStateModuleIO::E_MUGSHOT_RESPONSE_PREPARE_CAPTURE:
            if ( !lpMugshotEvent->mbMugshotRequiresBroadcast )
            {
                MugshotData* lpMugshotData =
                    GetMugshotDataEntry(mpNetworkModule->GetNetworkPlayerID(lpMugshotEvent->meTakedownAggressorIndex));
                CGS_ASSERT(lpMugshotData, "lpMugshotData");

                lpMugshotData->mTakedownVictimPlayerID =
                    mpNetworkModule->GetNetworkPlayerID(lpMugshotEvent->meTakedownVictimIndex);
                CGS_ASSERT(lpMugshotData->mTakedownVictimPlayerID != -1,
                           "lpMugshotData->mTakedownVictimPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
            }
            mbBroadcastCurrentImage = lpMugshotEvent->mbMugshotRequiresBroadcast;
            mRoadRuleBeatenID       = lpMugshotEvent->mRoadRuleBeatenRoadID;
            break;

        case BrnGameState::GameStateModuleIO::E_MUGSHOT_RESPONSE_CAPTURE:
            if ( !lpMugshotEvent->mbIsTakedownAggressorLocalPlayer )
            {
                CGS_ASSERT(lpMugshotEvent->meTakedownAggressorIndex != -1,
                           "lpMugshotEvent->meTakedownAggressorIndex != CgsNetwork::K_INVALID_PLAYER_ID");
                CGS_ASSERT(lpMugshotEvent->meMugshotType != BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_COUNT,
                           "lpMugshotEvent->meMugshotType != GsmIO::E_IMAGE_TYPE_COUNT");

                mTakedownAggressorPlayerID = mpNetworkModule->GetNetworkPlayerID(lpMugshotEvent->meTakedownAggressorIndex);
                meState                    = E_IMAGE_MANAGER_STATE_CAPTURE_MUGSHOT;
                meImageTypeToSend          = lpMugshotEvent->meMugshotType;
            }
            break;

        case BrnGameState::GameStateModuleIO::E_MUGSHOT_RESPONSE_TAKE:
            CGS_ASSERT(mTakedownAggressorPlayerID != -1,
                       "mTakedownAggressorPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
            meState = E_IMAGE_MANAGER_STATE_TAKE_MUGSHOT;
            break;

        case BrnGameState::GameStateModuleIO::E_MUGSHOT_RESPONSE_SHOW:
            if ( meState == E_IMAGE_MANAGER_STATE_COUNT )
            {
                CGS_ASSERT(mTakedownVictimPlayerID != -1,
                           "mTakedownVictimPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
                if ( mbMugshotsEnabled )
                {
                    meState = E_IMAGE_MANAGER_STATE_SHOW_MUGSHOT;
                }
                else
                {
                    mbAbortShowThisFrame = true;
                }
            }
            break;

        case BrnGameState::GameStateModuleIO::E_MUGSHOT_RESPONSE_STOP_MY_MUGSHOT:
            CGS_ASSERT(E_IMAGE_MANAGER_STATE_SHOW_MY_MUGSHOT == meState,
                       "E_IMAGE_MANAGER_STATE_SHOW_MY_MUGSHOT == meState");

            mpNetworkModule->GetNetworkManager()->PackTextureAndSendDisplayEventToGui(nullptr, -1);

            mbBroadcastCurrentImage    = false;
            mRoadRuleBeatenID          = 0;
            mTakedownAggressorPlayerID = -1;
            mTakedownVictimPlayerID    = -1;
            meState                    = E_IMAGE_MANAGER_STATE_COUNT;
            for ( s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MUGSHOT_PLAYERS; ++liPlayerIndex )
                maMugshotData[liPlayerIndex].miNumberOfPacketsToSend = 0;
            break;

        case BrnGameState::GameStateModuleIO::E_MUGSHOT_RESPONSE_STOP_THEIR_MUGSHOT:
            if ( meState == E_IMAGE_MANAGER_STATE_SHOW_MUGSHOT || meState == E_IMAGE_MANAGER_STATE_COUNT )
            {
                mpNetworkModule->GetNetworkManager()->PackTextureAndSendDisplayEventToGui(nullptr, -1);

                mbBroadcastCurrentImage    = false;
                mRoadRuleBeatenID          = 0;
                mTakedownAggressorPlayerID = -1;
                mTakedownVictimPlayerID    = -1;
                meState                    = E_IMAGE_MANAGER_STATE_COUNT;
                for ( s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MUGSHOT_PLAYERS; ++liPlayerIndex )
                    maMugshotData[liPlayerIndex].miNumberOfPacketsToSend = 0;
            }
            break;

        default:
        {
            CgsDev::Assert::BeginAssert();
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Unexpected mugshot response: " << static_cast<s32>(lpMugshotEvent->meMugshotResponse);
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            break;
        }
        }
    }

    // ----------------------------------------------------------------------------------
    // ProcessDirtyTrickEvents
    //   For every dirty trick the game reported this frame that is still live, record its
    //   aggressor as the victim id of the slot keyed by its victim.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::ProcessDirtyTrickEvents()
    {
        const BrnNetworkModuleIO::GameStateToNetworkInterface::DirtyTrickQueue* lpDirtyTrickEventQueue =
            mpNetworkModule->GetGameStateToNetworkInterface()->GetDirtyTrickQueue();

        for ( s32 liIndex = 0; liIndex < lpDirtyTrickEventQueue->GetLength(); ++liIndex )
        {
            const BrnNetworkModuleIO::DirtyTrickEvent lDirtyTrickEvent = lpDirtyTrickEventQueue->GetEvent(liIndex);
            if ( lDirtyTrickEvent.meDirtyTrickStatus == E_DIRTY_TRICK_NONE )
            {
                MugshotData* lpMugshotData =
                    GetMugshotDataEntry(mpNetworkModule->GetNetworkPlayerID(lDirtyTrickEvent.meVictimActiveRaceCarIndex));
                CGS_ASSERT(lpMugshotData, "lpMugshotData");

                lpMugshotData->mTakedownVictimPlayerID =
                    mpNetworkModule->GetNetworkPlayerID(lDirtyTrickEvent.meAggressorActiveRaceCarIndex);
            }
        }
    }

    // ----------------------------------------------------------------------------------
    // SendNextSegment
    //   Pick the first slot still owed picture segments and, once its previous segment has
    //   gone out, send it the next KI_PHOTO_SEGMENT_SIZE bytes of the aggressor slot's picture.
    //   When the last slot has been served the image type to send is cleared.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::SendNextSegment()
    {
        MugshotData* lpRemoteMugshotData = GetImageDataToSend();
        if ( lpRemoteMugshotData == nullptr )
            return;

        CGS_ASSERT(mTakedownAggressorPlayerID != -1,
                   "mTakedownAggressorPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
        MugshotData* lpAggressorMugshotData = GetMugshotDataEntry(mTakedownAggressorPlayerID);
        CGS_ASSERT(lpAggressorMugshotData, "lpAggressorMugshotData");

        const NetworkPlayerID lRemotePlayerID = lpRemoteMugshotData->mTakedownAggressorPlayerID;
        CGS_ASSERT(mpPlayerManager, "mpPlayerManager");
        CGS_ASSERT(!mpPlayerManager->IsLocalPlayer( lRemotePlayerID ),
                   "!mpPlayerManager->IsLocalPlayer( lRemotePlayerID )");

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lRemotePlayerID);
        CGS_ASSERT(lpNetworkPlayer, "lpNetworkPlayer");

        ImageMessage* lpImageMessage =
            static_cast<ImageMessage*>(lpNetworkPlayer->GetRegisteredSendMessage(KI_IMAGE_MESSAGE_TYPE));
        CGS_ASSERT(lpImageMessage, "lpImageMessage");

        // The previous segment is still queued: wait for it to go.
        if ( lpImageMessage->IsMessageValid() )
            return;

        char lacBuffer[ImageMessage::KI_PHOTO_SEGMENT_SIZE];
        const s32 liBytesSent      = lpRemoteMugshotData->miLastPacketSent * ImageMessage::KI_PHOTO_SEGMENT_SIZE;
        const s32 liBytesRemaining = lpAggressorMugshotData->mPicture.GetTextureSize() - liBytesSent;
        const s32 liBytesToWrite   = ( liBytesRemaining < ImageMessage::KI_PHOTO_SEGMENT_SIZE )
                                         ? liBytesRemaining : ImageMessage::KI_PHOTO_SEGMENT_SIZE;
        CGS_ASSERT(liBytesToWrite > 0, "liBytesToWrite > 0");
        CGS_ASSERT(liBytesToWrite <= ImageMessage::KI_PHOTO_SEGMENT_SIZE,
                   "liBytesToWrite <= ImageMessage::KI_PHOTO_SEGMENT_SIZE");

        const char* lpcReadPosition = lpAggressorMugshotData->mPicture.GetTexture() + liBytesSent;
        std::memcpy(lacBuffer, lpcReadPosition, liBytesToWrite);

        CGS_ASSERT(lpAggressorMugshotData->mTakedownAggressorPlayerID != -1,
                   "lpAggressorMugshotData->mTakedownAggressorPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
        CGS_ASSERT(lpAggressorMugshotData->mTakedownVictimPlayerID != -1,
                   "lpAggressorMugshotData->mTakedownVictimPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        lpImageMessage->PrepareForSend(mpTimeManager->GetU16FrameCount(),
                                       lpAggressorMugshotData->mTakedownVictimPlayerID,
                                       lpAggressorMugshotData->mTakedownAggressorPlayerID,
                                       meImageTypeToSend,
                                       mRoadRuleBeatenID,
                                       static_cast<u16>(lpRemoteMugshotData->miLastPacketSent),
                                       static_cast<u16>(lpRemoteMugshotData->miNumberOfPacketsToSend),
                                       static_cast<u16>(liBytesToWrite),
                                       lacBuffer);

        ++lpRemoteMugshotData->miLastPacketSent;
        if ( lpRemoteMugshotData->miLastPacketSent == lpRemoteMugshotData->miNumberOfPacketsToSend )
        {
            lpRemoteMugshotData->miNumberOfPacketsToSend = 0;
            if ( GetImageDataToSend() == nullptr )
                meImageTypeToSend = BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_COUNT;
        }
    }

    // ----------------------------------------------------------------------------------
    // IsThereEnoughBandwidthToSend
    //   Mugshots only go out while the reliable-message send buffer holds fewer than
    //   KI_MAX_BUFFERED_RELIABLE_MESSAGES_TO_SEND_MUGSHOT messages.
    // ----------------------------------------------------------------------------------
    inline bool NetworkImageManager::IsThereEnoughBandwidthToSend()
    {
        return mpPlayerManager->mReliableMessageManager.miNumBufferedReliableMessages
               < KI_MAX_BUFFERED_RELIABLE_MESSAGES_TO_SEND_MUGSHOT;
    }

    // ----------------------------------------------------------------------------------
    // SendMugshotPicture
    //   Start sending the victim's picture (held in the aggressor's slot) to lRemotePlayerID
    //   segment by segment. When there is no picture, the remote player has mugshots off or
    //   the reliable channel is too busy, image types that fall back to the gamer picture send
    //   one empty image message instead. Either way the image type is consumed.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::SendMugshotPicture(NetworkPlayerID lRemotePlayerID,
                                                 NetworkPlayerID lTakedownAggressorPlayerID,
                                                 NetworkPlayerID lTakedownVictimPlayerID)
    {
        CGS_ASSERT(mpPlayerManager, "mpPlayerManager");
        CGS_ASSERT(lRemotePlayerID != -1, "lRemotePlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(lRemotePlayerID);
        CGS_ASSERT(lpNetworkPlayer, "lpNetworkPlayer");

        ImageMessage* lpImageMessage =
            static_cast<ImageMessage*>(lpNetworkPlayer->GetRegisteredSendMessage(KI_IMAGE_MESSAGE_TYPE));
        CGS_ASSERT(lpImageMessage, "lpImageMessage");

        const u16 lu16CurrentFrame = mpTimeManager->GetU16FrameCount();

        MugshotData* lpAggressorMugshotData = GetMugshotDataEntry(lTakedownAggressorPlayerID);
        MugshotData* lpRemoteMugshotData    = GetMugshotDataEntry(lRemotePlayerID);

        if ( lpAggressorMugshotData->mbPictureValid &&
             !AreMugshotsDisabledForPlayer(lRemotePlayerID) &&
             IsThereEnoughBandwidthToSend() )
        {
            lpRemoteMugshotData->miLastPacketSent        = 0;
            lpRemoteMugshotData->miNumberOfPacketsToSend =
                ( lpAggressorMugshotData->mPicture.GetTextureSize() + ImageMessage::KI_PHOTO_SEGMENT_SIZE - 1 )
                / ImageMessage::KI_PHOTO_SEGMENT_SIZE;

            CGS_ASSERT(mpNetworkModule->GetNetworkManager(), "mpNetworkModule->GetNetworkManager()");
            mpNetworkModule->GetNetworkManager()->CaptureTelemetryEvent(E_TELEMETRY_MUGSHOT_PHOTO, nullptr);

            BrnNetworkModuleIO::NetworkOutSentMugshot lMugshotSentEvent;
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lMugshotSentEvent),
                                                              lMugshotSentEvent.GetEventType(), sizeof(lMugshotSentEvent));
            return;
        }

        CGS_ASSERT(meImageTypeToSend != BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_COUNT,
                   "meImageTypeToSend != GsmIO::E_IMAGE_TYPE_COUNT");
        if ( KAB_IMAGE_TRANSMITS_GAMERPIC[meImageTypeToSend] )
        {
            lpImageMessage->PrepareForSend(lu16CurrentFrame, lTakedownVictimPlayerID, lTakedownAggressorPlayerID,
                                           meImageTypeToSend, mRoadRuleBeatenID, 0, 0, 0, nullptr);

            CGS_ASSERT(mpNetworkModule->GetNetworkManager(), "mpNetworkModule->GetNetworkManager()");
            mpNetworkModule->GetNetworkManager()->CaptureTelemetryEvent(E_TELEMETRY_MUGSHOT_GAMERPIC, nullptr);
        }

        meImageTypeToSend = BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_COUNT;
    }

    // ----------------------------------------------------------------------------------
    // BroadcastImage
    //   The local player's picture goes to everyone: the local player becomes the aggressor
    //   and every other slot is armed to receive the picture from segment 0, except players
    //   whose mugshots are disabled.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::BroadcastImage(MugshotData* lpMugshotData)
    {
        CGS_ASSERT(mpPlayerManager, "mpPlayerManager");
        NetworkPlayerID lLocalPlayerID;
        mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);
        CGS_ASSERT(lLocalPlayerID != -1, "lLocalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        mTakedownAggressorPlayerID = lLocalPlayerID;

        for ( s32 liIndex = 0; liIndex < KI_MAX_MUGSHOT_PLAYERS; ++liIndex )
        {
            MugshotData& lrMugshotData = maMugshotData[liIndex];
            if ( lrMugshotData.mTakedownAggressorPlayerID == lLocalPlayerID )
            {
                lrMugshotData.mTakedownVictimPlayerID = lLocalPlayerID;
            }
            else if ( lrMugshotData.mTakedownAggressorPlayerID != -1 &&
                      AreMugshotsDisabledForPlayer(lrMugshotData.mTakedownAggressorPlayerID) )
            {
                lrMugshotData.miNumberOfPacketsToSend = 0;
            }
            else
            {
                lrMugshotData.miLastPacketSent        = 0;
                lrMugshotData.miNumberOfPacketsToSend =
                    ( lpMugshotData->mPicture.GetTextureSize() + ImageMessage::KI_PHOTO_SEGMENT_SIZE - 1 )
                    / ImageMessage::KI_PHOTO_SEGMENT_SIZE;
            }
        }
    }

    // ----------------------------------------------------------------------------------
    // ReceiveImageMessage
    //   One segment of a remote picture arrived. An empty segment means the sender has no
    //   picture, and the game is told the image arrived straight away. Otherwise the segment
    //   is copied into the aggressor's slot (unless a mugshot is on screen or it belongs to a
    //   different victim). Once every segment is in, the picture is validated, saved when it
    //   is the local player's own mugshot, and announced to the game; the first segment of a
    //   picture reaching an idle manager tells the game that the sender is capturing.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::ReceiveImageMessage(NetworkPlayerID lSendingPlayerID, ImageMessage* lpImageMessageRecv)
    {
        char                                        lacPhotoBuffer[ImageMessage::KI_PHOTO_SEGMENT_SIZE];
        EActiveRaceCarIndex                         lePaybackAggressorRaceCarIndex;
        EActiveRaceCarIndex                         lePaybackVictimRaceCarIndex;
        BrnGameState::GameStateModuleIO::EImageType leReceivedImageType;
        CgsID                                       lRoadRuleBeatenID;
        u16                                         lu16PhotoPacketNumber;
        u16                                         lu16TotalNumberOfPhotoPackets;
        u16                                         lu16PhotoBytes;
        NetworkPlayerID                             lTakedownVictimPlayerID;
        NetworkPlayerID                             lTakedownAggressorPlayerID;
        NetworkPlayerID                             lLocalPlayerID;

        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
        CGS_ASSERT(lpImageMessageRecv, "lpImageMessageRecv");

        lpImageMessageRecv->Retrieve(&lTakedownVictimPlayerID, &lTakedownAggressorPlayerID, &leReceivedImageType,
                                     &lRoadRuleBeatenID, &lu16PhotoPacketNumber, &lu16TotalNumberOfPhotoPackets,
                                     &lu16PhotoBytes, lacPhotoBuffer);

        CGS_ASSERT(lTakedownAggressorPlayerID != -1,
                   "lTakedownAggressorPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
        CGS_ASSERT(lTakedownVictimPlayerID != -1,
                   "lTakedownVictimPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        MugshotData* lpMugshotData = GetMugshotDataEntry(lTakedownAggressorPlayerID);
        CGS_ASSERT(lpMugshotData, "lpMugshotData");

        CGS_ASSERT(mpPlayerManager, "mpPlayerManager");
        mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);
        CGS_ASSERT(lLocalPlayerID != -1, "lLocalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        if ( lu16PhotoBytes > 0 )
        {
            if ( meState == E_IMAGE_MANAGER_STATE_SHOW_MUGSHOT )
                return;

            if ( mTakedownVictimPlayerID != -1 && mTakedownVictimPlayerID != lTakedownVictimPlayerID )
                return;

            mTakedownVictimPlayerID = lTakedownVictimPlayerID;

            char* lpcWritePosition = lpMugshotData->mPicture.GetTexture()
                                   + lu16PhotoPacketNumber * ImageMessage::KI_PHOTO_SEGMENT_SIZE;
            std::memcpy(lpcWritePosition, lacPhotoBuffer, lu16PhotoBytes);

            lpMugshotData->mReceivedPhotoPackets.SetBit(lu16PhotoPacketNumber);

            if ( lpMugshotData->mReceivedPhotoPackets.GetFirstZeroBit() >= lu16TotalNumberOfPhotoPackets )
            {
                lpMugshotData->mReceivedPhotoPackets.UnSetAll();
                HandleReceivedCameraPic(lSendingPlayerID, lpMugshotData, leReceivedImageType);

                BrnServerInterface* lpServerInterface = mpNetworkModule->GetNetworkManager()->GetServerInterface();
                CGS_ASSERT(lpServerInterface, "lpServerInterface");

                if ( lLocalPlayerID == lpMugshotData->mTakedownAggressorPlayerID &&
                     lpMugshotData->mbPictureValid &&
                     leReceivedImageType != BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_VICTORY_MUGSHOT )
                {
                    RequestMugshotSave(lpMugshotData, lTakedownVictimPlayerID, leReceivedImageType);
                }

                lePaybackVictimRaceCarIndex    = mpNetworkModule->GetActiveRaceCarIndex(lTakedownVictimPlayerID);
                lePaybackAggressorRaceCarIndex = mpNetworkModule->GetActiveRaceCarIndex(lTakedownAggressorPlayerID);

                BrnNetworkModuleIO::NetworkOutImageReceivedEvent lImageReceivedEvent;
                lImageReceivedEvent.mRoadID                   = lRoadRuleBeatenID;
                lImageReceivedEvent.meImageSenderRaceCarIndex = lePaybackVictimRaceCarIndex;
                lImageReceivedEvent.meReceivedImageType       = leReceivedImageType;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lImageReceivedEvent),
                                                                  lImageReceivedEvent.GetEventType(), sizeof(lImageReceivedEvent));
            }
            else if ( lu16PhotoPacketNumber == 0 &&
                      meState == E_IMAGE_MANAGER_STATE_COUNT &&
                      leReceivedImageType != BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_VICTORY_MUGSHOT )
            {
                lePaybackVictimRaceCarIndex = mpNetworkModule->GetActiveRaceCarIndex(lTakedownVictimPlayerID);

                BrnNetworkModuleIO::NetworkOutCapturingTheirImageEvent lCapturingImageEvent;
                lCapturingImageEvent.mRoadId      = lRoadRuleBeatenID;
                lCapturingImageEvent.meImageType  = leReceivedImageType;
                lCapturingImageEvent.meVictimARCI = lePaybackVictimRaceCarIndex;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCapturingImageEvent),
                                                                  lCapturingImageEvent.GetEventType(), sizeof(lCapturingImageEvent));
            }
        }
        else
        {
            mTakedownVictimPlayerID = lTakedownVictimPlayerID;

            lePaybackVictimRaceCarIndex    = mpNetworkModule->GetActiveRaceCarIndex(lTakedownVictimPlayerID);
            lePaybackAggressorRaceCarIndex = mpNetworkModule->GetActiveRaceCarIndex(lTakedownAggressorPlayerID);

            BrnNetworkModuleIO::NetworkOutImageReceivedEvent lImageReceivedEvent;
            lImageReceivedEvent.mRoadID                   = lRoadRuleBeatenID;
            lImageReceivedEvent.meImageSenderRaceCarIndex = lePaybackVictimRaceCarIndex;
            lImageReceivedEvent.meReceivedImageType       = leReceivedImageType;
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lImageReceivedEvent),
                                                              lImageReceivedEvent.GetEventType(), sizeof(lImageReceivedEvent));
        }

    }

    // ----------------------------------------------------------------------------------
    // HandleShowingMugshot
    //   Put a mugshot on screen: for my own mugshot the picture in the aggressor's slot,
    //   otherwise the one received into the local player's slot. With no valid picture the
    //   matching gamer picture is shown instead (mine, or the victim's).
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::HandleShowingMugshot(bool lbShowMyMugshot)
    {
        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
        BrnNetworkManager* lpNetworkManager = mpNetworkModule->GetNetworkManager();
        CGS_ASSERT(lpNetworkManager, "lpNetworkManager");

        NetworkPlayerID lMugshotDataPlayerID;
        if ( !lbShowMyMugshot )
        {
            CGS_ASSERT(mpPlayerManager, "mpPlayerManager");
            if ( !mpPlayerManager->GetNextLocalPlayerID(&lMugshotDataPlayerID) )
            {
                mbAbortCaptureThisFrame = true;
                return;
            }
        }
        else
        {
            lMugshotDataPlayerID = mTakedownAggressorPlayerID;
            CGS_ASSERT(lMugshotDataPlayerID != -1,
                       "lMugshotDataPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
        }

        MugshotData* lpMugshotData = GetMugshotDataEntry(lMugshotDataPlayerID);
        CGS_ASSERT(lpMugshotData, "lpMugshotData");

        if ( lpMugshotData->mbPictureValid )
        {
            lpNetworkManager->PackTextureAndSendDisplayEventToGui(&lpMugshotData->mPicture, 0);
            return;
        }

        NetworkPlayerID lDisplayPictureID;
        if ( lbShowMyMugshot )
        {
            CGS_ASSERT(mpPlayerManager, "mpPlayerManager");
            mpPlayerManager->GetNextLocalPlayerID(&lDisplayPictureID);
        }
        else
        {
            lDisplayPictureID = mTakedownVictimPlayerID;
        }
        CGS_ASSERT(lDisplayPictureID != -1, "lDisplayPictureID != CgsNetwork::K_INVALID_PLAYER_ID");

        const CgsNetwork::NetworkTexture* lpGamerPicture =
            lpNetworkManager->GetGamerPictureManager()->GetGamerPictureTexture(lDisplayPictureID);
        if ( lpGamerPicture )
        {
            lpNetworkManager->PackTextureAndSendDisplayEventToGui(lpGamerPicture, 0);
        }
    }

    // ----------------------------------------------------------------------------------
    // GetCompressedTexture
    //   Queue the local camera picture for compression into lpCompressedTexture (quality 0;
    //   _GetCompressedCameraPicCallback finishes the job) and hand the uncompressed picture to
    //   the debug component.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::GetCompressedTexture(CgsNetwork::NetworkTexture* lpCompressedTexture)
    {
        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager(), "mpNetworkModule->GetNetworkManager()");
        CGS_ASSERT(mpNetworkModule->GetNetworkManager()->GetCamera(), "mpNetworkModule->GetNetworkManager()->GetCamera()");

        CameraX360* lpCamera = mpNetworkModule->GetNetworkManager()->GetCamera();
        lpCamera->GetCompressedLocalCameraPicture(lpCompressedTexture, 0,
                                                  &NetworkImageManager::_GetCompressedCameraPicCallback, this);

        CGS_ASSERT(mpPlayerManager, "mpPlayerManager");
        NetworkPlayerID lLocalPlayerID;
        mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);
        CGS_ASSERT(lLocalPlayerID != -1, "lLocalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        const CgsNetwork::NetworkTexture* lpTexture = lpCamera->GetCameraPicture(lLocalPlayerID);
        if ( lpTexture )
        {
            mDebugComponent.SetImageToEncode(lpTexture->GetTexture());
        }
    }

    // ----------------------------------------------------------------------------------
    // RequestMugshotSave
    //   Ask the game to save lpLocalMugshotData's picture to the gallery under the sender's
    //   unique id.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::RequestMugshotSave(MugshotData* lpLocalMugshotData, NetworkPlayerID lImageSenderPlayerID,
                                                 BrnGameState::GameStateModuleIO::EImageType leMugshotType)
    {
        CGS_ASSERT(lpLocalMugshotData, "lpLocalMugshotData");
        CGS_ASSERT(lImageSenderPlayerID != -1, "lImageSenderPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
        CGS_ASSERT(lpLocalMugshotData->mPicture.GetTexture() != NULL,
                   "lpLocalMugshotData->mPicture.GetTexture() != NULL");

        switch ( leMugshotType )
        {
        case BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_FREEBURN_MUGSHOT:
        case BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_MUGSHOT:
        case BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_PAYBACK_MUGSHOT:
        case BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_ROAD_RULE_TIME_MUGSHOT:
        case BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_ROAD_RULE_CRASH_MUGSHOT:
        case BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_VICTORY_MUGSHOT:
        {
            BrnNetworkModuleIO::NetworkOutMugshotToSaveEvent lMugshotToSaveEvent;

            CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
            GetUniqueIDByPlayerID(lImageSenderPlayerID, &lMugshotToSaveEvent.mUniquePlayerID);
            lMugshotToSaveEvent.mpTexture          = &lpLocalMugshotData->mPicture;
            lMugshotToSaveEvent.meImageGalleryType = leMugshotType;
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lMugshotToSaveEvent),
                                                              lMugshotToSaveEvent.GetEventType(), sizeof(lMugshotToSaveEvent));
            break;
        }

        default:
            break;
        }
    }

    // ----------------------------------------------------------------------------------
    // _GetCompressedCameraPicCallback
    //   The compressed camera picture is ready: copy it into the aggressor's slot, mark it
    //   valid (and a photo finish for a victory mugshot), then send it (to everyone for a
    //   broadcast, otherwise to the aggressor) and show it. Without a local player or an
    //   aggressor the capture is aborted next frame.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::_GetCompressedCameraPicCallback(void* lpPixels, void* lpUserData)
    {
        CGS_ASSERT(lpPixels, "lpPixels");
        CGS_ASSERT(lpUserData, "lpUserData");
        NetworkImageManager* lpImageManager = static_cast<NetworkImageManager*>(lpUserData);
        CGS_ASSERT(lpImageManager, "lpImageManager");

        MugshotData* lpMugshotData = lpImageManager->GetMugshotDataEntry(lpImageManager->mTakedownAggressorPlayerID);
        CGS_ASSERT(lpMugshotData, "lpMugshotData");

        std::memcpy(lpMugshotData->mPicture.GetTexture(), lpPixels, lpMugshotData->mPicture.GetTextureSize());
        lpMugshotData->mbPictureValid = true;
        if ( lpImageManager->meImageTypeToSend == BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_VICTORY_MUGSHOT )
        {
            lpMugshotData->mbPhotoFinishValid = true;
        }

        CgsNetwork::PlayerManager* lpPlayerManager = lpImageManager->mpPlayerManager;
        CGS_ASSERT(lpPlayerManager, "lpPlayerManager");

        NetworkPlayerID lLocalPlayerID;
        if ( lpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID) &&
             lpImageManager->mTakedownAggressorPlayerID != -1 )
        {
            const NetworkPlayerID lTakedownAggressorPlayerID = lpImageManager->mTakedownAggressorPlayerID;
            if ( lpImageManager->mbBroadcastCurrentImage )
            {
                lpImageManager->BroadcastImage(lpMugshotData);
            }
            else
            {
                lpImageManager->SendMugshotPicture(lTakedownAggressorPlayerID, lTakedownAggressorPlayerID, lLocalPlayerID);
            }
            lpImageManager->meState                 = E_IMAGE_MANAGER_STATE_SHOW_MY_MUGSHOT;
            lpImageManager->mbBroadcastCurrentImage = false;
        }
        else
        {
            lpImageManager->mbAbortCaptureThisFrame = true;
        }
    }

    // ----------------------------------------------------------------------------------
    // _GetCompressedGamerPicCallback
    //   The compressed gamer picture is ready: copy it into the local player's slot and ask
    //   for it to be saved under the slot's victim as the pending gamer-picture image type,
    //   which is then cleared.
    // ----------------------------------------------------------------------------------
    void NetworkImageManager::_GetCompressedGamerPicCallback(void* lpPixels, void* lpUserData)
    {
        CGS_ASSERT(lpPixels, "lpPixels");
        CGS_ASSERT(lpUserData, "lpUserData");
        NetworkImageManager* lpImageManager = static_cast<NetworkImageManager*>(lpUserData);
        CGS_ASSERT(lpImageManager, "lpImageManager");

        CgsNetwork::PlayerManager* lpPlayerManager = lpImageManager->mpPlayerManager;
        CGS_ASSERT(lpPlayerManager, "lpPlayerManager");

        NetworkPlayerID lLocalPlayerID;
        if ( lpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID) )
        {
            MugshotData* lpMugshotData = lpImageManager->GetMugshotDataEntry(lLocalPlayerID);
            CGS_ASSERT(lpMugshotData, "lpMugshotData");

            std::memcpy(lpMugshotData->mPicture.GetTexture(), lpPixels, lpMugshotData->mPicture.GetTextureSize());
            lpImageManager->RequestMugshotSave(lpMugshotData, lpMugshotData->mTakedownVictimPlayerID,
                                               lpImageManager->meImageTypeOfGamerPicToSave);
        }

        lpImageManager->meImageTypeOfGamerPicToSave = BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_COUNT;
    }

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

    // Build the unique id (name + XUID) of lNetworkPlayerID from its lobby player parameters.
    void NetworkImageManager::GetUniqueIDByPlayerID(NetworkPlayerID lNetworkPlayerID,
                                                    CgsNetwork::UniquePlayerIDX360* lpUniqueID)
    {
        PlayerParams lPlayerParams;

        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
        BrnNetworkManager* lpNetworkManager = mpNetworkModule->GetNetworkManager();
        CGS_ASSERT(lpNetworkManager, "lpNetworkManager");
        BrnServerInterface* lpServerInterface = lpNetworkManager->GetServerInterface();
        CGS_ASSERT(lpServerInterface, "lpServerInterface");

        lPlayerParams.Prepare();
        lpServerInterface->GetGameComponent()->GetPlayerParametersByPlayerID(lNetworkPlayerID, &lPlayerParams);
        lpUniqueID->Construct(&lPlayerParams);
    }
} // namespace BrnNetwork
