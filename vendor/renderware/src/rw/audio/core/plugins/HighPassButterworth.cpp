// =====================================================================================
// rw::audio::core::HighPassButterworth bodies -- the "HighPassButterworth" high-pass
// filter audio plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. No Feb-2007 leak source, no DecFIGS DWARF, and no
// ProStreet08 rwaudio PDB entry exist for this type. See plugins/HighPassButterworth.h for
// the byte-exact layout and the per-function X360 addresses.
//   GetPlugInDescRunTime         @0x82B976D0 -- returns the registered descriptor record
//   Process                      @0x82B976E0 -- recompute-guard + delegate to Butterworth
//   `vector deleting destructor' @0x82BA17A0 -- reinstall base vtable, conditional free
//   GetSize                      @0x82B9DF88 -- Butterworth::GetSize(order) + 96
//   CreateInstance               @0x82BA2CB0 -- store-for-store
//
// Process @0x82B976E0 calls rw::audio::core::Butterworth::CalculateFilterCoefficients, which
// is a KEYSTONE (see Butterworth.cpp): its X360 body forwards into the anonymous, fully
// un-typed helper sub_82C09970 and reads three undecoded rodata design tables. The call site
// here passes (bw, ctx, (int)mfFilterOrder, <uninitialised r6>, 1, mfCutoffFreq, mfParam3) --
// an argument shape IDA does not fully recover (r6 is never initialised before the call, the
// same un-decodable-ABI smell already flagged for Limiter1::Configure). Per the no-fabrication
// rule the extra arguments are documented but NOT invented: only the primary `self` is
// forwarded to the committed blocked stub. Process's own control flow is fully grounded.
// =====================================================================================

#include "rw/audio/core/plugins/HighPassButterworth.h"

namespace rw
{
namespace audio
{
namespace core
{

// off_82F8CE60 -- the registered run-time descriptor record (its +0x00 label is the string
// "HighPassButterworth"). off_820AA810 -- the base PlugIn v-table the deleting destructor
// reinstalls before any free (shared with Limiter1 / Gain / ReverbModel1). The concrete
// HighPassButterworth v-table symbol (off_8217F3F4) installed at construction is an opaque
// data symbol in the XEX; PlugIn::Initialize<T> installs it inside its own (separate-TU)
// body, so it is modelled as honest placeholder storage here rather than fabricating an
// address. These are opaque data symbols; modelled as placeholders so the bodies below link
// without fabricating their contents.
static char       *g_HighPassButterworthDesc = nullptr; // off_82F8CE60 (the "HighPassButterworth" record)
static void *const KHB_BasePlugInVTable = nullptr;       // off_820AA810
static void *const KHB_HighPassButterworthVTable = nullptr; // off_8217F3F4 (installed by Initialize)

// CreateInstance immediates (single numeric rodata constants; values grounded by the
// pseudocode's direct stores and the shared rwaudio plug-in family).
static const f32 KF_ZERO = 0.0f;             // flt_82001CC0
static const f32 KF_FILTER_ORDER = 4.0f;     // flt_82004EF4 (4th-order Butterworth)
static const f32 KF_PARAM3 = 1.0f;           // flt_82001C98
static const f32 KF_LAST_CUTOFF_INIT = 15000.0f; // flt_8217F5AC
static const f32 KF_LATENCY = 450.0f;        // flt_8203869C

// Process guard-band constants.
static const f32 KF_HALF = 0.5f;             // flt_82001DA0 (Nyquist = sampleRate * 0.5)
static const f32 KF_MARGIN = 0.0099999998f;  // flt_82002138 (~1% Nyquist guard band)

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B976D0 -- return the address of the registered descriptor record
// (its label is the string "HighPassButterworth").
//   return &off_82F8CE60;
// -------------------------------------------------------------------------------------
char **HighPassButterworth::GetPlugInDescRunTime()
{
    return &g_HighPassButterworthDesc; // &off_82F8CE60
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B9DF88
//   return Butterworth::GetSize(*(u8 *)(desc + 8)) + 96;
// The size is queried before CreateInstance repurposes the +0x08 PlugIn-base slot (mpInput)
// as the upstream-input pointer: at query time the factory has stashed the requested
// channel/section count (a byte) in that slot. Read faithfully as the byte at +0x08. The
// +96 is the X360 header size (0x60); the embedded Butterworth follows.
// -------------------------------------------------------------------------------------
int HighPassButterworth::GetSize(HighPassButterworth *desc)
{
    // asm: lbz r3, 8(this) -- the low-order count byte the factory stored in the +0x08 slot.
    const u8 order = reinterpret_cast<const u8 *>(&desc->mBase.mpInput)[0];
    return static_cast<int>(Butterworth::GetSize(order) + 96u);
}

// -------------------------------------------------------------------------------------
// `vector deleting destructor' @0x82BA17A0 -- reinstall the base PlugIn v-table, then
// conditionally free. (~HighPassButterworth is trivial and folded away -- the asm performs
// no member teardown, only the vtable reinstall.)
//   self->mBase.mpVTable = off_820AA810;
//   if (flags & 1) operator delete(self);
//   return self;
// -------------------------------------------------------------------------------------
void *HighPassButterworth::VectorDeletingDestructor(HighPassButterworth *self, char flags)
{
    self->mBase.mpVTable = KHB_BasePlugInVTable; // off_820AA810
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA2CB0 -- placement-init a HighPassButterworth over `self`.
//
// Installs the concrete v-table (only when self != null), seeds the three design attributes
// and the cutoff-history init value, lays out the embedded Butterworth kernel over
// mButterworth (recording its relative offset), then registers this node's group-delay
// latency into the upstream input's accumulator (+0x28) and parks its own latency attribute
// at 450 -- the same latency re-base idiom the Iir2* shapes use. Returns 1 (the asm loads
// r3 = 1 for the return).
// -------------------------------------------------------------------------------------
HighPassButterworth *HighPassButterworth::CreateInstance(HighPassButterworth *self)
{
    if (self)
        self->mBase.mpVTable = KHB_HighPassButterworthVTable; // off_8217F3F4

    const u8 order = self->mBase.mbChannelCount; // *(self+0x21) -- channel/section count

    self->mfCutoffFreq = KF_ZERO;                    // stfs +0x28 (0.0)
    self->mBase.mpAttributes = &self->mfCutoffFreq;  // stw  +0x0C -> &+0x28
    self->mfFilterOrder = KF_FILTER_ORDER;           // stfs +0x30 (4.0)
    self->mfParam3 = KF_PARAM3;                      // stfs +0x38 (1.0)
    self->mfLastCutoffFreq = KF_LAST_CUTOFF_INIT;    // stfs +0x40 (15000.0)

    // GetSize(order) is evaluated and discarded here in the X360 build -- its result is
    // overwritten before use (a size query the release build kept). Reproduced for parity.
    (void)Butterworth::GetSize(order);

    // Lay out the embedded Butterworth kernel + work buffers over mButterworth and record
    // its relative offset. (The X360 also passes mBase.mpSystem in r3 to Butterworth::
    // CreateInstance, which the committed 2-arg CreateInstance does not consume -- dropped.)
    Butterworth::CreateInstance(order, &self->mButterworth);
    self->muButterworthOffset = static_cast<u16>(
        reinterpret_cast<char *>(&self->mButterworth) - reinterpret_cast<char *>(self));

    // Re-base the upstream input's +0x28 latency accumulator by this node's latency delta,
    // then park this node's latency attribute at 450 (same idiom as the Iir2* shapes).
    const f32 oldLatency = self->mBase.mfAttrib1; // lfs 0x18
    f32 *inputLatency = reinterpret_cast<f32 *>(
        reinterpret_cast<char *>(self->mBase.mpInput) + 0x28); // *(*(self+8)+0x28)
    *inputLatency = (KF_LATENCY - oldLatency) + *inputLatency;
    self->mBase.mfAttrib1 = KF_LATENCY; // stfs +0x18 (450.0)
    return self;
}

// -------------------------------------------------------------------------------------
// Process @0x82B976E0
//
// Locate the embedded Butterworth (self + muButterworthOffset). Above the low guard band
// (~1% of Nyquist) it clamps the cutoff to the upper guard band, recomputes the cascade
// coefficients when any of the three design parameters changed since last frame, and runs
// the Butterworth filter. Below the low guard band it flushes the running state on the first
// frame the cutoff drops out of band, and parks the last-cutoff. Returns 1.
// -------------------------------------------------------------------------------------
int HighPassButterworth::Process(AudioProcessContext *ctx)
{
    Butterworth *bw = reinterpret_cast<Butterworth *>(
        reinterpret_cast<char *>(this) + muButterworthOffset); // v7 = self + *(self+0x58)

    const f32 cutoff = mfCutoffFreq; // v6 = *(self+0x28)

    const f32 nyquist = ctx->mpFormat->mfSampleRate * KF_HALF; // sampleRate * 0.5
    const f32 lowGuard = nyquist * KF_MARGIN;                  // v8 = Nyquist * margin
    const f32 highGuard = nyquist - lowGuard;                  // Nyquist * (1 - margin)

    if (cutoff >= lowGuard)
    {
        // Clamp the live cutoff down into the upper guard band.
        if (cutoff > highGuard)
            mfCutoffFreq = highGuard;

        // Recompute coefficients only when a design parameter actually changed.
        if (mfCutoffFreq != mfLastCutoffFreq ||
            mfFilterOrder != mfLastFilterOrder ||
            mfParam3 != mfLastParam3)
        {
            // KEYSTONE call (see file header + Butterworth.cpp). X360 passes
            // (bw, ctx, (int)mfFilterOrder, <uninit r6>, 1, mfCutoffFreq, mfParam3) into the
            // un-decodable coefficient designer; only the grounded `self` is forwarded to the
            // committed blocked stub, the rest documented but not fabricated.
            Butterworth::CalculateFilterCoefficients(bw);
            mfLastCutoffFreq = mfCutoffFreq;   // stfs +0x40
            mfLastFilterOrder = mfFilterOrder; // stfs +0x48  (note: +0x50/mfLastParam3 NOT latched)
        }

        bw->Filter(ctx);
    }
    else
    {
        // Cutoff below the guard band: flush the filter state the first frame it drops out.
        if (mfLastCutoffFreq >= lowGuard)
            bw->ClearBuffer();
        mfLastCutoffFreq = mfCutoffFreq; // stfs +0x40
    }
    return 1;
}

} // namespace core
} // namespace audio
} // namespace rw
