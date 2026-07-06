#include "types.hpp"
#include "GameSource/Network/Messages/BrnCarSelectMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::CarSelectMessage::GetPackedMessageSize @ 0x8257A538
//   BrnNetwork::CarSelectMessage::PackOrUnpack         @ 0x8257A560
//   BrnNetwork::CarSelectMessage::PrepareForSend       @ 0x8257A498
//   BrnNetwork::CarSelectMessage::Retrieve             @ 0x8257DFE8
// (GetName @ 0x827DFBA8 is inline in the header.)
//
// A reliable message carrying a player's car/wheel/livery choice. See the header for the
// layout; the base ReliableMessage adds no data, so the first leaf member is at +0x28.

namespace BrnNetwork
{
    // Message-type id (asm `li r4, 0xF` in PrepareForSend).
    static const s32 KI_CAR_SELECT_MESSAGE_TYPE = 15;

    // Wire field ranges (asm-attested in PackOrUnpack): colour index [0, 200], paint-finish
    // index [0, 4], base deformation amount [0.0, 1.0] quantised at 0.01.
    static const s32 KI_MIN_LIVERY_INDEX         = 0;
    static const s32 KI_MAX_COLOUR_INDEX         = 200;   // r6 = 0xC8
    static const s32 KI_MAX_PAINT_FINISH_INDEX   = 4;
    static const f32 KF_MIN_DEFORMATION_AMOUNT   = 0.0f;
    static const f32 KF_MAX_DEFORMATION_AMOUNT   = 1.0f;
    static const f32 KF_DEFORMATION_RESOLUTION   = 0.01f;

    // ---------------------------------------------------------------------------------
    // GetPackedMessageSize @ 0x8257A538
    //   Re-zeroes the whole payload (ids, deformation amount, the two u16 livery indices
    //   and the final-selection flag) so the size query measures a clean worst case, then
    //   tail-calls the (COMDAT/ICF-folded) base reliable-message size query.
    // ---------------------------------------------------------------------------------
    s32 CarSelectMessage::GetPackedMessageSize()
    {
        mfBaseDeformationAmount = 0.0f;   // stfs flt_82001CC0(=0.0), +0x38
        mCarId                  = 0;      // std 0, +0x28
        mWheelId                = 0;      // std 0, +0x30
        mu16ColourIndex         = 0;      // sth 0, +0x3C
        mu16PaintFinishIndex    = 0;      // sth 0, +0x3E
        mbFinalSelection        = false;  // stb 0, +0x40

        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    // ---------------------------------------------------------------------------------
    // PackOrUnpack @ 0x8257A560
    //   (De)serialises the payload; each per-field status is OR-ed into the running result
    //   (0 == all succeeded). Wire order: base reliable id, car id, wheel id, colour index,
    //   paint-finish index, base deformation amount, final-selection flag.
    // ---------------------------------------------------------------------------------
    CgsNetwork::PackOrUnpackResult CarSelectMessage::PackOrUnpack()
    {
        CgsNetwork::PackOrUnpackResult lxResult = CgsNetwork::ReliableMessage::PackOrUnpack();
        lxResult = CgsNetwork::PackOrUnpackCgsID(this, &mCarId) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackCgsID(this, &mWheelId) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackU16(this, &mu16ColourIndex, KI_MIN_LIVERY_INDEX, KI_MAX_COLOUR_INDEX) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackU16(this, &mu16PaintFinishIndex, KI_MIN_LIVERY_INDEX, KI_MAX_PAINT_FINISH_INDEX) | lxResult;
        CgsNetwork::PackOrUnpackResult lxFloat =
            CgsNetwork::PackOrUnpackFloat(this, &mfBaseDeformationAmount,
                                          KF_MIN_DEFORMATION_AMOUNT, KF_MAX_DEFORMATION_AMOUNT,
                                          KF_DEFORMATION_RESOLUTION);
        return CgsNetwork::PackOrUnpackBool(this, &mbFinalSelection) | (lxFloat | lxResult);
    }

    // ---------------------------------------------------------------------------------
    // PrepareForSend @ 0x8257A498
    //   Only stamps the send if the slot is not already VALID. Stores the car / wheel ids,
    //   the livery indices, the base deformation amount and the final-selection flag, stamps
    //   the reliable send (wire type 15 + frame) through the base, then asserts the message
    //   came out reliable.
    // ---------------------------------------------------------------------------------
    void CarSelectMessage::PrepareForSend(u16 lu16Frame, CgsID lCarId, CgsID lWheelId,
                                          u16 lu16ColourIndex, u16 lu16PaintFinishIndex,
                                          f32 lfBaseDeformationAmount, bool lbFinalSelection)
    {
        if (IsMessageValid())
            return;

        mfBaseDeformationAmount = lfBaseDeformationAmount;   // stfs f1, +0x38
        mCarId                  = lCarId;                    // std, +0x28
        mWheelId                = lWheelId;                  // std, +0x30
        mu16ColourIndex         = lu16ColourIndex;           // sth, +0x3C
        mu16PaintFinishIndex    = lu16PaintFinishIndex;      // sth, +0x3E
        mbFinalSelection        = lbFinalSelection;          // stb, +0x40

        CgsNetwork::ReliableMessage::PrepareForSend(KI_CAR_SELECT_MESSAGE_TYPE, lu16Frame);
        CGS_ASSERT(IsReliable(), "IsReliable()");
    }

    // ---------------------------------------------------------------------------------
    // Retrieve @ 0x8257DFE8
    //   Asserts all six out-pointers are non-null. If the slot is VALID, copies the payload
    //   out, clears the VALID flag, asserts the flag is now clear, and returns true;
    //   otherwise returns false without touching the outputs.
    // ---------------------------------------------------------------------------------
    bool CarSelectMessage::Retrieve(CgsID* lpCarId, CgsID* lpWheelId,
                                    u16* lpu16CarColourIndex, u16* lpu16CarPaintFinishIndex,
                                    f32* lpfBaseDeformationAmount, bool* lpbFinalSelection)
    {
        CGS_ASSERT(lpCarId, "lpCarId");
        CGS_ASSERT(lpWheelId, "lpWheelId");
        CGS_ASSERT(lpbFinalSelection, "lpbFinalSelection");
        CGS_ASSERT(lpu16CarColourIndex, "lpu16CarColourIndex");
        CGS_ASSERT(lpu16CarPaintFinishIndex, "lpu16CarPaintFinishIndex");
        CGS_ASSERT(lpfBaseDeformationAmount, "lpfBaseDeformationAmount");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        *lpCarId                   = mCarId;                    // ld  +0x28
        *lpWheelId                 = mWheelId;                  // ld  +0x30
        *lpbFinalSelection         = mbFinalSelection;          // lbz +0x40
        *lpu16CarColourIndex       = mu16ColourIndex;           // lhz +0x3C
        *lpu16CarPaintFinishIndex  = mu16PaintFinishIndex;      // lhz +0x3E
        *lpfBaseDeformationAmount  = mfBaseDeformationAmount;   // lfs +0x38

        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0,
                   "!CgsNetwork::ReliableMessage::IsMessageValid()");
        return true;
    }
} // namespace BrnNetwork
