#include "GameShared/GameClasses/System/PC/CgsDacOutputPC.h"
#include "GameShared/GameClasses/System/PC/CgsAudioOutputPC.h"
#include "GameShared/GameClasses/System/PC/CgsRwacCommandTracePC.h" // private pre-overwrite diagnostic

#include "rw/audio/core/plugins/Dac.h" // Dac + the external-fill support surface
#include "rw/audio/core/Mixer.h"       // Mixer::KU_FRAME_SIZE
#include "rw/audio/core/PlugIn.h"      // System (ExecuteCommands + the two locks)

#include <atomic>
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
    std::atomic<rw::audio::core::Dac*> s_pDac(nullptr);
    std::atomic<rw::audio::core::System*> s_pSystem(nullptr);

    // sqrt(1/2) -- the standard equal-power centre/surround fold-down weight.
    const f32 KF_DOWNMIX = 0.70710677f;

    // The engine fill: one engine mix frame per 256-frame device buffer -- the
    // XenonThread producer iteration, inverted (the console held the commands lock
    // for the thread's life; the per-callback bracket here is the same mutual
    // exclusion at the same per-packet granularity).
    void EngineFill(f32* lpOut, int liFrames, void* lpUser)
    {
        using rw::audio::core::Dac;
        using rw::audio::core::System;

        const int liValues = liFrames * 2;   // the device voice is stereo

        System* lpSystem = static_cast<System*>(lpUser);
        std::memset(lpOut, 0, sizeof(f32) * liValues);
        if (!lpSystem || liFrames != rw::audio::core::Mixer::KU_FRAME_SIZE)
            return;

        CgsSystem::RwacCommandTracePC::MarkEngineBegin(lpSystem);
        System::ExecuteCommandsLock(lpSystem);
        CgsSystem::RwacCommandTracePC::MarkConsumerEvent(
            CgsSystem::RwacCommandTracePC::CommandLockAcquired, 2);
        CgsSystem::RwacCommandTracePC::MarkConsumerEvent(
            CgsSystem::RwacCommandTracePC::CommandPassBegin, 3);
        System::ExecuteCommands(lpSystem);
        CgsSystem::RwacCommandTracePC::MarkConsumerEvent(
            CgsSystem::RwacCommandTracePC::CommandPassEnd, 4);
        // Commands may retire the DAC. Read it only AFTER command execution and
        // keep the command lock through the copy of its shared interleave frame.
        Dac* lpDac = s_pDac.load(std::memory_order_acquire);
        int liFrameValid = 0;
        if (lpDac && rw::audio::core::DacIsEngineRunning() &&
            rw::audio::core::DacIsStarted())
        {
            CgsSystem::RwacCommandTracePC::MarkConsumerEvent(
                CgsSystem::RwacCommandTracePC::MixBegin, 5);
            liFrameValid = Dac::Mix(lpDac);
            CgsSystem::RwacCommandTracePC::MarkConsumerEvent(
                CgsSystem::RwacCommandTracePC::MixEnd, 6);
        }

        if (liFrameValid == 1)
        {
            // Six-channel engine transport, WAVE order {FL,FR,C,LFE,BL,BR}.
            // FLAG PC-platform leaf: the logical device is stereo. Keep the fold
            // in float; AudioOutputPC limits AFTER adding the movie/primary bus.
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
                lpOut[liSample * 2 + 0] = lfLeft;
                lpOut[liSample * 2 + 1] = lfRight;
            }
        }
        System::ExecuteCommandsUnlock(lpSystem);
        CgsSystem::RwacCommandTracePC::MarkConsumerEvent(
            CgsSystem::RwacCommandTracePC::EngineEnd, 7);
    }
}

void DacOutputPC::Attach(rw::audio::core::Dac* apDac)
{
    if (!apDac)
    {
        ReleaseSystem(s_pSystem.load(std::memory_order_acquire));
        return;
    }
    rw::audio::core::System* lpSystem = apDac->mpSystemUseGetSystemAccessor;
    s_pSystem.store(lpSystem, std::memory_order_release);
    s_pDac.store(apDac, std::memory_order_release);
    CgsSystem::AudioOutputPC::SetEngineFill(&EngineFill, lpSystem);
    // Open only queues silence before enabling callbacks. It never pumps RWAC
    // synchronously while the factory caller owns the System lock.
    CgsSystem::AudioOutputPC::Open(48000, 2, nullptr, nullptr);
}

void DacOutputPC::Detach(rw::audio::core::Dac* apDac)
{
    s_pDac.compare_exchange_strong(apDac, nullptr, std::memory_order_acq_rel);
}

void DacOutputPC::ReleaseSystem(rw::audio::core::System* apSystem)
{
    if (s_pSystem.load(std::memory_order_acquire) != apSystem)
        return;
    CgsSystem::AudioOutputPC::ReleaseEngineFill();
    s_pDac.store(nullptr, std::memory_order_release);
    s_pSystem.store(nullptr, std::memory_order_release);
}

rw::audio::core::Dac* DacOutputPC::GetDac()
{
    return s_pDac.load(std::memory_order_acquire);
}

} // namespace Playback
} // namespace CgsSound
