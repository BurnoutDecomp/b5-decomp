#ifndef CGS_VOIP_MANAGER_DIRTYSOCK_H
#define CGS_VOIP_MANAGER_DIRTYSOCK_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsHeadsetStatusMessage.h"

// ===========================================================================
// CgsNetwork::VoIPClient  (DirtySock VoIP)
//   Home: GameShared/GameClasses/Network/VoIP/DirtySock/CgsVoIPManagerDirtySock.{h,cpp}
//
// One registered voice-chat talker tracked by the VoIP manager: which player and
// connection it is, whether it is muted/blocked, and the two headset-status
// messages used to broadcast and receive headset state.
//
// LAYOUT (DWARF references/DecFIGS/dwarfdump/.../CgsVoIPManagerDirtySock.h:85-91,
// pinned by the X360 VoIPClient::operator= @ 0x82893618 load/store offsets):
//   +0x00  mPlayerID         (NetworkPlayerID == s32)   lwz/stw 0
//   +0x04  miConnectionID    (s32)                       lwz/stw 4
//   +0x08  mbIsBlocked       (bool)                      lbz/stb 8
//   +0x0C  mHeadsetSendMsg   (HeadsetStatusMessage)      copied at this+0xC
//   +0x30  mHeadsetRecMsg    (HeadsetStatusMessage)      copied at this+0x30
// Each HeadsetStatusMessage (0x21 bytes: Message base 0x20 + mu8HeadsetStatus) is
// copied memberwise by the assignment -- the asm reproduces the Message base's
// word/byte fields (vptr, mePackOrUnpack, the four SmartBitStream cursor words,
// mu8GameID/mx8Flags/mi8Type, mu16Frame) then the trailing mu8HeadsetStatus byte,
// exactly the compiler's memberwise HeadsetStatusMessage::operator=.
//
// The DWARF declares mPlayerID's type as RoadRulesRecvData::NetworkPlayerID; the
// effective type is the BrnNetwork NetworkPlayerID alias (s32) -- modelled here as
// a local s32 alias so this header does not pull in the RoadRules event tree (the
// only thing operator= touches is the 4-byte word at +0).
// ===========================================================================

namespace CgsNetwork
{
    // NetworkPlayerID is an s32 player handle across the network layer
    // (CgsVoIPManagerDirtySock.h:87 declares mPlayerID with this type).
    typedef s32 VoIPNetworkPlayerID;

    struct VoIPClient
    {
        VoIPNetworkPlayerID mPlayerID;        // +0x00  (CgsVoIPManagerDirtySock.h:87)
        s32                 miConnectionID;   // +0x04  (CgsVoIPManagerDirtySock.h:88)
        bool                mbIsBlocked;      // +0x08  (CgsVoIPManagerDirtySock.h:89)
        HeadsetStatusMessage mHeadsetSendMsg; // +0x0C  (CgsVoIPManagerDirtySock.h:90)
        HeadsetStatusMessage mHeadsetRecMsg;  // +0x30  (CgsVoIPManagerDirtySock.h:91)

        // X360 copy-assignment @ 0x82893618: memberwise copy of the scalar prefix
        // plus the two HeadsetStatusMessage members (each copied via the Message
        // base's memberwise fields + the trailing headset-status byte).
        VoIPClient& operator=(const VoIPClient& arOther);
    };
} // namespace CgsNetwork

#endif // CGS_VOIP_MANAGER_DIRTYSOCK_H
