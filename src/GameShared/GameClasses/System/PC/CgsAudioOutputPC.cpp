#include "GameShared/GameClasses/System/PC/CgsAudioOutputPC.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 // Windows 10 -> in-box XAudio 2.9
#endif
#include <Windows.h>
#include <xaudio2.h>
#include <cmath>
#include <cstring>
#include <cstdio>

#include "GameShared/GameClasses/Development/Log/CgsLog.h"

// ===========================================================================
//  CgsSystem::AudioOutputPC -- XAudio2 2.9 PC output backend. See the header for the
//  X360 (Dac -> XAudio render-driver) correspondence. A single stereo 16-bit source
//  voice is fed by a triple-buffered, callback-driven pull (OnBufferEnd refills the
//  drained buffer and re-submits it), matching the X360 render-driver frame callback.
// ===========================================================================

// Stream-style log, guarded (gpDebugPrint may be null very early at boot).
#define AUDIO_LOG if (CgsDev::Log::gpDebugPrint) (*CgsDev::Log::gpDebugPrint)

namespace CgsSystem
{

namespace
{
typedef HRESULT (__stdcall *PFN_XAudio2Create)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR);

const int kBuffers = 3;
const int kFrames  = 256;           // frames per buffer (~5.3 ms @ 48 kHz; 3 -> ~16 ms total latency)
const int kMaxChannels = 2;

HMODULE                 g_hXAudioDll  = nullptr;
IXAudio2*               g_pXAudio2    = nullptr;
IXAudio2MasteringVoice* g_pMaster     = nullptr;
IXAudio2SourceVoice*    g_pSource     = nullptr;
int                     g_channels    = 2;
int                     g_openRate    = 0;      // sample rate the voice was opened at (0 = closed)
AudioOutputPC::FillFn   g_fill        = nullptr;
void*                   g_user        = nullptr;
// The additive OVERLAY fill (the one-shot GUI blips): mixed saturating on top of
// the primary stream each refill. Persistent across Open/Close (the primary fill
// owner churns as the movie/music streams swap the voice; the overlay must not).
AudioOutputPC::FillFn   g_overlayFill = nullptr;
void*                   g_overlayUser = nullptr;
// The additive VOICE fill (the speech / voice-over stream). Its own slot: on the
// console speech is a separate effect with a separate voice, sounding concurrently
// with the music stream AND the presentation blips.
AudioOutputPC::FillFn   g_voiceFill   = nullptr;
void*                   g_voiceUser   = nullptr;
s16                     g_buf[kBuffers][kFrames * kMaxChannels];
s16                     g_overlayBuf[kFrames * kMaxChannels];
s16                     g_voiceBuf[kFrames * kMaxChannels];

// Saturating add of one already-filled mix source into the outgoing buffer.
void MixInto(s16* lpDst, const s16* lpSrc, int liValues)
{
    for (int li = 0; li < liValues; ++li)
    {
        int liMix = int(lpDst[li]) + int(lpSrc[li]);
        if (liMix >  32767) liMix =  32767;
        if (liMix < -32768) liMix = -32768;
        lpDst[li] = s16(liMix);
    }
}

void SubmitBuffer(int liIndex)
{
    s16* lpBuf = g_buf[liIndex];
    if (g_fill)
        g_fill(lpBuf, kFrames, g_user);
    else
        std::memset(lpBuf, 0, sizeof(s16) * kFrames * g_channels);

    const int liValues = kFrames * g_channels;
    if (g_overlayFill)
    {
        g_overlayFill(g_overlayBuf, kFrames, g_overlayUser);
        MixInto(lpBuf, g_overlayBuf, liValues);
    }
    if (g_voiceFill)
    {
        g_voiceFill(g_voiceBuf, kFrames, g_voiceUser);
        MixInto(lpBuf, g_voiceBuf, liValues);
    }

    XAUDIO2_BUFFER lBuf;
    std::memset(&lBuf, 0, sizeof(lBuf));
    lBuf.AudioBytes = static_cast<UINT32>(kFrames * g_channels * sizeof(s16));
    lBuf.pAudioData = reinterpret_cast<const BYTE*>(lpBuf);
    lBuf.pContext   = reinterpret_cast<void*>(static_cast<intptr_t>(liIndex));
    if (g_pSource)
        g_pSource->SubmitSourceBuffer(&lBuf);
}

// XAudio2 voice callback: when a buffer drains, refill that same slot and re-queue it.
class VoiceCallback : public IXAudio2VoiceCallback
{
public:
    void STDMETHODCALLTYPE OnBufferEnd(void* pBufferContext) override
    {
        SubmitBuffer(static_cast<int>(reinterpret_cast<intptr_t>(pBufferContext)));
    }
    void STDMETHODCALLTYPE OnBufferStart(void*) override {}
    void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
    void STDMETHODCALLTYPE OnStreamEnd() override {}
    void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT) override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
};
VoiceCallback g_callback;

// --- the built-in diagnostic test tone (440 Hz) ---
struct ToneState
{
    double phase;
    int    remaining;   // frames left to sound; then silence
    int    sampleRate;
    int    channels;
};
ToneState g_tone = { 0.0, 0, 48000, 2 };

void TestToneFill(s16* lpOut, int liFrames, void* lpUser)
{
    ToneState* lpT = static_cast<ToneState*>(lpUser);
    const double lfStep = 2.0 * 3.14159265358979323846 * 440.0 / lpT->sampleRate;
    for (int li = 0; li < liFrames; ++li)
    {
        s16 lsV = 0;
        if (lpT->remaining > 0)
        {
            lsV = static_cast<s16>(8000.0 * std::sin(lpT->phase));
            lpT->phase += lfStep;
            --lpT->remaining;
        }
        for (int lc = 0; lc < lpT->channels; ++lc)
            *lpOut++ = lsV;
    }
}
} // namespace

bool AudioOutputPC::Open(int liSampleRate, int liChannels, FillFn lpFill, void* lpUser)
{
    if (g_pSource)
        return true; // already open
    if (liChannels < 1) liChannels = 1;
    if (liChannels > kMaxChannels) liChannels = kMaxChannels;

    g_hXAudioDll = ::LoadLibraryW(L"xaudio2_9.dll");
    if (!g_hXAudioDll)
        g_hXAudioDll = ::LoadLibraryW(L"xaudio2_8.dll"); // older Win10 fallback
    if (!g_hXAudioDll)
    {
        AUDIO_LOG << "[Audio] XAudio2 unavailable (no xaudio2_9/_8.dll) -- running muted\n";
        return false;
    }
    PFN_XAudio2Create lpCreate = reinterpret_cast<PFN_XAudio2Create>(
        ::GetProcAddress(g_hXAudioDll, "XAudio2Create"));
    if (!lpCreate || FAILED(lpCreate(&g_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR)))
    {
        AUDIO_LOG << "[Audio] XAudio2Create failed -- running muted\n";
        Close();
        return false;
    }
    if (FAILED(g_pXAudio2->CreateMasteringVoice(&g_pMaster, liChannels, liSampleRate)))
    {
        AUDIO_LOG << "[Audio] CreateMasteringVoice failed -- running muted\n";
        Close();
        return false;
    }

    WAVEFORMATEX lWfx;
    std::memset(&lWfx, 0, sizeof(lWfx));
    lWfx.wFormatTag      = WAVE_FORMAT_PCM;
    lWfx.nChannels       = static_cast<WORD>(liChannels);
    lWfx.nSamplesPerSec  = static_cast<DWORD>(liSampleRate);
    lWfx.wBitsPerSample  = 16;
    lWfx.nBlockAlign     = static_cast<WORD>(liChannels * sizeof(s16));
    lWfx.nAvgBytesPerSec = liSampleRate * lWfx.nBlockAlign;

    if (FAILED(g_pXAudio2->CreateSourceVoice(&g_pSource, &lWfx, 0,
                                             XAUDIO2_DEFAULT_FREQ_RATIO, &g_callback)))
    {
        AUDIO_LOG << "[Audio] CreateSourceVoice failed -- running muted\n";
        Close();
        return false;
    }

    g_channels = liChannels;
    g_openRate = liSampleRate;
    g_fill = lpFill;
    g_user = lpUser;
    g_pSource->Start(0);
    for (int li = 0; li < kBuffers; ++li) // prime the queue
        SubmitBuffer(li);

    AUDIO_LOG << "[Audio] XAudio2 opened: " << liSampleRate << " Hz, " << liChannels << " ch (16-bit PCM)\n";
    return true;
}

void AudioOutputPC::Close()
{
    if (g_pSource) { g_pSource->Stop(0); g_pSource->DestroyVoice(); g_pSource = nullptr; }
    if (g_pMaster) { g_pMaster->DestroyVoice(); g_pMaster = nullptr; }
    if (g_pXAudio2) { g_pXAudio2->Release(); g_pXAudio2 = nullptr; }
    if (g_hXAudioDll) { ::FreeLibrary(g_hXAudioDll); g_hXAudioDll = nullptr; }
    g_fill = nullptr; g_user = nullptr;
    g_openRate = 0;
}

bool AudioOutputPC::IsOpen() { return g_pSource != nullptr; }

int AudioOutputPC::GetOpenSampleRate() { return g_openRate; }

void AudioOutputPC::SetFill(FillFn lpFill, void* lpUser) { g_fill = lpFill; g_user = lpUser; }

void AudioOutputPC::SetOverlayFill(FillFn lpFill, void* lpUser) { g_overlayFill = lpFill; g_overlayUser = lpUser; }

void AudioOutputPC::SetVoiceFill(FillFn lpFill, void* lpUser) { g_voiceFill = lpFill; g_voiceUser = lpUser; }

void AudioOutputPC::PlayTestTone(float lfSeconds)
{
    if (!g_pSource && !Open(48000, 2, nullptr, nullptr))
        return;
    g_tone.phase = 0.0;
    g_tone.sampleRate = 48000;
    g_tone.channels = g_channels;
    g_tone.remaining = static_cast<int>(lfSeconds * 48000.0f);
    SetFill(&TestToneFill, &g_tone);
    AUDIO_LOG << "[Audio] test tone: 440 Hz for " << static_cast<int>(lfSeconds) << " s\n";
}

} // namespace CgsSystem
