// The real Statistics fields/history template and extracted production bodies,
// with instrumented allocator, playback and renderer endpoints. No device/game.
#include <cstddef>
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Debug/BrnSoundDebugStatistics.h"

static s32 sAssertCount = 0;
static s32 sLockDepth = 0, sLockCount = 0, sUnlockCount = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++sAssertCount; return 0; }
void* EndAssert() { return nullptr; }
} }

struct Vector2
{
    f32 x, y, z, w;
};
namespace CgsDev
{
typedef u32 RGBA;
struct Debug2DImmediateRender
{
    struct Text { std::string value; f32 x,y,size,border; RGBA colour,background; bool locked; };
    struct Line { Vector2 start,end; RGBA colour; };
    std::vector<Text> texts;
    std::vector<Line> lines;
    void DrawTextWithBackground(const char* p, f32 x, f32 y, f32 size, RGBA c, RGBA b, f32 border)
    { texts.push_back({p,x,y,size,border,c,b,sLockDepth != 0}); }
    void DrawLine(Vector2 a, Vector2 b, RGBA c) { lines.push_back({a,b,c}); }
};
}
namespace rw { namespace audio { namespace core {
struct System
{
    u32 muDeferredRingHighWater = 0;
    u32 muMixerCpuTicks = 0;
    u32 muCommandExecutionCpuTicks = 0;
    u32 muTimerExecutionCpuTicks = 0;
    u32 muBalanceCycles = 0;
    u32 muExpelVoicesCpuTicks = 0;
};
} } }
namespace CgsSound
{
namespace TestBed
{
    struct Allocator
    {
        size_t bytes = 0;
        size_t GetBytesAllocated() const { return bytes; }
    };
}
namespace Playback
{
    struct Voice
    {
        enum EProfileVoiceType { E_VOICETYPE_AEMS, E_VOICETYPE_SPLICER,
            E_VOICETYPE_GENERICRWACMASTER, E_VOICETYPE_GENERICRWACSUBMIX,
            E_VOICETYPE_GENERICRWACPLAYER, E_VOICETYPE_COUNT };
    };
    struct Environment
    {
        u32 voices = 0, content = 0;
        f32 ticks[5] = {};
        u32 GetNumberOfActiveVoices() { return voices; }
        u32 GetNumberOfActiveContent() { return content; }
        f32 GetVoiceTypeTickTotal(Voice::EProfileVoiceType t) { return ticks[t]; }
    };
    namespace Module
    {
        class Module
        {
        public:
            Environment env;
            u32 reads = 0;
            Environment* GetEnvironment() { ++reads; return &env; }
        };
    }
    rw::audio::core::System sSystem;
    rw::audio::core::System* GetDefaultRwacSystem() { return &sSystem; }
    struct RwacLock
    {
        explicit RwacLock(rw::audio::core::System*) { ++sLockDepth; ++sLockCount; }
        ~RwacLock() { --sLockDepth; ++sUnlockCount; }
    };
}
}
CgsSound::TestBed::Allocator gRwacTestBedAlloc, gCsisTestBedAlloc;
CgsSound::TestBed::Allocator gPlaybackTestBedAlloc, gLogicTestBedAlloc;
namespace BrnSound { namespace Module { struct SoundLogicModule {}; }
namespace Debug { bool gbResetStatistics = false; } }

#include "sound_statistics_batch.inc"

static int sChecks = 0, sFailures = 0;
static void Check(bool b, const char* p)
{ ++sChecks; if (!b) { ++sFailures; std::printf("FAIL: %s\n",p); } }
static bool Near(f32 a, f32 b) { return std::fabs(a-b) < 0.00002f; }
int main()
{
    using BrnSound::Debug::Statistics;
    using namespace CgsSound::Playback;
    Statistics stats;
    Module::Module playback;
    BrnSound::Module::SoundLogicModule logic;
    Check(stats.mRwacAlloc.GetCurrent() == 0 && stats.mRwacAlloc.GetMax() == 0,
          "constructor clears counters");
    Check(Near(stats.mMixerProfile.GetTimeByIndex(127),0) &&
          Near(stats.mVoiceProfiles[4].mData.GetAverageTime(),0), "constructor clears every history family");
    stats.mMixerProfile.SubmitNewTime(2.0f,1.0f);
    stats.Construct(&playback,&logic);
    Check(stats.mpPlaybackModule == &playback && stats.mpLogicModule == &logic, "Construct binds real module owners");
    Check(Near(stats.mMixerProfile.GetMostRecentTime(),2), "Construct preserves existing history");
    Check(std::string(stats.mVoiceProfiles[0].mpcName) == "AEMS Voices" &&
          std::string(stats.mVoiceProfiles[4].mpcName) == "Player Voices", "original voice-profile labels");
    gRwacTestBedAlloc.bytes=100; gCsisTestBedAlloc.bytes=200;
    gPlaybackTestBedAlloc.bytes=300; gLogicTestBedAlloc.bytes=400;
    playback.env.voices=7; playback.env.content=11;
    for(u32 i=0;i<5;++i) playback.env.ticks[i]=4000.0f*(i+1);
    stats.Update();
    Check(stats.mRwacAlloc.GetCurrent()==100 && stats.mCsisAlloc.GetCurrent()==200 &&
          stats.mPlaybackAlloc.GetCurrent()==300 && stats.mLogicAlloc.GetCurrent()==400,
          "all four allocator owners feed current counts");
    Check(stats.mVoiceCount.GetCurrent()==7 && stats.mContentCount.GetCurrent()==11,
          "active environment counts");
    Check(playback.reads==7, "environment is queried for both counts and every voice category");
    bool profiles=true;
    for(u32 i=0;i<5;++i) profiles &= Near(stats.mVoiceProfiles[i].mData.GetMostRecentTime(),1.5f*(i+1)) &&
        Near(stats.mVoiceProfiles[i].mData.GetAverageTime(),1.5f*(i+1)/16.0f);
    Check(profiles, "five runtime voice categories receive original tick scaling");
    gRwacTestBedAlloc.bytes=25; playback.env.voices=3;
    playback.env.ticks[0]=2000;
    stats.Update();
    Check(stats.mRwacAlloc.GetCurrent()==25 && stats.mRwacAlloc.GetMax()==100 &&
          stats.mVoiceCount.GetCurrent()==3 && stats.mVoiceCount.GetMax()==7, "high-water counts survive lower currents");
    Check(Near(stats.mVoiceProfiles[0].mData.GetMinimumTime(),0.75f) &&
          Near(stats.mVoiceProfiles[0].mData.GetMaximumTime(),1.5f) &&
          Near(stats.mVoiceProfiles[0].mData.GetAverageTime(),2.25f/16), "voice history advances across updates");
    stats.mPpuProfile[3].SubmitNewTime(8,1);
    stats.mCommandFillLevel.Set(77);
    BrnSound::Debug::gbResetStatistics=true;
    stats.Update();
    Check(BrnSound::Debug::gbResetStatistics, "reset is a held toggle");
    Check(stats.mRwacAlloc.GetMax()==100 && stats.mVoiceCount.GetMax()==7,
          "timing reset preserves object and allocator maxima");
    Check(Near(stats.mVoiceProfiles[0].mData.GetMostRecentTime(),0.75f) &&
          Near(stats.mVoiceProfiles[0].mData.GetMaximumTime(),0.75f) &&
          Near(stats.mVoiceProfiles[0].mData.GetAverageTime(),0.75f/16), "reset precedes current voice sample");
    Check(Near(stats.mMixerProfile.GetMostRecentTime(),0) && Near(stats.mPpuProfile[3].GetMostRecentTime(),0),
          "reset clears mixer and PPU histories");
    Check(stats.mCommandFillLevel.GetCurrent()==0 && stats.mCommandFillLevel.GetMax()==77,
          "original command-percent current zero preserves maximum");
    stats.Update();
    Check(Near(stats.mVoiceProfiles[0].mData.GetAverageTime(),0.75f/16), "held reset clears again next update");
    BrnSound::Debug::gbResetStatistics=false;

    sSystem.muDeferredRingHighWater=1024;
    sSystem.muMixerCpuTicks=264000;
    sSystem.muCommandExecutionCpuTicks=4000;
    sSystem.muTimerExecutionCpuTicks=8000;
    sSystem.muBalanceCycles=12000;
    sSystem.muExpelVoicesCpuTicks=16000;
    CgsDev::Debug2DImmediateRender hud;
    stats.RenderHUD(&hud,false);
    Check(sLockDepth==0 && sLockCount==1 && sUnlockCount==1, "HUD owns a complete RWAC lock scope");
    Check(hud.texts.size()==25 && hud.lines.empty(), "HUD emits complete original text without optional graph");
    Check(hud.texts[0].value=="Memory:" && hud.texts[1].value=="* RWAC: 25 \t(100) bytes" &&
          hud.texts[10].value=="* RWAC Command Buffer Usage(1024)", "live counter text and original RWAC high-water field");
    Check(Near(hud.texts.front().x,60) && Near(hud.texts.front().y,180) && Near(hud.texts.back().y,564) &&
          Near(stats.mfY,580), "original text origin and line advance");
    Check(!hud.texts[9].locked && hud.texts[10].locked && hud.texts[24].locked, "RWAC text/sample tail runs inside lock");
    Check(Near(hud.texts[13].size,20) && hud.texts[13].background==0x80000000u &&
          Near(hud.texts[13].border,0), "original text size, background and border");
    Check(hud.texts[13].colour==0xFF0000FFu, "99 percent mixer boundary turns red");
    Check(Near(stats.mMixerProfile.GetMostRecentTime(),99) && Near(stats.mMixerProfile.GetAverageTime(),99.0f/128),
          "render submits mixer timing history");
    Check(Near(stats.mPpuProfile[0].GetMostRecentTime(),15) && Near(stats.mPpuProfile[1].GetMostRecentTime(),1.5f) &&
          Near(stats.mPpuProfile[2].GetMostRecentTime(),3) && Near(stats.mPpuProfile[3].GetMostRecentTime(),4.5f),
          "overhead sums four costs and retains three original subprofiles");
    stats.mVoiceProfiles[2].mData.SubmitNewTime(std::numeric_limits<f32>::quiet_NaN(),1);
    CgsDev::Debug2DImmediateRender nanHud;
    stats.RenderHUD(&nanHud,true);
    Check(nanHud.texts[16].colour==0xFF0000FFu, "unordered voice timing retains PPC alert branch");
    Check(nanHud.lines.size()==128 && sLockDepth==0, "optional mixer graph draws inside complete HUD scope");
    sSystem.muCommandExecutionCpuTicks=0xFFFFFFFFu; sSystem.muTimerExecutionCpuTicks=1;
    sSystem.muBalanceCycles=0; sSystem.muExpelVoicesCpuTicks=0;
    CgsDev::Debug2DImmediateRender overflowHud;
    stats.RenderHUD(&overflowHud,false);
    Check(Near(stats.mPpuProfile[0].GetMostRecentTime(),0), "PPU total keeps original u32 wrap before float conversion");
    bool ppuWhite=true;
    for(u32 i=19;i<25;++i) ppuWhite &= overflowHud.texts[i].colour==0xFFFFFFFFu;
    Check(ppuWhite, "every PPU row shares total-overhead alert even when a child is large");

    VoiceProfilingData<128> graphProfile;
    for(u32 i=0;i<128;++i) graphProfile.SubmitNewTime(static_cast<f32>(i),1);
    CgsDev::Debug2DImmediateRender graph;
    stats.DrawGraph(&graph,&graphProfile,0xFF00FF00u);
    Check(graph.lines.size()==128, "graph covers full history");
    Check(Near(graph.lines.front().start.x,120) && Near(graph.lines.front().start.y,700) &&
          Near(graph.lines.front().end.x,126.875f) && Near(graph.lines.front().end.y,700),
          "graph starts with original oldest-sample segment");
    Check(Near(graph.lines.back().end.x,1000) && Near(graph.lines.back().end.y,-62),
          "graph ends at current sample with original 6x height scale");
    u32 red=0; bool joined=true;
    for(u32 i=0;i<128;++i)
    {
        red += graph.lines[i].colour==0xFF0000FFu;
        if(i) joined &= Near(graph.lines[i].start.x,graph.lines[i-1].end.x) &&
                         Near(graph.lines[i].start.y,graph.lines[i-1].end.y);
    }
    Check(red==29 && graph.lines.front().colour==0xFF00FF00u, "graph colour changes at exact 99 threshold");
    Check(joined, "graph segments retain history continuity");
    stats.mfX=1; stats.mfY=2;
    const std::string longText(400,'x');
    CgsDev::Debug2DImmediateRender clipped;
    stats.RenderLine(&clipped,false,"%s",longText.c_str());
    Check(clipped.texts[0].value.size()==255 && Near(stats.mfY,18), "bounded varargs formatting still advances line");
    Check(sAssertCount==0, "valid batch inputs raise no assertions");
    std::printf("SoundStatisticsBatch: %d checks, %d failures\n",sChecks,sFailures);
    return sFailures ? 1 : 0;
}
