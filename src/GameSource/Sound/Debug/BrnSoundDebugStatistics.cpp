#include "GameSource/Sound/Debug/BrnSoundDebugStatistics.h"

namespace BrnSound
{
namespace Debug
{
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
}
}
