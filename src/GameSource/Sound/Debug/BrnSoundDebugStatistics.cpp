#include "GameSource/Sound/Debug/BrnSoundDebugStatistics.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Sound/CgsTestBedAllocator.h"
#include "GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "rw/audio/core/PlugIn.h"

#include <cstdarg>
#include <cstdio>
#include <cmath>

// BrnRootSoundModule.cpp owns the four real allocator wrappers (DecFIGS :57..60).
extern CgsSound::TestBed::Allocator gRwacTestBedAlloc;
extern CgsSound::TestBed::Allocator gCsisTestBedAlloc;
extern CgsSound::TestBed::Allocator gPlaybackTestBedAlloc;
extern CgsSound::TestBed::Allocator gLogicTestBedAlloc;

namespace BrnSound
{
namespace Debug
{
// BrnDebugComponent.cpp:48; ARTIST byte_82FFB93E.
extern bool gbResetStatistics;

// ARTIST 0x827DF720. The typed counter/history constructors perform exactly
// the original clears; module pointers, positions and labels await use/init.
Statistics::Statistics()
{
}

// Inlined by DebugComponent::Construct, ARTIST 0x826A1250..0x826A1290.
void Statistics::Construct(CgsSound::Playback::Module::Module* lpPlaybackModule,
                           BrnSound::Module::SoundLogicModule* lpLogicModule)
{
    mpPlaybackModule = lpPlaybackModule;
    mpLogicModule = lpLogicModule;
    mVoiceProfiles[0].mpcName = "AEMS Voices";    // 0x820AD404
    mVoiceProfiles[1].mpcName = "Splicer Voices"; // 0x820AD3F4
    mVoiceProfiles[2].mpcName = "Master Voices";  // 0x820AD3E4
    mVoiceProfiles[3].mpcName = "Submix Voices";  // 0x820AD3D4
    mVoiceProfiles[4].mpcName = "Player Voices";  // 0x820AD3C4
}

// ARTIST 0x826A15E0. Reset is a held menu toggle: sample it once, clear each
// voice history before its new sample, and clear the mixer/PPU histories after.
// The allocation/object high-water marks survive that reset.
void Statistics::Update()
{
    mRwacAlloc.Set(static_cast<u32>(gRwacTestBedAlloc.GetBytesAllocated()));
    mCsisAlloc.Set(static_cast<u32>(gCsisTestBedAlloc.GetBytesAllocated()));
    mPlaybackAlloc.Set(static_cast<u32>(gPlaybackTestBedAlloc.GetBytesAllocated()));
    mLogicAlloc.Set(static_cast<u32>(gLogicTestBedAlloc.GetBytesAllocated()));
    mVoiceCount.Set(mpPlaybackModule->GetEnvironment()->GetNumberOfActiveVoices());
    mContentCount.Set(mpPlaybackModule->GetEnvironment()->GetNumberOfActiveContent());

    const bool lbReset = gbResetStatistics;
    for (u32 luType = 0; luType < CgsSound::Playback::Voice::E_VOICETYPE_COUNT; ++luType)
    {
        CgsSound::Playback::VoiceProfilingData<16>& lrProfile = mVoiceProfiles[luType].mData;
        if (lbReset)
            lrProfile.Reset();
        const f32 lfTicks = mpPlaybackModule->GetEnvironment()->GetVoiceTypeTickTotal(
            static_cast<CgsSound::Playback::Voice::EProfileVoiceType>(luType));
        lrProfile.SubmitNewTime(lfTicks, 0.000375f); // flt_820AE1F0
    }
    if (lbReset)
    {
        mMixerProfile.Reset();
        for (u32 luProfile = 0; luProfile < 4; ++luProfile)
            mPpuProfile[luProfile].Reset();
    }

    // The target stores zero to current (+0x44); there is no queue query here.
    mCommandFillLevel.Set(0);
}

// ARTIST 0x82689528. r3=this, r4=renderer, r5=alert, r6=format, r7..=varargs.
void Statistics::RenderLine(CgsDev::Debug2DImmediateRender* lpRender, bool lbRedAlert,
                            const char* lpFormat, ...)
{
    char lacBuffer[256];
    va_list lList;
    va_start(lList, lpFormat);
    ::vsnprintf(lacBuffer, sizeof(lacBuffer), lpFormat, lList);
    va_end(lList);

    const CgsDev::RGBA lColour = lbRedAlert ? 0xFF0000FFu : 0xFFFFFFFFu;
    lpRender->DrawTextWithBackground(lacBuffer, mfX, mfY, 20.0f,
                                     lColour, 0x80000000u, 0.0f);
    mfY += 16.0f;
}

// ARTIST 0x826A1470. r4=renderer, r5=history, r6=packed RGBA; Vector2 values
// reach DrawLine in vector registers. The oldest sample starts at (120,y),
// then all 128 samples are drawn left-to-right, ending at x=1000.
void Statistics::DrawGraph(CgsDev::Debug2DImmediateRender* lpRender,
                           CgsSound::Playback::VoiceProfilingData<128>* lpVoiceProfilingData,
                           u32 lColour)
{
    CGS_ASSERT(lpVoiceProfilingData, "lpVoiceProfilingData");
    Vector2 lv2LineStart = {120.0f,
        700.0f - lpVoiceProfilingData->GetTimeByIndex(127) * 6.0f, 0.0f, 0.0f};
    f32 lfX = 126.875f;
    for (u32 luHistoryIndex = 128; luHistoryIndex != 0; --luHistoryIndex)
    {
        const f32 lfTime = lpVoiceProfilingData->GetTimeByIndex(luHistoryIndex - 1);
        const Vector2 lv2LineEnd = {lfX, 700.0f - lfTime * 6.0f, 0.0f, 0.0f};
        lpRender->DrawLine(lv2LineStart, lv2LineEnd,
                           lfTime < 99.0f ? lColour : 0xFF0000FFu);
        lv2LineStart = lv2LineEnd;
        lfX += 6.875f;
    }
}

// ARTIST 0x826A1960. RWAC samples are submitted while rendering, under the
// system lock, independently of Update's per-voice samples.
void Statistics::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender, bool lbDrawDacGraph)
{
    mfX = 60.0f;
    mfY = 180.0f;
    RenderLine(lpRender, false, "Memory:");
    RenderLine(lpRender, false, "* RWAC: %u \t(%u) bytes", mRwacAlloc.GetCurrent(), mRwacAlloc.GetMax());
    RenderLine(lpRender, false, "* Csis: %u \t(%u) bytes", mCsisAlloc.GetCurrent(), mCsisAlloc.GetMax());
    RenderLine(lpRender, false, "* Playback: %u \t(%u) bytes", mPlaybackAlloc.GetCurrent(), mPlaybackAlloc.GetMax());
    RenderLine(lpRender, false, "* Logic: %u \t(%u) bytes", mLogicAlloc.GetCurrent(), mLogicAlloc.GetMax());
    RenderLine(lpRender, false, "Objects:");
    RenderLine(lpRender, false, "* Voice count: %u \t(%u)", mVoiceCount.GetCurrent(), mVoiceCount.GetMax());
    RenderLine(lpRender, false, "* Content count: %u \t(%u)", mContentCount.GetCurrent(), mContentCount.GetMax());
    RenderLine(lpRender, false, "Buffers:");
    RenderLine(lpRender, false, "* Command buffer usage (percent): %u \t(%u)",
               mCommandFillLevel.GetCurrent(), mCommandFillLevel.GetMax());

    rw::audio::core::System* lpSystem = CgsSound::Playback::GetDefaultRwacSystem();
    CgsSound::Playback::RwacLock lRwacLock(lpSystem);
    RenderLine(lpRender, false, "* RWAC Command Buffer Usage(%u)", lpSystem->muDeferredRingHighWater);
    mMixerProfile.SubmitNewTime(0.000375f, static_cast<f32>(lpSystem->muMixerCpuTicks));
    const u32 luPpuTicks = lpSystem->muCommandExecutionCpuTicks + lpSystem->muTimerExecutionCpuTicks
                        + lpSystem->muBalanceCycles + lpSystem->muExpelVoicesCpuTicks;
    mPpuProfile[0].SubmitNewTime(0.000375f, static_cast<f32>(luPpuTicks));
    mPpuProfile[1].SubmitNewTime(0.000375f, static_cast<f32>(lpSystem->muCommandExecutionCpuTicks));
    mPpuProfile[2].SubmitNewTime(0.000375f, static_cast<f32>(lpSystem->muTimerExecutionCpuTicks));
    mPpuProfile[3].SubmitNewTime(0.000375f, static_cast<f32>(lpSystem->muBalanceCycles));

    if (lbDrawDacGraph)
        DrawGraph(lpRender, &mMixerProfile, 0xFFFFFFFFu);
    RenderLine(lpRender, false, "");
    RenderLine(lpRender, false, "SPU Costs:");
    RenderLine(lpRender, !(mMixerProfile.GetMostRecentTime() < 99.0f),
               "* Mixer:        %7.3f %7.3f %7.3f", mMixerProfile.GetMostRecentTime(),
               mMixerProfile.GetAverageTime(), mMixerProfile.GetMaximumTime());
    for (u32 luType = 0; luType < 5; ++luType)
    {
        const NamedVoiceProfilingData& lrVoice = mVoiceProfiles[luType];
        RenderLine(lpRender, !(lrVoice.mData.GetMostRecentTime() < 99.0f),
                   "        * %s:   %7.3f %7.3f %7.3f", lrVoice.mpcName,
                   lrVoice.mData.GetMostRecentTime(), lrVoice.mData.GetAverageTime(),
                   lrVoice.mData.GetMaximumTime());
    }

    // All PPU rows intentionally share the total-overhead alert condition.
    const bool lbPpuAlert = !(mPpuProfile[0].GetMostRecentTime() < 99.0f);
    RenderLine(lpRender, lbPpuAlert, "");
    RenderLine(lpRender, lbPpuAlert, "PPU Costs:");
    RenderLine(lpRender, lbPpuAlert, "* Overhead:%7.3f %7.3f %7.3f",
               mPpuProfile[0].GetMostRecentTime(), mPpuProfile[0].GetAverageTime(), mPpuProfile[0].GetMaximumTime());
    RenderLine(lpRender, lbPpuAlert, "* Command:%7.3f %7.3f %7.3f",
               mPpuProfile[1].GetMostRecentTime(), mPpuProfile[1].GetAverageTime(), mPpuProfile[1].GetMaximumTime());
    RenderLine(lpRender, lbPpuAlert, "* Timer:      %7.3f %7.3f %7.3f",
               mPpuProfile[2].GetMostRecentTime(), mPpuProfile[2].GetAverageTime(), mPpuProfile[2].GetMaximumTime());
    RenderLine(lpRender, lbPpuAlert, "* Profile:   %7.3f %7.3f %7.3f",
               mPpuProfile[3].GetMostRecentTime(), mPpuProfile[3].GetAverageTime(), mPpuProfile[3].GetMaximumTime());
}
}
}
