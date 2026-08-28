#pragma once

#include "types.hpp"

// ===========================================================================
//  CgsSound::Playback::DacOutputPC -- the PC bridge between the reconstructed
//  rw::audio::core::Dac (the engine's output plug-in) and the PC speaker device
//  (CgsSystem::AudioOutputPC). Phase D of the faithful-audio-engine campaign.
//
//  On the X360 the Dac owns an XAudio source voice and the "RWAudioCore Dac"
//  worker thread (Dac::XenonThread @0x82B96F40) that pumps one 256-frame engine
//  mix per packet. The PC runs the console's own EXTERNAL-DAC configuration
//  (rw/audio/core/plugins/Dac.cpp, sExternalDacMode): no voice, no thread -- and
//  THIS leaf is the external DAC. Its engine fill callback (registered through
//  AudioOutputPC::SetEngineFill) inverts the XenonThread packet loop:
//     ExecuteCommandsLock -> ExecuteCommands -> Mix -> ok ? interleave-frame
//     fold 6->2 + f32->s16 : silence -> ExecuteCommandsUnlock
//  once per 256-frame device buffer (AudioOutputPC's kFrames == the engine's
//  MIXER frame quantum). FLAG PC-platform leaf throughout -- the engine side of
//  every call is the faithful reconstruction; only the device marshalling here
//  is PC-authored.
// ===========================================================================

namespace rw { namespace audio { namespace core { class Dac; } } }

namespace CgsSound
{
namespace Playback
{

class DacOutputPC
{
public:
    // Adopt the created Dac plug-in, register the engine fill with the PC output
    // backend, and open the device if nothing has yet (idempotent; 48 kHz stereo,
    // the engine's own rate). Null detaches.
    static void Attach(rw::audio::core::Dac* apDac);

    // The attached plug-in (null before Attach).
    static rw::audio::core::Dac* GetDac();
};

} // namespace Playback
} // namespace CgsSound
