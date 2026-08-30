#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacMasterVoice.h"

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"

namespace CgsSound
{
namespace Playback
{

GenericRwacMasterVoice::GenericRwacMasterVoice(
    GenericRwacFactory& arFactory, const VoiceSpec& akrSpec, u32 au32Ident)
    : MasterVoice(sizeof(GenericRwacMasterVoice), arFactory, akrSpec, au32Ident),
      GenericRwacVoice()
{
}

GenericRwacMasterVoice::~GenericRwacMasterVoice()
{
}

f32 GenericRwacMasterVoice::GetCpuTicks()
{
    return GenericRwacVoice::GetCpuTicks();
}

Voice::EProfileVoiceType GenericRwacMasterVoice::GetProfileVoiceType()
{
    return E_VOICETYPE_GENERICRWACMASTER;
}

void GenericRwacMasterVoice::DoUpdate(System* apSystem, f32 /*af32DeltaTime*/)
{
    GenericRwacVoice::Update(apSystem, *this);
}

bool GenericRwacMasterVoice::DoConnectSend(u32 au32Index,
                                           SubmixVoice* apSubmix)
{
    return GenericRwacVoice::ConnectSend(au32Index, apSubmix);
}

} // namespace Playback
} // namespace CgsSound
