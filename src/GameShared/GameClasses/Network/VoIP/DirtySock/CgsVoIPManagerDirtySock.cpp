#include "GameShared/GameClasses/Network/VoIP/DirtySock/CgsVoIPManagerDirtySock.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::VoIPClient::operator= @ 0x82893618
//
// ---------------------------------------------------------------------------
// VoIPClient::operator=(const VoIPClient&)  @ 0x82893618
//
// The X360 codegen is the compiler-synthesised memberwise copy-assignment:
//
//   r3 = this (dest), r4 = source
//   stw  0(r4)->0(r3)        ; mPlayerID        (s32 @ +0x00)
//   stw  4(r4)->4(r3)        ; miConnectionID   (s32 @ +0x04)
//   stb  8(r4)->8(r3)        ; mbIsBlocked      (bool @ +0x08)
//   stb  9(r4)->9(r3)        ; mbIsBlocked padding/tail byte (within the +0x08 slot)
//   ; --- mHeadsetSendMsg (this+0xC) copied memberwise via Message base + byte ---
//     lwz/stw +0x0C(=vptr)            lwz/stw +0x10(=mePackOrUnpack)
//     4x lwz/stw at +0x18..+0x24      ; the four SmartBitStream cursor words
//     3x lbz/stb +0x24..+0x26         ; mu8GameID / mx8Flags / mi8Type
//     lhz/sth +0x28                   ; mu16Frame
//     lbz/stb +0x2C                   ; mu8HeadsetStatus
//   ; --- mHeadsetRecMsg (this+0x30) copied with the identical field pattern ---
//   return this
//
// The two HeadsetStatusMessage members are each copied field-for-field through the
// Message base (vptr, mePackOrUnpack, the four cursor words, the three status
// bytes, the frame half-word) plus the trailing mu8HeadsetStatus byte -- exactly
// the compiler's memberwise HeadsetStatusMessage::operator=. The reconstruction
// therefore assigns each member by name; the per-field expansion above is the
// compiler's codegen of those memberwise copies, not separately hand-written
// stores.
// ---------------------------------------------------------------------------

namespace CgsNetwork
{
    VoIPClient& VoIPClient::operator=(const VoIPClient& arOther)
    {
        mPlayerID       = arOther.mPlayerID;
        miConnectionID  = arOther.miConnectionID;
        mbIsBlocked     = arOther.mbIsBlocked;
        mHeadsetSendMsg = arOther.mHeadsetSendMsg;
        mHeadsetRecMsg  = arOther.mHeadsetRecMsg;
        return *this;
    }
}
