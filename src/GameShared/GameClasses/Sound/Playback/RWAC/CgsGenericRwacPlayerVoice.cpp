#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacPlayerVoice.h"

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"

namespace CgsSound
{
namespace Playback
{

GenericRwacPlayerVoice::GenericRwacPlayerVoice(
    GenericRwacFactory& arFactory, const VoiceSpec& akrSpec, u32 au32Ident)
    : PlayerVoice(sizeof(GenericRwacPlayerVoice), arFactory, akrSpec, au32Ident),
      GenericRwacVoice(),
      mIsRequestDoneParams()
{
    mIsRequestDoneParams.requestHandle = 0.0f;
    mIsRequestDoneParams.isRequestDone = 0.0f;
    AcknowledgePlaybackStateChange();
}

GenericRwacPlayerVoice::~GenericRwacPlayerVoice()
{
}

f32 GenericRwacPlayerVoice::GetCpuTicks()
{
    return GenericRwacVoice::GetCpuTicks();
}

Voice::EProfileVoiceType GenericRwacPlayerVoice::GetProfileVoiceType()
{
    return E_VOICETYPE_GENERICRWACPLAYER;
}

void GenericRwacPlayerVoice::DoUpdate(System* apSystem, f32 /*af32DeltaTime*/)
{
    GenericRwacVoice::Update(apSystem, *this);
}

bool GenericRwacPlayerVoice::DoConnectSend(u32 au32Index,
                                           SubmixVoice* apSubmix)
{
    return GenericRwacVoice::ConnectSend(au32Index, apSubmix);
}

} // namespace Playback
} // namespace CgsSound
