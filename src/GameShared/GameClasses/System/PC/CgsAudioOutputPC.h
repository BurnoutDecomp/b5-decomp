#pragma once

#include "types.hpp"

// ===========================================================================
//  CgsSystem::AudioOutputPC -- the PC audio output backend (XAudio2 2.9).
//
//  This is the PC equivalent of the Xbox 360 audio output path: on the X360 the
//  rw::audio::core::Dac pushes its final mixed PCM frame to the XAudio render driver
//  (XAudioSubmitRenderDriverFrame / CXenonRenderer / CMasteringVoice). XAudio2 is the
//  direct PC successor of the 360's XAudio (same engine/mastering/source-voice model),
//  so the Dac -> render-driver path maps onto a single XAudio2 source voice fed by
//  SubmitSourceBuffer. Modelled as a self-contained PC-platform sink (the same
//  "PC platform layer at the leaf" pattern as the D3D9 / FFmpeg shims).
//
//  XAudio2Create is resolved at runtime via LoadLibrary/GetProcAddress -- no linked .lib,
//  so a missing engine degrades to "run muted" instead of failing to start. The provider
//  is Microsoft's XAudio2 Redistributable (xaudio2_9redist.dll, shipped beside the exe):
//  the same 2.9 engine as the in-box one, but supported down to Windows 7 SP1 instead of
//  Windows 10 1803+. The in-box xaudio2_9/_8.dll remain fallbacks.
//
//  The shared sink has one 48 kHz stereo clock. Movie producers supply signed
//  16-bit PCM; the six-channel RWAC transport is folded to float stereo by
//  DacOutputPC. XAudio2 receives their float sum with an output mastering limiter.
//  One OnBufferEnd callback owns production. Priming submits only silence before
//  starting the voice; capture and diagnostic I/O run on a separate bounded worker.
// ===========================================================================

namespace CgsSystem
{

class AudioOutputPC
{
public:
    // Fill interleaved 16-bit stereo movie frames. Called by the sole audio
    // producer; write silence for "nothing to play".
    typedef void (*FillFn)(s16* lpOut, int liFrames, void* lpUser);
    typedef void (*EngineFillFn)(f32* lpOut, int liFrames, void* lpUser);

    // Open the device (default 48 kHz stereo). lpFill may be null (-> silence). Returns
    // false if XAudio2 is unavailable (the game then runs muted).
    static bool Open(int liSampleRate, int liChannels, FillFn lpFill, void* lpUser);
    static void Close();
    // Release the movie/primary producer while preserving the engine producer.
    // Waits for the last primary fill without interrupting the engine/device.
    static void ReleasePrimaryFill();
    static bool IsOpen();

    // The sample rate the voice was opened at (0 when closed). The single source
    // voice is fixed-rate, so a stream at another rate must Close + re-Open.
    static int  GetOpenSampleRate();

    // Swap the active fill source at runtime (e.g. test tone -> the real Dac pull).
    static void SetFill(FillFn lpFill, void* lpUser);

    // Publish the float stereo engine producer and its context as one pair.
    // Setters never wait for a fill (Attach can hold the RWAC System lock).
    // Use ReleaseEngineFill before destroying the context, outside RWAC locks.
    static void SetEngineFill(EngineFillFn lpFill, void* lpUser);
    // Unpublish and wait for the engine producer before freeing its System.
    // Call outside the RWAC System and command locks.
    static void ReleaseEngineFill();

    // Diagnostic: route a finite 440 Hz sine through the backend (lfSeconds) to confirm
    // output works by ear, then fall silent. Opens the device first if needed.
    static void PlayTestTone(float lfSeconds);
};

} // namespace CgsSystem
