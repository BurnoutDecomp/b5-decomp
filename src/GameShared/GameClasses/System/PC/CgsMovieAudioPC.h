#ifndef CGS_SYSTEM_PC_MOVIEAUDIOPC_H
#define CGS_SYSTEM_PC_MOVIEAUDIOPC_H

#include "types.hpp"

// ============================================================================
//  CgsSystem::MovieAudioPC -- PC playback of the boot/movie audio streams.
//
//  The X360 plays movie sound NOT from the video file but from a separate EA
//  stream: the per-stream wave header (GenericRwacWaveContent / "SNR", in
//  SOUND/STREAMS/STREAMHEADERS) describes an EA-XMA (EAXMA) stream whose blocks
//  live in a named ".SNS" file; SndStream/SndPlayer1 feed EaXmaDec (the XMA HW)
//  -> Dac -> XAudio. See the movie-audio dossier.
//
//  This is the PC realisation of that DATA path with PC leaves (the same model
//  the VP6 video player uses -- FFmpeg standing in for the platform codec):
//    .SNS  -> EA deblock (-> XMA2 2048-byte packets) -> XMA-frame extraction
//          -> FFmpeg "xmaframes" decode (the Xenia fork's raw-XMA-frame leaf)
//          -> 48 kHz stereo s16 PCM -> CgsSystem::AudioOutputPC (XAudio2).
//
//  PC SIMPLIFICATIONS (faithful data, PC leaf):
//   * FFmpeg xmaframes replaces the X360 XMA hardware (EaXmaDec).
//   * The whole stream is decoded up front into a PCM buffer and played from a
//     cursor, rather than streamed block-by-block through SndStream/SndPlayer1
//     (fine for the short boot clips; the streaming subsystem is the deferred
//     faithful layer).
//   * The SNR (GenericRwacWaveContent) is not yet resolved by id -- channels
//     default to stereo (the boot streams decode correctly as 2ch) and the
//     stream is taken straight from its named .SNS.
// ============================================================================

#include <vector>
#include <cstdint>

namespace CgsSystem
{
    // In-memory SNR (GenericRwacWaveContent) sample decode -- the presentation
    // splice-bank samples are fully-resident SNR images. Shares this TU's EA-XMA
    // frame decode. Bodied in CgsMovieAudioPC.cpp.
    bool SnrSampleDecodePC(const std::uint8_t* lpData, std::size_t luLen,
                           std::vector<std::int16_t>& lrPcm, int& lrRate, int& lrChannels);

    class MovieAudioPC
    {
    public:
        void Construct();
        void Release();

        // Decode <lpSnsPath> (an EA .SNS, EAXMA) fully into the internal PCM
        // buffer. Returns false if the file is missing/unreadable or no audio
        // decoded. liChannels is the source channel mode (default stereo).
        bool Load(const char* lpSnsPath, int liChannels = 2);

        // Begin playback through AudioOutputPC (opens the device if needed).
        void Start();
        // Stop playback (silences the output fill).
        void Stop();

        bool IsLoaded() const;
        bool IsFinished() const;   // true once the buffer has fully played out

    private:
        static void FillStatic(s16* lpOut, int liFrames, void* lpUser);
        void Fill(s16* lpOut, int liFrames);
    };

    // ------------------------------------------------------------------------
    // CgsSystem::MenuMusicPC -- PC playback of the menu-stream MUSIC (the title
    // screen's "GunsAndRoses" 44.1 kHz EA-XMA stream). Same faithful-data/PC-leaf
    // model as MovieAudioPC (shares this TU's SNS+SNR decode); LOOPS the track,
    // and resamples to the already-open device rate when they differ.
    //
    // On the console the menu stream is fed by BrnSound::Logic::MusicStream through
    // SndStream (the deferred faithful layer -- the MusicEffect behavioural cluster
    // is un-homed); this host player reproduces the OBSERVABLE: event-155 hash ->
    // the named stream playing/looping, hash 0 -> stopped. FLAG.
    //
    // The movie stream (attract/boot videos) has fill priority: Update() only
    // (re)claims the output while no movie stream is mid-play -- matching the
    // console, which stops the menu stream (hash-0 post) before the attract video.
    // ------------------------------------------------------------------------
    class MenuMusicPC
    {
    public:
        static bool PlaySpec(const char* lpacSpecName);   // ContentSpec name -> registry zone-1 .SNS -> Play
        static bool Play(const char* lpSnsPath);   // decode (cached by path) + loop from the start
        static void Stop();                        // stop (keeps the decoded PCM cached)
        static void Update();                      // per-frame: (re)claim the output fill when free
        static bool IsActive();
    private:
        static void FillStatic(s16* lpOut, int liFrames, void* lpUser);
    };

    // ------------------------------------------------------------------------
    // CgsSystem::SpeechAudioPC -- PC playback of a SPEECH (voice-over) stream:
    // the DJ-Atomika intro lines, and any other line the GUI requests by name.
    //
    // Console correspondence: a GUI voice-over request (out-event 466, payload =
    // a CgsSound::Playback::Name::MakeHash id) reaches
    // BrnSound::Logic::SoundLogicModule::ProcessGuiEvents @0x826ED6C8, whose
    // `case 466` arm raises a CgsSound::Io::Message id 36 that the
    // BrnSound::Logic::SpeechEffect answers -- PlaySpeech/PlaySpeechMapping ->
    // PlayStream @0x8269EAF0 starts the stream voice, and SpeechEffect::UpdateParams
    // @0x826F8074 posts the completion (GUI event 467) when the state word says the
    // line has run out. SpeechEffect owns its OWN voice, concurrent with the music
    // stream and the presentation blips.
    //
    // FLAG (PC leaf, same shape as MenuMusicPC): the sound module's message web
    // (ProcessGuiEvents / Logic::Module::ProcessMessageQueue / SpeechEffect) is not
    // reconstructed, so this host player reproduces the OBSERVABLE -- name -> the
    // named stream sounding once, and a finished flag the caller turns into 467.
    // It never takes the PRIMARY fill (that belongs to the movie / menu music); it
    // mixes on AudioOutputPC's dedicated VOICE slot and RESAMPLES from the stream's
    // own rate to whatever rate the device is already open at, so a line can start
    // over a playing movie or music track without disturbing it. The intro lines are
    // mono 48 kHz with zero prefetch, so the .SNS alone is the whole line.
    // ------------------------------------------------------------------------
    class SpeechAudioPC
    {
    public:
        // ContentSpec name (e.g. "int_lperm") -> registry zone-1 .SNS -> play once.
        static bool PlaySpec(const char* lpacSpecName);
        // The GUI-facing entry: a CgsSound::Playback::Name::MakeHash id (the payload
        // BrnGui::Intro's out-event 466 carries) -> the line's stream -> play once.
        // Returns false when the id has no known stream, so the caller can fall back
        // to answering the request immediately rather than stalling its flow.
        static bool PlayByNameHash(u32 luNameHash);
        static void Stop();
        // True while a line is still sounding.
        static bool IsActive();
        // True once a line that DID start has played out (one-shot: cleared by the
        // next PlaySpec). This is what the caller turns into GUI event 467.
        static bool ConsumeFinished();
    private:
        static void FillStatic(s16* lpOut, int liFrames, void* lpUser);
    };
}

#endif // CGS_SYSTEM_PC_MOVIEAUDIOPC_H
