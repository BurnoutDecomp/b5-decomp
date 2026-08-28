// =====================================================================================
// rw::audio::core::Pan2D bodies -- the surround spatial panner plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. No Feb-2007 leak source, no DecFIGS DWARF, and no
// ProStreet08 PDB entry exist for this type -- see Pan2D.h for the byte-exact layout and
// the per-function ARTIST addresses. Every body is a branch-for-branch / store-for-store
// equivalent of the disassembly; the compiler register-save/restore thunks
// (_savefpr_2x / _savegprlr_2x / _restfpr_2x / _restgprlr_2x) are pure prologue/epilogue
// and are dropped, and the `blkmov` intrinsic is the compiler's inlined memcpy.
// =====================================================================================

#include "rw/audio/core/plugins/Pan2D.h"
#include "rw/audio/core/PlugIn.h"  // PlugInDescRunTime (the real descriptor record)
#include "rw/audio/core/MixKernels.h" // CopyWithGain, CopyWithGainRamp

#include <cmath>   // sin, cos, floor, sqrtf
#include <cstring> // memcpy (the X360 `blkmov` intrinsic)

namespace rw
{
namespace audio
{
namespace core
{

// off_8217F4A4 -- the Pan2D v-table installed at construction. off_820AA810 -- the base
// PlugIn v-table the deleting destructor reinstalls before any free. off_82F8EFB8 -- the
// registered run-time descriptor record (its +0x00 label is the string "Pan2D"). These are
// opaque data symbols in the XEX (no exported contents); modelled as honest placeholder
// storage so the bodies below link without fabricating their contents (same idiom as Gain).
static void *const KP2_Pan2DVTable      = nullptr; // off_8217F4A4
static void *const KP2_BasePlugInVTable = nullptr; // off_820AA810
// off_82F8EFB8 -- the "Pan2D" runtime descriptor, REAL (descriptor-record wave).
// Metadata FLAG'd null.
static PlugInDescRunTime g_Pan2DDesc = {
    "Pan2D",
    reinterpret_cast<void *>(&Pan2D::GetSize),        // @0x82B982C0
    reinterpret_cast<void *>(&Pan2D::CreateInstance), // @0x82BA34A0
    0,
    reinterpret_cast<void *>(&Pan2D::Process),        // @0x82B99ED8
    0, 0, 0, 0,
    0,
    0x506E3230u,       // 'Pn20'
    4, 2, 5, 0, 0, 0,
    0
};

// Recovered rodata float constants (identified from the pseudocode literals / asm usage).
static const f32 KF_DEG2RAD     = 0.017453292f; // flt_8217F364 (PI/180)
static const f32 KF_INV_TWO_PI  = 0.15915494f;  // flt_82001C90 (1 / 2*PI)
static const f32 KF_TWO_PI      = 6.2831855f;   // flt_82001C94 (2*PI)
static const f32 KF_PI          = 3.1415927f;   // flt_8215A980
static const f32 KF_DECLICK_STEP = 0.015625f;   // flt_820ADC00 (1 / GAIN_DECLICK_FRAME_SIZE)

// The full mixer frame is processed per copy (li r6/r7, 0x100 == MIXER_FRAME_SIZE).
enum { KI_MIXER_FRAME_SIZE = 256 };

// flt_82F8EEC8 / flt_82F8EECC -- the default speaker azimuth / rear-spread angles (degrees)
// used by SpeakerConfig when no explicit config was supplied. RECOVERED 2026-08-25
// (audio-faithfulness wave 4) from the decrypted XEX rodata (file_off 0x3000 + vaddr -
// 0x82000000 -> 0xF91EC8/0xF91ECC; BE bytes 42 34 00 00 / 43 07 00 00): 45 and 135
// degrees -- the classic front-pair / rear-pair speaker azimuths. (Formerly declared-only
// unresolved externals, an honest refusal that also made the TU unlinkable.)
static const f32 KF_PAN2D_DEFAULT_AZIMUTH = 45.0f;  // flt_82F8EEC8
static const f32 KF_PAN2D_DEFAULT_SPREAD  = 135.0f; // flt_82F8EECC

// -------------------------------------------------------------------------------------
// GetSize @0x82B982C0 -- the plug-in's instance size in bytes.
// -------------------------------------------------------------------------------------
int Pan2D::GetSize()
{
    return 240; // li r3, 0xF0
}

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B984E8 -- return the address of the registered descriptor
// record (its label is the string "Pan2D").
// -------------------------------------------------------------------------------------
char **Pan2D::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&g_Pan2DDesc); // &off_82F8EFB8 (the record address)
}

// -------------------------------------------------------------------------------------
// ComputeTransform @0x82B984F8 -- build the 2x2 rotation matrix for output sector `sector`
// from the two boundary angles (angleA, angleB), and store it into mfSectorMatrix[sector].
//   det = 1 / (cos(angleB)*sin(angleA) - sin(angleB)*cos(angleA))
//   m[0] =  det*cos(angleB);  m[1] = -det*sin(angleB)
//   m[2] = -det*cos(angleA);  m[3] =  det*sin(angleA)
// (sin/cos are double library calls narrowed to single per store, matching the frsp asm.)
// -------------------------------------------------------------------------------------
void Pan2D::ComputeTransform(Pan2D *self, int sector, f64 angleA, f64 angleB)
{
    const f32 sinA = static_cast<f32>(sin(angleA));
    const f32 cosA = static_cast<f32>(cos(angleA));
    const f32 sinB = static_cast<f32>(sin(angleB));
    const f32 cosB = static_cast<f32>(cos(angleB));

    const f32 det = 1.0f / ((cosB * sinA) - (sinB * cosA));

    self->mfSectorMatrix[sector][0] =  det * cosB; // +0x7C
    self->mfSectorMatrix[sector][1] = -(det * sinB); // +0x80
    self->mfSectorMatrix[sector][2] = -(det * cosA); // +0x84
    self->mfSectorMatrix[sector][3] =  det * sinA; // +0x88
}

// -------------------------------------------------------------------------------------
// SpeakerConfig @0x82B985D0 -- derive the speaker geometry from the (configured or default)
// front/rear half-angles, then build the four per-sector rotation matrices.
// -------------------------------------------------------------------------------------
int Pan2D::SpeakerConfig(Pan2D *self)
{
    f32 spreadRad;
    if (self->mbHasConfig)
    {
        self->mfSpeakerAngle = self->mfConfigAzimuth * KF_DEG2RAD; // +0x6C
        spreadRad            = self->mfConfigSpread * KF_DEG2RAD;
    }
    else
    {
        self->mfSpeakerAngle = KF_PAN2D_DEFAULT_AZIMUTH * KF_DEG2RAD;
        spreadRad            = KF_PAN2D_DEFAULT_SPREAD * KF_DEG2RAD;
    }
    self->mfSpeakerAngle2 = spreadRad; // +0x70

    self->mfFrontWidth = static_cast<f32>(cos(self->mfSpeakerAngle)) * 2.0f;              // +0x74
    self->mfRearWidth  = static_cast<f32>(cos(KF_PI - self->mfSpeakerAngle2)) * 2.0f;     // +0x78

    ComputeTransform(self, 0, -self->mfSpeakerAngle,  self->mfSpeakerAngle);
    ComputeTransform(self, 1,  self->mfSpeakerAngle,  self->mfSpeakerAngle2);
    ComputeTransform(self, 2,  self->mfSpeakerAngle2, -self->mfSpeakerAngle2);
    ComputeTransform(self, 3, -self->mfSpeakerAngle2, -self->mfSpeakerAngle);
    return 1; // (dead: the X360 tail-returns the last ComputeTransform; the sole caller ignores it)
}

// -------------------------------------------------------------------------------------
// ComputeZeroRadiusLevels @0x82B99930 -- the omnidirectional (radius == 0) per-channel
// level set: front L/R share one gain, rear L/R share one gain, and (6-ch only) the centre
// carries the spread level; all normalised to unit power.
// -------------------------------------------------------------------------------------
Pan2D *Pan2D::ComputeZeroRadiusLevels(Pan2D *self)
{
    f32 centre = 0.0f;
    if (self->mBase.mbChannelCount == 6)
        centre = self->mfSpread; // +0x38

    const f32 rear  = 1.0f / self->mfRearWidth;                 // +0x78
    const f32 front = (1.0f - centre) / self->mfFrontWidth;     // +0x74

    const f32 norm = 1.0f /
        sqrtf((centre * centre) + (((rear * rear) * 2.0f) + ((front * front) * 2.0f)));

    self->mfZeroRadiusLevel[0] = norm * front; // +0xBC
    self->mfZeroRadiusLevel[1] = norm * front; // +0xC0
    self->mfZeroRadiusLevel[2] = norm * rear;  // +0xC4
    self->mfZeroRadiusLevel[3] = norm * rear;  // +0xC8
    self->mfZeroRadiusLevel[4] = norm * centre; // +0xCC
    return self;
}

// -------------------------------------------------------------------------------------
// ComputeLevels @0x82B999B0 -- compute the per-output-channel levels for the current
// azimuth/radius. Stereo (2-ch) uses a constant-power sin/cos law; surround crossfades the
// omnidirectional zero-radius levels (scaled by sqrt(1-radius^2)) with a directional
// contribution steered through the per-sector rotation matrices (scaled by radius), then
// renormalises to constant power * master gain.
// -------------------------------------------------------------------------------------
void Pan2D::ComputeLevels(Pan2D *self)
{
    const u8  channels    = self->mBase.mbChannelCount; // +0x21
    const f32 radius      = self->mfRadius;             // +0x30
    const f32 spread      = self->mfSpread;             // +0x38
    const f32 masterLevel = self->mfGain;              // +0x40

    if (channels == 2)
    {
        const f32 pan = static_cast<f32>(sin(self->mfAzimuth * KF_DEG2RAD));
        self->mfLevel[0] = sqrtf(1.0f - ((pan + 1.0f) * 0.5f)) * masterLevel;
        self->mfLevel[1] = sqrtf((pan + 1.0f) * 0.5f) * masterLevel;
        return;
    }

    const f32 azimuthRad = self->mfAzimuth * KF_DEG2RAD;

    // Direct LFE/6th-channel pass-through and the sqrt(1-radius^2) omnidirectional blend.
    self->mfLevel[5] = self->mfLfeSend;                  // +0xE8 = +0x48
    const f32 dirScale = sqrtf(1.0f - (radius * radius));
    self->mfLevel[0] = self->mfZeroRadiusLevel[0] * dirScale;
    self->mfLevel[1] = self->mfZeroRadiusLevel[1] * dirScale;
    self->mfLevel[2] = self->mfZeroRadiusLevel[2] * dirScale;
    self->mfLevel[3] = self->mfZeroRadiusLevel[3] * dirScale;
    self->mfLevel[4] = self->mfZeroRadiusLevel[4] * dirScale;

    // Wrap the steering angle into [0, 2*PI) relative to the front speaker, then locate the
    // output sector it falls in and mix the directional contribution through that sector's
    // rotation matrix.
    const f32 wrapped = (azimuthRad + self->mfSpeakerAngle) * KF_INV_TWO_PI;
    const f32 theta   = ((wrapped - static_cast<f32>(floor(static_cast<f64>(wrapped)))) * KF_TWO_PI)
                        - self->mfSpeakerAngle;
    const f32 s = static_cast<f32>(sin(theta));
    const f32 c = static_cast<f32>(cos(theta));

    if (theta >= self->mfSpeakerAngle)
    {
        if (theta >= self->mfSpeakerAngle2)
        {
            if (theta >= (KF_TWO_PI - self->mfSpeakerAngle2))
            {
                // Sector 3 (front-left return).
                const f32 base0 = self->mfLevel[0];
                const f32 x = (self->mfSectorMatrix[3][1] * c) + (self->mfSectorMatrix[3][0] * s);
                const f32 y = (self->mfSectorMatrix[3][3] * c) + (self->mfSectorMatrix[3][2] * s);
                const f32 inv = 1.0f / sqrtf((y * y) + (x * x));
                self->mfLevel[2] = (x * inv * radius) + self->mfLevel[2];
                self->mfLevel[0] = (y * inv * radius) + base0;
            }
            else
            {
                // Sector 2 (rear-right).
                const f32 base2 = self->mfLevel[2];
                const f32 x = (self->mfSectorMatrix[2][1] * c) + (self->mfSectorMatrix[2][0] * s);
                const f32 y = (self->mfSectorMatrix[2][3] * c) + (self->mfSectorMatrix[2][2] * s);
                const f32 inv = 1.0f / sqrtf((y * y) + (x * x));
                self->mfLevel[3] = (x * inv * radius) + self->mfLevel[3];
                self->mfLevel[2] = (y * inv * radius) + base2;
            }
        }
        else
        {
            // Sector 1 (front-right to rear-right).
            const f32 base3 = self->mfLevel[3];
            const f32 x = (self->mfSectorMatrix[1][1] * c) + (self->mfSectorMatrix[1][0] * s);
            const f32 y = (self->mfSectorMatrix[1][3] * c) + (self->mfSectorMatrix[1][2] * s);
            const f32 inv = 1.0f / sqrtf((y * y) + (x * x));
            self->mfLevel[1] = (x * inv * radius) + self->mfLevel[1];
            self->mfLevel[3] = (y * inv * radius) + base3;
        }
    }
    else
    {
        // Sector 0 (front centre span). 6-ch folds the shared component into the centre.
        f32 centre = 0.0f;
        f32 x = (self->mfSectorMatrix[0][1] * c) + (self->mfSectorMatrix[0][0] * s);
        f32 y = (self->mfSectorMatrix[0][3] * c) + (self->mfSectorMatrix[0][2] * s);
        if (channels == 6)
        {
            const f32 fold = ((x >= y) ? y : x) * spread;
            x -= fold;
            y -= fold;
            centre = self->mfFrontWidth * fold;
        }
        const f32 base0 = self->mfLevel[0];
        const f32 base4 = self->mfLevel[4];
        const f32 inv = 1.0f / sqrtf((x * x) + ((centre * centre) + (y * y)));
        self->mfLevel[1] = (y * inv * radius) + self->mfLevel[1];
        self->mfLevel[0] = (x * inv * radius) + base0;
        self->mfLevel[4] = (centre * inv * radius) + base4;
    }

    // Renormalise the five spatial levels to constant power, scaled by the master gain.
    const f32 l4 = self->mfLevel[4];
    const f32 l3 = self->mfLevel[3];
    const f32 l0 = self->mfLevel[0];
    const f32 l2 = self->mfLevel[2];
    const f32 gain = (1.0f / sqrtf((self->mfLevel[2] * self->mfLevel[2])
                       + ((self->mfLevel[0] * self->mfLevel[0])
                       + ((self->mfLevel[1] * self->mfLevel[1])
                       + ((self->mfLevel[3] * self->mfLevel[3])
                       + (self->mfLevel[4] * self->mfLevel[4])))))) * masterLevel;
    self->mfLevel[1] = self->mfLevel[1] * gain;
    self->mfLevel[0] = l0 * gain;
    self->mfLevel[2] = l2 * gain;
    self->mfLevel[3] = l3 * gain;
    self->mfLevel[4] = l4 * gain;
}

// -------------------------------------------------------------------------------------
// RampPanOutput @0x82B99D18 -- de-clicked apply of freshly-computed levels: ramp each
// output channel from its previous level (pOldLevel, snapshotted by Process before
// ComputeLevels) toward the new mfLevel over the 64-sample window via CopyWithGainRamp.
// The internal level order is remapped to WAV channel order (6-ch: ch1<-4, ch2<-1, ...).
// -------------------------------------------------------------------------------------
int Pan2D::RampPanOutput(Pan2D *self, AudioChannelBuffer *dstBuffer,
                         AudioChannelBuffer *srcBuffer, const f32 *pOldLevel)
{
    const u32 channelCount = self->mBase.mbChannelCount; // +0x21

    f32 step[6];
    for (u32 i = 0; i < channelCount; ++i)
        step[i] = (self->mfLevel[i] - pOldLevel[i]) * KF_DECLICK_STEP;

    f32       *srcSamples = srcBuffer->mpSamples; // a3->mpSamples
    f32       *dstSamples = dstBuffer->mpSamples; // a2->mpSamples
    const u16  stride     = dstBuffer->muStride;

    // Channel 0 is always emitted first.
    CopyWithGainRamp(dstSamples, srcSamples, pOldLevel[0], step[0], KI_MIXER_FRAME_SIZE);

    if (channelCount == 6)
    {
        CopyWithGainRamp(dstSamples + stride * 1, srcSamples, pOldLevel[4], step[4], KI_MIXER_FRAME_SIZE);
        CopyWithGainRamp(dstSamples + stride * 2, srcSamples, pOldLevel[1], step[1], KI_MIXER_FRAME_SIZE);
        CopyWithGainRamp(dstSamples + stride * 3, srcSamples, pOldLevel[2], step[2], KI_MIXER_FRAME_SIZE);
        CopyWithGainRamp(dstSamples + stride * 4, srcSamples, pOldLevel[3], step[3], KI_MIXER_FRAME_SIZE);
        CopyWithGainRamp(dstSamples + stride * 5, srcSamples, pOldLevel[5], step[5], KI_MIXER_FRAME_SIZE);
    }
    else if (channelCount == 4)
    {
        CopyWithGainRamp(dstSamples + stride * 1, srcSamples, pOldLevel[1], step[1], KI_MIXER_FRAME_SIZE);
        CopyWithGainRamp(dstSamples + stride * 2, srcSamples, pOldLevel[2], step[2], KI_MIXER_FRAME_SIZE);
        CopyWithGainRamp(dstSamples + stride * 3, srcSamples, pOldLevel[3], step[3], KI_MIXER_FRAME_SIZE);
    }
    else
    {
        CopyWithGainRamp(dstSamples + stride * 1, srcSamples, pOldLevel[1], step[1], KI_MIXER_FRAME_SIZE);
    }
    return 1; // (dead: the X360 tail-returns the last CopyWithGainRamp; the sole caller ignores it)
}

// -------------------------------------------------------------------------------------
// Process @0x82B99ED8 -- the per-frame panner. On `recompute`, refresh the omnidirectional
// and per-channel levels. If the (radians) azimuth and the radius/spread/gain/LFE inputs are
// unchanged since last frame, fast-path: copy the cached levels straight through
// (CopyWithGain). Otherwise recompute and de-click-ramp from the previous levels
// (RampPanOutput). Finally ping-pong the context's src/dst channel-buffer slots.
// -------------------------------------------------------------------------------------
int Pan2D::Process(Pan2D *self, AudioProcessContext *ctx, char recompute)
{
    if (recompute)
    {
        ComputeZeroRadiusLevels(self);
        ComputeLevels(self);
    }

    const u8  channels    = self->mBase.mbChannelCount;        // +0x21
    const f32 azimuthRad  = self->mfAzimuth * KF_DEG2RAD;      // +0x28
    const f32 radius      = self->mfRadius;                    // +0x30
    const f32 spread      = self->mfSpread;                    // +0x38
    const f32 gain        = self->mfGain;                     // +0x40
    const f32 lfe         = self->mfLfeSend;                  // +0x48

    AudioChannelBuffer *srcBuffer = ctx->mpSrcBuffer; // +0x3000C
    AudioChannelBuffer *dstBuffer = ctx->mpDstBuffer; // +0x30010

    if (azimuthRad == self->mfLastAzimuthRad && radius == self->mfLastRadius
        && spread == self->mfLastSpread && gain == self->mfLastGain
        && lfe == self->mfLastLfeSend)
    {
        // Fast path: inputs unchanged, apply the cached per-channel levels.
        f32       *srcSamples = srcBuffer->mpSamples;
        f32       *dstSamples = dstBuffer->mpSamples;
        const u16  stride     = dstBuffer->muStride;

        CopyWithGain(dstSamples, srcSamples, self->mfLevel[0], KI_MIXER_FRAME_SIZE);
        if (channels == 6)
        {
            CopyWithGain(dstSamples + stride * 1, srcSamples, self->mfLevel[4], KI_MIXER_FRAME_SIZE);
            CopyWithGain(dstSamples + stride * 2, srcSamples, self->mfLevel[1], KI_MIXER_FRAME_SIZE);
            CopyWithGain(dstSamples + stride * 3, srcSamples, self->mfLevel[2], KI_MIXER_FRAME_SIZE);
            CopyWithGain(dstSamples + stride * 4, srcSamples, self->mfLevel[3], KI_MIXER_FRAME_SIZE);
            CopyWithGain(dstSamples + stride * 5, srcSamples, self->mfLevel[5], KI_MIXER_FRAME_SIZE);
        }
        else if (channels == 4)
        {
            CopyWithGain(dstSamples + stride * 1, srcSamples, self->mfLevel[1], KI_MIXER_FRAME_SIZE);
            CopyWithGain(dstSamples + stride * 2, srcSamples, self->mfLevel[2], KI_MIXER_FRAME_SIZE);
            CopyWithGain(dstSamples + stride * 3, srcSamples, self->mfLevel[3], KI_MIXER_FRAME_SIZE);
        }
        else
        {
            CopyWithGain(dstSamples + stride * 1, srcSamples, self->mfLevel[1], KI_MIXER_FRAME_SIZE);
        }
    }
    else
    {
        // Slow path: an input changed -- recompute and de-click-ramp from the old levels.
        if (spread != self->mfLastSpread)
            ComputeZeroRadiusLevels(self);

        f32 oldLevel[6];
        if (channels)
            memcpy(oldLevel, &self->mfLevel[0], 4u * channels); // blkmov

        ComputeLevels(self);

        self->mfLastAzimuthRad = azimuthRad;
        self->mfLastRadius     = radius;
        self->mfLastSpread     = spread;
        self->mfLastGain       = gain;
        self->mfLastLfeSend    = lfe;

        RampPanOutput(self, dstBuffer, srcBuffer, oldLevel);
    }

    // Ping-pong the src/dst channel-buffer slots for the next stage.
    AudioChannelBuffer *swapTmp = ctx->mpSrcBuffer;
    ctx->mpSrcBuffer = ctx->mpDstBuffer;
    ctx->mpDstBuffer = swapTmp;
    return 1;
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA34A0 -- placement-init a Pan2D over `self`. `pConfig` (optional) is
// the {front half-angle, rear half-angle} pair (degrees); when null the defaults apply.
// -------------------------------------------------------------------------------------
int Pan2D::CreateInstance(Pan2D *self, const f32 *pConfig)
{
    if (self)
        self->mBase.mpVTable = KP2_Pan2DVTable; // off_8217F4A4

    // Point the base attribute-table slot at the first live input attribute (self+0x28).
    self->mBase.mpAttributes = &self->mfAzimuth; // +0x0C = self+0x28

    if (pConfig)
    {
        self->mfConfigAzimuth = pConfig[0]; // +0x64
        self->mfConfigSpread  = pConfig[1]; // +0x68
        self->mbHasConfig     = 1;
    }
    else
    {
        self->mbHasConfig = 0;
    }

    // Inputs and their cached snapshot: azimuth/LFE start at 0, radius/spread/gain at 1.
    self->mfLastAzimuthRad = 0.0f; // +0x50
    self->mfAzimuth        = 0.0f; // +0x28
    self->mfLastLfeSend    = 0.0f; // +0x60
    self->mfLfeSend        = 0.0f; // +0x48
    self->mfLastRadius     = 1.0f; // +0x54
    self->mfRadius         = 1.0f; // +0x30
    self->mfLastSpread     = 1.0f; // +0x58
    self->mfSpread         = 1.0f; // +0x38
    self->mfLastGain       = 1.0f; // +0x5C
    self->mfGain           = 1.0f; // +0x40

    SpeakerConfig(self);
    return 1;
}

// -------------------------------------------------------------------------------------
// `vector deleting destructor' @0x82BA1AB8 -- reinstall the base PlugIn vtable, then free
// the instance when the deleting bit (flags & 1) is set.
// -------------------------------------------------------------------------------------
void *Pan2D::VectorDeletingDestructor(Pan2D *self, char flags)
{
    self->mBase.mpVTable = KP2_BasePlugInVTable; // off_820AA810
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
