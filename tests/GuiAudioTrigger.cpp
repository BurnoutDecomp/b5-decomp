// Actual extracted production bodies, actual event/message/IO/queue headers.
// Only asserts and PresentationEffect Resolve/Play/log destinations are observed.
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <string>
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"

static unsigned checks, failures, assertions, writeAssertions;
static void Check(bool good, const std::string& why)
{
    ++checks;
    if (!good) { ++failures; std::printf("FAIL %s\n", why.c_str()); }
}
bool gbLogGuiAudioTriggers = false;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* expression, const char*, int)
{
    ++assertions;
    if (std::strcmp(expression, "Not locked for writing\n") == 0) ++writeAssertions;
    return 0;
}
void* EndAssert() { return nullptr; }
}
namespace Log { StrStreamBase* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; }
}
namespace BrnGui {
struct GuiModule {
    CgsGui::CgsGuiModuleIO::OutputBuffer* mpOutputBuffer = nullptr;
    static void FlaptSoundTriggerCallback(void*, const char*, const char*, const char*, const char*);
};
}
namespace BrnSound { namespace Debug {
struct BoundaryLog { template <class T> BoundaryLog& operator<<(const T&) { return *this; } };
struct DebugComponent { static BoundaryLog mLogCombined; };
BoundaryLog DebugComponent::mLogCombined;
} namespace Logic {
bool KB_SPEW_PRESENTATION_MESSAGES = false;
struct PresentationEffect {
#include "presentation_entry.inc"
    f32 mfRunningTime = 0;
    mutable unsigned resolves = 0;
    unsigned plays = 0;
    mutable s32 action = -1;
    mutable u64 stringId = 0, screenId = 0;
    bool resolveResult = true, entryValid = true;
    bool Resolve(s32 inputAction, u64 inputString, u64 inputScreen, PresentationEntry& entry) const
    {
        ++resolves; action=inputAction; stringId=inputString; screenId=inputScreen;
        entry.mu8Valid = entryValid ? 1 : 0;
        return resolveResult;
    }
    void Play(const PresentationEntry&) { ++plays; }
    void Notify(const CgsSound::Io::MessageHeader*);
};
}}
#include "gui_audio_extracted.inc"

using GuiQueue = CgsModule::VariableEventQueue<18432,16>;
using SoundQueue = CgsModule::VariableEventQueue<8192,16>;
using AudioPayload = BrnGui::GuiAudioTriggerWirePayload457;
using AudioMessage = CgsSound::Io::Message<AudioPayload>;
using Output = CgsGui::CgsGuiModuleIO::OutputBuffer;
struct OracleCase {
    const char *name, *actionName, *component, *label, *movie;
    s32 action;
    u32 stringHash, movieHash;
};
static const OracleCase cases[] = {
#include "audio_oracle.inc"
};
static std::string Prefix31(const char* text) { return std::string(text).substr(0,31); }

static void Consume(SoundQueue& sound, s32 type, const CgsModule::Event* event, s32 size)
{
    sound.Clear();
    BrnSound::Module::ProcessAudioCase(sound, type, event, size);
}

static void CheckSound(SoundQueue& sound, const OracleCase& oracle, const std::string& prefix)
{
    const CgsModule::Event* event = nullptr;
    s32 size = 0;
    const s32 type = sound.GetFirstEvent(&event,&size);
    Check(type==6 && size==116 && event, prefix+" sound6/116");
    if (!event || size!=sizeof(AudioMessage)) return;
    AudioMessage message;
    std::memcpy(&message,event,sizeof(message));
    Check(message.GetStateManagerId()==0 && message.GetInstanceId()==0 &&
          message.GetEffectId()==0 && message.GetEffectType()==1, prefix+" sound destination0/0/0/object");
    Check(message.mData.meAction==oracle.action, prefix+" sound action");
    Check(std::strcmp(message.mData.macComponent,Prefix31(oracle.component).c_str())==0 &&
          std::strcmp(message.mData.macLabel,Prefix31(oracle.label).c_str())==0 &&
          std::strcmp(message.mData.macMovie,Prefix31(oracle.movie).c_str())==0,
          prefix+" sound named payload/truncation");
    const CgsModule::Event* next=nullptr;
    s32 nextSize=0;
    sound.GetNextEvent(event,&next,&nextSize);
    Check(!next, prefix+" exactly one sound message");
    BrnSound::Logic::PresentationEffect effect;
    effect.Notify(&message);
    Check(effect.resolves==1 && effect.action==oracle.action && effect.stringId==oracle.stringHash &&
          effect.screenId==oracle.movieHash && effect.plays==1, prefix+" actual Notify reaches Resolve/Play with original action and name hashes");
    effect.resolveResult=false;
    effect.Notify(&message);
    Check(effect.plays==1, prefix+" unresolved Notify does not Play");
    effect.resolveResult=true; effect.entryValid=false;
    effect.Notify(&message);
    Check(effect.plays==1, prefix+" invalid entry Notify does not Play");
}

static void Callback(const OracleCase& oracle, bool locked=true)
{
    Output output;
    output.Construct();
    BrnGui::GuiModule module;
    module.mpOutputBuffer=&output;
    if (locked) output.LockForWrite();
    const unsigned oldAsserts=assertions, oldWrite=writeAssertions;
    BrnGui::GuiModule::FlaptSoundTriggerCallback(&module,oracle.component,oracle.movie,
                                               oracle.actionName,oracle.label);
    Check(assertions-oldAsserts==(locked?0:1) && writeAssertions-oldWrite==(locked?0:1),
          std::string(oracle.name)+" original write-lock assertion invariant");
    if (locked) output.UnlockForWrite();
    output.LockForRead();
    const GuiQueue* queue=output.GetOutEventQueue();
    const CgsModule::Event* event=nullptr;
    s32 size=0;
    const s32 type=queue->GetFirstEvent(&event,&size);
    s32 observedAction=-1;
    if (event && size>=100)
        std::memcpy(&observedAction,reinterpret_cast<const u8*>(event)+(type==201?44:32),4);
    std::printf("OBSERVED callback=%s type=%d size=%d action=%d asserts=%u\n",oracle.name,type,size,
                observedAction,assertions-oldAsserts);
    const std::string prefix=std::string(oracle.name)+(locked?"":" unlocked");
    Check(type==457,prefix+" original GUI id457");
    Check(size==100,prefix+" original GUI size100");
    Check(event!=nullptr,prefix+" queued event");
    if (event && size>=100) {
        const u8* bytes=reinterpret_cast<const u8*>(event);
        s32 rawAction=-1; std::memcpy(&rawAction,bytes+32,4);
        Check(std::memcmp(bytes,Prefix31(oracle.component).c_str(),Prefix31(oracle.component).size()+1)==0,
              prefix+" raw component offset0");
        Check(rawAction==oracle.action,prefix+" raw action offset32");
        Check(std::memcmp(bytes+36,Prefix31(oracle.label).c_str(),Prefix31(oracle.label).size()+1)==0,
              prefix+" raw label offset36");
        Check(std::memcmp(bytes+68,Prefix31(oracle.movie).c_str(),Prefix31(oracle.movie).size()+1)==0,
              prefix+" raw movie offset68");
        SoundQueue sound; sound.Construct(); Consume(sound,type,event,size);
        CheckSound(sound,oracle,prefix);
    }
    const CgsModule::Event* next=nullptr;s32 nextSize=0;
    if (event) queue->GetNextEvent(event,&next,&nextSize);
    Check(!next,prefix+" exactly one GUI record");
    output.UnlockForRead();
}

int main()
{
    static_assert(sizeof(AudioPayload)==100,"raw100");
    static_assert(sizeof(BrnGui::GuiAudioTriggerEvent)==112,"legacy112 ABI retained");
    static_assert(sizeof(AudioMessage)==116,"sound116");
    for (const auto& oracle:cases) Callback(oracle);
    Callback(cases[0],false);
    GuiQueue gui;gui.Construct();
    SoundQueue sound;sound.Construct();
    BrnGui::GuiAudioTriggerEvent legacy;
    legacy.Construct(cases[7].action,cases[7].component,cases[7].label,cases[7].movie);
    AudioPayload raw(legacy);
    gui.AddEvent(reinterpret_cast<const CgsModule::Event*>(&raw),457,sizeof(raw));
    const CgsModule::Event* event=nullptr;s32 size=0;
    s32 type=gui.GetFirstEvent(&event,&size);
    Consume(sound,type,event,size);CheckSound(sound,cases[7],"raw457 continuation");
    gui.Clear();gui.AddEvent(reinterpret_cast<const CgsModule::Event*>(&legacy),201,sizeof(legacy));
    type=gui.GetFirstEvent(&event,&size);
    Consume(sound,type,event,size);CheckSound(sound,cases[7],"legacy201 compatibility");
    const u64 hideId=0x0123456789ABCDEFull;
    gui.Clear();gui.AddEvent(reinterpret_cast<const CgsModule::Event*>(&hideId),201,sizeof(hideId));
    type=gui.GetFirstEvent(&event,&size);Consume(sound,type,event,size);
    event=nullptr;sound.GetFirstEvent(&event,&size);
    Check(!event,"unrelated HideDriveThru201 raw8 produces no sound message");
    std::printf("GuiAudioTrigger: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
