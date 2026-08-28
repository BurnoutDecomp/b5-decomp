// =====================================================================================
// rw::audio::core::LowPassButterworth bodies -- the low-pass Butterworth plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch (decode report
// progress/scratch_dossiers/gainfader_lowpassbutterworth_decode_codex.md).
//   GetPlugInDescRunTime         @0x82B97BF0 -- returns the registered "LowPassButterworth" record
//   GetSize                      @0x82B9E4B0 -- Butterworth::GetSize(channels) + header span
//   CreateInstance               @0x82BA2FA0 -- store-for-store
//   Process                      @0x82B97C00 -- branch-for-branch
//   `vector deleting destructor' @0x82BA1908 -- reinstall base vtable, conditional free
// See plugins/LowPassButterworth.h for the layout and the four divergences from the
// committed HighPassButterworth mirror.
// =====================================================================================

#include "rw/audio/core/plugins/LowPassButterworth.h"
#include "rw/audio/core/PlugIn.h"  // PlugInDescRunTime (the real descriptor record)
#include "rw/audio/core/Mixer.h"   // the process context (mpFormat -> mfSampleRate)

#include <cstddef> // offsetof (the host header span in GetSize)

namespace rw
{
namespace audio
{
namespace core
{

// off_8217F444 -- the LowPassButterworth v-table installed at construction; off_820AA810 --
// the base PlugIn v-table the deleting destructor reinstalls. Opaque data symbols in the
// XEX; modelled as honest placeholders so the bodies link without fabricating contents.
static void *const KLB_BasePlugInVTable = nullptr;          // off_820AA810
static void *const KLB_LowPassButterworthVTable = nullptr;  // off_8217F444

// CreateInstance immediates, re-read big-endian from the decrypted XEX:
static const f32 KF_INIT_CUTOFF = 96000.0f;      // flt_820AA8F0 -- ⭐ the high-pass seeds 0.0
static const f32 KF_FILTER_ORDER = 4.0f;         // flt_82004EF4 (4th-order Butterworth)
static const f32 KF_PARAM3 = 1.0f;               // flt_82001C98
static const f32 KF_LAST_CUTOFF_INIT = 15000.0f; // flt_8217F5AC
static const f32 KF_LATENCY = 450.0f;            // flt_8203869C

// Process guard-band constants (shared with the high-pass sibling).
static const f32 KF_HALF = 0.5f;                 // flt_82001DA0 (Nyquist = sampleRate * 0.5)
static const f32 KF_MARGIN = 0.0099999998f;      // flt_82002138 (~1% Nyquist guard band)

// The dispatch thunk for the record's Process slot is unnecessary -- the PC body is already
// the dispatched static shape.

// off_82F8D24C -- the "LowPassButterworth" runtime descriptor, REAL (its 52 bytes were
// re-read from the XEX by the decode: 'LPB0', type 4, 0 ctor params, 3 attributes, 0 events).
// Metadata FLAG'd null per the descriptor-wave convention.
static PlugInDescRunTime g_LowPassButterworthDesc = {
    "LowPassButterworth",
    reinterpret_cast<void *>(&LowPassButterworth::GetSize),        // @0x82B9E4B0
    reinterpret_cast<void *>(&LowPassButterworth::CreateInstance), // @0x82BA2FA0
    0,
    reinterpret_cast<void *>(&LowPassButterworth::Process),        // @0x82B97C00
    0, 0, 0, 0,
    0,
    0x4C504230u,       // 'LPB0'
    4, 0, 3, 0, 0, 0,
    0
};

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B97BF0 -- return &off_82F8D24C.
// -------------------------------------------------------------------------------------
char **LowPassButterworth::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&g_LowPassButterworthDesc);
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B9E4B0
//   return Butterworth::GetSize(config->outputChannels) + 0x60;
//
// NOTE the r3 here is the stage CONFIG, not an instance: the console reads the channel count
// with `lbz r3, 8(r3)` from PlugInConfig+0x08. (The committed high-pass sibling models the
// same query through a pre-init parked byte because its console GetSize is a different
// shape; this one takes the config directly, which is the vendor's own signature.)
//
// X360-LITERAL TRAP: the console's +0x60 is ITS OWN header span before the embedded kernel;
// the host header is wider (widened base-view pointers), so the span is the host offsetof.
// -------------------------------------------------------------------------------------
int LowPassButterworth::GetSize(const VoiceStageConfig *config)
{
    const u8 lu8Channels = static_cast<u8>(config->mFlagAndField8);
    return static_cast<int>(Butterworth::GetSize(lu8Channels)
                            + offsetof(LowPassButterworth, mButterworth)); // X360: +0x60
}

// -------------------------------------------------------------------------------------
// `vector deleting destructor' @0x82BA1908 -- reinstall the base v-table, conditional free.
// -------------------------------------------------------------------------------------
void *LowPassButterworth::VectorDeletingDestructor(LowPassButterworth *self, char flags)
{
    self->mBase.mpVTable = KLB_BasePlugInVTable; // off_820AA810
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA2FA0 -- placement-init. Seeds the three design attributes and the
// cutoff-history guard, lays the embedded Butterworth kernel out over mButterworth
// (recording its relative offset), then folds this node's decay-tail delta into the owning
// voice's accumulator and parks its own mDecaySamples at 450. Returns 1 (the asm loads
// r3 = 1; it does NOT return self).
// -------------------------------------------------------------------------------------
int LowPassButterworth::CreateInstance(LowPassButterworth *self)
{
    if (self)
        self->mBase.mpVTable = KLB_LowPassButterworthVTable; // off_8217F444

    const u8 lu8Channels = self->mBase.mbChannelCount;  // lbz +0x21

    self->mfCutoffFreq = KF_INIT_CUTOFF;                // stfs +0x28 (96000.0 -- wide open)
    self->mBase.mpAttributes = &self->mfCutoffFreq;     // stw  +0x0C -> &+0x28
    self->mfFilterOrder = KF_FILTER_ORDER;              // stfs +0x30 (4.0)
    self->mfParam3 = KF_PARAM3;                         // stfs +0x38 (1.0)
    self->mfLastCutoffFreq = KF_LAST_CUTOFF_INIT;       // stfs +0x40 (15000.0)

    // The console evaluates Butterworth::GetSize here and DISCARDS the result (a size query
    // the release build kept). Reproduced for parity.
    (void)Butterworth::GetSize(lu8Channels);

    // Lay out the embedded kernel + work buffers and record the relative offset. (The
    // console also passes mBase.mpSystem in r3, which Butterworth::CreateInstance never
    // reads -- dropped, as in the high-pass sibling.)
    Butterworth::CreateInstance(lu8Channels, &self->mButterworth);
    self->muButterworthOffset = static_cast<u16>(
        reinterpret_cast<char *>(&self->mButterworth) - reinterpret_cast<char *>(self));

    // Fold this node's decay-tail delta into the owning voice's accumulator (voice+0x28),
    // then park mDecaySamples at 450 -- the family's decay-rebase idiom.
    const f32 lfOldDecay = self->mBase.mDecaySamples;   // lfs +0x18
    self->mBase.mpVoice->mfFadeStart =
        (KF_LATENCY - lfOldDecay) + self->mBase.mpVoice->mfFadeStart;
    self->mBase.mDecaySamples = KF_LATENCY;             // stfs +0x18 (450.0)
    return 1;
}

// -------------------------------------------------------------------------------------
// Process @0x82B97C00 -- the mirror of the high-pass body with the band sense inverted.
// r5 (discontinuity) is unused; every exit returns BUFFERSTATUS_AVAILABLE (1).
//
// BELOW the high guard the filter runs: the cutoff is clamped UP into the low guard band,
// the cascade is redesigned when any of the three parameters changed, and the frame is
// filtered. ABOVE it the filter is bypassed entirely -- no Filter call and no buffer swap,
// so the frame passes through untouched -- with the running state flushed on the first frame
// that leaves the band.
// -------------------------------------------------------------------------------------
int LowPassButterworth::Process(LowPassButterworth *self, AudioProcessContext *ctx,
                                bool /*discontinuity*/)
{
    Butterworth *lpButterworth = reinterpret_cast<Butterworth *>(
        reinterpret_cast<char *>(self) + self->muButterworthOffset);  // self + *(self+0x58)

    const f32 lfCutoff = self->mfCutoffFreq;                          // lfs +0x28
    const f32 lfSampleRate = ctx->mpFormat->mfSampleRate;             // lfs 0xC(+0x30018)
    const f32 lfNyquist = lfSampleRate * KF_HALF;
    const f32 lfLowGuard = lfNyquist * KF_MARGIN;
    const f32 lfHighGuard = lfNyquist - lfLowGuard;

    // NaN POLARITY: the console's `ble` after fcmpu does NOT branch when unordered, so a
    // NaN cutoff takes the BYPASS arm. The negated ordered predicate reproduces that.
    if (!(lfCutoff <= lfHighGuard))
    {
        // Bypass. The clear is skipped only on an ORDERED greater-than, so a NaN cached
        // cutoff DOES flush the state.
        if (!(self->mfLastCutoffFreq > lfHighGuard))
            lpButterworth->ClearBuffer();
        self->mfLastCutoffFreq = self->mfCutoffFreq;                  // stfs +0x40
        return 1;                                                     // no Filter, no swap
    }

    // Clamp the live cutoff UP into the low guard band (the high-pass clamps DOWN instead).
    // Again the negated ordered form, so NaN is left unclamped exactly as the console does.
    if (!(lfCutoff >= lfLowGuard))
        self->mfCutoffFreq = lfLowGuard;                              // stfs +0x28

    // Recompute the cascade only when a design parameter actually changed.
    if (self->mfCutoffFreq != self->mfLastCutoffFreq ||
        self->mfFilterOrder != self->mfLastFilterOrder ||
        self->mfParam3 != self->mfLastParam3)
    {
        // The designer's recovered ABI: r3=bw, f1=cutoff, f2=sampleRate, f3=the third
        // attribute, r5=fctidz(order), r7=0 (the LOW-pass selector). Its BODY is still a
        // keystone pending the three rodata design tables (see Butterworth.cpp).
        Butterworth::CalculateFilterCoefficients(
            lpButterworth, self->mfCutoffFreq, lfSampleRate, self->mfParam3,
            static_cast<s32>(self->mfFilterOrder), Butterworth::KFILTER_LOWPASS);

        self->mfLastCutoffFreq = self->mfCutoffFreq;      // stfs +0x40
        self->mfLastFilterOrder = self->mfFilterOrder;    // stfs +0x48
        // FAITHFUL OMISSION: the console never re-latches mfLastParam3 (+0x50), so a change
        // to the third attribute alone re-triggers the redesign every frame. Same in the
        // high-pass sibling; reproduced rather than "fixed".
    }

    lpButterworth->Filter(ctx);
    return 1;   // BUFFERSTATUS_AVAILABLE
}

} // namespace core
} // namespace audio
} // namespace rw
