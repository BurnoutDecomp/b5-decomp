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

namespace CgsSystem
{
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
        static bool Play(const char* lpSnsPath);   // decode (cached by path) + loop from the start
        static void Stop();                        // stop (keeps the decoded PCM cached)
        static void Update();                      // per-frame: (re)claim the output fill when free
        static bool IsActive();
    private:
        static void FillStatic(s16* lpOut, int liFrames, void* lpUser);
    };
}

#endif // CGS_SYSTEM_PC_MOVIEAUDIOPC_H
