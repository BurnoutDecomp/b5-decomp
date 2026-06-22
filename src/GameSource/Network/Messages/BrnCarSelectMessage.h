// ===================================================================================
// BrnNetwork::CarSelectMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnCarSelectMessage.h
//
// A CgsNetwork::ReliableMessage subclass carrying a player's car/wheel/livery choice.
// Class shape from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Messages/BrnCarSelectMessage.h);
// the base (CgsNetwork::ReliableMessage) is the committed home in CgsMessage.h and is
// reused BY NAME here -- not forked. CgsID is the committed core id type.
//
// LEDGER FUNCTION reconstructed in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::CarSelectMessage::GetName  @ 0x827DFBA8
//     -> returns the literal "Car Select Message" (lis/addi a rodata string, blr).
//        No member or base access.
//
// The other declared methods (Construct/Destruct/PrepareForSend/Retrieve/
// GetPackedMessageSize/PackOrUnpack) live in the sibling BrnCarSelectMessage.cpp TU and
// are declared here for class shape but NOT bodied in this TU.
#pragma once

#include "types.hpp"                                                       // bool, s32, u16
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"  // CgsNetwork::ReliableMessage (committed base; moved out of CgsMessage.h)
#include "GameShared/GameClasses/Core/CgsID.h"                             // CgsID (committed core id)

namespace BrnNetwork
{
    // DWARF BrnCarSelectMessage.h:46.
    struct CarSelectMessage : public CgsNetwork::ReliableMessage
    {
    private:
        CgsID mCarId;                  // DWARF :91
        CgsID mWheelId;                // DWARF :92
        u16   mu16ColourIndex;         // DWARF :93
        u16   mu16PaintFinishIndex;    // DWARF :94
        bool  mbFinalSelection;        // DWARF :95

    public:
        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void Construct();
        void Destruct();
        void PrepareForSend(u16 lu16Frame, CgsID lCarId, CgsID lWheelId,
                            u16 lu16ColourIndex, u16 lu16PaintFinishIndex, bool lbFinalSelection);
        bool Retrieve(CgsID* lpCarId, CgsID* lpWheelId,
                      u16* lpu16ColourIndex, u16* lpu16PaintFinishIndex, bool* lpbFinalSelection);
        virtual s32 GetPackedMessageSize();

        // LEDGER func @ 0x827DFBA8 -- bodied in this TU.
        virtual const char* GetName() const;

    protected:
        virtual CgsNetwork::PackOrUnpackResult PackOrUnpack();
    };

    // BrnNetwork::CarSelectMessage::GetName  @ 0x827DFBA8
    inline const char* CarSelectMessage::GetName() const
    {
        return "Car Select Message";
    }
} // namespace BrnNetwork
