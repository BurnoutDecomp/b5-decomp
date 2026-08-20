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
//  Output format is 16-bit signed PCM, interleaved. The backend pulls audio through a
//  fill callback (called on XAudio2's audio thread when a buffer drains); the rw::audio
//  Dac will register its mixed-buffer pull here once it is wired. A built-in diagnostic
//  test tone proves the path end-to-end before the engine exists.
// ===========================================================================

namespace CgsSystem
{

class AudioOutputPC
{
public:
    // Fill `liFrames` interleaved 16-bit stereo (or mono) frames into lpOut. Called on
    // the audio thread; keep it lock-light. Write silence for "nothing to play".
    typedef void (*FillFn)(s16* lpOut, int liFrames, void* lpUser);

    // Open the device (default 48 kHz stereo). lpFill may be null (-> silence). Returns
    // false if XAudio2 is unavailable (the game then runs muted).
    static bool Open(int liSampleRate, int liChannels, FillFn lpFill, void* lpUser);
    static void Close();
    static bool IsOpen();

    // The sample rate the voice was opened at (0 when closed). The single source
    // voice is fixed-rate, so a stream at another rate must Close + re-Open.
    static int  GetOpenSampleRate();

    // Swap the active fill source at runtime (e.g. test tone -> the real Dac pull).
    static void SetFill(FillFn lpFill, void* lpUser);

    // Register the additive OVERLAY fill (the one-shot GUI blips), mixed saturating
    // on top of the primary stream. Persists across Open/Close -- the primary fill
    // owner churns as the movie/music streams swap the voice; the overlay must not.
    static void SetOverlayFill(FillFn lpFill, void* lpUser);

    // Register the additive VOICE fill (the speech/voice-over stream), mixed the
    // same saturating way on top of the primary + overlay. A separate slot because
    // on the console speech is its OWN effect (BrnSound::Logic::SpeechEffect) with
    // its own voice, concurrent with both the music stream and the UI presentation
    // blips -- DJ Atomika talks over whatever else is sounding. Persists across
    // Open/Close for the same reason the overlay does.
    static void SetVoiceFill(FillFn lpFill, void* lpUser);

    // Diagnostic: route a finite 440 Hz sine through the backend (lfSeconds) to confirm
    // output works by ear, then fall silent. Opens the device first if needed.
    static void PlayTestTone(float lfSeconds);
};

} // namespace CgsSystem
