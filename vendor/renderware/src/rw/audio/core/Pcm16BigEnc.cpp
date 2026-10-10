// =====================================================================================
// rw::audio::core::Pcm16BigEnc bodies -- the 'P6B0' (16-bit PCM) stream encoder.
//
// EARenderWare "rwaudio". Reconstructed from the console image; its PowerPC asm is
// authoritative for every store. See Pcm16BigEnc.h / Encoder.h.
//
// The samples are written as native 16-bit words, the same convention Pcm16BigDec reads
// them back with on this host.
// =====================================================================================

#include "rw/audio/core/Pcm16BigEnc.h"
#include "rw/audio/core/PlugIn.h" // System::Alloc

#include <new> // placement new (the v-table install over the System allocation)

namespace rw
{
namespace audio
{
namespace core
{

namespace
{
const f32 KF_ZERO = 0.0f;               // the +0x08 seed
const s32 KI_FIELD0C_SEED = 4000;       // the +0x0C seed
const f32 KF_BYTES_PER_SAMPLE = 2.0f;   // 16-bit PCM
const f32 KF_PCM16_FULL_SCALE = 32767.0f;
} // namespace

// -------------------------------------------------------------------------------------
// CreateInstance -- System::Alloc(pSystem, size, no name, align 16), then on success install
// the v-table and seed +0x08 / +0x0C / +0x04 in the console's store order.
// The console asks for its own sizeof (0x20); the host asks for the host sizeof, which the
// widened mpSystem makes larger.
// -------------------------------------------------------------------------------------
Pcm16BigEnc *Pcm16BigEnc::CreateInstance(s32 iNumChannels, s32 iSampleRate, System *pSystem)
{
    void *lpMem = System::Alloc(pSystem, static_cast<u32>(sizeof(Pcm16BigEnc)), nullptr, 16,
                                nullptr);
    if (!lpMem)
        return nullptr;

    Pcm16BigEnc *lpEnc = ::new (lpMem) Pcm16BigEnc; // the v-table store, nothing else
    lpEnc->mfField08 = KF_ZERO;
    lpEnc->miField0C = KI_FIELD0C_SEED;
    lpEnc->mfDataRate = static_cast<f32>(iNumChannels * iSampleRate) * KF_BYTES_PER_SAMPLE;
    return lpEnc;
}

// -------------------------------------------------------------------------------------
// Encode -- convert channels * iNumSamples f32 samples to 16-bit words: each sample is
// scaled by 32767, truncated toward zero, and its low 16 bits are stored (no clamp).
// Reports 2 bytes per sample written, clears the optional out word, and returns
// iNumSamples (everything was consumed).
// -------------------------------------------------------------------------------------
s32 Pcm16BigEnc::Encode(const f32 *pafInput, void *pOutput, s32 iNumSamples,
                        s32 *piBytesWritten, s32 /*iArg5*/, s32 *piAuxOut)
{
    s16 *lpOut = static_cast<s16 *>(pOutput);
    const s32 liTotal = mucChannelCount * iNumSamples;
    for (s32 i = 0; i < liTotal; ++i)
        lpOut[i] = static_cast<s16>(static_cast<s32>(pafInput[i] * KF_PCM16_FULL_SCALE));

    *piBytesWritten = 2 * liTotal;
    if (piAuxOut)
        *piAuxOut = 0;
    return iNumSamples;
}

// -------------------------------------------------------------------------------------
// Flush -- nothing is ever buffered: report 0 bytes, clear the optional out word, return 0.
// -------------------------------------------------------------------------------------
s32 Pcm16BigEnc::Flush(void * /*pOutput*/, s32 *piBytesWritten, s32 /*iArg3*/, s32 *piAuxOut)
{
    *piBytesWritten = 0;
    if (piAuxOut)
        *piAuxOut = 0;
    return 0;
}

} // namespace core
} // namespace audio
} // namespace rw
