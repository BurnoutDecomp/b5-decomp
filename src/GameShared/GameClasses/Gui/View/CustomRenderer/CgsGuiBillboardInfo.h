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
        f32 mfPosX;            // +0  (DWARF mv2Pos.x -- vpu Vector2)
        f32 mfPosY;            // +4  (DWARF mv2Pos.y)
        u8  mau8PosPad[8];     // +8..15  (vpu Vector2 z/w lanes -- unused)
        f32 mfRotation;        // +16 (DWARF mfRotation)
        u8  mau8RotPad[12];    // +20..31 (align to the next 16-byte vpu Vector2)
        f32 mfSizeX;           // +32 (DWARF mv2Size.x)
        f32 mfSizeY;           // +36 (DWARF mv2Size.y)
        u8  mau8SizePad[8];    // +40..47
        u32 muDiffuse;         // +48 (DWARF mcDiffuse -- packed RGBA colour word)
        s32 miTextureFrame;    // +52 (DWARF miTextureFrame)
        u8  mau8Tail[8];       // +56..63
    };

    // The element stride the Array<BillboardInfo,32> bodies copy/index (Append's 8-qword std loop,
    // GetItem's `idx << 6`) must stay exactly 64 bytes.
    struct BillboardInfo_AssertLayout
    {
        static void Check() { static_assert(sizeof(BillboardInfo) == 64, "BillboardInfo stride must be 64"); }
    };
}
