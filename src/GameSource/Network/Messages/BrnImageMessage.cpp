#include "types.hpp"

#include <cstring>   // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)

#include "GameSource/Network/Messages/BrnImageMessage.h"
#include "GameSource/Network/BrnNetworkManager.h"                                       // BrnNetworkManager::PackOrUnpack (NetworkPlayerID field primitive)
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"              // PackOrUnpackU8/U16/CgsID/Buffer field primitives
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"      // ReliableMessage::PackOrUnpack base
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsTestConnectionMessage.h" // GetPackedMessageSize ICF-folded base probe
#include "GameShared/GameClasses/Core/CgsAssert.h"                                      // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::ImageMessage::GetPackedMessageSize  @ 0x8257C0D0
//   BrnNetwork::ImageMessage::PackOrUnpack          @ 0x8257C100
//   BrnNetwork::ImageMessage::Retrieve              @ 0x8257F078
//
// A CgsNetwork::ReliableMessage subclass that ferries one segment of photo ("mugshot")
// data between players. The payload, all packed after the inherited reliable id, is:
//   mBeatenRoadID                (CgsID/u64, +0x220)   the road this photo relates to
//   mImageSenderPlayerID         (NetworkPlayerID/s32, +0x228)
//   mImageReceiverPlayerID       (NetworkPlayerID/s32, +0x22C)
//   mu16PhotoPacketNumber        (u16, +0x230)         this segment's index    [0, 50]
//   mu16TotalPhotoPacketCount    (u16, +0x232)         total segment count     [0, 50]
//   mu16NumberOfBytesOfPhotoData (u16, +0x234)         bytes in this segment    [0, 500]
//   mu8ImageType                 (u8,  +0x236)         EImageType               [0, 6]
//   macPhotoBuffer[500]          (raw bytes, +0x28)    the photo segment itself
//
// GetPackedMessageSize seeds the payload with worst-case representative values (segment
// length == KI_PHOTO_SEGMENT_SIZE == 500 so the variable buffer measures full, image
// type == E_IMAGE_TYPE_COUNT == 6 == the max enum value, every id/counter zeroed) and
// then delegates to the bare-ReliableMessage size probe. The X360 build tail-calls
// CgsNetwork::TestConnectionMessage::GetPackedMessageSize (ICF-folded with the
// ReliableMessage base probe -- a bare ReliableMessage carries no extra payload), so the
// delegate is taken with `this` reinterpreted as that sibling, matching the binary.
//
// NOTE: the Hex-Rays pseudocode renders the +0x220 `std 0` and the +0x236 `stb 6` as a
// single `*(a1 + 544) = 0x600000000LL`; the ASM proves they are two independent stores
// (mBeatenRoadID = 0 and mu8ImageType = 6).
//
// PackOrUnpack ORs the base reliable-id status with each quantised field (in the ASM's
// store order) -- 0 == success -- asserts the segment length never exceeds the segment
// cap, then (de)serialises exactly mu16NumberOfBytesOfPhotoData bytes of the buffer.
//
// Retrieve copies the unpacked payload back out to the caller's out-params (only when the
// VALID flag is set), block-copies the photo bytes, clears VALID, and returns whether a
// message was present. Hex-Rays mangles the prototype into 28 ints; the ASM prologue
// proves there are eight out-params (this + r4..r10 + one stacked), in the same order the
// owning header declares.

namespace BrnNetwork
{
    // BrnNetwork::ImageMessage::GetPackedMessageSize @ 0x8257C0D0
    s32 ImageMessage::GetPackedMessageSize()
    {
        mImageSenderPlayerID         = 0;
        mImageReceiverPlayerID       = 0;
        mu16PhotoPacketNumber        = 0;
        mu16TotalPhotoPacketCount    = 0;
        mBeatenRoadID                = 0;
        mu8ImageType                 = BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_COUNT; // 6 -- max enum value
        mu16NumberOfBytesOfPhotoData = KI_PHOTO_SEGMENT_SIZE;                               // 500 -- worst-case buffer

        // @0x8257C0F8: b CgsNetwork__TestConnectionMessage__GetPackedMessageSize -- a bare
        // ReliableMessage size probe (no extra payload), ICF-folded with the base. Taken
        // with `this` reinterpreted as that sibling, matching the X360 tail call.
        return reinterpret_cast<CgsNetwork::TestConnectionMessage*>(this)->GetPackedMessageSize();
    }

    // BrnNetwork::ImageMessage::PackOrUnpack @ 0x8257C100
    CgsNetwork::PackOrUnpackResult ImageMessage::PackOrUnpack()
    {
        CgsNetwork::PackOrUnpackResult lxResult = CgsNetwork::ReliableMessage::PackOrUnpack();

        lxResult |= BrnNetwork::BrnNetworkManager::PackOrUnpack(this, &mImageSenderPlayerID);
        lxResult |= BrnNetwork::BrnNetworkManager::PackOrUnpack(this, &mImageReceiverPlayerID);
        lxResult |= CgsNetwork::PackOrUnpackU8(this, &mu8ImageType, 0, 6);
        lxResult |= CgsNetwork::PackOrUnpackU16(this, &mu16PhotoPacketNumber, 0, KI_MAX_PHOTO_PACKETS);
        lxResult |= CgsNetwork::PackOrUnpackU16(this, &mu16TotalPhotoPacketCount, 0, KI_MAX_PHOTO_PACKETS);
        lxResult |= CgsNetwork::PackOrUnpackU16(this, &mu16NumberOfBytesOfPhotoData, 0, KI_PHOTO_SEGMENT_SIZE);
        lxResult |= CgsNetwork::PackOrUnpackCgsID(this, &mBeatenRoadID);

        CGS_ASSERT(mu16NumberOfBytesOfPhotoData <= KI_PHOTO_SEGMENT_SIZE,
                   "mu16NumberOfBytesOfPhotoData <= KI_PHOTO_SEGMENT_SIZE");

        lxResult |= CgsNetwork::PackOrUnpackBuffer(this, reinterpret_cast<u8*>(macPhotoBuffer),
                                                   mu16NumberOfBytesOfPhotoData);
        return lxResult;
    }

    // BrnNetwork::ImageMessage::Retrieve @ 0x8257F078
    bool ImageMessage::Retrieve(NetworkPlayerID* lpSenderPlayerID, NetworkPlayerID* lpReceiverPlayerID,
                                BrnGameState::GameStateModuleIO::EImageType* lpeImageType,
                                CgsID* lpBeatenRoadID, u16* lpu16PacketNumber,
                                u16* lpu16TotalPacketCount, u16* lpu16NumberOfBytes,
                                void* lpvPhotoData)
    {
        CGS_ASSERT(lpSenderPlayerID, "lpImageSenderPlayerID");
        CGS_ASSERT(lpReceiverPlayerID, "lpImageReceiverPlayerID");
        CGS_ASSERT(lpu16PacketNumber, "lpu16PhotoPacketNumber");
        CGS_ASSERT(lpu16TotalPacketCount, "lpu16TotalPhotoPacketCount");
        CGS_ASSERT(lpu16NumberOfBytes, "lpu16NumberOfBytesOfPhotoData");
        CGS_ASSERT(lpvPhotoData, "lpPhotoData");
        CGS_ASSERT(lpBeatenRoadID, "lpBeatenRoadID");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
        {
            *lpeImageType        = static_cast<BrnGameState::GameStateModuleIO::EImageType>(mu8ImageType);
            *lpSenderPlayerID    = mImageSenderPlayerID;
            *lpReceiverPlayerID  = mImageReceiverPlayerID;
            *lpBeatenRoadID      = mBeatenRoadID;
            *lpu16PacketNumber   = mu16PhotoPacketNumber;
            *lpu16TotalPacketCount = mu16TotalPhotoPacketCount;
            *lpu16NumberOfBytes  = mu16NumberOfBytesOfPhotoData;

            CGS_ASSERT(mu16NumberOfBytesOfPhotoData <= KI_PHOTO_SEGMENT_SIZE,
                       "mu16NumberOfBytesOfPhotoData <= KI_PHOTO_SEGMENT_SIZE");

            std::memcpy(lpvPhotoData, macPhotoBuffer, mu16NumberOfBytesOfPhotoData);

            mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
            CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0,
                       "!CgsNetwork::ReliableMessage::IsMessageValid()");
            return true;
        }

        return false;
    }
} // namespace BrnNetwork
