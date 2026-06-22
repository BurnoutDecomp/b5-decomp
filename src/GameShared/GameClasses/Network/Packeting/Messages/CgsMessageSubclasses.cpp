#include "CgsMessage.h"
#include "BrnMessageSubclasses.h"

// Reconstructed bodies for four functions of the CgsMessage.h message hierarchy
// that were previously dropped (a prior pass wrongly folded this TU into
// CgsMessage.cpp, which actually bodies the distinct class:CgsNetwork::Message TU).
//
//   BrnNetwork::HullSyncMessage::Release             @ 0x8257DD40
//   BrnNetwork::UpdateMessage::Release               @ 0x8257D990
//   CgsNetwork::HostMigrationManager::IsHostAlive    @ 0x82872258
//   CgsNetwork::ReliableMessage::PrepareForSend      @ 0x82882100
//
// Store-for-store faithful to the X360 asm; members referenced by name; asserts
// use the house CGS_ASSERT machinery (declared via CgsAssert.h, pulled by
// CgsMessage.h).

namespace BrnNetwork
{
    // ---- HullSyncMessage::Release @ 0x8257DD40 ---------------------------------
    // lbz r11,0x19(r3) / clrrwi r11,r11,1 / stb r11,0x19(r3)  -> clear VALID bit;
    // li r10,0 / stw r10,0x54(r3)                             -> zero the +0x54 word.
    void HullSyncMessage::Release()
    {
        mx8Flags &= static_cast<u8>(~CgsNetwork::KX8_FLAGS_VALID);
        miSyncState = 0;
    }

    // ---- UpdateMessage::Release @ 0x8257D990 -----------------------------------
    // lbz r11,0x19(r3) / clrrwi r11,r11,1 / stb r11,0x19(r3)  -> clear VALID bit.
    void UpdateMessage::Release()
    {
        mx8Flags &= static_cast<u8>(~CgsNetwork::KX8_FLAGS_VALID);
    }
}

namespace CgsNetwork
{
    // ---- HostMigrationManager::IsHostAlive @ 0x82872258 ------------------------
    // return UInt16IsLargerWrapped(mu16HostFrame, lu16CurrentFrame)
    //     || mu16AliveWindow >= GetFrameDiffWrapped16(lu16CurrentFrame, mu16HostFrame);
    // The subfc/subfe/addi idiom @0x828722A4-AC is the unsigned >= compare.
    bool HostMigrationManager::IsHostAlive(u16 lu16CurrentFrame) const
    {
        return UInt16IsLargerWrapped(mu16HostFrame, lu16CurrentFrame)
            || mu16AliveWindow >= GetFrameDiffWrapped16(lu16CurrentFrame, mu16HostFrame);
    }

    // ---- ReliableMessage::PrepareForSend @ 0x82882100 --------------------------
    // assert !IsMessageValid()            (CgsMessage.h:940)
    // mx8Flags |= RELIABLE                 (ori r11,r11,2 / stb @+0x19, UNCONDITIONAL)
    // assert lu16Frame != KU16_INVALID_FRAME (CgsMessage.h:594)
    // SetType(leType)                      (bl CgsNetwork::Message::SetType)
    // mu16Frame = lu16Frame                (sth r29,0x1C)
    // mx8Flags |= VALID                    (ori r11,r11,1 / stb @+0x19)
    void ReliableMessage::PrepareForSend(s32 leType, u16 lu16Frame)
    {
        CGS_ASSERT((mx8Flags & KX8_FLAGS_VALID) == 0, "!IsMessageValid()");
        mx8Flags |= KX8_FLAGS_RELIABLE;
        CGS_ASSERT(lu16Frame != KU16_INVALID_FRAME, "lu16Frame != KU16_INVALID_FRAME");
        SetType(leType);
        mu16Frame = lu16Frame;
        mx8Flags |= KX8_FLAGS_VALID;
    }
}
