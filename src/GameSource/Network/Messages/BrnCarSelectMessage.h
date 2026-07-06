// ===================================================================================
// BrnNetwork::CarSelectMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnCarSelectMessage.h
//
// A CgsNetwork::ReliableMessage subclass carrying a player's car/wheel/livery choice.
// Class shape from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Messages/BrnCarSelectMessage.h)
// gated against the X360 asm (BURNOUT_X360_ARTIST.XEX). The base
// (CgsNetwork::ReliableMessage) is the committed home in CgsReliableMessage.h and is
// reused BY NAME here -- not forked. CgsID is the committed core id type (typedef u64).
//
// LAYOUT (ReliableMessage adds no data of its own, so the first leaf member lands at
// +0x28; every store below is attested by the X360 GetPackedMessageSize / PrepareForSend
// / Retrieve / PackOrUnpack bodies):
//   +0x00  (CgsNetwork::ReliableMessage base, 0x28 bytes)
//   +0x28  CgsID mCarId                    (std / ld)
//   +0x30  CgsID mWheelId                  (std / ld)
//   +0x38  f32   mfBaseDeformationAmount   (stfs / lfs; PackOrUnpackFloat [0.0,1.0]@0.01)
//   +0x3C  u16   mu16ColourIndex           (sth / lhz; PackOrUnpackU16 [0,200])
//   +0x3E  u16   mu16PaintFinishIndex      (sth / lhz; PackOrUnpackU16 [0,4])
//   +0x40  bool  mbFinalSelection          (stb / lbz)
//
// NOTE ON DWARF DRIFT: the DecFIGS DWARF stub lists members mCarId, mWheelId,
// mu16ColourIndex, mu16PaintFinishIndex, mbFinalSelection and gives PrepareForSend/Retrieve
// five-value signatures. The X360 asm carries an ADDITIONAL f32 mfBaseDeformationAmount at
// +0x38 that the two methods pass a pointer/value for (Retrieve's :139 assert names it
// "lpfBaseDeformationAmount"); the asm is authoritative, so the float member + the sixth
// PrepareForSend/Retrieve value are restored here.
//
// LEDGER FUNCTION reconstructed in the header TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::CarSelectMessage::GetName  @ 0x827DFBA8
//     -> returns the literal "Car Select Message" (lis/addi a rodata string, blr).
//        (Pinned by the committed BrnCarSelectMessage_embed_check.cpp.)
//
// GetPackedMessageSize / PrepareForSend / Retrieve / PackOrUnpack are bodied in the sibling
// BrnCarSelectMessage.cpp TU. Construct/Destruct remain declared (own TUs, not in this batch).
// ===================================================================================
#pragma once

#include "types.hpp"                                                       // bool, s32, u16, f32
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"  // CgsNetwork::ReliableMessage (committed base)
#include "GameShared/GameClasses/Core/CgsID.h"                             // CgsID (committed core id, typedef u64)

namespace BrnNetwork
{
    // DWARF BrnCarSelectMessage.h:46. The X360 build models the vtable as the explicit
    // Message::mpVTable member (no C++ `virtual`), so these are plain methods -- adding a
    // C++ `virtual` here would inject a second vptr and shift every member off its
    // recovered byte offset.
    struct CarSelectMessage : public CgsNetwork::ReliableMessage
    {
    public:
        // Sibling-.cpp methods.
        void Construct();
        void Destruct();
        void PrepareForSend(u16 lu16Frame, CgsID lCarId, CgsID lWheelId,
                            u16 lu16ColourIndex, u16 lu16PaintFinishIndex,
                            f32 lfBaseDeformationAmount, bool lbFinalSelection);
        bool Retrieve(CgsID* lpCarId, CgsID* lpWheelId,
                      u16* lpu16CarColourIndex, u16* lpu16CarPaintFinishIndex,
                      f32* lpfBaseDeformationAmount, bool* lpbFinalSelection);
        s32  GetPackedMessageSize();

        // LEDGER func @ 0x827DFBA8 -- bodied inline below.
        const char* GetName() const;

    protected:
        CgsNetwork::PackOrUnpackResult PackOrUnpack();

    private:
        CgsID mCarId;                    // +0x28  DWARF :91
        CgsID mWheelId;                  // +0x30  DWARF :92
        f32   mfBaseDeformationAmount;   // +0x38  (asm-only; DWARF omitted)
        u16   mu16ColourIndex;           // +0x3C  DWARF :93
        u16   mu16PaintFinishIndex;      // +0x3E  DWARF :94
        bool  mbFinalSelection;          // +0x40  DWARF :95
    };

    // BrnNetwork::CarSelectMessage::GetName  @ 0x827DFBA8
    //   lis/addi a rodata string literal, blr -- no member or base access.
    inline const char* CarSelectMessage::GetName() const
    {
        return "Car Select Message";
    }
} // namespace BrnNetwork
