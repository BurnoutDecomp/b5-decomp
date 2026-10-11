#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Playback/CgsVoiceProfilingData.h"

namespace CgsSound { namespace Playback { namespace Module { class Module; } } }
namespace BrnSound { namespace Module { struct SoundLogicModule; } }
namespace CgsDev { struct Debug2DImmediateRender; }

namespace BrnSound
{
namespace Debug
{
    // DecFIGS BrnSoundStatistics.h:55. The target counters are u32, including
    // their maxima. They count bytes/objects, independently of host size_t width.
    template<class T> struct Stat
    {
        Stat() : mMax(0), mCurrent(0) {}
        void Set(T luValue)
        {
            mCurrent = luValue;
            if (mMax < luValue)
                mMax = luValue;
        }
        T GetCurrent() const { return mCurrent; }
        T GetMax() const { return mMax; }
    private:
        T mMax;
        T mCurrent;
    };

    // ARTIST 0x827DF720 clears seven counters, a 128-entry history, four
    // 16-entry histories and five named voice histories. Construct's stores at
    // 0x826A1250..1290 identify the first two words as module pointers and each
    // final voice-history word as a label pointer, not padding.
    class Statistics
    {
    public:
        Statistics();
        void Construct(CgsSound::Playback::Module::Module* lpPlaybackModule,
                       BrnSound::Module::SoundLogicModule* lpLogicModule);
        void Update();
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender, bool lbDrawDacGraph);

    private:
        void RenderLine(CgsDev::Debug2DImmediateRender* lpRender, bool lbRedAlert,
                        const char* lpFormat, ...);
        void DrawGraph(CgsDev::Debug2DImmediateRender* lpRender,
                       CgsSound::Playback::VoiceProfilingData<128>* lpVoiceProfilingData,
                       u32 lColour);

        struct NamedVoiceProfilingData
        {
            CgsSound::Playback::VoiceProfilingData<16> mData;
            const char* mpcName;
        };

        CgsSound::Playback::Module::Module* mpPlaybackModule; // target +0x00
        BrnSound::Module::SoundLogicModule* mpLogicModule;    // +0x04
        f32 mfX; // +0x08
        f32 mfY; // +0x0C
        Stat<u32> mRwacAlloc;       // +0x10
        Stat<u32> mCsisAlloc;       // +0x18
        Stat<u32> mPlaybackAlloc;   // +0x20
        Stat<u32> mLogicAlloc;      // +0x28
        Stat<u32> mVoiceCount;      // +0x30
        Stat<u32> mContentCount;    // +0x38
        Stat<u32> mCommandFillLevel;// +0x40
        CgsSound::Playback::VoiceProfilingData<128> mMixerProfile; // +0x48
        CgsSound::Playback::VoiceProfilingData<16> mPpuProfile[4]; // +0x258
        NamedVoiceProfilingData mVoiceProfiles[5];               // +0x398
    };
}
}
