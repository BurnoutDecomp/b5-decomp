#pragma once

// ===========================================================================
// D3D::GPUTimingMarkerSmall -- a lightweight GPU timing marker scope object
// inside BURNOUT_X360_ARTIST.XEX. This header is the canonical OWNING home for
// the GPUTimingMarkerSmall TYPE and its two attested methods (bodied in
// GPUTimingMarkerSmall.cpp):
//
//     D3D::GPUTimingMarkerSmall::GPUTimingMarkerSmall  @ 0x829442B0  (ctor)
//     D3D::GPUTimingMarkerSmall::~GPUTimingMarkerSmall @ 0x82944348  (dtor)
//
// A GPUTimingMarkerSmall is a short-lived object the D3D draw path
// (D3DDevice_DrawVertices, D3DDevice_DrawIndexedVertices) constructs to bracket
// a GPU timing span around a draw. It is the "small" sibling of
// D3D::GPUTimingMarkerFull (GPUTimingMarkerFull.h): both hold a PIX op token and
// a pointer to the owning device and only act when the device has GPU timing
// enabled (CDevice::muGpuTimingEnabled @ +0x5424). The differences recovered
// from the asm are:
//
//   * FIELD ORDER is reversed vs the Full variant: word[0] = CDevice*,
//     word[1] = op token (Full is op token, then CDevice*).
//   * Instead of the out-of-line PIXAddPixOpWithData call the Full/TimingMarker
//     variants use, the Small variant INLINES the PIX CPU "small op" fast path
//     -- a masked-interrupt bump-append into the calling thread's PIX small-op
//     ring, falling out to D3D::PixCpuAddSmall_Next when the page is full.
//   * The begin token is the op as-is; the end token is op + 1 (the Full
//     variant OR's 0x400000 for its end-op instead).
//
// `D3D` is the X360 graphics-SDK boundary, so its identifiers
// (GPUTimingMarkerSmall) are preserved verbatim per the naming convention.
//
// LAYOUT -- there is NO DWARF and NO reference source for this TU. The two-word
// object shape below is reconstructed purely from the loads/stores the ctor and
// dtor emit in the X360 asm (word[0] = CDevice*, word[1] = PIX op token). The
// device field touched here (muGpuTimingEnabled @ +0x5424) is a named member of
// D3D::CDevice (see CDevice.h) reached through mpDevice -- GPUTimingMarkerSmall
// is a `friend` of CDevice; nothing is offset-hacked.
// ===========================================================================

#include "types.hpp"

namespace D3D
{

class CDevice; // GPUTimingMarkerSmall only holds a CDevice* -- forward decl suffices.

class GPUTimingMarkerSmall
{
public:
    // @ 0x829442B0 -- record the owning device, and IF it has GPU timing
    // enabled, record the op token and emit the PIX "small op" begin token.
    GPUTimingMarkerSmall(s32 iOp, CDevice* pDevice);

    // @ 0x82944348 -- IF the device has GPU timing enabled, emit the PIX
    // "small op" end token (op + 1) and append the span via D3D::PixGpuAdd.
    ~GPUTimingMarkerSmall();

private:
    CDevice* mpDevice;  // +0x00  owning device (word[0]) -- X360 offset; host widens
    u32      muOp;      // +0x04  PIX op token (word[1]); only written when timing is on

    friend void _GPUTimingMarkerSmallAssertLayout();
};

} // namespace D3D
