#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "rw/rwcore_structs.h"

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
// Interior layout recovered from CgsGui::BillboardRenderer::Render @0x828577E0 (the first in-scope
// body that reads the sub-fields), cross-checked against the DWARF member list (DWARF names in
// parentheses). The position/size are 16-byte vpu Vector2s on the guest (only x/y are used), which
// is why the guest offsets are 16-aligned (pos@0, rotation@16, size@32); modelled here as plain
// scalar fields with explicit padding so the element stays a byte-exact 64-byte POD (the
// Array<BillboardInfo,32> copy/index stride is unchanged). Access is by name.
namespace CgsGui
{
    struct BillboardInfo
    {
        BillboardInfo() : mcDiffuse(rw::RGBA::NoInitTag()) {}
        Vector2 mv2Pos;
        f32 mfRotation;
        Vector2 mv2Size;
        rw::RGBA mcDiffuse;
        s32 miTextureFrame;
    };

    // The element stride the Array<BillboardInfo,32> bodies copy/index (Append's 8-qword std loop,
    // GetItem's `idx << 6`) must stay exactly 64 bytes.
    struct BillboardInfo_AssertLayout
    {
        static void Check() { CGS_STATIC_ASSERT((sizeof(BillboardInfo) == 64), "BillboardInfo stride must be 64"); }
    };
}
