// Execute the production lookup/creation bodies with optional NRVO disabled.
// Handles are the real header; allocation, lookup objects and asserts are fixtures.
#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>
#undef CGS_ASSERT
static unsigned asserts;
#define CGS_ASSERT(value, ...) do { if (!(value)) ++asserts; } while (0)
namespace rw { namespace audio { namespace core { struct PlugIn {}; } } }

namespace CgsSound { namespace Playback {
using Name = uintptr_t;
struct Counter {
    unsigned refs=1, acquired=0, released=0;
    void Acquire(){++refs;++acquired;}
    void Release(){--refs;++released;}
};
struct Factory : Counter {Name name=0;Name GetName()const{return name;}};
struct Voice : Counter {
    u32 ident=0;Factory* factory=nullptr;bool hasSlot=true;
    u32 GetIdent()const{return ident;}
    Factory& GetFactory(){return *factory;}
    void* FindNamedSlot(Name){return hasSlot?this:nullptr;}
};
struct GenericRwacVoice {
    unsigned secondaryBaseMarker[4] = {};
    std::vector<const rw::audio::core::PlugIn*> plugins;
    u32 GetPluginCount()const{return static_cast<u32>(plugins.size());}
    const rw::audio::core::PlugIn* GetPlugin(u32 i)const{return plugins[i];}
};
struct GenericRwacPlayerVoice : Voice, GenericRwacVoice {};
struct EnvironmentSpec { bool fail=false; };
struct Environment : Counter {
    u32 mu32FactoryCount=0,mu32VoiceCount=0;
    Handle<Factory>* mphFactory=nullptr;
    Handle<Voice>* mphVoice=nullptr;
    inline static u32 gu32VoiceTypeTag=7,gu32NamedSlotSentinel=3;
    Environment()=default;
    explicit Environment(const EnvironmentSpec&){refs=0;}
    static void* operator new(size_t bytes,const EnvironmentSpec& spec) noexcept{return spec.fail?nullptr:std::malloc(bytes);}
    Handle<Factory> GetFactory(Name);
    Handle<Voice> GetVoice(u32);
    Handle<Voice> GetRwacVoiceByPlugin(const rw::audio::core::PlugIn*);
    static Handle<Environment> Create(const EnvironmentSpec&);
};
#include "playback_handle_return.inc"
} }

static unsigned checks,failures;
static void Check(bool value,const char* name){++checks;if(!value){++failures;std::printf("FAIL %s\n",name);}}
int main()
{
    using namespace CgsSound::Playback;
    Environment env;
    Factory wrong,found;wrong.name=2;found.name=7;
    Handle<Factory> factories[]={Handle<Factory>(nullptr),Handle<Factory>(&wrong),Handle<Factory>(&found)};
    env.mphFactory=factories;env.mu32FactoryCount=3;
    const auto factory=env.GetFactory(7);
    Check(factory.GetObject()==&found&&found.refs==2&&found.acquired==1,"factory result owns one acquired reference");
    Check(!env.GetFactory(99)&&found.refs==2&&wrong.refs==1,"missing factory returns empty without altering references");
    GenericRwacPlayerVoice a,b,c;
    a.ident=12;a.factory=&wrong;b.ident=23;b.factory=&found;c.ident=34;c.factory=&found;
    Handle<Voice> voices[]={Handle<Voice>(nullptr),Handle<Voice>(&a),Handle<Voice>(&b),Handle<Voice>(&c)};
    env.mphVoice=voices;env.mu32VoiceCount=4;
    const auto voice=env.GetVoice(23);
    Check(voice.GetObject()==&b&&b.refs==2&&b.acquired==1,"voice lookup returns the first match with exactly one reference");
    Check(!env.GetVoice(99)&&b.refs==2&&a.refs==1,"missing voice leaves every reference intact");
    rw::audio::core::PlugIn plugin,other;
    b.plugins={&other};c.plugins={&plugin};
    const auto selected=env.GetRwacVoiceByPlugin(&plugin);
    Check(selected.GetObject()==&c&&c.refs==2&&c.acquired==2&&c.released==1,"plugin match preserves the temporary and returned reference operations");
    Check(a.acquired==0&&b.acquired==2&&b.released==1&&b.refs==2,"wrong factory is skipped and unmatched candidate releases its temporary");
    c.hasSlot=false;
    Check(!env.GetRwacVoiceByPlugin(&plugin)&&c.acquired==2&&c.refs==2,"missing named slot cannot acquire or return a voice");
    env.mu32VoiceCount=0;
    Check(!env.GetVoice(23)&&!env.GetRwacVoiceByPlugin(&plugin),"empty voice table returns empty handles");
    auto created=Environment::Create(EnvironmentSpec{});
    Check(created&&created.GetObject()->refs==1&&created.GetObject()->acquired==1,"creation transfers one acquired environment reference");
    auto* owned=created.GetObject();owned->~Environment();std::free(owned);
    Check(!Environment::Create(EnvironmentSpec{true})&&asserts==1,"failed allocation reports its assertion and returns empty");
    std::printf("PCPlaybackHandleReturn: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
