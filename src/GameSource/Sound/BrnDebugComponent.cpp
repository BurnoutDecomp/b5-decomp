#include "GameSource/Sound/BrnDebugComponent.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameSource/Sound/Module/LogicModule/BrnSubmixesEffect.h"
#include "GameSource/Sound/World/BrnEmitterEffect.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnclosureControl.h"

namespace BrnSound
{
namespace Debug
{
CgsDev::Log::LogCombined DebugComponent::mLogCombined; // ARTIST 0x82F2F9BC

// ARTIST 0x827E0720: only base/member construction; Construct seeds state.
DebugComponent::DebugComponent()
{
}

// ARTIST 0x826A1230..0x826A12BC (export hole; tools/re/ppcdis.py).
void DebugComponent::Construct(CgsSound::Playback::Module::Module* lpPlaybackModule,
                               BrnSound::Module::SoundLogicModule* lpLogicModule)
{
    // The original's call at 0x826A1248 is the shared blr body 0x8284CB38;
    // it adds no stores (the current host base Construct resets the list link).
    mStatistics.Construct(lpPlaybackModule, lpLogicModule);
    mpPlaybackModule = lpPlaybackModule;
    mpLogicModule = lpLogicModule;
    meOutput = E_AUDIO;
    mbSpliceAuditionAutoCycle = false;
    mbSpliceAudition = false;
    miSpliceBankIndex = 0;
    miSpliceIndexTens = 0;
    miSpliceIndexUnits = 0;
    mpSpliceAuditionVoice = nullptr;
}

// ARTIST 0x826893A0: r3=value pointer, r4=user data; load enum then tail call.
void ConsoleChangeCallback(void* lpValue, void* lpUserData)
{
    static_cast<DebugComponent*>(lpUserData)->InstallConsole(
        static_cast<DebugComponent::EConsole>(*static_cast<s32*>(lpValue)));
}

// ARTIST 0x826893B0. Its enum argument is unused in this build; the live
// registered member is read at 0x82689470, including when invoked by callback.
void DebugComponent::InstallConsole(EConsole)
{
    static bool sbConsoleRegistered = false; // byte_8300C6A4, process lifetime
    if (!sbConsoleRegistered)
    {
        RegisterVariable(&meOutput, "Console");
        SetRange(&meOutput, 0, 2);
        maConsoleNames[0] = { E_AUDIO, "Audio Only" }; // table 0x82F2FCF0
        maConsoleNames[1] = { E_GAME, "Game Only" }; // 0x82F2FCF4
        maConsoleNames[2] = { E_AUDIO_AND_GAME, "Audio AND Game" }; // 0x82F2FCF8
        maConsoleNames[3] = { E_MAX_CONSOLES, nullptr }; // 0x82F2FCFC
        SetOptions(&meOutput, maConsoleNames);
        SetChangeCallback(&meOutput, &ConsoleChangeCallback, this);
        sbConsoleRegistered = true;
    }

    CgsDev::Log::gpDebugPrint = meOutput == E_GAME || meOutput == E_AUDIO_AND_GAME
        ? static_cast<CgsDev::StrStreamBase*>(&mLogTty)
        : static_cast<CgsDev::StrStreamBase*>(&mLogNull);
    mLogCombined.RemoveAllStreams();
    if (meOutput != E_GAME)
    {
        mLogCombined.AddStream(&mLogTty);
        mLogCombined.AddStream(&GetUI().GetConsole().mLog);
    }
}

// Recovered subset of ARTIST 0x826D5CB0: Console; Global (0x826D7118 onward),
// Emitters and static-environment flags. These bind the actual consumer storage.
// The other registration groups and Render3dControl callback remain unrecovered.
void DebugComponent::OnActivate()
{
    CgsDev::DebugComponent::OnActivate(); // ICF-folded blr at 0x8284CB38
    InstallConsole(static_cast<EConsole>(meOutput));

    RegisterVariable(&Logic::World::KB_DEBUG_WORLD_EMITTERS, "Emitters", "Debug"); // 0x82FFB8CB
    RegisterVariable(&Vehicles::Environment::KB_SHOW_STATIC_ENVIRONMENT,
                     "Passbys&Reverb", "Show Static"); // 0x82FFB8C8

    RegisterVariable(&Logic::KF_GLOBAL_GAIN, "Global", "Master Gain"); // 0x82F2CE94
    SetRange(&Logic::KF_GLOBAL_GAIN, 0.0f, 50.0f);
    SetStep(&Logic::KF_GLOBAL_GAIN, 0.25f);
    RegisterVariable(&Logic::KF_GLOBAL_LOW_SHELF_GAIN, "Global", "LF Shelf Gain"); // 0x82F2CE98
    SetRange(&Logic::KF_GLOBAL_LOW_SHELF_GAIN, 0.0f, 50.0f);
    SetStep(&Logic::KF_GLOBAL_LOW_SHELF_GAIN, 0.1f);
    RegisterVariable(&Logic::KF_GLOBAL_LOW_SHELF_FREQ, "Global", "LF Shelf Freq"); // 0x82F2CE9C
    SetRange(&Logic::KF_GLOBAL_LOW_SHELF_FREQ, 0.0f, 120.0f);
    SetStep(&Logic::KF_GLOBAL_LOW_SHELF_FREQ, 2.0f);
    RegisterVariable(&Logic::KF_GLOBAL_LIMITER_THRESHOLD, "Global", "Limiter Thresh"); // 0x82FFB8D4
    SetRange(&Logic::KF_GLOBAL_LIMITER_THRESHOLD, -100.0f, 20.0f);
    SetStep(&Logic::KF_GLOBAL_LIMITER_THRESHOLD, 1.0f);
    RegisterVariable(&Logic::KF_GLOBAL_LIMITER_RELEASE, "Global", "Limiter Release"); // 0x82F2CEA8
    SetRange(&Logic::KF_GLOBAL_LIMITER_RELEASE, 0.0f, 1000.0f);
    SetStep(&Logic::KF_GLOBAL_LIMITER_RELEASE, 1.0f);
    RegisterVariable(&Logic::KF_GLOBAL_GAIN_ARRAY_CENTRE, "Global", "GainArray Centre"); // 0x82F2CEB0
    SetRange(&Logic::KF_GLOBAL_GAIN_ARRAY_CENTRE, 0.0f, 50.0f);
    SetStep(&Logic::KF_GLOBAL_GAIN_ARRAY_CENTRE, 0.02f);
    RegisterVariable(&Logic::KF_GLOBAL_GAIN_ARRAY_FRONT, "Global", "GainArray Front"); // 0x82F2CEB4
    SetRange(&Logic::KF_GLOBAL_GAIN_ARRAY_FRONT, 0.0f, 50.0f);
    SetStep(&Logic::KF_GLOBAL_GAIN_ARRAY_FRONT, 0.02f);
    RegisterVariable(&Logic::KF_GLOBAL_GAIN_ARRAY_REAR, "Global", "GainArray Rear"); // 0x82F2CEB8
    SetRange(&Logic::KF_GLOBAL_GAIN_ARRAY_REAR, 0.0f, 50.0f);
    SetStep(&Logic::KF_GLOBAL_GAIN_ARRAY_REAR, 0.02f);
    RegisterVariable(&Logic::KF_GLOBAL_GAIN_ARRAY_SUB, "Global", "GainArray Sub"); // 0x82F2CEBC
    SetRange(&Logic::KF_GLOBAL_GAIN_ARRAY_SUB, 0.0f, 50.0f);
    SetStep(&Logic::KF_GLOBAL_GAIN_ARRAY_SUB, 0.02f);
}
}
}
