#pragma once

#include "types.hpp"

// CgsGui::BillboardInfo - one billboard descriptor a GUI custom renderer collects into a
// fixed Array<BillboardInfo,32> before submitting (the X360 BrnGui::BoostBarRenderer
// render paths Append billboards into this array and read them back via GetItem/GetLength).
//
// Recovered from the X360 ARTIST binary via the Array<CgsGui::BillboardInfo,32> container
// bodies (Append @0x82448D48 / GetItem @0x82449550 / GetLength @0x82448E80):
//   - Element stride is 64 bytes: Append copies 8 qwords (`li r9,8; ld/std` loop) per element
//     and GetItem indexes with `slwi r..,idx,6` (idx * 64). So sizeof(BillboardInfo) == 64.
//   - The container's trailing live-count word lands at +0x800 (2048) == 32 * 64, fixing the
//     capacity at 32 and the element size at 64.
//
// FLAG (opaque element): BillboardInfo's interior fields are NOT attested by the DWARF or by
// any in-scope X360 body (the Array container copies/indexes whole 64-byte elements; nothing
// in scope reads a sub-field). It is modelled here as a correctly-sized 64-byte POD aggregate
// so the Array<BillboardInfo,32> stride/sizeof are exact (element copy = the X360's 8-qword
// std loop). When a body that reads BillboardInfo's interior lands, replace maRaw with the
// named fields additively (sizeof must stay 64).
namespace CgsGui
{
    struct BillboardInfo
    {
        // 64 bytes (eight 64-bit words) -- the exact element stride the Array<BillboardInfo,32>
        // bodies copy/index. Interior layout opaque (see FLAG above).
        u64 maRaw[8];
    };
}
