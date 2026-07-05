#ifndef CGS_VOIP_MANAGER_DIRTYSOCK_H
#define CGS_VOIP_MANAGER_DIRTYSOCK_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsHeadsetStatusMessage.h"

// ===========================================================================
// CgsNetwork::VoIPClient / CgsNetwork::VoIPManager  (DirtySock VoIP)
//   Home: GameShared/GameClasses/Network/VoIP/DirtySock/CgsVoIPManagerDirtySock.{h,cpp}
//
// VoIPClient: one registered voice-chat talker tracked by the VoIP manager --
// which player and connection it is, whether it is muted/blocked, and the two
// headset-status messages used to broadcast and receive headset state.
//
// VoIPManager: the per-machine DirtySock voice manager -- holds the manager's
// service pointers, the fixed array of registered talkers, and the local
// headset/volume/chat-restriction state.
//
// VoIPClient LAYOUT (DWARF references/DecFIGS/dwarfdump/.../CgsVoIPManagerDirtySock.h:85-91,
// pinned by the X360 VoIPClient::operator= @ 0x82893618 load/store offsets):
//   +0x00  mPlayerID         (NetworkPlayerID == s32)   lwz/stw 0
//   +0x04  miConnectionID    (s32)                       lwz/stw 4
//   +0x08  mbIsBlocked       (bool)                      lbz/stb 8
//   +0x09  mbCanTalk         (bool)                      lbz/stb 9   [see note]
//   +0x0C  mHeadsetSendMsg   (HeadsetStatusMessage)      copied at this+0xC
//   +0x30  mHeadsetRecMsg    (HeadsetStatusMessage)      copied at this+0x30
//   sizeof(VoIPClient) == 0x54  (stride attested by the 0x54 array walk in
//   GetRegisteredTalker/EnableAllComms).
//
// The +0x09 byte is NOT named by the DWARF (it falls in the alignment padding
// between mbIsBlocked and the first HeadsetStatusMessage). The X360 code writes
// it as a live per-talker flag: EnableAllComms @ 0x82893868 sets it to 1 for all
// seven talkers, DisableCommsWithPlayer @ 0x828937B8 clears it to 0 for the
// matched talker before muting them. Modelled here as `mbCanTalk` (the
// enable/disable-comms flag) -- the NAME is reconstructed, the OFFSET is attested.
//
// Each HeadsetStatusMessage (0x24 bytes: Message base 0x20 + mu8HeadsetStatus,
// padded to 4-byte alignment) is copied memberwise by the assignment operator.
//
// The DWARF declares mPlayerID's type as RoadRulesRecvData::NetworkPlayerID; the
// effective type is the BrnNetwork NetworkPlayerID alias (s32) -- modelled here as
// a local s32 alias so this header does not pull in the RoadRules event tree (the
// only thing operator= touches is the 4-byte word at +0).
//
// VoIPManager LAYOUT (DWARF references/DecFIGS/dwarfdump/.../CgsVoIPManagerDirtySock.h:103,193-210,
// pinned by the X360 offsets in this TU: maRegisteredTalkers @ +0x18 (a1+24) with
// 0x54 stride, mpVoiceRef @ +0x00 (*a1), mbLocalPlayerHasHeadset @ +0x276 (a1+630)):
//   +0x000  mpVoiceRef        (VoipRefT*)
//   +0x004  mpPlayerManager   (PlayerManager*)
//   +0x008  mpTimeManager     (TimeManager*)
//   +0x00C  mpNetworkManager  (NetworkManager*)
//   +0x010  mpServerInterface (ServerInterface*)
//   +0x014  mpBuddyManager    (BuddyManagerBase*)
//   +0x018  maRegisteredTalkers[7]   (VoIPClient, stride 0x54)  -> ends 0x264
//   +0x264  miControllerPort  (s32)
//   +0x268  miVoipVolumePercent (s32)
//   +0x26C  meVoIPOutput      (ENetworkHeadsetOutputMode)
//   +0x270  meStatus          (EVoIPManagerStatus)
//   +0x274  mu8HeadsetStatus  (u8)
//   +0x275  mbIsVoiceAllowed  (bool)
//   +0x276  mbLocalPlayerHasHeadset (bool)
//   +0x277  mbIsLocalPlayerInGame (bool)
//   +0x278  mbIsChatRestricted (bool)
//   +0x279  mbGameIsHandlingHeadsetStatus (bool)
//
// The manager's service-pointer types (VoipRefT/PlayerManager/TimeManager/
// NetworkManager/ServerInterface/BuddyManagerBase) are declared opaque here: this
// TU's three reconstructed functions only touch mpVoiceRef, the talker array and
// mbLocalPlayerHasHeadset, so full definitions are not pulled in.
// ===========================================================================

namespace CgsNetwork
{
    // NetworkPlayerID is an s32 player handle across the network layer
    // (CgsVoIPManagerDirtySock.h:87 declares mPlayerID with this type).
    typedef s32 VoIPNetworkPlayerID;

    // DWARF CgsVoIPManagerDirtySock.h:48 / :57
    enum ENetworkHeadsetOutputMode
    {
        E_NETWORK_HEADSET_OUTPUT_MODE_HEADSET   = 0,
        E_NETWORK_HEADSET_OUTPUT_MODE_TVSPEAKERS = 1,
        E_NETWORK_HEADSET_OUTPUT_MODE_COUNT     = 2,
    };

    enum EVoIPManagerStatus
    {
        E_VOIP_MANAGER_STATUS_UNPREPARED = 0,
        E_VOIP_MANAGER_STATUS_READY      = 1,
        E_VOIP_MANAGER_STATUS_COUNT      = 2,
    };

    struct VoIPClient
    {
        VoIPNetworkPlayerID mPlayerID;        // +0x00  (CgsVoIPManagerDirtySock.h:87)
        s32                 miConnectionID;   // +0x04  (CgsVoIPManagerDirtySock.h:88)
        bool                mbIsBlocked;      // +0x08  (CgsVoIPManagerDirtySock.h:89)
        bool                mbCanTalk;        // +0x09  (padding slot; see header note)
        HeadsetStatusMessage mHeadsetSendMsg; // +0x0C  (CgsVoIPManagerDirtySock.h:90)
        HeadsetStatusMessage mHeadsetRecMsg;  // +0x30  (CgsVoIPManagerDirtySock.h:91)

        // X360 copy-assignment @ 0x82893618: memberwise copy of the scalar prefix
        // plus the two HeadsetStatusMessage members (each copied via the Message
        // base's memberwise fields + the trailing headset-status byte).
        VoIPClient& operator=(const VoIPClient& arOther);
    };

    // Opaque service types the manager only holds by pointer (not touched by this TU).
    namespace DirtySock { struct VoipRefT; }
    struct PlayerManager;
    struct TimeManager;
    struct NetworkManager;
    struct ServerInterface;
    struct BuddyManagerBase;

    struct VoIPManager
    {
        DirtySock::VoipRefT* mpVoiceRef;                 // +0x000
        PlayerManager*       mpPlayerManager;            // +0x004
        TimeManager*         mpTimeManager;              // +0x008
        NetworkManager*      mpNetworkManager;           // +0x00C
        ServerInterface*     mpServerInterface;          // +0x010
        BuddyManagerBase*    mpBuddyManager;             // +0x014
        VoIPClient           maRegisteredTalkers[7];     // +0x018  (stride 0x54)
        s32                  miControllerPort;           // +0x264
        s32                  miVoipVolumePercent;        // +0x268
        ENetworkHeadsetOutputMode meVoIPOutput;          // +0x26C
        EVoIPManagerStatus   meStatus;                   // +0x270
        u8                   mu8HeadsetStatus;           // +0x274
        bool                 mbIsVoiceAllowed;           // +0x275
        bool                 mbLocalPlayerHasHeadset;    // +0x276
        bool                 mbIsLocalPlayerInGame;      // +0x277
        bool                 mbIsChatRestricted;         // +0x278
        bool                 mbGameIsHandlingHeadsetStatus; // +0x279

        // Connection bitmask for the talker's server connection (external TU).
        u32 GetConnectionMask();

        // --- reconstructed in this TU ---
        // GetRegisteredTalker @ 0x8287E4D8: linear-scan maRegisteredTalkers for the
        // entry whose mPlayerID == liPlayerID; asserts + returns null if not found.
        VoIPClient* GetRegisteredTalker(VoIPNetworkPlayerID liPlayerID);

        // DisableCommsWithPlayer @ 0x828937B8: clear the matched talker's mbCanTalk
        // and mute both speaker and (headset-gated) microphone toward it.
        VoIPClient* DisableCommsWithPlayer(VoIPNetworkPlayerID liPlayerID);

        // EnableAllComms @ 0x82893868: re-enable mbCanTalk for every talker and
        // restore full speaker/microphone routing.
        VoIPManager* EnableAllComms();
    };
} // namespace CgsNetwork

#endif // CGS_VOIP_MANAGER_DIRTYSOCK_H
