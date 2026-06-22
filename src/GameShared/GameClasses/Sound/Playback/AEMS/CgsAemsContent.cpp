// ============================================================================
// CgsAemsContent.cpp -- CgsSound::Playback::AemsContentSlot runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   AemsContentSlot::DoPlay(Slot&, PlayerVoice&, Content&, u32)        @ 0x826DAFF0
//   AemsContentSlot::DoStop(Slot&, PlayerVoice&, Content&)             @ 0x826DB008
//   AemsContentSlot::DoUpdatePlaying(System*, Slot&, PlayerVoice&, Content&, f32) @ 0x826DAFE8
//
// All three are thin AEMS-bank dispatchers: they downcast the PlayerVoice& base to
// AemsPlayerVoice& and forward to its Play/Stop/Update. DoPlay additionally raises
// the PLAYING bit (2) on the player voice's playback-flags byte (+0x80 X360)
// before starting playback. The X360 tail-calls (`b ...Play/Stop/Update`) keep the
// player-voice `this` in r3 and forward the trailing argument register/float, so
// the slot's own Slot/Content args are passed through unread (DoPlay/DoStop/
// DoUpdatePlaying ignore them), exactly as reconstructed below.
//
// FLAG: AemsPlayerVoice::Play/Stop/Update bodies live in their own (not-yet-done)
// AemsPlayerVoice TUs; they are declared in CgsAemsContent.h and linked at
// consolidation. The +0x80 playback-flags member's exact layout slot is DEFERRED
// to the AemsPlayerVoice keystone (see header FLAG).
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsContent.h"

namespace CgsSound
{
namespace Playback
{

// ---------------------------------------------------------------------------
// AemsContentSlot::DoPlay  @ 0x826DAFF0
//   playerVoice.mu8PlaybackFlags |= 2;  return playerVoice.Play(param);
// ---------------------------------------------------------------------------
bool AemsContentSlot::DoPlay(const Slot& /*aSlot*/, PlayerVoice& aVoice,
                             Content& /*aContent*/, u32 au32Param)
{
    aVoice.SetPlaying();                 // lbz/ori 2/stb 0x80(playerVoice)
    return aVoice.Play(au32Param);       // tail-call AemsPlayerVoice::Play
}

// ---------------------------------------------------------------------------
// AemsContentSlot::DoStop  @ 0x826DB008
//   return playerVoice.Stop();
// ---------------------------------------------------------------------------
bool AemsContentSlot::DoStop(const Slot& /*aSlot*/, PlayerVoice& aVoice, Content& /*aContent*/)
{
    return aVoice.Stop();                // tail-call AemsPlayerVoice::Stop
}

// ---------------------------------------------------------------------------
// AemsContentSlot::DoUpdatePlaying  @ 0x826DAFE8
//   return playerVoice.Update(dt);
// (X360 `mr r3,r6` forwards the PlayerVoice& as `this`; the float dt rides in f1.)
// ---------------------------------------------------------------------------
bool AemsContentSlot::DoUpdatePlaying(System* /*apSystem*/, const Slot& /*aSlot*/,
                                      PlayerVoice& aVoice, Content& /*aContent*/, f32 af32Dt)
{
    return aVoice.Update(af32Dt);        // tail-call AemsPlayerVoice::Update
}

} // namespace Playback
} // namespace CgsSound
