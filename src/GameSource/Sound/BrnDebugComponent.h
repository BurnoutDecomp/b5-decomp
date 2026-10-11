#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsLogWindow.h"
#include "GameShared/GameClasses/Development/Log/CgsLogCombined.h"
#include "GameShared/GameClasses/Development/Log/CgsLogNull.h"
#include "GameShared/GameClasses/Development/Log/CgsLogOutput.h"
#include "GameSource/Sound/Debug/BrnSoundDebugStatistics.h"

namespace CgsSound { namespace Playback { namespace Module { class Module; } } }
namespace BrnSound { namespace Module { struct SoundLogicModule; } }
struct Splice;

namespace BrnSound
{
namespace Debug
{
    void ConsoleChangeCallback(void* lpValue, void* lpUserData);

    // DecFIGS BrnDebugComponent.h:69; ARTIST constructor 0x827E0720 and
    // Construct 0x826A1230. The +0x0C/+0x14 members are StrStreamBase-derived
    // LogNull/LogOutput, not queues: vtable sinks are blr and LogOutput::Append.
    class DebugComponent : public CgsDev::DebugComponent
    {
    public:
        enum EConsole
        {
            E_AUDIO = 0,
            E_GAME = 1,
            E_AUDIO_AND_GAME = 2,
            E_MAX_CONSOLES = 3
        };

        DebugComponent();
        void Construct(CgsSound::Playback::Module::Module* lpPlaybackModule,
                       BrnSound::Module::SoundLogicModule* lpLogicModule);
        void InstallConsole(EConsole leConsole);

        static CgsDev::Log::LogCombined mLogCombined;

    protected:
        const char* GetName() const override { return "Sound"; }
        const char* GetPath() const override { return "Sound Module"; }
        void OnActivate() override;

    private:
        CgsDev::Log::LogNull mLogNull;       // ARTIST +0x0C
        CgsDev::Log::LogOutput mLogTty;      // +0x14
        CgsDev::DebugUI::LogWindow mLogWindow; // +0x1C
        CgsSound::Playback::Module::Module* mpPlaybackModule; // +0x78
        BrnSound::Module::SoundLogicModule* mpLogicModule;    // +0x7C
        CgsDev::DebugUI::StringList maCollisionMaterialNames[26]; // +0x80
        CgsDev::DebugUI::StringList maCollisionActionNames[8];    // +0x150
        CgsDev::DebugUI::StringList maConsoleNames[4];            // +0x190
        CgsDev::DebugUI::StringList maSpliceBankNames[50];        // +0x1B0
        s32 meOutput;                      // +0x340, registered as an s32 enum
        bool mbSpliceAudition;              // +0x344
        bool mbSpliceAuditionAutoCycle;     // +0x345
        s32 miSpliceBankIndex;              // +0x348
        s32 miSpliceIndexTens;              // +0x34C
        s32 miSpliceIndexUnits;             // +0x350
        Splice* mpSpliceAuditionVoice;      // +0x354
        Statistics mStatistics;            // +0x358
    };
}
}
