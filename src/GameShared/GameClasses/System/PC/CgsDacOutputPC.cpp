#include "GameShared/GameClasses/System/PC/CgsDacOutputPC.h"
#include "GameShared/GameClasses/System/PC/CgsAudioOutputPC.h"

#include "rw/audio/core/plugins/Dac.h" // Dac + the external-fill support surface
#include "rw/audio/core/Mixer.h"       // Mixer::KU_FRAME_SIZE
#include "rw/audio/core/PlugIn.h"      // System (ExecuteCommands + the two locks)

#include "GameShared/GameClasses/Development/Log/CgsLog.h" // the one-shot first-valid-frame diagnostic

#include <cstring> // memset (the silence frames)

// ===========================================================================
//  CgsSound::Playback::DacOutputPC -- see the header. FLAG PC-platform leaf:
//  the fill below is the PC stand-in for the console's XenonThread packet loop
//  (the loop shape is documented in rw/audio/core/plugins/Dac.cpp's header);
//  every engine call inside it is the faithful reconstruction.
// ===========================================================================

namespace CgsSound
{
namespace Playback
{

namespace
{
    rw::audio::core::Dac* s_pDac = 0;

    // sqrt(1/2) -- the standard equal-power centre/surround fold-down weight.
    const f32 KF_DOWNMIX = 0.70710677f;

    // The engine fill: one engine mix frame per 256-frame device buffer -- the
    // XenonThread producer iteration, inverted (the console held the commands lock
    // for the thread's life; the per-callback bracket here is the same mutual
    // exclusion at the same per-packet granularity).
    void EngineFill(s16* lpOut, int liFrames, void* /*lpUser*/)
    {
        using rw::audio::core::Dac;
        using rw::audio::core::System;

        const int liValues = liFrames * 2;   // the device voice is stereo

        Dac* lpDac = s_pDac;
        if (!lpDac || !rw::audio::core::DacIsEngineRunning()
            || liFrames != rw::audio::core::Mixer::KU_FRAME_SIZE)
        {
            // No engine, or a device quantum that is not one engine frame (the
            // backend's kFrames is the engine quantum today; a future backend with a
            // different request size needs a staging ring here, not a partial pump).
            std::memset(lpOut, 0, sizeof(s16) * liValues);
            return;
        }

        System* lpSystem = lpDac->mpSystemUseGetSystemAccessor;
        System::ExecuteCommandsLock(lpSystem);

        // The console iteration: ExecuteCommands ALWAYS (started or not -- it is the
        // engine's frame pump: timers, deferred commands, voice expulsion), then the
        // mix only while the DAC is started.
        System::ExecuteCommands(lpSystem);
        int liFrameValid = 0;
        if (rw::audio::core::DacIsStarted())
            liFrameValid = Dac::Mix(lpDac);

        System::ExecuteCommandsUnlock(lpSystem);

        if (liFrameValid == 1)
        {
            // One-shot diagnostic: the first VALID engine frame through the fill
            // (log-visible proof the full console path produced output). The
            // probe-era peak scan that accompanied it is retired with the probe
            // (2026-08-28: 440 Hz confirmed by ear + a logged peak of 0.999).
            static bool s_bLoggedFirstValid = false;
            if (!s_bLoggedFirstValid)
            {
                s_bLoggedFirstValid = true;
                if (CgsDev::Log::gpDebugPrint)
                    (*CgsDev::Log::gpDebugPrint)
                        << "[Audio] engine fill: first VALID mixed frame reached the device\n";
            }
            // The engine frame is interleaved WAVE-order f32 in the Dac's static
            // buffer ({FL,FR,C,LFE,BL,BR} x 256, already clipped to [-1,1]).
            // FLAG PC-platform fold: the console hands all six channels to its 5.1
            // device; the PC stereo device gets the standard equal-power fold-down
            // (L = FL + 0.707*(C + BL), R = FR + 0.707*(C + BR); LFE omitted), then
            // saturating f32 -> s16 (the sum can exceed the clipped per-channel range).
            const f32* lpFrame = rw::audio::core::DacGetInterleaveBuffer();
            const int liChannels = rw::audio::core::DacGetChannelCount();
            for (int liSample = 0; liSample < liFrames; ++liSample)
            {
                f32 lfLeft;
                f32 lfRight;
                if (liChannels == 6)
                {
                    const f32* lp = lpFrame + liSample * 6;
                    lfLeft  = lp[0] + KF_DOWNMIX * (lp[2] + lp[4]);
                    lfRight = lp[1] + KF_DOWNMIX * (lp[2] + lp[5]);
                }
                else if (liChannels == 2)
                {
                    const f32* lp = lpFrame + liSample * 2;
                    lfLeft  = lp[0];
                    lfRight = lp[1];
                }
                else
                {
                    // The other console modes (mono/quad/7.1) have no PC producer
                    // today; take the leading pair.
                    const f32* lp = lpFrame + liSample * liChannels;
                    lfLeft  = lp[0];
                    lfRight = (liChannels > 1) ? lp[1] : lp[0];
                }
                if (lfLeft  >  1.0f) lfLeft  =  1.0f;
                if (lfLeft  < -1.0f) lfLeft  = -1.0f;
                if (lfRight >  1.0f) lfRight =  1.0f;
                if (lfRight < -1.0f) lfRight = -1.0f;
                lpOut[liSample * 2 + 0] = static_cast<s16>(lfLeft  * 32767.0f);
                lpOut[liSample * 2 + 1] = static_cast<s16>(lfRight * 32767.0f);
            }
        }
        else
        {
            // The console's XMemSet(packet, 0, 0x1800) arm: no valid frame (zero
            // active voices is the normal phase-D state) -> silence.
            std::memset(lpOut, 0, sizeof(s16) * liValues);
        }
    }
}

void DacOutputPC::Attach(rw::audio::core::Dac* apDac)
{
    s_pDac = apDac;
    if (apDac)
    {
        CgsSystem::AudioOutputPC::SetEngineFill(&EngineFill, 0);
        // Ensure the device exists (idempotent: an already-open voice -- e.g. the
        // boot movie's -- is kept; silence mixes in at any rate). 48 kHz stereo is
        // the engine's own output rate.
        CgsSystem::AudioOutputPC::Open(48000, 2, 0, 0);
    }
    else
    {
        CgsSystem::AudioOutputPC::SetEngineFill(0, 0);
    }
}

rw::audio::core::Dac* DacOutputPC::GetDac()
{
    return s_pDac;
}

} // namespace Playback
} // namespace CgsSound
