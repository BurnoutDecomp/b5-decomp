#pragma once

// ===================================================================================
// BrnNetwork::PlayerMenuData -- owning header
//   b5-decomp/src/GameSource/Network/BrnNetworkPlayerMenuData.h
//
// The Burnout-specific lobby/menu view of a player. SHAPE is taken from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/GameSource/Network/BrnNetworkPlayerMenuData.h:45):
//       struct BrnNetwork::PlayerMenuData : public CgsNetwork::PlayerMenuData { ... }
// reusing the committed CgsNetwork::PlayerMenuData base (CgsNetworkPlayer.h) BY NAME.
//
// X360 offset note (32-bit layout the ARTIST asm dereferences): the base
// CgsNetwork::PlayerMenuData occupies +0x00..+0x1F, then the four CgsID(u64) ids fill
// +0x20..+0x3F, so mMarkedManID lands at +0x40 -- exactly the slot
// BrnNetworkMarkedManManager.cpp stores the marked-man id into (`stw r11, 0x40(rN)` in
// _MarkedManMessageArrivedCallback @0x82548E90, and `lwz r5, 0x40(r28)` in
// SendMarkedManDataToAll @0x82548DC8). Every field below is accessed BY NAME; the C++
// layout uses native widths (semantic parity, not byte-exact X360 layout).
//
// The two KU_* sentinels and the colour/finish/car-selection fields are DWARF-attested
// members; Clear() is the virtual override homed in BrnNetworkPlayerMenuData.cpp's own TU
// (declared here for class shape, NOT bodied in this TU).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"                                 // CgsID (== u64)
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"           // CgsNetwork::PlayerMenuData (base), NetworkPlayerID
#include "GameSource/Network/Messages/BrnCameraStatusMessage.h"                // BrnNetwork::ECameraStatus

namespace BrnNetwork
{
    struct PlayerMenuData : public CgsNetwork::PlayerMenuData
    {
        // DWARF BrnNetworkPlayerMenuData.h:48-49.
        static const u16 KU_INVALID_COLOUR_INDEX       = 65535;
        static const u16 KU_INVALID_PAINT_FINISH_INDEX = 65535;

        // DWARF BrnNetworkPlayerMenuData.h:51-61.
        CgsID                       mFreeBurnCarId;               // :51
        CgsID                       mFreeBurnWheelId;             // :52
        CgsID                       mCarId;                       // :53
        CgsID                       mWheelId;                     // :54
        CgsNetwork::NetworkPlayerID mMarkedManID;                 // :55  (+0x40)
        ECameraStatus               meCameraStatus;               // :56
        u16                         mu16FreeburnCarColourIndex;   // :57
        u16                         mu16FreeburnPaintFinishIndex; // :58
        u16                         mu16CarColourIndex;           // :59
        u16                         mu16PaintFinishIndex;         // :60
        bool                        mbFinalCarSelection;          // :61

        // Virtual override (DWARF BrnNetworkPlayerMenuData.cpp:27); bodied in its own TU.
        virtual void Clear();
    };
} // namespace BrnNetwork
