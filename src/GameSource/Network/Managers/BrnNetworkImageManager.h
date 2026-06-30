// ===================================================================================
// BrnNetwork::NetworkImageManager -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkImageManager.h
//
// The online "image" (mugshot / photo-finish) relay. It owns a fixed table of per-player
// receive slots (maMugshotData[8] -- one NetworkTexture mugshot picture + photo-packet
// bitset + the takedown aggressor/victim ids + the segment send cursor per player) and a
// parallel table of in-flight ImageMessage send/receive pairs (maImageData[7]). It drives a
// small capture/show state machine (EImageManagerState) each frame: it asks the X360 camera /
// gamer-picture manager for the local capture, DXT-compresses it, segments it over the reliable
// ImageMessage channel, reassembles inbound segments into a per-player mugshot texture, and
// packs the finished texture out to the GUI via the BrnNetworkManager. It also owns an in-game
// debug-menu component (ImageManagerDebugComponent, the committed home).
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../BrnNetworkImageManager.h), gated against the X360 binary.
// The X360 manager-`this` member offsets pin the trailing scalar block exactly:
//   maMugshotData[8]   @ +0x028  (8 * 64-byte MugshotData stride)
//   maImageData[7]     @ +0x208  (7 * 1144-byte ImageMessageData stride)
//   mTakedownVictimPlayerID    @ +0x2148   (8520)
//   mTakedownAggressorPlayerID @ +0x214C   (8524)
//   meState                    @ +0x2150   (8528)  (Construct = 5 == E_..._STATE_COUNT)
//   meImageTypeOfGamerPicToSave@ +0x2154   (8532)  (Construct = 6 == E_IMAGE_TYPE_COUNT)
//   meImageTypeToSend          @ +0x2158   (8536)  (Construct = 6 == E_IMAGE_TYPE_COUNT)
//   mRoadRuleBeatenID          @ +0x2160   (8544)  (Construct std 0)
//   mpNetworkModule            @ +0x2168   (8552)
//   mpPlayerManager            @ +0x216C   (8556)
//   mpTimeManager              @ +0x2170   (8560)
//   mbOutputMugshotData        @ +0x2174   (8564)
//   mbBroadcastCurrentImage    @ +0x2175   (8565)
//   mbAbortCaptureThisFrame    @ +0x2176   (8566)
//   mbAbortShowThisFrame       @ +0x2177   (8567)
//   mbMugshotsEnabled          @ +0x2178   (8568)
//   mDebugComponent            @ +0x217C   (8572)
// This project targets SEMANTIC parity on the host (x64, 8-byte pointers/vtables), so the host
// sizeof of NetworkTexture / ImageMessage / the debug component differs from the X360 widths;
// members are accessed strictly BY NAME, which is stride-correct on either target. The X360
// 64/1144 strides are documented from the asm only and intentionally NOT static_asserted on the
// host. The committed CgsNetwork::NetworkTexture, BrnNetwork::ImageMessage and
// BrnNetwork::ImageManagerDebugComponent are reused BY NAME -- not forked.
//
// FUNCTION OWNERSHIP (this TU has 30 X360 functions):
//   BODIED here/in the sibling .cpp -- the lifecycle + table accessors + the self-contained
//   dispatch helpers that touch only named members and committed-by-name APIs.
//   DECLARATION-ONLY + FLAGGED (body deferred) -- the functions that walk un-homed internals of
//   BrnNetwork::CameraX360 / GamerPictureManagerX360 / BrnNetworkManager / the BrnNetworkModuleIO
//   raw-offset interfaces, or that drive the multi-stage compress/segment/pack message pipeline.
//   Each such method is marked  // FLAG: declaration-only  at its declaration AND .cpp slot.
// ===================================================================================
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsID.h"                                // CgsID
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"                      // CgsMemory::HeapMalloc (Prepare param)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                    // CgsContainers::BitArray<N> (committed)
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"         // CgsNetwork::NetworkTexture (by value)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                   // BrnNetwork::NetworkPlayerID
#include "GameSource/GameState/BrnGameStateSharedIO.h"                        // BrnGameState::GameStateModuleIO::EImageType
#include "GameSource/Network/Messages/BrnImageMessage.h"                      // BrnNetwork::ImageMessage (by value)
#include "GameSource/Network/Debug Components/BrnNetworkImageManagerDebugComponent.h" // BrnNetwork::ImageManagerDebugComponent (by value)

namespace CgsNetwork
{
    class PlayerManager;                 // pointer-only
    class TimeManager;                   // pointer-only
    class NetworkTextureDXTCompress;     // Construct param (pointer-only)
    struct ReliableMessage;              // _ImageMessageArrivedCallback param (committed home: CgsReliableMessage.h)
    struct SignalMessage;                // _ImageMessageDeliveredCallback param
}

namespace BrnNetwork
{
    class BrnNetworkModule;              // pointer-only

    namespace BrnNetworkModuleIO
    {
        struct OutputBuffer;                 // ProcessBeforeSimulation / OutputMugshotData param
        struct PostSimulationInputBuffer;    // ProcessAfterSimulation param
    }

    // Forward-only network event / message types touched through committed homes.
    class  NetworkEventQueue;            // ProcessNetworkEvents param
    struct OnlineRoundResults;           // HandleRoundResults param
    struct NetworkInPaybackMugshotEvent; // HandleMugshotEvent param

    // DWARF BrnNetworkImageManager.cpp:45 -- the reliable-message budget gate.
    const s32 KI_MAX_BUFFERED_RELIABLE_MESSAGES_TO_SEND_MUGSHOT = 21;
    // The player-table widths (X360 loops run 8 / 7 times respectively).
    const s32 KI_MAX_MUGSHOT_PLAYERS = 8;
    const s32 KI_MAX_IMAGE_PLAYERS   = 7;

    struct NetworkImageManager
    {
        // DWARF BrnNetworkImageManager.h:166 -- the capture/show state machine.
        enum EImageManagerState
        {
            E_IMAGE_MANAGER_STATE_CAPTURE_MUGSHOT       = 0,
            E_IMAGE_MANAGER_STATE_TAKE_MUGSHOT          = 1,
            E_IMAGE_MANAGER_STATE_WAIT_COMPRESS_MUGSHOT = 2,
            E_IMAGE_MANAGER_STATE_SHOW_MY_MUGSHOT       = 3,
            E_IMAGE_MANAGER_STATE_SHOW_MUGSHOT          = 4,
            E_IMAGE_MANAGER_STATE_COUNT                 = 5,
        };

        // DWARF BrnNetworkImageManager.h:178
        enum EMugshotPrivilege
        {
            E_MUGSHOT_PRIVILEGE_ANYONE  = 0,
            E_MUGSHOT_PRIVILEGE_FRIENDS = 1,
            E_MUGSHOT_PRIVILEGE_NOONE   = 2,
            E_MUGSHOT_PRIVILEGE_COUNT   = 3,
        };

        // DWARF BrnNetworkImageManager.h:188 -- one player's send/receive ImageMessage pair.
        struct ImageMessageData
        {
            NetworkPlayerID mPlayerID;          // +0x000
            ImageMessage    mImageMessageSend;  // +0x004
            ImageMessage    mImageMessageRecv;  // (X360 +0x238 within the entry)
        };

        // DWARF BrnNetworkImageManager.h:196 -- one player's reassembled mugshot picture + state.
        struct MugshotData
        {
            CgsNetwork::NetworkTexture     mPicture;                  // +0x00
            CgsContainers::BitArray<50u>   mReceivedPhotoPackets;     // X360 @ +0x20 (8-byte word covers the 50 bits)
            NetworkPlayerID            mTakedownAggressorPlayerID;// X360 @ +0x28
            NetworkPlayerID            mTakedownVictimPlayerID;   // X360 @ +0x2C
            s32                        miLastPacketSent;          // X360 @ +0x30
            s32                        miNumberOfPacketsToSend;   // X360 @ +0x34
            bool                       mbPictureValid;            // X360 @ +0x38
            bool                       mbPhotoFinishValid;        // X360 @ +0x39
        };

        // ---- public lifecycle / per-frame API ------------------------------------------
        void Construct(BrnNetworkModule* lpNetworkModule, CgsNetwork::PlayerManager* lpPlayerManager,
                       CgsNetwork::TimeManager* lpTimeManager,
                       CgsNetwork::NetworkTextureDXTCompress* lpTextureCompressor);   // @ 0x8255D7D0  (bodied)
        bool Prepare(CgsMemory::HeapMalloc* lpHeapMalloc);                            // @ 0x8255D978  (bodied)
        bool Release();                                                              // @ 0x8255DA20  (bodied)
        void Destruct();                                                            // @ 0x8255D8B0  (bodied)

        void ProcessBeforeSimulation(BrnNetworkModuleIO::OutputBuffer* lpOutput);                     // @ 0x8256F6E8  // FLAG: declaration-only
        void ProcessAfterSimulation(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput);    // @ 0x8256BB38  // FLAG: declaration-only

        void AddPlayer(NetworkPlayerID lPlayerID);    // @ 0x82576110  (bodied)
        void RemovePlayer(NetworkPlayerID lPlayerID); // @ 0x8255DB40  (bodied)
        void Disconnected();                          // @ 0x8255DC08  (bodied)

        CgsNetwork::NetworkTexture* GetMugshotImageByAggressor(NetworkPlayerID lTakedownAggressorID);        // @ 0x8254AC28  (bodied)
        NetworkPlayerID             GetMugshotVictimID(NetworkPlayerID lTakedownAggressorID);                // @ 0x8254AE10  (bodied)
        CgsNetwork::NetworkTexture* GetPhotoFinishImageByRoundWinner(NetworkPlayerID lRoundWinnerID,
                                                                     bool* lpbIsPhotoFinish);               // @ 0x8254ACA0  // FLAG: declaration-only
        void OnRoundStart();                                          // @ 0x8255DC98  (bodied)
        void HandleRoundResults(const OnlineRoundResults* lpResults); // @ 0x82573778  // FLAG: declaration-only

    private:
        // ---- internals ------------------------------------------------------------------
        void         SendNextSegment();                                                 // @ 0x82555658  // FLAG: declaration-only
        MugshotData* GetMugshotDataEntry(NetworkPlayerID lPlayerID);                     // @ (sibling GetMugshotDataEn; bodied helper)
        ImageMessageData* GetImageMessageDataEntry(NetworkPlayerID lPlayerID);           // @ (sibling GetImageMes; bodied helper)
        void         ProcessNetworkEvents(const NetworkEventQueue* lpQueue);             // @ 0x8255DAB0  // FLAG: declaration-only (reaches VariableEventQueue<14000,16>)
        void         OutputMugshotData(BrnNetworkModuleIO::OutputBuffer* lpOutput);      // @ 0x82564E38  // FLAG: declaration-only
        void         HandleMugshotEvent(const NetworkInPaybackMugshotEvent* lpEvent);    // @ 0x82555188  // FLAG: declaration-only
        void         AbortMugshotCapture();                                             // @ 0x825649A8  (bodied)
        void         AbortMugshotShow();                                                // @ 0x82564A98  (bodied)
        void         SendMugshotPicture(NetworkPlayerID lAggressorID, NetworkPlayerID lVictimID,
                                        NetworkPlayerID lReceiverID);                    // @ 0x82564B80  // FLAG: declaration-only
        void         BroadcastImage(MugshotData* lpMugshotData);                         // @ 0x825559C8  // FLAG: declaration-only
        void         ReceiveImageMessage(NetworkPlayerID lSenderID, ImageMessage* lpImageMessage); // @ 0x825732A8  // FLAG: declaration-only
        void         HandleReceivedCameraPic(NetworkPlayerID lSenderID, MugshotData* lpMugshotData,
                                             BrnGameState::GameStateModuleIO::EImageType leImageType); // @ 0x82555AD8  // FLAG: declaration-only
        void         HandleShowingMugshot(bool lbShowMyMugshot);                         // @ 0x82555B38  // FLAG: declaration-only
        void         GetCompressedTexture(CgsNetwork::NetworkTexture* lpTexture);         // @ 0x8256BC38  // FLAG: declaration-only
        void         RequestMugshotSave(NetworkPlayerID lAggressorID, NetworkPlayerID lVictimID,
                                        u32 lu32Frame);                                  // @ 0x8256FC18  // FLAG: declaration-only

        // Static compress callbacks (the void* user-data is this manager). Bodies reach the
        // un-homed compress/segment pipeline -> declaration-only.
        static void  _GetCompressedCameraPicCallback(void* lpData);                      // @ 0x82564EF8  // FLAG: declaration-only
        static void  _GetCompressedGamerPicCallback(void* lpData);                       // @ 0x8256FD40  // FLAG: declaration-only

        // Reliable-message arrival callback registered with CgsNetwork::NetworkPlayer for the
        // ImageMessage type (matches CgsNetwork::ReliableMessageArrivedCallback). Forwards to
        // ReceiveImageMessage. The user-data is this manager.  @ 0x82573700  (bodied)
        static void  _ImageMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                  NetworkPlayerID liFromPlayerID, void* lpUserData);
        // The delivery callback twin lives in the sibling .cpp (not in this TU's X360 set); declared
        // here only so AddPlayer can register it.
        static void  _ImageMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                    CgsNetwork::SignalMessage* lpMessage,
                                                    NetworkPlayerID liToPlayerID, void* lpUserData);

        // ---- data layout ----------------------------------------------------------------
        MugshotData       maMugshotData[KI_MAX_MUGSHOT_PLAYERS];   // X360 @ +0x028 (8 x 64B)
        ImageMessageData  maImageData[KI_MAX_IMAGE_PLAYERS];       // X360 @ +0x208 (7 x 1144B)
        NetworkPlayerID   mTakedownVictimPlayerID;                 // X360 @ +0x2148
        NetworkPlayerID   mTakedownAggressorPlayerID;              // X360 @ +0x214C
        EImageManagerState meState;                                // X360 @ +0x2150
        BrnGameState::GameStateModuleIO::EImageType meImageTypeOfGamerPicToSave; // X360 @ +0x2154
        BrnGameState::GameStateModuleIO::EImageType meImageTypeToSend;           // X360 @ +0x2158
        CgsID             mRoadRuleBeatenID;                        // X360 @ +0x2160
        BrnNetworkModule*          mpNetworkModule;                 // X360 @ +0x2168
        CgsNetwork::PlayerManager* mpPlayerManager;                 // X360 @ +0x216C
        CgsNetwork::TimeManager*   mpTimeManager;                   // X360 @ +0x2170
        bool              mbOutputMugshotData;                      // X360 @ +0x2174
        bool              mbBroadcastCurrentImage;                  // X360 @ +0x2175
        bool              mbAbortCaptureThisFrame;                  // X360 @ +0x2176
        bool              mbAbortShowThisFrame;                     // X360 @ +0x2177
        bool              mbMugshotsEnabled;                        // X360 @ +0x2178
        ImageManagerDebugComponent mDebugComponent;                // X360 @ +0x217C
    };
} // namespace BrnNetwork
