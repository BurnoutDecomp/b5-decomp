#include "types.hpp"

#include <cstring>   // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)

#include "GameSource/Network/Messages/BrnImageMessage.h"
#include "GameSource/Network/BrnNetworkManager.h"                                       // BrnNetworkManager::PackOrUnpack (NetworkPlayerID field primitive)
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"              // PackOrUnpackU8/U16/CgsID/Buffer field primitives
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"      // ReliableMessage::PackOrUnpack base
#include "GameShared/GameClasses/Core/CgsAssert.h"                                      // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"                            // CgsDev::StrStream (streamed asserts)
#include "GameShared/GameClasses/Network/Players/CgsReliableMessageManager.h"          // KI_MAX_RELIABLE_MESSAGE_SIZE

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
// then delegates to ReliableMessage::GetPackedMessageSize (the tail call carries the
// TestConnectionMessage name only because the two bodies are identically folded).
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
    namespace
    {
        // The image message type id.
        const s32 KI_IMAGE_MESSAGE_TYPE = 19;

        // The largest packed image message that still fits a packet alongside the rest.
        const s32 KI_MAX_PACKED_IMAGE_MESSAGE_SIZE = 1000;
    }

    // The image message is the largest reliable message: a resend slot must hold a whole copy.
    static_assert(sizeof(ImageMessage) <= CgsNetwork::ReliableMessageManager::KI_MAX_RELIABLE_MESSAGE_SIZE,
                  "sizeof(ImageMessage) <= KI_MAX_RELIABLE_MESSAGE_SIZE");

    // Check the worst-case packed size still fits a packet, then reset the base reliable
    // fields and the payload (no players, no road, image type "none").
    void ImageMessage::Construct()
    {
        if (GetPackedMessageSize() > KI_MAX_PACKED_IMAGE_MESSAGE_SIZE)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "We are " << GetPackedMessageSize() << " but we can only fit "
                    << KI_MAX_PACKED_IMAGE_MESSAGE_SIZE << " in a packet.";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        MessageWithPlayerIDs::Construct();
        mImageSenderPlayerID         = CgsNetwork::MessageWithPlayerIDs::KI_INVALID_PLAYER_ID;
        mImageReceiverPlayerID       = CgsNetwork::MessageWithPlayerIDs::KI_INVALID_PLAYER_ID;
        mBeatenRoadID                = 0;
        mu16PhotoPacketNumber        = 0;
        mu16TotalPhotoPacketCount    = 0;
        mu8ImageType                 = BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_COUNT;
        mu16NumberOfBytesOfPhotoData = 0;
    }

    // Stamp one photo segment for sending (unless the previous one is still waiting): the
    // header fields, the range reports for the segment numbering and size, the segment bytes,
    // then the reliable base stamps type 19 for the frame.
    void ImageMessage::PrepareForSend(u16 lu16Frame, NetworkPlayerID lSenderPlayerID,
                                      NetworkPlayerID lReceiverPlayerID,
                                      BrnGameState::GameStateModuleIO::EImageType leImageType,
                                      CgsID lBeatenRoadID, u16 lu16PacketNumber,
                                      u16 lu16TotalPacketCount, u16 lu16NumberOfBytes,
                                      void* lpvPhotoData)
    {
        if (IsMessageValid())
        {
            return;
        }

        mu8ImageType                 = static_cast<u8>(leImageType);
        mImageSenderPlayerID         = lSenderPlayerID;
        mImageReceiverPlayerID       = lReceiverPlayerID;
        mBeatenRoadID                = lBeatenRoadID;
        mu16PhotoPacketNumber        = lu16PacketNumber;
        mu16TotalPhotoPacketCount    = lu16TotalPacketCount;
        mu16NumberOfBytesOfPhotoData = lu16NumberOfBytes;

        if ((lu16PacketNumber != 0 || lu16TotalPacketCount != 0) && !(lu16PacketNumber < lu16TotalPacketCount))
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Trying to send packet number " << static_cast<s32>(lu16PacketNumber)
                    << " when the max packet number is " << static_cast<s32>(lu16TotalPacketCount);
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        if (!(lu16TotalPacketCount < KI_MAX_PHOTO_PACKETS))
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "This photo has been broken into " << static_cast<s32>(lu16TotalPacketCount)
                    << " and exceeded the maximum packet count of " << KI_MAX_PHOTO_PACKETS;
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        if (lu16NumberOfBytes > KI_PHOTO_SEGMENT_SIZE)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Trying to pack " << static_cast<s32>(lu16NumberOfBytes)
                    << " into a dirty trick message whose photo buffer is " << KI_PHOTO_SEGMENT_SIZE;
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        // Bytes and a buffer go together: some bytes from no buffer, or no bytes from a
        // buffer, is reported (the bytes are still copied whenever there is a buffer).
        if ((lu16NumberOfBytes != 0) != (lpvPhotoData != nullptr))
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Either we are trying to copy some data (" << static_cast<s32>(lu16NumberOfBytes)
                    << " bytes) from a NULL pointer (" << lpvPhotoData
                    << ") or we are trying to copy no data (" << static_cast<s32>(lu16NumberOfBytes)
                    << " bytes) from a valid pointer(" << lpvPhotoData << ")";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        if (lpvPhotoData != nullptr)
        {
            std::memcpy(macPhotoBuffer, lpvPhotoData, lu16NumberOfBytes);
        }

        CgsNetwork::ReliableMessage::PrepareForSend(KI_IMAGE_MESSAGE_TYPE, lu16Frame);
        CGS_ASSERT(IsReliable(), "IsReliable()");
    }

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

        // Tail call to the reliable-base size probe (identically folded with
        // TestConnectionMessage::GetPackedMessageSize).
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    // BrnNetwork::ImageMessage::PackOrUnpack @ 0x8257C100
    CgsNetwork::PackOrUnpackResult ImageMessage::PackOrUnpack()
    {
        CgsNetwork::PackOrUnpackResult lxResult = CgsNetwork::ReliableMessage::PackOrUnpack();

        lxResult |= CgsNetwork::Message::PackOrUnpack(&mImageSenderPlayerID);
        lxResult |= CgsNetwork::Message::PackOrUnpack(&mImageReceiverPlayerID);
        lxResult |= CgsNetwork::PackOrUnpackU8(this, &mu8ImageType, 0, 6);
        lxResult |= CgsNetwork::PackOrUnpackU16(this, &mu16PhotoPacketNumber, 0, KI_MAX_PHOTO_PACKETS);
        lxResult |= CgsNetwork::PackOrUnpackU16(this, &mu16TotalPhotoPacketCount, 0, KI_MAX_PHOTO_PACKETS);
        lxResult |= CgsNetwork::PackOrUnpackU16(this, &mu16NumberOfBytesOfPhotoData, 0, KI_PHOTO_SEGMENT_SIZE);
        lxResult |= CgsNetwork::PackOrUnpackCgsID(this, &mBeatenRoadID);

        CGS_ASSERT(mu16NumberOfBytesOfPhotoData <= KI_PHOTO_SEGMENT_SIZE,
                   "mu16NumberOfBytesOfPhotoData <= KI_PHOTO_SEGMENT_SIZE");

        lxResult |= CgsNetwork::Message::PackOrUnpackBuffer(reinterpret_cast<char*>(macPhotoBuffer),
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

    // An older message is still accepted (slot 1 is the shared `return true` leaf).
    bool ImageMessage::OldMessagesAreValid() const
    {
        return true;
    }
} // namespace BrnNetwork
