// =====================================================================================
// rw::audio::core::Snd9Service bodies -- the Snd9 (Xbox/Win32) hardware audio service.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. No Feb-2007 leak source, no DecFIGS DWARF, and no
// ProStreet08 PDB entry exist for this type.
//   CreateInstance                @0x82BA0130 -- store-for-store placement init
//   GetPlugInDescRunTime          @0x82B9BB28 -- returns the registered "Snd9Service" desc
//   Process                       @0x82B9BD80 -- 6-channel interleave + block ping-pong
//   ReleaseEvent                  @0x82B9BB38 -- unregister timer / restore / free
//   RwacTimerClient               @0x82B9BC38 -- branch-for-branch 100 Hz mixer pump
//   UpdateSnd9SampleRate          @0x82B9BB98 -- destroy+recreate the Snd9 mixer at a rate
//   `vector deleting destructor'  @0x82BA00D0 -- reinstall base vtable, conditional free
// See Snd9Service.h for the byte-exact layout.
// =====================================================================================

#include "rw/audio/core/Snd9Service.h"
#include "rw/audio/core/PlugIn.h"        // rw::audio::core::System (mpSystem->mTimerManager, Alloc/Free/RemoveTimer)
#include "rw/audio/core/TimerManager.h"  // rw::audio::core::TimerManager::AddTimer
#include "rw/audio/core/Iir2Filters.h"   // AudioProcessContext / AudioChannelBuffer (Process context)

#include <cstring> // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)

namespace rw
{
namespace audio
{
namespace core
{

// The audio-core System singleton (off_83271928) -- ReleaseEvent frees the mix buffer back
// through it. Defined (as C linkage) in the System TU; declared here so this TU links.
extern "C" System *off_83271928;

// -------------------------------------------------------------------------------------
// Opaque data symbols in the XEX (no exported contents); modelled as honest placeholder
// storage so the bodies below link without fabricating their contents.
//   off_8217F384 -- the Snd9Service v-table installed by CreateInstance.
//   off_820AA810 -- the base-Service v-table the deleting destructor reinstalls (the same
//                   base v-table the PlugIn family's deleting destructors reinstall).
//   off_82F90158 -- the registered run-time descriptor record (its +0x00 label is the
//                   string "Snd9Service").
// -------------------------------------------------------------------------------------
static void *const KSND9_ServiceVTable = nullptr; // off_8217F384
static void *const KSND9_BaseVTable    = nullptr; // off_820AA810
static char       *g_Snd9ServiceDesc   = nullptr; // off_82F90158 (the "Snd9Service" record)

// The mixer rate every path forces (flt_820AA808) and the per-channel copy span (0x400
// bytes == a 256-sample f32 frame).
static const f32 KF_SND9_SAMPLE_RATE   = 48000.0f;
enum { KU_SND9_MIX_CHANNEL_BYTES = 0x400 }; // 1024 bytes == 256 f32 samples

// The RwacTimerClient 100 Hz pacing constants: pump a full 256-sample frame per iteration,
// meter 48000 samples across 100 ticks, wrap the tick accumulator at 30000, and align each
// owed-sample count down to a 16-sample boundary (mask 0x0FFFFFF0).
enum
{
    KI_SND9_FRAME_SAMPLES   = 0x100,       // 256 -- samples pumped per service iteration
    KI_SND9_SAMPLE_RATE_HZ  = 48000,       // metered against the 100 Hz tick counter
    KI_SND9_TICKS_PER_SEC   = 100,
    KI_SND9_COUNTER_WRAP    = 0x7530,      // 30000
    KI_SND9_SAMPLE_ALIGN    = 0x0FFFFFF0,
};

// -------------------------------------------------------------------------------------
// Snd9 low-level mixer / voice / packet-player driver API. These are un-exported C-linkage
// driver entry points (no disassembly available); declared here and left to link (trap-stub
// semantics) rather than reconstructed. The r3 threading the ARTIST asm shows between the
// void-ish service calls is preserved via int returns so the observable data flow matches.
// -------------------------------------------------------------------------------------
extern "C" int  SNDSYS_service(void);
extern "C" int  SNDSYS_restore(void);
extern "C" int  SNDSYSI_100hzserver(int status);
extern "C" int  SNDSYSI_variabletimerservice(int status);
extern "C" int  SNDPKTPLAYI_flushcallbackdata(int status);
extern "C" int  MIX_audio(void *channels, int sampleCount);
extern "C" int  MIX_destroy(void);
extern "C" int  MIX_create(void *params);
extern "C" void SNDVOICEI_free(void); // the voice-free callback stored in the MIX_create params

// The Snd9 driver's global channel-config region (byte_83277838). UpdateSnd9SampleRate reads
// three of its bytes into the MIX_create params. Un-homed driver storage; declared as an
// opaque byte region and indexed exactly as the asm's `lbz` loads read it (endian-neutral).
extern "C" unsigned char byte_83277838[];

// The embedded TimerHandle sub-object's destructor, invoked on (self+0x24) by the vector
// deleting destructor. IDA renders the callee as an unresolved STUB and it is un-exported
// with no disassembly, so it is declared here and left to link rather than fabricated.
void Snd9Service_DestructTimerHandle(TimerHandle *handle);

// The 11-byte parameter block UpdateSnd9SampleRate builds on the stack for MIX_create.
// Grounded in the store sequence at 0x82B9BC00..0x82B9BC1C.
struct Snd9MixCreateParams
{
    s32   miSampleRate;  // +0x00  rounded target rate
    void *mpVoiceFree;   // +0x04  &SNDVOICEI_free
    u8    mbChannelCfg0; // +0x08  byte_83277838[0x0A]
    u8    mbChannelCfg1; // +0x09  byte_83277838[0x10] + byte_83277838[0x0F]
    u8    mbChannelCfg2; // +0x0A  0
};

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B9BB28 -- return the address of the registered descriptor
// record (its label is the string "Snd9Service").
// -------------------------------------------------------------------------------------
char **Snd9Service::GetPlugInDescRunTime()
{
    return &g_Snd9ServiceDesc; // &off_82F90158
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA0130 -- placement-init a Snd9Service over `self`.
//   if (self) { self->mpVTable = off_8217F384; TimerHandle_ctor(&self->mTimerHandle); }
//   self->mbTimerCreated = 0;
//   buf = System::Alloc(self->mpSystem, 6144, "...mpMixBufStart", 128, 0);
//   self->mpMixBufStart = buf;  if (!buf) return 0;
//   carve the six channel sub-buffers, seed rate + counters,
//   register the pump timer; on failure return 0, else mbTimerCreated = 1, return 1.
// -------------------------------------------------------------------------------------
int Snd9Service::CreateInstance(Snd9Service *self)
{
    if (self)
    {
        self->mpVTable = KSND9_ServiceVTable;              // off_8217F384 -> +0x00
        TimerHandle::TimerHandle_ctor(&self->mTimerHandle); // ctor(self+0x24)
    }

    self->mbTimerCreated = 0; // stb 0 -> +0x68 (before the alloc)

    void *buf = System::Alloc(self->mpSystem, 6144,
                              "rw::audio::core::Snd9Service::mpMixBufStart", 128, 0);
    self->mpMixBufStart = buf; // +0x3C
    if (!buf)
        return 0;

    // Carve six 1024-byte channel sub-buffers out of the single 6144-byte allocation.
    // The stored order matches the asm's displacements exactly (not a simple ascending map).
    unsigned char *base = static_cast<unsigned char *>(buf);
    self->mpMixChannels[0] = base + 0x400;  // +0x40
    self->mpMixChannels[1] = base + 0x800;  // +0x44
    self->mpMixChannels[2] = base + 0x1000; // +0x48
    self->mpMixChannels[3] = base + 0xC00;  // +0x4C
    self->mpMixChannels[4] = base + 0x000;  // +0x50
    self->mpMixChannels[5] = base + 0x1400; // +0x54

    self->mfSampleRate      = KF_SND9_SAMPLE_RATE; // +0x58  48000.0
    self->miSampleCounter   = 0;                   // +0x5C
    self->miSampleAccum     = 0;                   // +0x60
    self->miSamplesRemaining = 0;                  // +0x64

    // Register the 100 Hz pump timer in the System's TimerManager (collection 1, visible).
    // The asm passes RwacTimerClient's address directly as the callback; the timer's
    // (context, time) signature differs from the body's single-arg use, so cast the address.
    if (TimerManager::AddTimer(&self->mpSystem->mTimerManager, &self->mTimerHandle,
                               reinterpret_cast<TimerManager::TimerCallback>(
                                   &Snd9Service::RwacTimerClient),
                               self, "Snd9Service", 1, 1) & 0xFF)
    {
        return 0; // AddTimer failed (non-zero low byte)
    }

    self->mbTimerCreated = 1; // stb 1 -> +0x68
    return 1;
}

// -------------------------------------------------------------------------------------
// Process @0x82B9BD80 -- interleave the six internal mix channels into the context's
// destination output block, then ping-pong the src/dst block slots.
//   dst = ctx->mpDstBuffer(+0x30010);
//   for the six channels: memcpy(dst->mpSamples + dst->muStride*k, mMixChannels[perm(k)], 1024);
//   swap(ctx->mpSrcBuffer, ctx->mpDstBuffer);  return 1;
// The destination stride step matches the asm (base + 4*stride*k bytes == mpSamples[stride*k]).
// -------------------------------------------------------------------------------------
int Snd9Service::Process(Snd9Service *self, AudioProcessContext *ctx)
{
    AudioChannelBuffer *dstBuffer = ctx->mpDstBuffer; // *(ctx+0x30010)
    f32 *base = dstBuffer->mpSamples;                 // *(dst+0x04)
    const u32 stride = dstBuffer->muStride;           // *(u16*)(dst+0x0E)

    // Fixed source->slot permutation (mMixChannels index per destination channel k).
    std::memcpy(base + stride * 0, self->mpMixChannels[4], KU_SND9_MIX_CHANNEL_BYTES);
    std::memcpy(base + stride * 1, self->mpMixChannels[0], KU_SND9_MIX_CHANNEL_BYTES);
    std::memcpy(base + stride * 2, self->mpMixChannels[1], KU_SND9_MIX_CHANNEL_BYTES);
    std::memcpy(base + stride * 3, self->mpMixChannels[3], KU_SND9_MIX_CHANNEL_BYTES);
    std::memcpy(base + stride * 4, self->mpMixChannels[2], KU_SND9_MIX_CHANNEL_BYTES);
    std::memcpy(base + stride * 5, self->mpMixChannels[5], KU_SND9_MIX_CHANNEL_BYTES);

    // Ping-pong the src/dst channel-buffer slots for the next stage.
    AudioChannelBuffer *swapTmp = ctx->mpSrcBuffer;
    ctx->mpSrcBuffer = ctx->mpDstBuffer;
    ctx->mpDstBuffer = swapTmp;
    return 1;
}

// -------------------------------------------------------------------------------------
// ReleaseEvent @0x82B9BB38 -- unregister the pump timer (only if it was created), restore
// the Snd9 driver, then free the mix buffer if it exists.
//   if (mbTimerCreated == 1) System::RemoveTimer(mpSystem, &mTimerHandle);
//   result = SNDSYS_restore();
//   if (mpMixBufStart) System::Free(off_83271928, mpMixBufStart, 0);
//   return result;
// (System::RemoveTimer / System::Free are void here; the asm's r3 pass into SNDSYS_restore
// and the returned r3 from Free are that void-return register left-over, so the meaningful
// return is SNDSYS_restore's status -- preserved as `result`.)
// -------------------------------------------------------------------------------------
int Snd9Service::ReleaseEvent(Snd9Service *self)
{
    if (self->mbTimerCreated == 1)
        System::RemoveTimer(self->mpSystem, &self->mTimerHandle);

    int result = SNDSYS_restore();

    if (self->mpMixBufStart)
        System::Free(off_83271928, self->mpMixBufStart, 0);

    return result;
}

// -------------------------------------------------------------------------------------
// RwacTimerClient @0x82B9BC38 -- the registered pump callback.
//   snapshot the six channel cursors; if the mixer drifted off 48 kHz, retune + reset;
//   then for one 256-sample service window: when the owed-sample count runs out, tick the
//   100 Hz meter and refill it; pump min(owed, remaining) samples through MIX_audio +
//   the packet player, advance the cursors, and repeat until the window is drained.
// (The timer's time value is ignored -- the asm uses only r3 = the service context.)
// -------------------------------------------------------------------------------------
int Snd9Service::RwacTimerClient(Snd9Service *self)
{
    // Local read cursors over the six channel buffers (advanced as samples are consumed;
    // the members themselves are not moved). Byte pointers: advanced by 4 bytes/sample.
    unsigned char *channelCursors[6];
    for (int i = 0; i < 6; ++i)
        channelCursors[i] = static_cast<unsigned char *>(self->mpMixChannels[i]);

    if (self->mfSampleRate != KF_SND9_SAMPLE_RATE)
    {
        UpdateSnd9SampleRate(48000.0);
        self->mfSampleRate       = KF_SND9_SAMPLE_RATE; // +0x58
        self->miSampleCounter    = 0;                   // +0x5C
        self->miSampleAccum      = 0;                   // +0x60
        self->miSamplesRemaining = 0;                   // +0x64
    }

    int result = SNDSYS_service();
    int remaining = KI_SND9_FRAME_SAMPLES; // 256
    do
    {
        if (self->miSamplesRemaining <= 0)
        {
            ++self->miSampleCounter;
            int hz = SNDSYSI_100hzserver(result);
            SNDSYSI_variabletimerservice(hz);

            int counter = self->miSampleCounter;
            int accum   = self->miSampleAccum;
            bool wrap   = counter > KI_SND9_COUNTER_WRAP;
            int owed = (KI_SND9_SAMPLE_RATE_HZ * counter / KI_SND9_TICKS_PER_SEC - accum)
                       & KI_SND9_SAMPLE_ALIGN;
            self->miSamplesRemaining = owed;
            self->miSampleAccum      = owed + accum;
            if (wrap)
            {
                self->miSampleCounter = 0;
                self->miSampleAccum   = 0;
            }
        }

        int owed = self->miSamplesRemaining;
        int chunk = (owed >= remaining) ? remaining : owed;
        remaining -= chunk;
        self->miSamplesRemaining = owed - chunk;

        int mixed = MIX_audio(channelCursors, chunk);
        result = SNDPKTPLAYI_flushcallbackdata(mixed);

        for (int i = 0; i < 6; ++i)
            channelCursors[i] += 4 * chunk;
    } while (remaining > 0);

    return result;
}

// -------------------------------------------------------------------------------------
// UpdateSnd9SampleRate @0x82B9BB98 -- destroy the current Snd9 mixer and recreate it at
// `sampleRate` (rounded to nearest), wiring the voice-free callback and the driver's
// channel-config bytes into the MIX_create parameter block.
//   MIX_destroy();
//   params.mbChannelCfg0 = byte_83277838[0x0A];
//   params.mbChannelCfg1 = byte_83277838[0x10] + byte_83277838[0x0F];
//   params.miSampleRate  = (int)(sampleRate < 0 ? sampleRate - 0.5 : sampleRate + 0.5);
//   params.mpVoiceFree   = &SNDVOICEI_free;   params.mbChannelCfg2 = 0;
//   return MIX_create(&params);
// (The asm passes the rate in f1 only; r3 into MIX_destroy is a stale caller register.)
// -------------------------------------------------------------------------------------
int Snd9Service::UpdateSnd9SampleRate(f64 sampleRate)
{
    MIX_destroy();

    Snd9MixCreateParams params;
    params.mbChannelCfg0 = byte_83277838[0x0A];
    params.mbChannelCfg1 = static_cast<u8>(byte_83277838[0x10] + byte_83277838[0x0F]);

    // Round-to-nearest via the classic +/-0.5-then-truncate the asm performs (fctiwz).
    f64 rounded = (sampleRate < 0.0) ? (sampleRate - 0.5) : (sampleRate + 0.5);
    params.miSampleRate = static_cast<s32>(rounded);
    params.mpVoiceFree  = reinterpret_cast<void *>(&SNDVOICEI_free);
    params.mbChannelCfg2 = 0;

    return MIX_create(&params);
}

// -------------------------------------------------------------------------------------
// `vector deleting destructor' @0x82BA00D0
//   Snd9Service_DestructTimerHandle(&self->mTimerHandle); // STUB(self+0x24)
//   self->mpVTable = off_820AA810;   // reinstall the base-Service v-table
//   if (flags & 1) operator delete(self);
//   return self;
// -------------------------------------------------------------------------------------
void *Snd9Service::VectorDeletingDestructor(Snd9Service *self, char flags)
{
    Snd9Service_DestructTimerHandle(&self->mTimerHandle); // STUB(self+0x24)
    self->mpVTable = KSND9_BaseVTable;                    // off_820AA810
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
