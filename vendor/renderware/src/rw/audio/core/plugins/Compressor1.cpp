// =====================================================================================
// rw::audio::core::Compressor1 bodies -- the "Compressor1" dynamics audio plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. No Feb-2007 leak source, no DecFIGS DWARF, and no
// ProStreet08 rwaudio PDB entry exist for this type. See plugins/Compressor1.h for the
// layout, the HOST-LAYOUT RULE and the per-function X360 addresses.
//   GetSize                      @0x82B96B20 -- the instance footprint (console li r3, 0xC0)
//   Configure                    @0x82B96B28 -- store-for-store
//   CreateInstance               @0x82BA2408 -- store-for-store
//   Process                      @0x82B9D988 -- store-for-store
//   `scalar deleting destructor' @0x82BA1680 -- reinstalls base vtable, conditional free
//
// Configure was previously left BLOCKED on two claims, both of which the asm refutes:
//   (a) "sub_82C09970 has an unrecoverable register-level argument shape". It does not: the
//       call site @0x82B96BB8..0x82B96BD0 loads ONLY f1 = dbl_8202D7F8 (10.0) and
//       f2 = fmuls(mfThreshold, flt_820047C8 = 0.05) -- no GPR is set for the call at all --
//       and sub_82C09970's own xrefs_from are _decomp / _d_inttype / _copysign / _get_exp /
//       _set_exp / log / _powhlp, with the symbol `pow` @0x82674CD0 among its callers. It is
//       the CRT pow(double,double); BrnMathUtils.cpp @0x82361600 already identifies it as such.
//       The call is therefore pow(10.0, threshold * 0.05f) -- the dB->linear conversion --
//       and 0x82B96BE0 `frsp f1, f1` narrows the result back to f32.
//   (b) "flt_82004FDC is un-recovered". It is not: dumped from the IDA database
//       (IDA Files/BURNOUT_X360_ARTIST.XEX.i64) it is 0x3F733333 == 0.949999988079071f, and
//       GPTriangle.cpp had already recovered the same rodata symbol as 0.94999999f.
// Every other constant in this body was dumped from the same database (see the K* block).
// It did not execute in the boot-trace milestone.
// =====================================================================================

#include "rw/audio/core/plugins/Compressor1.h"

#include <cmath> // std::pow (the X360 sub_82C09970)

namespace rw
{
namespace audio
{
namespace core
{

// off_820AA810 -- the base PlugIn v-table the deleting destructor reinstalls before any free
// (shared with Gain / Limiter1 / ReverbModel1). The concrete Compressor1 v-table symbol
// installed at construction is NOT exposed in CreateInstance's asm -- PlugIn::Initialize<T>
// installs it inside its own (separate-TU) body -- so it is modelled as honest placeholder
// storage here rather than fabricating an address. These are opaque data symbols in the XEX
// (no exported contents); modelled as placeholders so the bodies below link without fabricating
// their contents.
static void *const KCM_BasePlugInVTable = nullptr;   // off_820AA810
static void *const KCM_Compressor1VTable = nullptr;  // the Compressor1 v-table (installed by Initialize)

// CreateInstance immediates.
static const f32 KF_THRESHOLD_DEFAULT = 120.0f; // flt_82004A28
static const f32 KF_RELEASE_DEFAULT   = 0.5f;   // flt_82001DA0
static const f32 KF_ONE               = 1.0f;   // flt_82001C98
static const f32 KF_ATTACK_DEFAULT    = 0.1f;   // flt_8215BF00
static const f32 KF_ZERO              = 0.0f;   // flt_82001CC0

// Process branch immediates.
static const f32 KF_BYPASS_THRESHOLD  = 20.0f;  // flt_820054CC (mfThreshold >= this -> bypass)
static const f32 KF_BYPASS_RATIO      = 1.0f;   // flt_82001C98 (mfRatio <= this -> bypass)

// Configure immediates. All dumped from the XEX rodata via headless IDA; the console symbol
// is named against each so a later sweep can re-verify without re-deriving them.
static const f32 KF_ATTACK_MAX        = 10.0f;   // flt_8215BF14 (41200000)
static const f32 KF_RELEASE_MAX       = 30.0f;   // flt_8215BF18 (41F00000)
static const f32 KF_THRESHOLD_MIN     = -500.0f; // flt_8217F31C (C3FA0000)
static const f64 KD_DB_LOG_BASE       = 10.0;    // dbl_8202D7F8 (4024000000000000) -- pow base
static const f32 KF_DB_EXP_SCALE      = 0.05f;   // flt_820047C8 (3D4CCCCD) == 1/20, dB -> decade
static const f32 KF_THRESHOLD_OFF_MUL = 0.95f;   // flt_82004FDC (3F733333) -- off/on threshold ratio
static const f32 KF_ROUND_HALF        = 0.5f;    // flt_82001DA0 (3F000000) -- the round-half addend

// -------------------------------------------------------------------------------------
// GetSize @0x82B96B20 -- the plug-in instance footprint.
//   li r3, 0xC0   ; 192, the CONSOLE sizeof (32-bit pointers in the PlugIn base)
//
// This is the plug-in descriptor's allocation-size slot (it has no bl xref of its own; the
// descriptor table is what reads it), so it must size the real object on THIS target. The
// embedded PlugInBaseView is pointer-bearing and widens on x64, so returning the console
// literal 192 would under-allocate every Compressor1 instance (measured host sizeof is 216 --
// a 24-byte shortfall). Host sizeof it is -- the same doctrine EaXmaDec_wG_02.cpp states for
// its own carve ("must agree with GetSize").
//
// FLAG (tree-wide, NOT fixed here): the sibling rwaudio plug-ins still return their console
// literals -- Delay 184, Limiter1 168, Pan2D 240, Pan2D1 440, Pause 0x40, ReverbModel1 1088,
// SinePlayer 72, VuMeter 336 -- and SinePlayer.cpp's GetSize comment cites "Compressor1::GetSize
// 192" as the family precedent, which this change makes stale. Those files are outside this TU;
// the family needs one sweep, and this note is the breadcrumb for it.
// -------------------------------------------------------------------------------------
int Compressor1::GetSize()
{
    return static_cast<int>(sizeof(Compressor1)); // X360: li r3, 0xC0 == 192
}

// -------------------------------------------------------------------------------------
// `scalar deleting destructor' @0x82BA1680 -- reinstall the base PlugIn v-table, then
// conditionally free. (~Compressor1 is trivial and folded away -- the asm performs no member
// teardown, only the vtable reinstall.)
//   self->mBase.mpVTable = off_820AA810;
//   if (flags & 1) operator delete(self);
//   return self;
// -------------------------------------------------------------------------------------
void *Compressor1::ScalarDeletingDestructor(Compressor1 *self, char flags)
{
    self->mBase.mpVTable = KCM_BasePlugInVTable; // off_820AA810
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA2408 -- placement-init a Compressor1 over `self`.
//
// PlugIn::Initialize<Compressor1>(self, 0x28) constructs the plug-in base and bases its
// attribute table at the first graph attribute (console self+0x28 == &mfThreshold; the 0x28
// is a CONSOLE immediate, so the host form is the address-of, not the literal). The templated
// PlugIn::Initialize helper lives in another (separate) TU; its locally-observable effect
// (v-table install + attribute-base wiring) is reproduced inline here, exactly as the
// Limiter1 / Gain / Iir2* shapes do. FLAGGED: the real Initialize<T> also performs the base
// mpSystem/mpVoice wiring from the owning voice/factory.
//
// Then the compressor latches its five graph-attribute defaults, mirrors them into the
// "last-configured" snapshot block, seeds mfLastSampleRate to 0 and clears the active flag.
// The store order below matches the asm. Returns 1.
// -------------------------------------------------------------------------------------
int Compressor1::CreateInstance(Compressor1 *self)
{
    // PlugIn::Initialize<Compressor1>(self, 0x28) -- observable effect reproduced inline.
    self->mBase.mpVTable = KCM_Compressor1VTable;  // installed by Initialize (symbol hidden)
    self->mBase.mpAttributes = &self->mfThreshold; // attribute table base (console self+0x28)

    self->mfThreshold         = KF_THRESHOLD_DEFAULT; // stfs console +0x28 (120.0)
    self->mfLastThreshold     = KF_THRESHOLD_DEFAULT; // stfs console +0xA0 (120.0)
    self->muActive            = 0;                    // stw  console +0xB8
    self->mfRelease           = KF_RELEASE_DEFAULT;   // stfs console +0x40 (0.5)
    self->mfRatio             = KF_ONE;               // stfs console +0x30 (1.0)
    self->mfChannelLink       = KF_ONE;               // stfs console +0x48 (1.0)
    self->mfLastRatio         = KF_ONE;               // stfs console +0xA4 (1.0)
    self->mfLastRelease       = KF_RELEASE_DEFAULT;   // stfs console +0xAC (0.5)
    self->mfLastAttack        = KF_ATTACK_DEFAULT;    // stfs console +0xA8 (0.1)
    self->mfAttack            = KF_ATTACK_DEFAULT;    // stfs console +0x38 (0.1)
    self->mfLastChannelLink   = KF_ONE;               // stfs console +0xB0 (1.0)
    self->mfLastSampleRate    = KF_ZERO;              // stfs console +0xB4 (0.0)
    return 1;
}

// -------------------------------------------------------------------------------------
// Configure @0x82B96B28 -- translate the five graph attributes into the engine's
// threshold/exponent/attack/release/link configuration.
//
// r3 = self, f1 = the output sample-rate (Hz); no other incoming argument register is read.
//
// ASM walk-through (0x82B96B28..0x82B96CAC):
//   0x82B96B4C..0x82B96B74  clamp mfAttack  into [0.0, 10.0]  (flt_82001CC0 / flt_8215BF14)
//   0x82B96B78..0x82B96B9C  clamp mfRelease into [0.0, 30.0]  (flt_82001CC0 / flt_8215BF18)
//   0x82B96BA0..0x82B96BB4  clamp mfThreshold to >= -500.0    (flt_8217F31C)
//     -- all three clamps write BACK into the live attribute, which is why Process re-reads
//        the attributes when it refreshes its snapshot after this call.
//   0x82B96BB8..0x82B96BE0  f1 = frsp(pow(10.0, mfThreshold * 0.05f))  -- dB -> linear "on"
//                           threshold (sub_82C09970 == the CRT pow, see the header note).
//   0x82B96BD8/BF0/BF8      f3 = 1.0f / mfRatio - 1.0f       (fdivs then fsubs)
//   0x82B96BFC/C04          f2 = thresholdOn * 0.95f         (flt_82004FDC)
//   0x82B96BE4..0x82B96C3C  r7 = round-half-away(sampleRate * mfAttack), 0 mapped to 1
//                           (fadds/fsubs 0.5 chosen on the sign, then fctiwz = truncate)
//   0x82B96C40..0x82B96C74  r8 = round-half-away(mfRelease * sampleRate), 0 mapped to 1
//   0x82B96C78..0x82B96C88  r9 = (mfChannelLink == 1.0f)
//   0x82B96C8C/C90          tail into CompressorLimiter1::Configure(self + 0x50, ...)
//
// ARGUMENT COUNT: the bl sets r3, f1, f2, f3, r7, r8, r9 and NOTHING else. Under the Xenon
// ABI each parameter owns one doubleword slot, so the three float args consume the r4/r5/r6
// slots and r7/r8/r9 are parameters 5/6/7 -- seven arguments total. The callee @0x82B67188
// confirms it: it reads exactly r3/f1/f2/f3/r7/r8/r9 and touches no further argument register.
// The a8/a9/a10 tail parameters that used to sit on the CompressorLimiter1::Configure
// declaration were Hex-Rays artifacts; the console leaves those slots UNSET and the callee
// never reads them. FIXED 2026-08-28 (phase E): the filed fix landed -- the three phantom
// parameters are GONE from CompressorLimiter1.h, and the inert `0, 0, 0` filler that used to
// pad this call site is retired with them. (The independent Limiter1::Configure decode
// reached the identical seven-argument conclusion from its own call site.)
//
// The return value is whatever CompressorLimiter1::Configure leaves in r3 -- it never writes
// r3, so it is the engine pointer that was passed in. Process discards it.
// -------------------------------------------------------------------------------------
CompressorLimiter1 *Compressor1::Configure(Compressor1 *self, f32 lfSampleRate)
{
    // NaN POLARITY. fcmpu sets LT/GT/EQ/FU; `bge` is `bc 4,LT` and `ble` is `bc 4,GT`, so BOTH
    // are TAKEN when the compare is unordered. A NaN attribute therefore takes the branch AWAY
    // from the floor store on the console -- it is left unclamped, not floored. The C++ `>=`
    // and `<=` forms are false for NaN and would store the floor, so each lower-bound test is
    // written as the NEGATED ordered predicate, which reproduces the console's unordered path.
    const f32 lfAttack = self->mfAttack;
    if (!(lfAttack < KF_ZERO)) // fcmpu/bge @0x82B96B54: NaN branches here, no floor store
    {
        if (lfAttack > KF_ATTACK_MAX)
            self->mfAttack = KF_ATTACK_MAX;
    }
    else
    {
        self->mfAttack = KF_ZERO;
    }

    const f32 lfRelease = self->mfRelease;
    if (!(lfRelease < KF_ZERO)) // fcmpu/bge @0x82B96B7C: same unordered shape as the attack clamp
    {
        if (lfRelease > KF_RELEASE_MAX)
            self->mfRelease = KF_RELEASE_MAX;
    }
    else
    {
        self->mfRelease = KF_ZERO;
    }

    if (self->mfThreshold < KF_THRESHOLD_MIN) // fcmpu/bge @0x82B96BAC: NaN skips the store
        self->mfThreshold = KF_THRESHOLD_MIN;

    // dB -> linear: 10^(threshold/20). The exponent multiply is a single-precision fmuls, the
    // pow itself runs in double, and frsp narrows the result back to f32 before it is used.
    const f32 lfThresholdOn = static_cast<f32>(
        std::pow(KD_DB_LOG_BASE, static_cast<f64>(self->mfThreshold * KF_DB_EXP_SCALE)));

    // Attack length in samples: round half away from zero (fctiwz truncates, so the console
    // biases by +/-0.5 on the sign first), with 0 promoted to 1 so the engine never divides
    // by zero when it derives mCompExponentStepOn.
    const f32 lfAttackSamples = lfSampleRate * self->mfAttack; // fmuls f0, f30, f0
    s32 liAttackSamples = static_cast<s32>(lfAttackSamples < KF_ZERO
                                               ? lfAttackSamples - KF_ROUND_HALF
                                               : lfAttackSamples + KF_ROUND_HALF);
    if (liAttackSamples == 0)
        liAttackSamples = 1;

    const f32 lfReleaseSamples = self->mfRelease * lfSampleRate; // fmuls f0, f0, f30
    s32 liReleaseSamples = static_cast<s32>(lfReleaseSamples < KF_ZERO
                                                ? lfReleaseSamples - KF_ROUND_HALF
                                                : lfReleaseSamples + KF_ROUND_HALF);
    if (liReleaseSamples == 0)
        liReleaseSamples = 1;

    // li r9, 1 / fcmpu vs 1.0 / beq keeps it, else li r9, 0.
    const s32 liGroupChannels = (self->mfChannelLink == KF_ONE) ? 1 : 0;

    return CompressorLimiter1::Configure(&self->mCompressorLimiter,
                                         lfThresholdOn,                          // f1
                                         lfThresholdOn * KF_THRESHOLD_OFF_MUL,   // f2
                                         KF_ONE / self->mfRatio - KF_ONE,        // f3
                                         liAttackSamples,                        // r7
                                         liReleaseSamples,                       // r8
                                         liGroupChannels);                       // r9
}

// -------------------------------------------------------------------------------------
// Process @0x82B9D988 -- per-block entry.
//
// If the compressor is effectively disengaged (threshold >= 20 dB, or ratio <= 1.0) the stage
// bypasses: on the active->bypass edge it clears the engine's envelope buffer once, then keeps
// the threshold/ratio snapshot current so a later re-engage reconfigures. Otherwise it engages
// (setting the active flag), reads the frame's output sample-rate from the context format, and
// re-Configures the embedded CompressorLimiter1 only when a live attribute or the sample-rate
// has drifted from the last-configured snapshot; then it runs the engine over the frame with the
// base channel count. Returns 1.
//
// The snapshot stores after the Configure call RE-READ the attributes from the object (asm
// 0x82B9DA54..0x82B9DA78) rather than reusing the values compared above -- that is load-bearing,
// because Configure clamps mfAttack / mfRelease / mfThreshold in place.
// -------------------------------------------------------------------------------------
int Compressor1::Process(Compressor1 *self, AudioProcessContext *ctx)
{
    const f32 lfThreshold = self->mfThreshold;
    const f32 lfRatio     = self->mfRatio;

    // fcmpu/bge @0x82B9D9B4 and fcmpu/ble @0x82B9D9C8 both branch to the bypass path, and both
    // are taken when unordered -- so a NaN threshold or ratio BYPASSES on the console. Negated
    // ordered predicates preserve that; the plain >= / <= forms would engage the compressor.
    if (!(lfThreshold < KF_BYPASS_THRESHOLD) || !(lfRatio > KF_BYPASS_RATIO))
    {
        // Disengaged: clear the engine buffer exactly once on the active->bypass edge.
        if (self->muActive == 1)
        {
            CompressorLimiter1::ClearBuffer(&self->mCompressorLimiter);
            self->muActive = 0;
        }
        self->mfLastThreshold = self->mfThreshold;
        self->mfLastRatio     = self->mfRatio;
    }
    else
    {
        if (self->muActive == 0)
            self->muActive = 1;

        const f32 lfSampleRate = ctx->mpFormat->mfSampleRate;

        // Re-Configure the engine only when a live attribute (or the sample-rate) has drifted
        // from the last-configured snapshot.
        if (lfThreshold != self->mfLastThreshold
            || lfRatio != self->mfLastRatio
            || self->mfAttack != self->mfLastAttack
            || self->mfRelease != self->mfLastRelease
            || self->mfChannelLink != self->mfLastChannelLink
            || lfSampleRate != self->mfLastSampleRate)
        {
            Compressor1::Configure(self, lfSampleRate); // return value discarded by the asm
            self->mfLastThreshold   = self->mfThreshold;
            self->mfLastRatio       = self->mfRatio;
            self->mfLastAttack      = self->mfAttack;
            self->mfLastRelease     = self->mfRelease;
            self->mfLastChannelLink = self->mfChannelLink;
            self->mfLastSampleRate  = lfSampleRate;
        }

        CompressorLimiter1::Process(&self->mCompressorLimiter, ctx,
                                    self->mBase.mbChannelCount);
    }
    return 1;
}

} // namespace core
} // namespace audio
} // namespace rw
