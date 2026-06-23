#pragma once

// ===================================================================================
// BrnGui::BoostMessageSlot  -- owning header for the boost-message slot array element
//   b5-decomp/src/GameSource/Gui/CustomRenderer/Renderers/BrnBoostMessageSlot.h
//
// The boost-message manager keeps a fixed array of up to 3 message slots. Each slot is a
// 68-byte (0x44) record. The container is the committed generic Array<T,N> (CgsArray.h)
// instantiated as Array<BoostMessage, 3>:
//   * the element stride is 0x44 (X360 Append @0x8241D600 does `mulli r11, count, 0x44`
//     then `memcpy(this + 0x44*count, src, 0x44)`),
//   * the live-element count word sits at this+0xCC == 0x44 * 3, i.e. immediately after
//     the 3-element buffer (the committed Array<T,N> shape: elements then count),
//   * GetItem @0x8241DD08 returns `0x44*index + this`, i.e. &maElements[index].
//
// The per-slot record fields are not individually recoverable from the two array methods
// in this TU, so BoostMessage is modelled as a sized POD (semantic parity: the array
// methods copy/return the whole record). All access is by name.
// ===================================================================================

#include "types.hpp"

namespace BrnGui
{
    // One boost-message slot record. 0x44 bytes (the X360 Append element stride / memcpy
    // size). POD so the committed Array<T,N>::Append element-assignment lowers to the
    // record copy the X360 performs.
    struct BoostMessage
    {
        u8 maRecord[0x44];   // 68-byte slot record (fields not reconstructed by this TU)
    };
}
