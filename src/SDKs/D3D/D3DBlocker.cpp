#include "SDKs/D3D/D3DBlocker.h"
#include "SDKs/D3D/CDevice.h"

#include <cstddef> // offsetof (uncalled _CBlockerAssertLayout)

// ===========================================================================
// D3D::CBlocker -- reconstructed from BURNOUT_X360_ARTIST.XEX. See D3DBlocker.h
// for the object layout; the device fields touched here are named members of
// D3D::CDevice (CDevice.h), reached through mpDevice via a friend declaration.
//
// Source-of-truth: X360 asm only (no DWARF, no reference source). All member
// stores/loads, the two 64-bit accumulator updates, the PIX begin/end ops and
// the profiling callback are reproduced faithfully in the asm's order with no
// added behaviour and no inverted branches.
// ===========================================================================

// --- External X360 platform symbols and .rodata referenced below -----------
// PIX (Performance Investigator for Xbox) profiling op stream. Platform API.
extern "C" int  PIXAddPixOpWithData(unsigned int uOp, int a2, int a3);
// Win32/X360 current-thread id. Platform API.
extern "C" unsigned long GetCurrentThreadId(void);

// PIX op-code table indexed by the block type (word[1]); lives in X360 .rodata.
// Its bytes are NOT recovered in this dossier, so it is referenced by name only.
extern const u32   dword_82102EE8[];
// Single-precision scale constant applied to the profiled duration; lives in
// X360 .rodata. Its value is NOT recovered, so it is referenced by name only.
extern const float flt_82009E10;

namespace D3D
{

// GPU deadlock diagnostic -- separate D3D TU (not yet homed). The asm passes the
// device in r3 only; returns non-zero when a genuine hang is confirmed.
s32 DebugGpuDeadlock(CDevice* pDevice);

// --- X360 platform hardware reads with no reconstructable C++ home ----------
// The live GPU command fence, read on the X360 from the per-thread context block
// through the PowerPC thread pointer: *(*(r13 + 0x100) + 0x58). This is platform
// thread-local state with no C++ type to recover, so it is surfaced as a named
// platform accessor rather than an offset-hack on an unknown type.
// FLAG PC-platform leaf: X360 per-thread GPU fence register.
u32 ReadCurrentGpuFence();
// PowerPC time-base register low word (the `mftb` instruction).
// FLAG PC-platform leaf: X360 CPU time-base register.
u32 ReadTimeBase();

// --- @ 0x82947498 ----------------------------------------------------------
// Record the block's starting state. mpDevice / miType are stored first, then
// the current readback progress word, the live GPU fence (into both the
// progress-fence and start-fence slots) and the CPU time-base. A non-zero block
// type emits a PIX begin-op (table entry | 0x800000).
CBlocker::CBlocker(CDevice* pDevice, s32 iType)
{
    mpDevice = pDevice;   // stw r29, 0(this)
    miType   = iType;     // stw r30, 4(this)

    const u32 uFence = ReadCurrentGpuFence();               // *(*(r13+0x100)+0x58)
    muProgressFence = uFence;                               // stw 0xC(this)
    muLastProgress  = pDevice->mpGpuReadback->muSecondaryRead; // stw 8(this)
    muStartFence    = uFence;                               // stw 0x10(this)
    muStartTimeBase = ReadTimeBase();                       // mftb ; stw 0x14(this)

    if (iType != 0)
        PIXAddPixOpWithData(dword_82102EE8[iType] | 0x800000u, 0, 0);
}

// --- @ 0x82947518 ----------------------------------------------------------
// On scope exit, for a non-zero block type: accumulate the elapsed time-base
// delta into the device's per-type blocked-ticks counter (type 3 -> the second
// counter, all other types -> the first), emit the PIX end-op (table entry |
// 0x400000) and, if a profiling callback is installed, invoke it with the scaled
// blocked duration in milliseconds and the fence delta.
CBlocker::~CBlocker()
{
    if (miType == 0)      // lwz 4(this) ; beq -> nothing to do
        return;

    CDevice* pDevice = mpDevice;
    const u32 uElapsedTicks = ReadTimeBase() - muStartTimeBase;  // r30 = now - start
    const s32 iFenceDelta   = (s32)(ReadCurrentGpuFence() - muStartFence); // r29

    // 64-bit accumulate of the zero-extended elapsed tick count.
    if (miType == 3)
        pDevice->mu64BlockTicks3 += uElapsedTicks;   // ld/add/std 0x5468
    else
        pDevice->mu64BlockTicks  += uElapsedTicks;   // ld/add/std 0x5460

    PIXAddPixOpWithData(dword_82102EE8[miType] | 0x400000u, 0, 0);

    void* pCallback = pDevice->mpProfileCallback;    // lwz 0x347C(device)
    if (pCallback != nullptr)
    {
        // ms = secondsPerTick * elapsedTicks * <rodata scale>. The X360 leaves
        // callback arg #3 (r5) holding the residual 0 from the PIX end-op call.
        typedef void (*ProfileBlockFn)(s32, s32, s32, s32, f32);
        const f32 fDuration =
            pDevice->mfSecondsPerTick * (f32)uElapsedTicks * flt_82009E10;
        reinterpret_cast<ProfileBlockFn>(pCallback)(0, miType, 0, iFenceDelta, fDuration);
    }
}

// --- @ 0x82947608 ----------------------------------------------------------
// Poll the block. Returns false only when the device has already flagged a
// deadlock, or when the fence has advanced past the stall threshold with no
// readback progress AND the deadlock diagnostic confirms a hang; otherwise true
// (keep waiting), refreshing the progress-fence watermark as the GPU advances.
bool CBlocker::Check()
{
    CDevice* pDevice = mpDevice;

    // Short CPU thread-priority hint spin (X360 db-hint no-ops, rendered by the
    // decompiler as a 4-count countdown). Yields the hardware thread; no effect.
    for (volatile s32 iSpin = 4; iSpin != 0; --iSpin) { }

    // A deadlock was already reported on this device -> the block cannot clear.
    if ((pDevice->muFlags & 0x2) != 0)
        return false;

    const u32 uFence    = ReadCurrentGpuFence();
    const u32 uProgress = pDevice->mpGpuReadback->muSecondaryRead;
    if (muLastProgress != uProgress) // GPU has advanced since the last poll
    {
        muProgressFence = uFence;
        muLastProgress  = uProgress;
    }

    // While this thread owns an in-flight submit, treat the fence as fresh so a
    // long CPU-side stall is not mistaken for a GPU hang.
    if (pDevice->muOwnerThreadId == GetCurrentThreadId() &&
        pDevice->muHangDetectArmed != 0)
    {
        muProgressFence = uFence;
    }

    // Under the stall threshold (5000 fence ticks) -> keep waiting.
    if ((uFence - muProgressFence) < 0x1388u)
        return true;

    // Threshold exceeded with no progress -> run the deadlock diagnostic.
    if (DebugGpuDeadlock(pDevice))
        return false;

    // Diagnostic cleared it (false alarm) -> reset the window and keep waiting.
    muProgressFence = uFence;
    return true;
}

// Layout facts (uncalled). One leading host pointer (mpDevice) then five 32-bit
// words. On the X360 the object is 0x18 bytes with a 4-byte pointer; on the
// LLP64 host the pointer widens to 8, so only field ORDER/stride is pinned, not
// the X360-absolute byte offsets (every access above is by name).
void _CBlockerAssertLayout()
{
    static_assert(offsetof(CBlocker, miType) == offsetof(CBlocker, mpDevice) + sizeof(void*),
                  "miType immediately follows mpDevice");
    static_assert(offsetof(CBlocker, muProgressFence) - offsetof(CBlocker, muLastProgress) == 4,
                  "progress-fence follows last-progress");
    static_assert(offsetof(CBlocker, muStartFence) - offsetof(CBlocker, muProgressFence) == 4,
                  "start-fence follows progress-fence");
    static_assert(offsetof(CBlocker, muStartTimeBase) - offsetof(CBlocker, muStartFence) == 4,
                  "start-time-base follows start-fence");
}

} // namespace D3D
