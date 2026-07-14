#include "SDKs/D3D/GPUTimingMarkerFull.h"
#include "SDKs/D3D/CDevice.h"

#include <cstddef> // offsetof (uncalled _GPUTimingMarkerFullAssertLayout)

// ===========================================================================
// D3D::GPUTimingMarkerFull -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// GPUTimingMarkerFull.h for the object layout; the device field touched here is
// a named member of D3D::CDevice (CDevice.h), reached through mpDevice via a
// friend declaration.
//
// Source-of-truth: X360 asm only (no DWARF, no reference source). The single
// device flag load, the guard branch, the PIX end-op and the PixGpuAdd append
// are reproduced faithfully in the asm's order with no added behaviour and no
// inverted branches.
// ===========================================================================

// --- External X360 platform symbols referenced below -----------------------
// PIX (Performance Investigator for Xbox) profiling op stream. Platform API.
extern "C" int PIXAddPixOpWithData(unsigned int uOp, int a2, int a3);

namespace D3D
{

// Append the just-closed GPU timing span to the device's PIX GPU timing stream.
// Separate D3D TU (not yet homed). The asm passes the device in r3 only; the
// return value is discarded by this destructor.
s32 PixGpuAdd(CDevice* pDevice);

// --- @ 0x829443E8 ----------------------------------------------------------
// End the GPU timing span. When the device has GPU timing enabled, emit the PIX
// end-op for this marker's op token and append the span; otherwise do nothing.
GPUTimingMarkerFull::~GPUTimingMarkerFull()
{
    if (mpDevice->muGpuTimingEnabled != 0)   // lwz 4(this) ; lwz 0x5424(dev) ; beq
    {
        PIXAddPixOpWithData(muOp | 0x400000u, 0, 0); // oris muOp,0x40 ; li 0,0
        PixGpuAdd(mpDevice);                         // r3 = 4(this)
    }
}

// Layout facts (uncalled). The op token is the first word; the device pointer
// follows it. On the X360 the object is 8 bytes with a 4-byte pointer; on the
// LLP64 host the pointer widens to 8, so only field ORDER is pinned, not the
// X360-absolute byte offsets (every access above is by name).
void _GPUTimingMarkerFullAssertLayout()
{
    static_assert(offsetof(GPUTimingMarkerFull, muOp) == 0,
                  "op token is the first word");
    static_assert(offsetof(GPUTimingMarkerFull, mpDevice) > offsetof(GPUTimingMarkerFull, muOp),
                  "device pointer follows the op token");
}

} // namespace D3D
