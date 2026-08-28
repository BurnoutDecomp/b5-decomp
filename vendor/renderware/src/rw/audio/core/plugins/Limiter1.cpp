// =====================================================================================
// rw::audio::core::Limiter1 bodies -- the "Limiter1" dynamics audio plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. The vendor limiter1.h (references/Feb-2007/.../
// rwaudiocore/2.11.00/include/rw/audio/core/plugins/limiter1.h) supplies the authoritative
// NAMES and matches the ARTIST layout member-for-member. See plugins/Limiter1.h.
//   GetPlugInDescRunTime         @0x82B97AA0 -- returns the registered "Limiter1" descriptor
//   GetSize                      @0x82B97DA8 -- console 168 (0xA8); host sizeof (stage carve)
//   `scalar deleting destructor' @0x82BA18C0 -- reinstalls base vtable, conditional free
//   CreateInstance               @0x82BA2F28 -- store-for-store
//   Configure                    @0x82B97AB0 -- store-for-store (UNBLOCKED, phase E)
//   Process                      @0x82B9E3A0 -- branch-for-branch (phase E callback wave)
//
// Configure's two former blockers are RESOLVED (decode report
// progress/scratch_dossiers/limiter1_configure_decode_codex.md, adversarially verified):
//   * sub_82C09970 IS the X360 CRT double-precision pow(x, y) core (x in f1, y in f2,
//     double result in f1) -- identified by its log2(e)/ln(2) polynomial constant block,
//     its negative-base integral-exponent classification, its _decomp/_get_exp/_set_exp
//     call tree, and the named `pow` wrapper @0x82674CD0 that frsp's its result. All 46
//     exported call sites across 25 dossiers agree on that register contract (including
//     Butterworth::CalculateFilterCoefficients @0x82B64698, whose own flagged block on
//     this helper is likewise resolved).
//   * the two rodata constants are flt_82004FDC == 0.95f and flt_8200D5A4 == -0.9f (see
//     the constant block below).
// =====================================================================================

#include "rw/audio/core/plugins/Limiter1.h"
#include "rw/audio/core/PlugIn.h"   // PlugInDescRunTime (the real descriptor record)
#include "rw/audio/core/Mixer.h"    // the process context (mpFormat -> mfSampleRate)

#include <cmath> // std::pow (the console's sub_82C09970 == the CRT pow core)

namespace rw
{
namespace audio
{
namespace core
{

// off_820AA810 -- the base PlugIn v-table the deleting destructor reinstalls before any
// free (shared with Gain / ReverbModel1). The concrete Limiter1 v-table symbol installed at
// construction is NOT exposed in CreateInstance's asm -- PlugIn::Initialize<T> installs it
// inside its own (separate-TU) body -- so it is modelled as honest placeholder storage here
// rather than fabricating an address.
static void *const KLM_BasePlugInVTable = nullptr;   // off_820AA810
static void *const KLM_Limiter1VTable = nullptr;     // the Limiter1 v-table (installed by Initialize)

// CreateInstance immediates. The first two are the vendor's INIT_THRESHOLD (120 dB) and
// INIT_RELEASETIME; the third is CHANNELMODE_GROUPED as a float.
static const f32 KF_INIT_THRESHOLD = 120.0f;   // flt_82004A28 (vendor INIT_THRESHOLD)
static const f32 KF_INIT_RELEASETIME = 0.1f;   // flt_8215BF00 (vendor INIT_RELEASETIME)
static const f32 KF_ONE = 1.0f;                // flt_82001C98
static const f32 KF_ZERO = 0.0f;               // flt_82001CC0

// Process/Configure immediates, every one re-read from the decrypted XEX big-endian at
// file_off = 0x3000 + vaddr - 0x82000000 (bytes in the decode report):
static const f32 KF_MAX_THRESHOLD = 20.0f;     // flt_820054CC -- vendor MAX_THRESHOLD; at or
                                               //   above this dB the limiter switches OFF
static const f32 KF_MAX_RELEASETIME = 10.0f;   // flt_8215BF14 -- vendor MAX_RELEASETIME
static const f32 KF_DB_SCALE_AND_ATTACK = 0.05f; // flt_820047C8 -- ONE constant serving two
                                               //   roles: the 1/20 dB exponent scale AND the
                                               //   vendor ATTACKTIME (a fixed 50 ms attack)
static const f64 KD_TEN = 10.0;                // dbl_8202D7F8 -- the pow base
static const f32 KF_THRESHOLD_OFF_SCALE = 0.95f; // flt_82004FDC (0x3F733333) -- the
                                               //   threshold-off hysteresis multiplier
static const f32 KF_COMP_EXPONENT = -0.9f;     // flt_8200D5A4 (0xBF666666) -- the fixed
                                               //   LIMITER_RATIO exponent: 1/10 - 1
static const f32 KF_HALF = 0.5f;               // flt_82001DA0 -- the round-half bias

namespace
{
    // The console's round-half-away-from-zero idiom: bias by +/-0.5 then fctiwz (truncate
    // toward zero). NaN takes the +0.5 arm because `blt` is false when unordered.
    // CONSOLE-SEMANTICS NOTE: fctiwz saturates on out-of-range/NaN where a C++ cast is
    // undefined; over Limiter1's real domain (a finite positive sample rate and a clamped
    // finite attribute) the two agree exactly.
    s32 RoundHalfAwayFromZero(f32 afValue)
    {
        const f32 lfBiased = (afValue < 0.0f) ? (afValue - KF_HALF) : (afValue + KF_HALF);
        return static_cast<s32>(lfBiased);
    }
}

// The dispatch thunk for the record's Process slot: the console callback's r3 is the
// instance and the PC body is already the (instance, ctx, flag) static, so the descriptor
// stores Process directly -- no thunk needed here (contrast the member-bodied filters).

// off_82F8D150 -- the "Limiter1" runtime descriptor, REAL (descriptor-record wave; record
// dump progress/scratch_dossiers/plugindesc_layout_codex.md). Metadata FLAG'd null per the
// descriptor-wave convention (no committed consumer reads them).
static PlugInDescRunTime g_Limiter1Desc = {
    "Limiter1",
    reinterpret_cast<void *>(&Limiter1::GetSize),         // @0x82B97DA8
    reinterpret_cast<void *>(&Limiter1::CreateInstance),  // @0x82BA2F28
    0,
    reinterpret_cast<void *>(&Limiter1::Process),         // @0x82B9E3A0
    0, 0, 0, 0,
    0,
    0x4C693130u,       // 'Li10'
    4, 0, 3, 0, 0, 0,
    0
};

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B97AA0 -- return the address of the registered descriptor record
// (its label is the string "Limiter1").
//   return &off_82F8D150;
// -------------------------------------------------------------------------------------
char **Limiter1::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&g_Limiter1Desc); // &off_82F8D150
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B97DA8 -- the plug-in instance footprint.
//   li r3, 0xA8
// -------------------------------------------------------------------------------------
int Limiter1::GetSize()
{
    // X360-LITERAL TRAP (the stage-carve audit, phase-D follow-up): the console
    // immediate under-allocates the widened host object -- GetSize is the stage
    // factory's allocation stride, so return host sizeof (the RawPuller2/Send/
    // SinePlayer precedent).
    return static_cast<int>(sizeof(Limiter1));   // X360: li r3, 0xA8 (168)
}

// -------------------------------------------------------------------------------------
// `scalar deleting destructor' @0x82BA18C0 -- reinstall the base PlugIn v-table, then
// conditionally free. (~Limiter1 is trivial and folded away -- the asm performs no member
// teardown, only the vtable reinstall.)
//   self->mBase.mpVTable = off_820AA810;
//   if (flags & 1) operator delete(self);
//   return self;
// -------------------------------------------------------------------------------------
void *Limiter1::ScalarDeletingDestructor(Limiter1 *self, char flags)
{
    self->mBase.mpVTable = KLM_BasePlugInVTable; // off_820AA810
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA2F28 -- placement-init a Limiter1 over `self`.
//
// PlugIn::Initialize<Limiter1>(self, 0x28) constructs the plug-in base and bases its
// 8-byte-stride attribute table at self+0x28 (mfAttribute0). The templated PlugIn::Initialize
// helper lives in another (separate) TU; its locally-observable effect (v-table install +
// attribute-base wiring) is reproduced inline here, exactly as the Gain / ReverbModel1 /
// Iir2* shapes do. FLAGGED: the real Initialize<T> also performs the base mpSystem/mpVoice
// wiring from the owning voice/factory.
//
// Then the limiter latches its three graph-attribute defaults (120.0 / 0.1 / 1.0), mirrors
// them into the default snapshot block at +0x90, and clears the +0x9C/+0xA0 words. The store
// order below matches the asm. Returns 1.
// -------------------------------------------------------------------------------------
int Limiter1::CreateInstance(Limiter1 *self)
{
    // PlugIn::Initialize<Limiter1>(self, 0x28) -- observable effect reproduced inline.
    self->mBase.mpVTable = KLM_Limiter1VTable;              // installed by Initialize (symbol hidden)
    self->mBase.mpAttributes = &self->mAttribute[0];        // attribute table base @ self+0x28

    // The store order below matches the asm. Each attribute's initial value is ALSO written
    // to its Process-side cache (+0x90..) so the first Process sees "unchanged" only if the
    // sample rate matches too -- the caches are the change-detect snapshot, not defaults.
    self->mAttribute[ATTRIBUTE_SETTHRESHOLD].mfValue   = KF_INIT_THRESHOLD;   // stfs @ +0x28 (120.0)
    self->mLastThreshold                                = KF_INIT_THRESHOLD;   // stfs @ +0x90
    self->mState                                        = STATE_OFF;           // stw  @ +0xA0
    self->mAttribute[ATTRIBUTE_SETCHANNELMODE].mfValue  = KF_ONE;              // stfs @ +0x38 (GROUPED)
    self->mAttribute[ATTRIBUTE_SETRELEASETIME].mfValue  = KF_INIT_RELEASETIME; // stfs @ +0x30 (0.1)
    self->mLastReleaseTime                              = KF_INIT_RELEASETIME; // stfs @ +0x94
    self->mLastChannelMode                              = KF_ONE;              // stfs @ +0x98
    self->mLastSampleRate                               = KF_ZERO;             // stfs @ +0x9C
    return 1;
}

// -------------------------------------------------------------------------------------
// Configure @0x82B97AB0 -- map the three graph attributes onto the embedded
// CompressorLimiter1's coefficient block. r3=self, f1=sampleRate (the ONLY incoming
// arguments; the former 8-arg spelling was a Hex-Rays artifact).
//
// Ordered clamp of the release attribute, dB -> linear threshold through pow(10, dB/20),
// the 0.95 hysteresis for the off threshold, a FIXED 50 ms attack, the attribute-driven
// release, and the grouped/independent channel flag.
// -------------------------------------------------------------------------------------
void Limiter1::Configure(Limiter1 *self, f32 afSampleRate)
{
    // Clamp mAttribute[1] into [0, 10]. Both compares are fcmpu + a "not LT"/"not GT"
    // branch, so a NaN release time takes BOTH branch-arounds and is left UNCHANGED.
    const f32 lfReleaseSeconds = self->mAttribute[ATTRIBUTE_SETRELEASETIME].mfValue;
    if (lfReleaseSeconds < KF_ZERO)
        self->mAttribute[ATTRIBUTE_SETRELEASETIME].mfValue = KF_ZERO;            // stfs @ +0x30
    else if (lfReleaseSeconds > KF_MAX_RELEASETIME)
        self->mAttribute[ATTRIBUTE_SETRELEASETIME].mfValue = KF_MAX_RELEASETIME; // stfs @ +0x30

    // thresholdOn = 10 ^ (thresholdDb / 20), through the CRT's DOUBLE pow core with the
    // exponent single-rounded first (fmuls) and the result narrowed after (frsp). The two
    // rounding points are load-bearing -- do not fold this into an algebraically equal exp.
    const f32 lfDbExponent =
        self->mAttribute[ATTRIBUTE_SETTHRESHOLD].mfValue * KF_DB_SCALE_AND_ATTACK; // fmuls
    const f32 lfThresholdOn =
        static_cast<f32>(std::pow(KD_TEN, static_cast<f64>(lfDbExponent)));        // pow + frsp
    const f32 lfThresholdOff = lfThresholdOn * KF_THRESHOLD_OFF_SCALE;             // fmuls (0.95)

    // The FIXED 50 ms attack. NOTE (divergence from the sibling Compressor1::Configure):
    // Limiter1 does NOT promote a zero attack to one -- only the release below is repaired.
    const s32 liAttackSamples =
        RoundHalfAwayFromZero(afSampleRate * KF_DB_SCALE_AND_ATTACK);

    // The attribute-driven release, re-read AFTER the clamp above. A zero result becomes 1
    // so the callee's `exponent / releaseSamples` step cannot divide by zero.
    s32 liReleaseSamples = RoundHalfAwayFromZero(
        afSampleRate * self->mAttribute[ATTRIBUTE_SETRELEASETIME].mfValue);
    if (liReleaseSamples == 0)
        liReleaseSamples = 1;

    // CHANNELMODE_GROUPED is an EXACT 1.0 test: any other value -- NaN included -- is
    // CHANNELMODE_INDEPENDENT (the console's `beq` is false when unordered).
    const s32 liGroupChannels =
        (self->mAttribute[ATTRIBUTE_SETCHANNELMODE].mfValue == KF_ONE) ? 1 : 0;

    CompressorLimiter1::Configure(&self->mCompressorLimiter1, lfThresholdOn, lfThresholdOff,
                                  KF_COMP_EXPONENT, liAttackSamples, liReleaseSamples,
                                  liGroupChannels);
}

// -------------------------------------------------------------------------------------
// Process @0x82B9E3A0 -- the registered Process callback. r5 (discontinuity) is NEVER read;
// every path returns BUFFERSTATUS_AVAILABLE (1).
//
// Above the MAX_THRESHOLD dB boundary the limiter is inaudible, so it switches itself OFF
// (clearing the envelope history once, on the ON->OFF edge) and processes NOTHING. Below it,
// it switches ON and reconfigures only when one of {threshold, release, channel mode, the
// context's sample rate} differs from its cache.
// -------------------------------------------------------------------------------------
int Limiter1::Process(Limiter1 *self, AudioProcessContext *ctx, bool /*discontinuity*/)
{
    const f32 lfThreshold = self->mAttribute[ATTRIBUTE_SETTHRESHOLD].mfValue; // lfs +0x28
    const s32 liState = self->mState;                                          // lwz +0xA0

    // `blt` is false when unordered, so a NaN threshold takes the OFF path.
    if (!(lfThreshold < KF_MAX_THRESHOLD))
    {
        if (liState == STATE_ON)
        {
            CompressorLimiter1::ClearBuffer(&self->mCompressorLimiter1);
            self->mState = STATE_OFF;
        }
        // Only the THRESHOLD cache is refreshed on this path -- the release/mode/rate
        // caches stay stale, so a later reactivation skips Configure only if those still
        // match as well (asm-exact; not an oversight to "fix").
        self->mLastThreshold = self->mAttribute[ATTRIBUTE_SETTHRESHOLD].mfValue; // stfs +0x90
        return 1;
    }

    if (liState == STATE_OFF)
        self->mState = STATE_ON;   // a nonzero (already-on) state is left unchanged

    const f32 lfSampleRate = ctx->mpFormat->mfSampleRate; // lfs 0xC(ctx->+0x30018)

    // Reconfigure when ANY of the four cached inputs differs. Each compare is fcmpu + bne,
    // so an unordered (NaN) live value counts as "changed".
    if (lfThreshold != self->mLastThreshold ||
        self->mAttribute[ATTRIBUTE_SETRELEASETIME].mfValue != self->mLastReleaseTime ||
        self->mAttribute[ATTRIBUTE_SETCHANNELMODE].mfValue != self->mLastChannelMode ||
        lfSampleRate != self->mLastSampleRate)
    {
        Configure(self, lfSampleRate);

        // The caches are refreshed ONLY after Configure, and they re-read the live values
        // (Configure may have clamped the release time in place).
        self->mLastThreshold   = self->mAttribute[ATTRIBUTE_SETTHRESHOLD].mfValue;   // +0x90
        self->mLastReleaseTime = self->mAttribute[ATTRIBUTE_SETRELEASETIME].mfValue; // +0x94
        self->mLastChannelMode = self->mAttribute[ATTRIBUTE_SETCHANNELMODE].mfValue; // +0x98
        self->mLastSampleRate  = lfSampleRate;                                        // +0x9C
    }

    // The kernel reads the source, writes the destination, and performs the src/dst SWAP
    // ITSELF -- which is exactly why this body does not swap (the two must stay distinct
    // while the gain curve is being built in the destination).
    CompressorLimiter1::Process(&self->mCompressorLimiter1, ctx,
                                self->mBase.mbChannelCount); // lbz +0x21
    return 1;                                                 // BUFFERSTATUS_AVAILABLE
}

} // namespace core
} // namespace audio
} // namespace rw
