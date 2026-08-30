#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacSubmixVoice.h"

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"

namespace CgsSound
{
namespace Playback
{

GenericRwacSubmixVoice::GenericRwacSubmixVoice(
    GenericRwacFactory& arFactory, const VoiceSpec& akrSpec, u32 au32Ident)
    : SubmixVoice(sizeof(GenericRwacSubmixVoice), arFactory, akrSpec, au32Ident),
      GenericRwacVoice()
{
}

GenericRwacSubmixVoice::~GenericRwacSubmixVoice()
{
}

f32 GenericRwacSubmixVoice::GetCpuTicks()
{
    return GenericRwacVoice::GetCpuTicks();
}

Voice::EProfileVoiceType GenericRwacSubmixVoice::GetProfileVoiceType()
{
    return E_VOICETYPE_GENERICRWACSUBMIX;
}

void GenericRwacSubmixVoice::DoUpdate(System* apSystem, f32 /*af32DeltaTime*/)
{
    GenericRwacVoice::Update(apSystem, *this);
}

bool GenericRwacSubmixVoice::DoConnectSend(u32 au32Index,
                                           SubmixVoice* apSubmix)
{
    return GenericRwacVoice::ConnectSend(au32Index, apSubmix);
}

} // namespace Playback
} // namespace CgsSound
