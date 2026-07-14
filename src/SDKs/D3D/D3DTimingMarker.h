#pragma once

// ===========================================================================
// D3D::TimingMarker -- a one-word PIX "begin timing" marker object inside
// BURNOUT_X360_ARTIST.XEX. This header is the canonical OWNING home for the
// TimingMarker TYPE and its single attested method (bodied in
// D3DTimingMarker.cpp):
//
//     D3D::TimingMarker::TimingMarker  @ 0x82938630   (ctor)
//
// A TimingMarker is a trivial object that the D3D device tiling/clear paths
// (D3DDevice_BeginTiling / D3DDevice_EndTiling / D3DDevice_Clear) construct to
// stamp a named marker into the PIX (Performance Investigator for Xbox) op
// stream. The ctor records the marker value and emits one PIX begin-op
// (marker | 0x800000). Sibling of D3D::CBlocker (D3DBlocker.h), which uses the
// same PIXAddPixOpWithData begin/end idiom.
//
// `D3D` is the X360 graphics-SDK boundary, so its identifiers (TimingMarker)
// are preserved verbatim per the naming convention.
//
// LAYOUT -- there is NO DWARF and NO reference source for this TU. The single
// 32-bit member below is reconstructed purely from the store the ctor emits in
// the X360 asm (`stw r30, 0(r31)`): word[0] holds the marker value.
// ===========================================================================

#include "types.hpp"

namespace D3D
{

class TimingMarker
{
public:
    // @ 0x82938630 -- record the marker value and emit a PIX begin-op
    // (marker | 0x800000).
    explicit TimingMarker(s32 iMarker);

private:
    s32 miMarker;   // +0x00  marker value / PIX op payload (word[0])

    friend void _TimingMarkerAssertLayout();
};

} // namespace D3D
