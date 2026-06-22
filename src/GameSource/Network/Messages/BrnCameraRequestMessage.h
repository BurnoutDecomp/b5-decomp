// ===================================================================================
// BrnNetwork::CameraRequestMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnCameraRequestMessage.h
//
// A CgsNetwork::ReliableMessage subclass carrying a single "release feed" flag.
// Class shape from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Messages/BrnCameraRequestMessage.h);
// the base (CgsNetwork::ReliableMessage) is the committed home in CgsMessage.h and is
// reused BY NAME here -- not forked.
//
// LEDGER FUNCTION reconstructed in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::CameraRequestMessage::GetName  @ 0x827DFD80
//     -> returns the literal "Camera Request Message" (lis/addi a rodata string, blr).
//        No member or base access.
//
// The other declared methods (Construct/Destruct/PrepareForSend/Retrieve/
// GetPackedMessageSize/PackOrUnpack) live in the sibling BrnCameraRequestMessage.cpp TU
// and are declared here for class shape but NOT bodied in this TU.
#pragma once

#include "types.hpp"                                                       // bool, s32, u16
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"  // CgsNetwork::ReliableMessage (committed base)

namespace BrnNetwork
{
    // DWARF BrnCameraRequestMessage.h:43.
    struct CameraRequestMessage : public CgsNetwork::ReliableMessage
    {
    private:
        bool mbReleaseFeed;   // DWARF BrnCameraRequestMessage.h:78

    public:
        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void Construct();
        void Destruct();
        void PrepareForSend(u16 lu16Frame, bool lbReleaseFeed);
        bool Retrieve(bool* lpbReleaseFeed);
        virtual s32 GetPackedMessageSize();

        // LEDGER func @ 0x827DFD80 -- bodied in this TU.
        virtual const char* GetName() const;

    protected:
        virtual CgsNetwork::PackOrUnpackResult PackOrUnpack();
    };

    // BrnNetwork::CameraRequestMessage::GetName  @ 0x827DFD80
    inline const char* CameraRequestMessage::GetName() const
    {
        return "Camera Request Message";
    }
} // namespace BrnNetwork
