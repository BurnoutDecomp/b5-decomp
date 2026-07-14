#pragma once

// ===========================================================================
// D3D::GPUTimingMarkerFull -- a GPU timing marker scope object inside
// BURNOUT_X360_ARTIST.XEX. This header is the canonical OWNING home for the
// GPUTimingMarkerFull TYPE and its destructor (bodied in
// GPUTimingMarkerFull.cpp):
//
//     D3D::GPUTimingMarkerFull::~GPUTimingMarkerFull @ 0x829443E8  (dtor)
//
// A GPUTimingMarkerFull is a short-lived object that the D3D device present /
// resolve / clear / swap paths (D3DDevice_Swap, D3DDevice_Resolve,
// D3DDevice_ClearF, D3DDevice_InsertCallback, D3D::HandleCaptureState,
// D3D::SynchronizeToPresentationInterval) construct to bracket a GPU timing
// span. It carries the PIX op token to close the span with and a pointer to
// the owning device. On destruction, IF the device currently has GPU timing
// enabled (CDevice::muGpuTimingEnabled), it emits the PIX end-op
// (token | 0x400000) and appends the span to the device's GPU timing stream
// via D3D::PixGpuAdd.
//
// `D3D` is the X360 graphics-SDK boundary, so its identifiers
// (GPUTimingMarkerFull) are preserved verbatim per the naming convention.
//
// LAYOUT -- there is NO DWARF and NO reference source for this TU. The two-word
// object shape below is reconstructed purely from the loads the destructor
// emits in the X360 asm (word[0] = PIX op token, word[1] = CDevice*). The
// device field touched here (muGpuTimingEnabled @ +0x5424) is a named member of
// D3D::CDevice (see CDevice.h) reached through mpDevice -- GPUTimingMarkerFull
// is a `friend` of CDevice; nothing is offset-hacked.
//
// NOTE: only the destructor is present in this TU. The constructor is not in
// the ARTIST dossier for this class (inlined at its call sites), so it is not
// reconstructed here.
// ===========================================================================

#include "types.hpp"

namespace D3D
{

class CDevice; // GPUTimingMarkerFull only holds a CDevice* -- forward decl suffices.

class GPUTimingMarkerFull
{
public:
    // @ 0x829443E8 -- if the device has GPU timing enabled, emit the PIX end-op
    // (muOp | 0x400000) and append the span via D3D::PixGpuAdd(mpDevice).
    ~GPUTimingMarkerFull();

private:
    u32      muOp;      // +0x00  PIX op token, OR'd with 0x400000 for the end-op (word[0])
    CDevice* mpDevice;  // +0x04  owning device (word[1]) -- X360 offset; host widens

    friend void _GPUTimingMarkerFullAssertLayout();
};

} // namespace D3D
