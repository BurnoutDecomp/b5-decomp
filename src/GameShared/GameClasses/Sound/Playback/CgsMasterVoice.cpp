#include "GameShared/GameClasses/Sound/Playback/CgsMasterVoice.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

namespace CgsSound
{
namespace Playback
{

MasterVoice* MasterVoice::spMasterVoice;   // cpp:43 (X360 dword_82FFB9E0)

MasterVoice::MasterVoice(size_t auClientSize, Factory& arFactory,
                         const VoiceSpec& arVoiceSpec, u32 au32Ident)
    : SubmixVoice(auClientSize, arFactory, arVoiceSpec, au32Ident)
{
    CGS_ASSERT_AT(spMasterVoice == 0, "0 == spMasterVoice",
        "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/CgsMasterVoice.h", 85);
    spMasterVoice = this;
}

MasterVoice& MasterVoice::GetMasterVoice()
{
    CGS_ASSERT(spMasterVoice != NULL, "spMasterVoice");
    return *spMasterVoice;
}

bool MasterVoice::IsCompatible(EVoiceType leVoiceType)
{
    return leVoiceType == E_MASTER_VOICE;
}

// @ 0x826D7A28
MasterVoice::~MasterVoice()
{
    CGS_ASSERT_AT(spMasterVoice, "spMasterVoice",
        "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/CgsMasterVoice.cpp", 48);
    spMasterVoice = NULL;
}

}
}
