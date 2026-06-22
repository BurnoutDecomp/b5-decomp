#pragma once

// ===================================================================================
// BrnNetwork::ImageMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnImageMessage.h
//
// A CgsNetwork::ReliableMessage subclass that ferries a segment of photo (mugshot)
// data between players. Class shape is taken from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Messages/BrnImageMessage.h) and
// gated on the X360 binary. The base CgsNetwork::ReliableMessage is the committed home
// in CgsReliableMessage.h and is reused BY NAME here -- not forked.
//
// LEDGER FUNCTION reconstructed in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::ImageMessage::GetName  @ 0x827DFD20
//     -> returns the literal "Image Message" (lis/addi a rodata string, blr).
//        No member or base access.
//
// The remaining declared methods (Construct/Destruct/PrepareForSend/Retrieve/
// GetPackedMessageSize/OldMessagesAreValid/PackOrUnpack) live in the sibling .cpp TU
// (BrnImageMessage.cpp) and are declared here for the class shape but NOT bodied in
// this TU. The two player-id fields are the committed BrnNetwork::NetworkPlayerID
// typedef (the DWARF spells it RoadRulesRecvData::NetworkPlayerID -- the same alias),
// reused here by name; the image-type enum is the committed
// BrnGameState::GameStateModuleIO::EImageType.
// ===================================================================================

#include "types.hpp"                                                                // s32, u16, u8, bool
#include "GameShared/GameClasses/Core/CgsID.h"                                       // CgsID (committed typedef)
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"   // CgsNetwork::ReliableMessage (committed base)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                         // BrnNetwork::NetworkPlayerID (committed typedef)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                              // BrnGameState::GameStateModuleIO::EImageType (committed home)

namespace BrnNetwork
{
    struct ImageMessage : public CgsNetwork::ReliableMessage
    {
        // DWARF BrnImageMessage.h:92-93 -- segmentation bounds for the photo stream.
        static const s32 KI_MAX_PHOTO_PACKETS  = 50;
        static const s32 KI_PHOTO_SEGMENT_SIZE = 500;

        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void          Construct();
        void          Destruct();
        void          PrepareForSend(u16 lu16Frame, NetworkPlayerID lSenderPlayerID,
                                     NetworkPlayerID lReceiverPlayerID,
                                     BrnGameState::GameStateModuleIO::EImageType leImageType,
                                     CgsID lBeatenRoadID, u16 lu16PacketNumber,
                                     u16 lu16TotalPacketCount, u16 lu16NumberOfBytes,
                                     void* lpvPhotoData);
        bool          Retrieve(NetworkPlayerID* lpSenderPlayerID, NetworkPlayerID* lpReceiverPlayerID,
                               BrnGameState::GameStateModuleIO::EImageType* lpeImageType,
                               CgsID* lpBeatenRoadID, u16* lpu16PacketNumber,
                               u16* lpu16TotalPacketCount, u16* lpu16NumberOfBytes,
                               void* lpvPhotoData);
        virtual s32   GetPackedMessageSize();        // DWARF :118 (sibling .cpp)

        // LEDGER func @ 0x827DFD20 -- bodied in this TU (DWARF BrnImageMessage.h:123).
        virtual const char* GetName() const;

        virtual bool  OldMessagesAreValid() const;   // DWARF :130 (sibling .cpp)

    protected:
        virtual CgsNetwork::PackOrUnpackResult PackOrUnpack();

    private:
        char            macPhotoBuffer[KI_PHOTO_SEGMENT_SIZE];  // DWARF :103
        CgsID           mBeatenRoadID;                          // DWARF :105
        NetworkPlayerID mImageSenderPlayerID;                   // DWARF :107
        NetworkPlayerID mImageReceiverPlayerID;                 // DWARF :108
        u16             mu16PhotoPacketNumber;                  // DWARF :110
        u16             mu16TotalPhotoPacketCount;              // DWARF :111
        u16             mu16NumberOfBytesOfPhotoData;           // DWARF :112
        u8              mu8ImageType;                           // DWARF :113
    };

    // BrnNetwork::ImageMessage::GetName  @ 0x827DFD20
    //   lis/addi a rodata string literal, blr -- no member or base access.
    inline const char* ImageMessage::GetName() const
    {
        return "Image Message";
    }
} // namespace BrnNetwork
