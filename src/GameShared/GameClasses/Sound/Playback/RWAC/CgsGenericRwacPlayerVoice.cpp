#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacPlayerVoice.h"

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"

namespace CgsSound
{
namespace Playback
{

// ARTIST 0x826C7FB8 (PS3 inlined into CreateVoiceInternal<GenericRwacPlayerVoice> 0x90C738):
// the bases, then the playback state reset to E_PLAYBACK_STATE_INVALID (SetPlaybackState's
// clear / compare / CHANGED store, 0x826C7FFC..0x826C8018) and the change acknowledged
// (0x826C801C..0x826C8028). mIsRequestDoneParams is left as allocated (no store to +0x84/+0x88).
GenericRwacPlayerVoice::GenericRwacPlayerVoice(
    Factory& arFactory, const VoiceSpec& akrSpec, u32 au32Ident)
    : PlayerVoice(sizeof(GenericRwacPlayerVoice), arFactory, akrSpec, au32Ident),
      GenericRwacVoice()
{
    SetPlaybackState(E_PLAYBACK_STATE_INVALID);
    AcknowledgePlaybackStateChange();
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
