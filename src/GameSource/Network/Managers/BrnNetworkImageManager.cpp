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
// FUNCTION OWNERSHIP for this TU (30 X360 functions):
//   BODIED (11): Construct, Destruct, Prepare, Release, OnRoundStart, AddPlayer, RemovePlayer,
//                Disconnected, GetMugshotImageByAggressor, GetMugshotVictimID,
//                _ImageMessageArrivedCallback. These touch only named members + committed-by-name
//                APIs (NetworkTexture / ImageMessage / debug-component lifecycle, PlayerManager /
//                NetworkPlayer message registration, the per-player table accessors).
//   DECLARATION-ONLY + FLAGGED (19): every method that walks the un-homed internals of
//                BrnNetwork::CameraX360 / GamerPictureManagerX360 / BrnNetworkManager
//                (PackTextureAndSendDisplayEventToGui, GetCompressedLocalCameraPicture, ...), the
//                raw-offset BrnNetworkModuleIO interfaces (the +818064 / +852392 hacks), the
//                CgsModule::VariableEventQueue<14000,16> event pump, or the multi-stage
//                compress / segment / pack message pipeline. Each is left as an empty body with a
//                // FLAG: declaration-only marker; bodying them faithfully needs those un-homed
//                deps first. Fabricating a scalar paraphrase of those pipelines is forbidden.
//
// The per-player table helpers GetMugshotDataEntry / GetImageMessageDataEntry (the DWARF spells
// the X360 callees GetMugshotDataEn / GetImageMes) are NOT in this TU's X360 function set -- they
// are sibling-TU functions. They are declared in the header and CALLED by the bodied functions
// here; their bodies link from the sibling .cpp. The reliable-message delivery callback twin
// (_ImageMessageDeliveredCallback) is likewise sibling-homed and only referenced for registration.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkImageManager.h"

#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"   // CgsNetwork::PlayerManager
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"   // CgsNetwork::NetworkPlayer + RegisterMessageType
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"               // CgsMemory::HeapMalloc
#include "GameSource/Network/BrnNetworkModule.h"                        // BrnNetworkModule::GetNetworkManager
#include "GameSource/Network/BrnNetworkManager.h"                       // BrnNetworkManager::GetLocalUserControllerPort
#include "GameShared/GameClasses/System/Input/CgsInputTypes.h"         // CgsInput::KU_NUMBER_OF_PADS

// Vendor XDK privilege query used by CheckMugshotPrivilege. Declared extern "C" at file
// scope (the real prototype lives in the Xbox 360 XDK), mirroring the corpus convention.
extern "C" { int XUserCheckPrivilege(u32 luUserIndex, u32 luPrivilegeType, u32* lpbResult); }

namespace BrnNetwork
{
    // The reliable-message channel id for the ImageMessage type (X360 RegisterMessageType leType = 19).
    static const s32 KI_IMAGE_MESSAGE_TYPE = 19;
    // The X360 packed ImageMessage byte length passed to RegisterMessageType (liLength = 568).
    static const s32 KI_IMAGE_MESSAGE_PACKED_LENGTH = 568;

    // Xbox communications privilege ids (asm 0xF7 / 0xF6) and the un-homed camera sub-object
    // offsets CheckMugshotPrivilege reaches off the network manager (attested displacements).
    static const u32 KU_XPRIVILEGE_COMMUNICATIONS              = 247; // 0xF7
    static const u32 KU_XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY = 246; // 0xF6
    static const s32 KI_NETWORK_MANAGER_CAMERA_OFFSET          = 0x425E0; // asm addis+addi -> &mpCamera
    static const s32 KI_CAMERA_MUGSHOT_PRIVILEGE_FIELD_OFFSET  = 0x43B9C; // lwzx field, ==2 -> FRIENDS

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

    // ==================================================================================
    // DECLARATION-ONLY + FLAGGED bodies (19).
    //   The faithful body of each of these reaches an un-homed dependency (CameraX360 /
    //   GamerPictureManagerX360 / BrnNetworkManager internals, the raw-offset BrnNetworkModuleIO
    //   GameStateToNetworkInterface / event-queue interfaces, the VariableEventQueue<14000,16>
    //   event pump) or a multi-stage compress/segment/pack pipeline. Per the anti-fabrication rule
    //   these are left empty rather than paraphrased; bodying them needs those homes committed.
    // ==================================================================================

    // FLAG: declaration-only -- per-case state machine over CameraX360 / GamerPictureManagerX360 /
    // BrnNetworkManager::PackTextureAndSendDisplayEventToGui + GetCompressedTexture.  @ 0x8256F6E8
    void NetworkImageManager::ProcessBeforeSimulation(BrnNetworkModuleIO::OutputBuffer* /*lpOutput*/)
    {
    }

    // FLAG: declaration-only -- drains the network event queue (ProcessNetworkEvents) + ProcessDirty
    // trick handling + AbortMugshotCapture/Show, all through un-homed module-IO interfaces.  @ 0x8256BB38
    void NetworkImageManager::ProcessAfterSimulation(const BrnNetworkModuleIO::PostSimulationInputBuffer* /*lpInput*/)
    {
    }

    // FLAG: declaration-only -- ServerInterfaceConnection::IsLoggedIn gate + StrStream debug output +
    // photo-finish abort bookkeeping into un-homed module internals.  @ 0x8254ACA0
    CgsNetwork::NetworkTexture* NetworkImageManager::GetPhotoFinishImageByRoundWinner(
        NetworkPlayerID /*lRoundWinnerID*/, bool* /*lpbIsPhotoFinish*/)
    {
        return nullptr;
    }

    // FLAG: declaration-only -- maps the round results to the per-player mugshot/photo-finish slots.  @ 0x82573778
    void NetworkImageManager::HandleRoundResults(const OnlineRoundResults* /*lpResults*/)
    {
    }

    // FLAG: declaration-only -- segments the compressed mugshot into ImageMessage packets and pumps
    // them over the reliable channel (CgsCore::MemCpy + NetworkPlayer send).  @ 0x82555658
    void NetworkImageManager::SendNextSegment()
    {
    }

    // FLAG: declaration-only -- drains the VariableEventQueue<14000,16> and dispatches mugshot events.  @ 0x8255DAB0
    void NetworkImageManager::ProcessNetworkEvents(const NetworkEventQueue* /*lpQueue*/)
    {
    }

    // FLAG: declaration-only -- packs the active-race-car id pairs into a 68-byte event on the output
    // buffer's VariableEventQueue via the raw +818064 GameStateToNetworkInterface.  @ 0x82564E38
    void NetworkImageManager::OutputMugshotData(BrnNetworkModuleIO::OutputBuffer* /*lpOutput*/)
    {
    }

    // FLAG: declaration-only -- turns an inbound payback-mugshot event into a capture request.  @ 0x82555188
    void NetworkImageManager::HandleMugshotEvent(const NetworkInPaybackMugshotEvent* /*lpEvent*/)
    {
    }

    // FLAG: declaration-only -- VariableEventQueue<14000,16>::AddEvent +
    // BrnNetworkManager::PackTextureAndSendDisplayEventToGui (un-homed).  @ 0x825649A8
    void NetworkImageManager::AbortMugshotCapture()
    {
    }

    // FLAG: declaration-only -- twin of AbortMugshotCapture (un-homed reach).  @ 0x82564A98
    void NetworkImageManager::AbortMugshotShow()
    {
    }

    // FLAG: declaration-only -- builds + sends an ImageMessage to the receiver via the un-homed
    // NetworkPlayer send path.  @ 0x82564B80
    void NetworkImageManager::SendMugshotPicture(NetworkPlayerID /*lAggressorID*/,
                                                 NetworkPlayerID /*lVictimID*/,
                                                 NetworkPlayerID /*lReceiverID*/)
    {
    }

    // FLAG: declaration-only -- computes per-player send-segment counts via AreMugshotsDisabledForPlayer
    // (un-homed) over the local player id.  @ 0x825559C8
    void NetworkImageManager::BroadcastImage(MugshotData* /*lpMugshotData*/)
    {
    }

    // FLAG: declaration-only -- reassembles inbound ImageMessage segments into a mugshot texture
    // (Retrieve + bitset + per-packet blit).  @ 0x825732A8
    void NetworkImageManager::ReceiveImageMessage(NetworkPlayerID /*lSenderID*/, ImageMessage* /*lpImageMessage*/)
    {
    }

    // FLAG: declaration-only -- promotes a fully-received camera picture into the player's mugshot
    // slot + drives the show state.  @ 0x82555AD8
    void NetworkImageManager::HandleReceivedCameraPic(NetworkPlayerID /*lSenderID*/,
                                                      MugshotData* /*lpMugshotData*/,
                                                      BrnGameState::GameStateModuleIO::EImageType /*leImageType*/)
    {
    }

    // FLAG: declaration-only -- packs the selected mugshot texture out to the GUI (un-homed
    // BrnNetworkManager).  @ 0x82555B38
    void NetworkImageManager::HandleShowingMugshot(bool /*lbShowMyMugshot*/)
    {
    }

    // FLAG: declaration-only -- kicks the CameraX360 compressed-local-picture job with the
    // _GetCompressedCameraPicCallback.  @ 0x8256BC38
    void NetworkImageManager::GetCompressedTexture(CgsNetwork::NetworkTexture* /*lpTexture*/)
    {
    }

    // FLAG: declaration-only -- requests an async save of the captured gamer-pic mugshot.  @ 0x8256FC18
    void NetworkImageManager::RequestMugshotSave(NetworkPlayerID /*lAggressorID*/,
                                                 NetworkPlayerID /*lVictimID*/, u32 /*lu32Frame*/)
    {
    }

    // FLAG: declaration-only -- compressed-camera-picture job completion: DXT-compress + BroadcastImage
    // + SendNextSegment kickoff.  @ 0x82564EF8
    void NetworkImageManager::_GetCompressedCameraPicCallback(void* /*lpData*/)
    {
    }

    // FLAG: declaration-only -- compressed-gamer-picture job completion twin.  @ 0x8256FD40
    void NetworkImageManager::_GetCompressedGamerPicCallback(void* /*lpData*/)
    {
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

            // X360 (0x8254AB10 addis r30,r3,4 / addi r30,r30,0x25E0): lpCamera = NetworkManager +
            // 0x425E0 (== &NetworkManager->mpCamera); the read is *(int*)(lpCamera + 0x43B9C) == 2.
            // The camera sub-object is not yet homed, so it is reached by its attested byte offset
            // off the manager (mirrors the sibling GamerPictureManagerX360 named-offset accessor).
            BrnNetworkManager* lpNetworkManager = mpNetworkModule->GetNetworkManager();
            u8* lpCamera = reinterpret_cast<u8*>(lpNetworkManager) + KI_NETWORK_MANAGER_CAMERA_OFFSET;
            CGS_ASSERT(lpCamera, "lpCamera");

            const s32 liCameraPrivilege =
                *reinterpret_cast<const s32*>(lpCamera + KI_CAMERA_MUGSHOT_PRIVILEGE_FIELD_OFFSET);
            lePrivilege = (liCameraPrivilege == 2) ? E_MUGSHOT_PRIVILEGE_FRIENDS : E_MUGSHOT_PRIVILEGE_ANYONE;
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
