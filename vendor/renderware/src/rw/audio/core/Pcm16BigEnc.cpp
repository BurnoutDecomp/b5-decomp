// =====================================================================================
// rw::audio::core::Pcm16BigEnc bodies -- the 'P6B0' (16-bit PCM) stream encoder.
//
// EARenderWare "rwaudio". Reconstructed from the console image; its PowerPC asm is
// authoritative for every store. See Pcm16BigEnc.h / Encoder.h.
//
// The samples are written as native 16-bit words, the same convention Pcm16BigDec reads
// them back with on this host.
// =====================================================================================

#if defined(_XBOX) && defined(_MSC_VER)
// Size optimization reproduces ARTIST Flush and the deleting destructor at 0x82BA6E78 and 0x82BA7A90.
#pragma optimize("s", on)
#endif

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
// CreateInstance -- pSystem->Alloc(size, no name, align 16), then on success install
// the v-table and seed +0x08 / +0x0C / +0x04 in the console's store order.
// The console asks for its own sizeof (0x20); the host asks for the host sizeof, which the
// widened mpSystem makes larger.
// -------------------------------------------------------------------------------------
Pcm16BigEnc *Pcm16BigEnc::CreateInstance(s32 iNumChannels, s32 iSampleRate, System *pSystem)
{
    void *lpMem = pSystem->Alloc(static_cast<u32>(sizeof(Pcm16BigEnc)), 0, 16,
                                0);
    if (!lpMem)
        return 0;

    Pcm16BigEnc *lpEnc = ::new (lpMem) Pcm16BigEnc; // the v-table store, nothing else
    lpEnc->mVbrQuality = KF_ZERO;
    lpEnc->mCbrRate = KI_FIELD0C_SEED;
    lpEnc->mAverageDataRate = static_cast<f32>(iNumChannels * iSampleRate) * KF_BYTES_PER_SAMPLE;
    return lpEnc;
}

// -------------------------------------------------------------------------------------
// Encode -- convert channels * iNumSamples f32 samples to 16-bit words: each sample is
// scaled by 32767, truncated toward zero, and its low 16 bits are stored (no clamp).
// Reports 2 bytes per sample written, clears the optional out word, and returns
// iNumSamples (everything was consumed).
// -------------------------------------------------------------------------------------
s32 Pcm16BigEnc::Encode(f32 *pafInput, u8 *pOutput, s32 iNumSamples,
                        s32 *piBytesWritten, void * /*pSeekData*/, s32 *piAuxOut)
{
    // ARTIST stores the PCM sample words with sth at 0x82BA7C4C.
    s16 *lpOut = reinterpret_cast<s16 *>(pOutput);
    const s32 liTotal = mNumChannels * iNumSamples;
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
s32 Pcm16BigEnc::Flush(u8 * /*pOutput*/, s32 *piBytesWritten, void * /*pSeekData*/, s32 *piAuxOut)
{
    *piBytesWritten = 0;
    if (piAuxOut)
        *piAuxOut = 0;
    return 0;
}

} // namespace core
} // namespace audio
} // namespace rw
