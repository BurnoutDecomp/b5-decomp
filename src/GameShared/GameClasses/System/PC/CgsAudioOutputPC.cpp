#include "GameShared/GameClasses/System/PC/CgsAudioOutputPC.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 // Windows 10 -> in-box XAudio 2.9
#endif
#include <Windows.h>
#include <mmdeviceapi.h>
// The XAudio2 Redistributable header (vendor/xaudio2redist, fetched by
// tools\build\fetch_xaudio2_redist.bat) rather than the Windows SDK <xaudio2.h>:
// same 2.9 engine and same interfaces, but it names xaudio2_9redist.dll -- the
// down-level-capable build we ship beside the exe. See Open() below.
#include <xaudio2Redist.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>   // std::getenv (the BRN_AUDIO_MUTE harness knob)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/System/PC/CgsRwacCommandTracePC.h" // private pre-overwrite diagnostic

// ===========================================================================
//  CgsSystem::AudioOutputPC -- XAudio2 2.9 PC output backend (Microsoft's XAudio2
//  Redistributable, xaudio2_9redist.dll). See the header for the
//  X360 (Dac -> XAudio render-driver) correspondence. A single stereo float source
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
// The additive ENGINE fill (the rw::audio Dac's mixed frame; phase D). Persistent
// across Open/Close while the movie-owned primary slot churns.
AudioOutputPC::EngineFillFn g_engineFill  = nullptr;
void*                   g_engineUser  = nullptr;
f32                     g_buf[kBuffers][kFrames * kMaxChannels];
f32                     g_engineBuf[kFrames * kMaxChannels];
s16                     g_primaryBuf[kFrames * kMaxChannels];

// FLAG PC-platform leaf: publish callback/context pairs atomically under a short
// mutex. No publication lock is held while a fill enters RWAC or reads movie PCM.
std::mutex g_bindingMutex;
std::condition_variable g_fillIdle;
u32 g_primaryActive = 0;
u32 g_engineActive = 0;

// [DIAG] NOT IN THE X360 BINARY -- BRN_AUDIO_CAPTURE=<path.wav>: every packet handed to the
// source voice (primary + engine mix, BEFORE the mastering volume, so it works with the
// harness mute on) is appended to a 16-bit PCM WAV at that path. The header is written with
// the rate/channels of the first packet and patched with the final sizes at Close/exit; a
// re-Open at another rate keeps appending (the header keeps the first rate -- read the log's
// "[Audio] XAudio2 opened" lines if a run re-opened). Measurement oracle for the sound lanes.
FILE* g_capture       = nullptr;
bool  g_captureTried  = false;
u32   g_captureBytes  = 0u;
int   g_captureRate   = 0;
int   g_captureChans  = 0;

void CapturePatchHeader()
{
    const u32 luData = g_captureBytes;
    const u32 luRiff = 36u + luData;
    const long liPos = std::ftell(g_capture);
    std::fseek(g_capture, 4, SEEK_SET);  std::fwrite(&luRiff, 4, 1, g_capture);
    std::fseek(g_capture, 40, SEEK_SET); std::fwrite(&luData, 4, 1, g_capture);
    std::fseek(g_capture, liPos, SEEK_SET);
    std::fflush(g_capture);
}

void CaptureFinish()
{
    if (!g_capture)
        return;
    CapturePatchHeader();
    std::fclose(g_capture);
    g_capture = nullptr;
}

void CaptureWrite(const s16* lpBuf, int liValues)
{
    if (!g_captureTried)
    {
        g_captureTried = true;
        const char* lpcPath = std::getenv("BRN_AUDIO_CAPTURE");
        if (lpcPath && *lpcPath)
        {
            g_capture = std::fopen(lpcPath, "wb");
            if (g_capture)
            {
                g_captureRate  = (g_openRate > 0) ? g_openRate : 48000;
                g_captureChans = g_channels;
                const u32 luZero = 0u, luFmtLen = 16u;
                const u16 luFmt = 1u, luCh = static_cast<u16>(g_captureChans), luBits = 16u;
                const u32 luRate = static_cast<u32>(g_captureRate);
                const u16 luAlign = static_cast<u16>(g_captureChans * 2);
                const u32 luByteRate = luRate * luAlign;
                std::fwrite("RIFF", 1, 4, g_capture); std::fwrite(&luZero, 4, 1, g_capture);
                std::fwrite("WAVE", 1, 4, g_capture);
                std::fwrite("fmt ", 1, 4, g_capture); std::fwrite(&luFmtLen, 4, 1, g_capture);
                std::fwrite(&luFmt, 2, 1, g_capture);  std::fwrite(&luCh, 2, 1, g_capture);
                std::fwrite(&luRate, 4, 1, g_capture); std::fwrite(&luByteRate, 4, 1, g_capture);
                std::fwrite(&luAlign, 2, 1, g_capture); std::fwrite(&luBits, 2, 1, g_capture);
                std::fwrite("data", 1, 4, g_capture); std::fwrite(&luZero, 4, 1, g_capture);

                AUDIO_LOG << "[Audio] capture -> " << lpcPath << " (" << g_captureRate << " Hz, "
                          << g_captureChans << " ch, 16-bit; sizes patched at exit)\n";
            }
        }
    }
    if (g_capture)
    {
        std::fwrite(lpBuf, sizeof(s16), static_cast<size_t>(liValues), g_capture);
        g_captureBytes += static_cast<u32>(liValues * sizeof(s16));
        static u32 suPackets = 0u;
        if ((++suPackets & 31u) == 0u)   // the harness ends a run with TerminateProcess: keep the header valid
            CapturePatchHeader();
    }
}

// [DIAG] NOT IN THE X360 BINARY -- the ENGINE-ONLY oracle.
// The existing BRN_AUDIO_CAPTURE writes the FINAL mix, in which EA Trax and the UI
// (the primary fill) sit on top of the engine; a quiet or dead engine is invisible in
// it. These two knobs measure the engine fill buffer BY ITSELF:
//   BRN_ENGINE_MIX_DIAG=1           one line per ~0.5 s: rms/peak of the engine frame.
//   BRN_AUDIO_CAPTURE_ENGINE=<wav>  that same buffer as its own 16-bit PCM WAV.
FILE* g_engCapture      = nullptr;
bool  g_engCaptureTried = false;
u32   g_engCaptureBytes = 0u;
int   g_engMixDiag      = -1;   // -1 = unread, 0 = off, 1 = on

void EngineCapturePatchHeader()
{
    const u32 luData = g_engCaptureBytes;
    const u32 luRiff = 36u + luData;
    const long liPos = std::ftell(g_engCapture);
    std::fseek(g_engCapture, 4, SEEK_SET);  std::fwrite(&luRiff, 4, 1, g_engCapture);
    std::fseek(g_engCapture, 40, SEEK_SET); std::fwrite(&luData, 4, 1, g_engCapture);
    std::fseek(g_engCapture, liPos, SEEK_SET);
    std::fflush(g_engCapture);
}

void EngineCaptureFinish()
{
    if (!g_engCapture)
        return;
    EngineCapturePatchHeader();
    std::fclose(g_engCapture);
    g_engCapture = nullptr;
}

// Called with the engine fill's own buffer, before it is mixed into the outgoing frame.
void EngineMeasure(const s16* lpBuf, int liValues)
{
    if (g_engMixDiag < 0)
    {
        const char* lpcDiag = std::getenv("BRN_ENGINE_MIX_DIAG");
        g_engMixDiag = (lpcDiag && *lpcDiag && *lpcDiag != '0') ? 1 : 0;
    }

    if (!g_engCaptureTried)
    {
        g_engCaptureTried = true;
        const char* lpcPath = std::getenv("BRN_AUDIO_CAPTURE_ENGINE");
        if (lpcPath && *lpcPath)
        {
            g_engCapture = std::fopen(lpcPath, "wb");
            if (g_engCapture)
            {
                const int liRate  = (g_openRate > 0) ? g_openRate : 48000;
                const u32 luZero = 0u, luFmtLen = 16u;
                const u16 luFmt = 1u, luCh = static_cast<u16>(g_channels), luBits = 16u;
                const u32 luRate = static_cast<u32>(liRate);
                const u16 luAlign = static_cast<u16>(g_channels * 2);
                const u32 luByteRate = luRate * luAlign;
                std::fwrite("RIFF", 1, 4, g_engCapture); std::fwrite(&luZero, 4, 1, g_engCapture);
                std::fwrite("WAVE", 1, 4, g_engCapture);
                std::fwrite("fmt ", 1, 4, g_engCapture); std::fwrite(&luFmtLen, 4, 1, g_engCapture);
                std::fwrite(&luFmt, 2, 1, g_engCapture);  std::fwrite(&luCh, 2, 1, g_engCapture);
                std::fwrite(&luRate, 4, 1, g_engCapture); std::fwrite(&luByteRate, 4, 1, g_engCapture);
                std::fwrite(&luAlign, 2, 1, g_engCapture); std::fwrite(&luBits, 2, 1, g_engCapture);
                std::fwrite("data", 1, 4, g_engCapture); std::fwrite(&luZero, 4, 1, g_engCapture);

                AUDIO_LOG << "[engine-mix] capture -> " << lpcPath << "\n";
            }
        }
    }

    if (g_engCapture)
    {
        std::fwrite(lpBuf, sizeof(s16), static_cast<size_t>(liValues), g_engCapture);
        g_engCaptureBytes += static_cast<u32>(liValues * sizeof(s16));
        static u32 suEngPackets = 0u;
        if ((++suEngPackets & 31u) == 0u)
            EngineCapturePatchHeader();
    }

    if (g_engMixDiag != 1)
        return;

    // ~0.5 s of 256-frame packets at 48 kHz == 94; round to 96 so the line rate is steady.
    static double sdSumSq   = 0.0;
    static int    siPeak    = 0;
    static u32    suPackets = 0u;
    static u32    suLines   = 0u;
    for (int li = 0; li < liValues; ++li)
    {
        const int liV = lpBuf[li] < 0 ? -lpBuf[li] : lpBuf[li];
        if (liV > siPeak) siPeak = liV;
        sdSumSq += double(lpBuf[li]) * double(lpBuf[li]);
    }
    if (++suPackets >= 96u)
    {
        const f32 lfRms = static_cast<f32>(
            std::sqrt(sdSumSq / (double(suPackets) * double(liValues))));
        AUDIO_LOG << "[engine-mix] line=" << static_cast<s32>(suLines++)
                  << " rms=" << lfRms << " peak=" << static_cast<s32>(siPeak)
                  << " (0 == the engine fill produced SILENCE for ~0.5 s)\n";
        sdSumSq   = 0.0;
        siPeak    = 0;
        suPackets = 0u;
    }
}

// Diagnostic storage is a bounded SPSC ring. Only the XAudio callback produces
// packets; a separate worker performs all capture writes and log formatting.
struct DiagnosticPacket
{
    f32 maMix[kFrames * kMaxChannels];
    f32 maEngine[kFrames * kMaxChannels];
    bool mbEngine;
};
const u32 kDiagnosticPackets = 256;
DiagnosticPacket g_diagnostics[kDiagnosticPackets];
std::atomic<u32> g_diagWrite(0), g_diagRead(0), g_diagDropped(0);
std::atomic<bool> g_diagStop(false);
HANDLE g_diagEvent = nullptr;
HANDLE g_diagThread = nullptr;

// FLAG PC-platform leaf: final normalized device range, with no gain or history.
f32 SaturateDeviceSample(f32 afValue)
{
    if (!std::isfinite(afValue)) return 0.0f;
    if (afValue > 1.0f) return 1.0f;
    if (afValue < -1.0f) return -1.0f;
    return afValue;
}

s16 DiagnosticS16(f32 afValue)
{
    return static_cast<s16>(SaturateDeviceSample(afValue) * 32767.0f);
}

DWORD WINAPI DiagnosticThread(void*)
{
    u32 luPackets = 0, luOverRange = 0;
    f32 lfPeak = 0.0f;
    for (;;)
    {
        u32 luRead = g_diagRead.load(std::memory_order_relaxed);
        const u32 luWrite = g_diagWrite.load(std::memory_order_acquire);
        while (luRead != luWrite)
        {
            const DiagnosticPacket& lrPacket = g_diagnostics[luRead % kDiagnosticPackets];
            s16 laMix[kFrames * kMaxChannels], laEngine[kFrames * kMaxChannels];
            const int liValues = kFrames * g_channels;
            for (int li = 0; li < liValues; ++li)
            {
                const f32 lfAbs = std::fabs(lrPacket.maMix[li]);
                if (lfAbs > lfPeak) lfPeak = lfAbs;
                if (lfAbs > 1.0f) ++luOverRange;
                laMix[li] = DiagnosticS16(lrPacket.maMix[li]);
                laEngine[li] = DiagnosticS16(lrPacket.maEngine[li]);
            }
            if (lrPacket.mbEngine) EngineMeasure(laEngine, liValues);
            CaptureWrite(laMix, liValues);
            if (++luPackets == 96)
            {
                AUDIO_LOG << "[audio-output] prelimit peak=" << lfPeak
                          << " overrange=" << luOverRange
                          << " diagnostic-drops=" << g_diagDropped.load() << "\n";
                luPackets = luOverRange = 0;
                lfPeak = 0.0f;
            }
            ++luRead;
            g_diagRead.store(luRead, std::memory_order_release);
        }
        if (g_diagStop.load(std::memory_order_acquire) &&
            luRead == g_diagWrite.load(std::memory_order_acquire))
            break;
        ::WaitForSingleObject(g_diagEvent, 250);
    }
    return 0;
}

void StartDiagnostics()
{
    const char* lapKnobs[] = { "BRN_AUDIO_CAPTURE", "BRN_AUDIO_CAPTURE_ENGINE", "BRN_ENGINE_MIX_DIAG" };
    bool lbEnabled = false;
    for (const char* lpcKnob : lapKnobs)
    {
        const char* lpcValue = std::getenv(lpcKnob);
        lbEnabled |= lpcValue && *lpcValue && *lpcValue != '0';
    }
    if (!lbEnabled) return;
    g_diagRead.store(0); g_diagWrite.store(0); g_diagDropped.store(0);
    g_diagStop.store(false);
    g_diagEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (g_diagEvent)
        g_diagThread = ::CreateThread(nullptr, 0, &DiagnosticThread, nullptr, 0, nullptr);
    if (!g_diagThread && g_diagEvent)
    {
        ::CloseHandle(g_diagEvent);
        g_diagEvent = nullptr;
    }
}

void StopDiagnostics()
{
    if (!g_diagThread) return;
    g_diagStop.store(true, std::memory_order_release);
    ::SetEvent(g_diagEvent);
    ::WaitForSingleObject(g_diagThread, INFINITE);
    ::CloseHandle(g_diagThread); ::CloseHandle(g_diagEvent);
    g_diagThread = g_diagEvent = nullptr;
}

void QueueDiagnostics(const f32* apMix, bool abEngine)
{
    if (!g_diagThread) return;
    const u32 luWrite = g_diagWrite.load(std::memory_order_relaxed);
    if (luWrite - g_diagRead.load(std::memory_order_acquire) == kDiagnosticPackets)
    {
        g_diagDropped.fetch_add(1, std::memory_order_relaxed);
        return; // never stall audio to wait for a capture disk
    }
    DiagnosticPacket& lrPacket = g_diagnostics[luWrite % kDiagnosticPackets];
    std::memcpy(lrPacket.maMix, apMix, sizeof(lrPacket.maMix));
    std::memcpy(lrPacket.maEngine, g_engineBuf, sizeof(lrPacket.maEngine));
    lrPacket.mbEngine = abEngine;
    g_diagWrite.store(luWrite + 1, std::memory_order_release);
    ::SetEvent(g_diagEvent);
}

void SubmitBuffer(int liIndex, bool abPrime = false)
{
    f32* lpBuf = g_buf[liIndex];
    const int liValues = kFrames * g_channels;
    std::memset(lpBuf, 0, sizeof(f32) * liValues);
    // Device creation may run under the System lock. Priming is SILENCE ONLY;
    // the sole engine producer is OnBufferEnd after all buffers are submitted.
    if (!abPrime)
    {
        AudioOutputPC::FillFn lpFill;
        AudioOutputPC::EngineFillFn lpEngineFill;
        void* lpUser;
        void* lpEngineUser;
        {
            std::lock_guard<std::mutex> lLock(g_bindingMutex);
            lpFill = g_fill; lpUser = g_user;
            lpEngineFill = g_engineFill; lpEngineUser = g_engineUser;
            if (lpFill) ++g_primaryActive;
            if (lpEngineFill) ++g_engineActive;
        }
        if (lpFill)
        {
            lpFill(g_primaryBuf, kFrames, lpUser);
            for (int li = 0; li < liValues; ++li)
                lpBuf[li] = static_cast<f32>(g_primaryBuf[li]) / 32768.0f;
            std::lock_guard<std::mutex> lLock(g_bindingMutex);
            --g_primaryActive;
            g_fillIdle.notify_all();
        }
        std::memset(g_engineBuf, 0, sizeof(g_engineBuf));
        if (lpEngineFill)
        {
            lpEngineFill(g_engineBuf, kFrames, lpEngineUser);
            for (int li = 0; li < liValues; ++li)
                lpBuf[li] += g_engineBuf[li];
            std::lock_guard<std::mutex> lLock(g_bindingMutex);
            --g_engineActive;
            g_fillIdle.notify_all();
        }
        // Retain the pre-saturation floats for peak/overrange diagnostics. The
        // worker's WAV conversion uses the same device-range guard as playback.
        QueueDiagnostics(lpBuf, lpEngineFill != nullptr);
        // FLAG PC-platform leaf: saturate only at the completed stereo device
        // boundary. Preserve all representable samples exactly, including a
        // large engine lane cancelled by the movie lane. RWAC already owns the
        // authored master dynamics; a second XAPOFX loudness processor changes
        // even quiet music and introduces another 1024 frames of latency.
        for (int li = 0; li < liValues; ++li)
            lpBuf[li] = SaturateDeviceSample(lpBuf[li]);
    }
    XAUDIO2_BUFFER lBuf = {};
    lBuf.AudioBytes = static_cast<UINT32>(liValues * sizeof(f32));
    lBuf.pAudioData = reinterpret_cast<const BYTE*>(lpBuf);
    lBuf.pContext = reinterpret_cast<void*>(static_cast<intptr_t>(liIndex));
    const HRESULT lSubmitResult = g_pSource->SubmitSourceBuffer(&lBuf);
    RwacCommandTracePC::MarkSubmit(lSubmitResult);
}

void FinishOutput()
{
    AudioOutputPC::Close();
    CaptureFinish();
    EngineCaptureFinish();
}

// XAudio2 voice callback: when a buffer drains, refill that same slot and re-queue it.
class VoiceCallback : public IXAudio2VoiceCallback
{
public:
    void STDMETHODCALLTYPE OnBufferEnd(void* pBufferContext) override
    {
        RwacCommandTracePC::Mark(RwacCommandTracePC::DeviceCallbackBegin);
        SubmitBuffer(static_cast<int>(reinterpret_cast<intptr_t>(pBufferContext)));
        RwacCommandTracePC::Mark(RwacCommandTracePC::DeviceCallbackEnd);
    }
    void STDMETHODCALLTYPE OnBufferStart(void*) override {}
    void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
    void STDMETHODCALLTYPE OnStreamEnd() override {}
    void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT lResult) override
    {
        RwacCommandTracePC::MarkVoiceError(lResult);
    }
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
};
VoiceCallback g_callback;

// FLAG PC-platform leaf: observe critical endpoint/engine failures without
// making XAudio calls or logging on its callback thread.
class EngineCallbackTrace : public IXAudio2EngineCallback
{
public:
    void STDMETHODCALLTYPE OnProcessingPassStart() override {}
    void STDMETHODCALLTYPE OnProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnCriticalError(HRESULT lResult) override
    {
        RwacCommandTracePC::MarkCriticalError(lResult);
    }
};
EngineCallbackTrace g_engineCallbackTrace;
bool g_engineCallbackTraceRegistered = false;

// FLAG PC-platform leaf: XAudio2 2.9 normally selects Windows' default render
// endpoint.  A machine may have active endpoints but no current default (for
// example after a USB/virtual-device topology change); in that case 2.9 returns
// HRESULT_FROM_WIN32(ERROR_NOT_FOUND).  The X360 had a single platform-owned
// output, so preserve that availability by trying each active PC render endpoint.
HRESULT CreateMasteringVoiceWithEndpointFallback(int liChannels, int liSampleRate,
                                                  UINT32* apEndpointIndex)
{
    *apEndpointIndex = UINT32_MAX;
    HRESULT lResult = g_pXAudio2->CreateMasteringVoice(
        &g_pMaster, liChannels, liSampleRate);
    if (SUCCEEDED(lResult))
        return lResult;

    IMMDeviceEnumerator* lpEnumerator = nullptr;
    IMMDeviceCollection* lpEndpoints = nullptr;
    const HRESULT lComResult = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HRESULT lEnumResult = ::CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&lpEnumerator));
    if (SUCCEEDED(lEnumResult))
        lEnumResult = lpEnumerator->EnumAudioEndpoints(
            eRender, DEVICE_STATE_ACTIVE, &lpEndpoints);

    UINT32 luCount = 0;
    if (SUCCEEDED(lEnumResult))
        lEnumResult = lpEndpoints->GetCount(&luCount);
    char lacEnumResult[16];
    sprintf_s(lacEnumResult, "0x%08X", static_cast<unsigned>(lEnumResult));
    AUDIO_LOG << "[Audio] default endpoint unavailable; active-endpoint fallback: COM "
              << (SUCCEEDED(lComResult) || lComResult == RPC_E_CHANGED_MODE ? "ready" : "failed")
              << ", enumerate " << lacEnumResult << ", candidates " << luCount << "\n";
    for (UINT32 luIndex = 0; SUCCEEDED(lEnumResult) && luIndex < luCount; ++luIndex)
    {
        IMMDevice* lpEndpoint = nullptr;
        LPWSTR lpEndpointId = nullptr;
        HRESULT lItemResult = lpEndpoints->Item(luIndex, &lpEndpoint);
        if (SUCCEEDED(lItemResult))
            lItemResult = lpEndpoint->GetId(&lpEndpointId);
        if (SUCCEEDED(lItemResult))
        {
            lItemResult = g_pXAudio2->CreateMasteringVoice(
                &g_pMaster, liChannels, liSampleRate, 0, lpEndpointId);
            if (SUCCEEDED(lItemResult))
            {
                *apEndpointIndex = luIndex;
                lResult = lItemResult;
            }
        }
        if (FAILED(lItemResult))
        {
            char lacItemResult[16];
            sprintf_s(lacItemResult, "0x%08X", static_cast<unsigned>(lItemResult));
            AUDIO_LOG << "[Audio] active endpoint " << luIndex
                      << " rejected: " << lacItemResult << "\n";
        }
        ::CoTaskMemFree(lpEndpointId);
        if (lpEndpoint)
            lpEndpoint->Release();
        if (SUCCEEDED(lResult))
            break;
    }

    if (lpEndpoints)
        lpEndpoints->Release();
    if (lpEnumerator)
        lpEnumerator->Release();
    if (SUCCEEDED(lComResult))
        ::CoUninitialize();
    return lResult;
}

// ===========================================================================
// FLAG PC-platform leaf: BRN_AUDIO_MUTE=1 -- a SILENT run, and nothing else.
//
// WHY IT EXISTS. Every bug-test case on this box boots a real game that opens the machine's
// default render endpoint at full volume, so a sixteen-case sweep is half an hour of Burnout
// playing out loud over whatever the box's owner is doing. flow_run.ps1 therefore sets this on
// every harness run (its -Audio switch turns it back off); launching Burnout_PC.exe by hand is
// unaffected, because nothing but the harness sets the variable.
//
// ⭐ WHAT IT IS: ONE SetVolume(0) ON THE MASTERING VOICE, at creation. The mastering voice is the
// last gain in the graph -- every source voice, submix, movie stream and engine mix passes
// through it -- so zeroing it is the complete and only change needed to make the run silent.
// ⛔ WHAT IT IS NOT, and this is the part that matters for every other lane: it does NOT skip
// audio init, stub XAudio2, drop a fill, or shorten a callback. The device is really opened, the
// source voice really runs, SubmitBuffer still pulls both the primary and the ENGINE fill every
// buffer, and every decoder, AEMS bytecode program and engine-note state machine behind them
// keeps running EXACTLY as before -- so the sound lanes' witnesses and timings do not move when
// a run is muted. A muted run and an audible one differ in one float in the OS mixer.
// ⚠️ It is deliberately NOT the "running muted" fallback further up this file. That phrase means
// the device could not be opened AT ALL (no XAudio2 DLL, no endpoint), which is a degraded run
// and reads as silence to a naive listener; this is a working device at zero gain, and the
// witness below is what lets a test tell the two apart.
// DELETE-WHEN the harness gains a per-process output endpoint it can throw away.
// ===========================================================================
bool AudioMuteRequested()
{
    static s32 siMute = -1;
    if (siMute < 0)
    {
        const char* lpcEnv = std::getenv("BRN_AUDIO_MUTE");
        siMute = (lpcEnv != 0 && lpcEnv[0] != '0') ? 1 : 0;
    }
    return siMute == 1;
}

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
    if (liSampleRate != 48000 || liChannels != 2)
        return false; // this shared sink has one explicit master clock and layout

    // XAUDIO2_DLL_W is L"xaudio2_9redist.dll" (xaudio2Redist.h; the _W spelling because
    // plain XAUDIO2_DLL follows UNICODE, which this build does not define). That is the
    // redistributable 2.9 engine, shipped beside Burnout_PC.exe by build_game_exe.bat and
    // found first by the exe-directory-first search order: the same engine as the in-box
    // 2.9 but supported down to Windows 7 SP1, so it is the preferred provider on every
    // OS. The in-box DLLs stay as fallbacks for a game folder whose redist DLL is missing.
    g_hXAudioDll = ::LoadLibraryW(XAUDIO2_DLL_W);
    if (!g_hXAudioDll)
        g_hXAudioDll = ::LoadLibraryW(L"xaudio2_9.dll"); // in-box 2.9 (Win10 1803+)
    if (!g_hXAudioDll)
        g_hXAudioDll = ::LoadLibraryW(L"xaudio2_8.dll"); // in-box 2.8 (Win8/older Win10)
    if (!g_hXAudioDll)
    {
        AUDIO_LOG << "[Audio] XAudio2 unavailable (no xaudio2_9redist/_9/_8.dll) -- running muted\n";
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
    const HRESULT lTraceRegistration =
        g_pXAudio2->RegisterForCallbacks(&g_engineCallbackTrace);
    g_engineCallbackTraceRegistered = SUCCEEDED(lTraceRegistration);
    RwacCommandTracePC::gState.muEngineCallbackRegistration.store(
        static_cast<unsigned>(lTraceRegistration), std::memory_order_relaxed);
    UINT32 luEndpointIndex = UINT32_MAX;
    const HRESULT lMasterResult = CreateMasteringVoiceWithEndpointFallback(
        liChannels, liSampleRate, &luEndpointIndex);
    if (FAILED(lMasterResult))
    {
        char lacResult[16];
        sprintf_s(lacResult, "0x%08X", static_cast<unsigned>(lMasterResult));
        AUDIO_LOG << "[Audio] CreateMasteringVoice failed (" << lacResult
                  << ", " << liSampleRate << " Hz, " << liChannels
                  << " ch) -- running muted\n";
        Close();
        return false;
    }

    // BRN_AUDIO_MUTE: the whole mute, applied to the voice every mix passes through. Applied
    // HERE, per Open, because the device is opened and re-opened many times in one boot (the
    // movie streams churn the primary fill through ReleasePrimaryFill) and a single un-muted
    // re-open is an audible run. See the knob's banner above.
    if (AudioMuteRequested())
        g_pMaster->SetVolume(0.0f, 0 /* XAUDIO2_COMMIT_NOW */);

    WAVEFORMATEX lWfx;
    std::memset(&lWfx, 0, sizeof(lWfx));
    lWfx.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
    lWfx.nChannels       = static_cast<WORD>(liChannels);
    lWfx.nSamplesPerSec  = static_cast<DWORD>(liSampleRate);
    lWfx.wBitsPerSample  = 32;
    lWfx.nBlockAlign     = static_cast<WORD>(liChannels * sizeof(f32));
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
    SetFill(lpFill, lpUser);
    static bool sbExitRegistered = false;
    if (!sbExitRegistered)
    {
        std::atexit(&FinishOutput);
        sbExitRegistered = true;
    }
    StartDiagnostics();
    for (int li = 0; li < kBuffers; ++li)
        SubmitBuffer(li, true);
    if (FAILED(g_pSource->Start(0)))
    {
        Close();
        return false;
    }

    RwacCommandTracePC::gState.muDeviceOpen.store(1, std::memory_order_relaxed);
    RwacCommandTracePC::Mark(RwacCommandTracePC::DeviceOpened);
    AUDIO_LOG << "[Audio] XAudio2 opened: " << liSampleRate << " Hz, " << liChannels
              << " ch (float PCM, final saturation)";
    if (luEndpointIndex != UINT32_MAX)
        AUDIO_LOG << " via active endpoint " << luEndpointIndex;
    // [FLAG PC witness] The mastering voice's OWN gain, read back out of the engine rather than
    // echoed from the request -- "this run is silent" is then a measurement and not a claim, and
    // a SetVolume that silently failed cannot pass for a mute. Once per Open, so it is bounded by
    // the number of device opens (a handful per boot). tools\tests\cases\quiet_audio_mute.ps1
    // requires EVERY one of these lines in a harness run to read 0.000.
    // ⚠️ Formatted with sprintf, not the stream: the log stream has no float precision control,
    // and the check parses this number.
    float lfMasterVolume = -1.0f;
    g_pMaster->GetVolume(&lfMasterVolume);
    char lacVolume[32];
    sprintf_s(lacVolume, "%.3f", lfMasterVolume);
    AUDIO_LOG << "; master volume=" << lacVolume
              << (AudioMuteRequested() ? " (BRN_AUDIO_MUTE=1)" : "");
    AUDIO_LOG << "\n";
    return true;
}

void AudioOutputPC::Close()
{
    RwacCommandTracePC::gState.muDeviceOpen.store(0, std::memory_order_relaxed);
    RwacCommandTracePC::Mark(RwacCommandTracePC::DeviceClosed);
    // DestroyVoice waits for the last callback before buffers, bindings or
    // capture headers are touched. Diagnostic writes have their own drain barrier.
    if (g_pSource) { g_pSource->Stop(0); g_pSource->DestroyVoice(); g_pSource = nullptr; }
    StopDiagnostics();
    if (g_capture) CapturePatchHeader();
    if (g_engCapture) EngineCapturePatchHeader();
    if (g_pMaster) { g_pMaster->DestroyVoice(); g_pMaster = nullptr; }
    if (g_pXAudio2)
    {
        if (g_engineCallbackTraceRegistered)
            g_pXAudio2->UnregisterForCallbacks(&g_engineCallbackTrace);
        g_engineCallbackTraceRegistered = false;
        g_pXAudio2->Release();
        g_pXAudio2 = nullptr;
    }
    if (g_hXAudioDll) { ::FreeLibrary(g_hXAudioDll); g_hXAudioDll = nullptr; }
    SetFill(nullptr, nullptr);
    g_openRate = 0;
}

void AudioOutputPC::ReleasePrimaryFill()
{
    std::unique_lock<std::mutex> lLock(g_bindingMutex);
    g_fill = nullptr; g_user = nullptr;
    g_fillIdle.wait(lLock, [] { return g_primaryActive == 0; });
}

bool AudioOutputPC::IsOpen() { return g_pSource != nullptr; }

int AudioOutputPC::GetOpenSampleRate() { return g_openRate; }

void AudioOutputPC::SetFill(FillFn lpFill, void* lpUser)
{
    std::lock_guard<std::mutex> lLock(g_bindingMutex);
    g_fill = lpFill; g_user = lpUser;
}

void AudioOutputPC::SetEngineFill(EngineFillFn lpFill, void* lpUser)
{
    std::lock_guard<std::mutex> lLock(g_bindingMutex);
    g_engineFill = lpFill; g_engineUser = lpUser;
}

void AudioOutputPC::ReleaseEngineFill()
{
    std::unique_lock<std::mutex> lLock(g_bindingMutex);
    g_engineFill = nullptr; g_engineUser = nullptr;
    g_fillIdle.wait(lLock, [] { return g_engineActive == 0; });
}

void AudioOutputPC::PlayTestTone(float lfSeconds)
{
    if (!g_pSource && !Open(48000, 2, nullptr, nullptr))
        return;
    ReleasePrimaryFill();
    g_tone.phase = 0.0;
    g_tone.sampleRate = 48000;
    g_tone.channels = g_channels;
    g_tone.remaining = static_cast<int>(lfSeconds * 48000.0f);
    SetFill(&TestToneFill, &g_tone);
    AUDIO_LOG << "[Audio] test tone: 440 Hz for " << static_cast<int>(lfSeconds) << " s\n";
}

} // namespace CgsSystem
