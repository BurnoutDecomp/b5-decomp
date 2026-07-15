#include "SDKs/D3D/GPUTimingMarkerSmall.h"
#include "SDKs/D3D/CDevice.h"

#include <cstddef> // offsetof (uncalled _GPUTimingMarkerSmallAssertLayout)

// ===========================================================================
// D3D::GPUTimingMarkerSmall -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// GPUTimingMarkerSmall.h for the object layout; the device field touched here is
// a named member of D3D::CDevice (CDevice.h), reached through mpDevice via a
// friend declaration.
//
// Source-of-truth: X360 asm only (no DWARF, no reference source). The device
// flag load, the guard branch, the inlined PIX "small op" fast path, the token
// math (begin = op, end = op + 1) and the PixGpuAdd append are reproduced
// faithfully in the asm's order with no added behaviour and no inverted
// branches. The PIX small-op fast path is inlined identically in both the ctor
// and the dtor asm; it is recovered here as the shared de-inlined helper
// PixCpuAddSmall (recovering inlined-helper shape).
// ===========================================================================

// --- External X360 platform symbols referenced below -----------------------
// PPC "move from time base" -- reads the CPU time-base counter. Platform
// intrinsic; the asm uses only the low 32 bits (`mftb r10`).
extern "C" u64 __mftb(void);

namespace D3D
{

// Out-of-line refill / slow path for the PIX small-op ring, taken when the
// current page is full. Separate D3D TU (not yet homed). Passed the op token in
// r3; the return value is discarded here.
s32 PixCpuAddSmall_Next(u32 uOp);

// Append the just-closed GPU timing span to the device's PIX GPU timing stream.
// Separate D3D TU (not yet homed). The asm passes the device in r3 only; the
// return value is discarded by the destructor.
s32 PixGpuAdd(CDevice* pDevice);

// De-inlined shape of the PIX "add small op" fast path that BOTH the ctor and
// the dtor inline in the X360 asm. Under a masked-interrupt critical section it
// bump-appends one packed word -- (time-base << 7) | op -- to the calling
// thread's PIX small-op ring, or falls to the out-of-line refill path
// D3D::PixCpuAddSmall_Next when the current page is full.
static void PixCpuAddSmall(u32 uOp)
{
    // The X360 keeps the ring cursors in the per-thread block, reached
    // r13-relative in the asm: +0x2B0 = write cursor (put), +0x2B4 = page end.
    // Only these two words of the thread block are touched by this TU; they are
    // owned/initialised by the PIX runtime, so they are modelled here as the
    // calling thread's own cursors rather than fabricated into a foreign layout.
    static thread_local u32* s_pPut = nullptr;  // r13+0x2B0  next write slot
    static thread_local u32* s_pEnd = nullptr;  // r13+0x2B4  end of current page

    // `mtmsree r13` -- disable external interrupts across the cursor update so
    // the append is atomic against the PIX drain interrupt. No-op on the host.
    u32* pPut = s_pPut;                 // lwz r11, 0x2B0(r13)
    u32* pEnd = s_pEnd;                 // lwz r10, 0x2B4(r13)
    if (pPut >= pEnd)                   // cmplw ; bge -> refill
    {
        // `mtmsree 0x8000` (re-enable EE) ; mr r3, op ; bl PixCpuAddSmall_Next
        PixCpuAddSmall_Next(uOp);
    }
    else
    {
        // mftb r10 ; slwi r10,r10,7 ; or r10,r10,op ; stw r10,0(put)
        *pPut = (static_cast<u32>(__mftb()) << 7) | uOp;
        s_pPut = pPut + 1;             // addi r9,put,4 ; stw r9,0x2B0(r13)
        // `mtmsree 0x8000` -- re-enable external interrupts.
    }
}

// --- @ 0x829442B0 ----------------------------------------------------------
// Record the owning device (stored unconditionally), then -- only when the
// device has GPU timing enabled -- record the op token and emit the PIX
// small-op begin token (the op value as-is).
GPUTimingMarkerSmall::GPUTimingMarkerSmall(s32 iOp, CDevice* pDevice)
{
    mpDevice = pDevice;                         // stw r30, 0(this)

    if (mpDevice->muGpuTimingEnabled != 0)      // lwz 0x5424(dev) ; cmpwi ; beq
    {
        muOp = static_cast<u32>(iOp);           // stw r31, 4(this)
        PixCpuAddSmall(static_cast<u32>(iOp));  // inlined small-op fast path (begin = op)
    }
}

// --- @ 0x82944348 ----------------------------------------------------------
// End the GPU timing span. When the device has GPU timing enabled, emit the PIX
// small-op end token (op + 1), then append the span via PixGpuAdd; otherwise do
// nothing.
GPUTimingMarkerSmall::~GPUTimingMarkerSmall()
{
    if (mpDevice->muGpuTimingEnabled != 0)      // lwz 0(this) ; lwz 0x5424(dev) ; beq
    {
        PixCpuAddSmall(muOp + 1u);              // addi r3, op, 1 ; inlined fast path (end = op + 1)
        PixGpuAdd(mpDevice);                    // lwz r3, 0(this) ; bl PixGpuAdd (result discarded)
    }
}

// Layout facts (uncalled). The device pointer is the first word; the op token
// follows it. On the X360 the object is 8 bytes with a 4-byte pointer; on the
// LLP64 host the pointer widens to 8, so only field ORDER is pinned, not the
// X360-absolute byte offsets (every access above is by name).
void _GPUTimingMarkerSmallAssertLayout()
{
    static_assert(offsetof(GPUTimingMarkerSmall, mpDevice) == 0,
                  "owning device is the first word");
    static_assert(offsetof(GPUTimingMarkerSmall, muOp) > offsetof(GPUTimingMarkerSmall, mpDevice),
                  "op token follows the device pointer");
}

} // namespace D3D
