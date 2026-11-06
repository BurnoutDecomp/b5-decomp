#define private public
#include "GameSource/Sound/Global/BrnMusicEffect.h"
#undef private
using namespace BrnSound::Logic;
unsigned BaseSize(){return sizeof(BrnEffectObject);}
unsigned EffectSize(){return sizeof(MusicEffect);}
#define OFF(n) unsigned off_##n(){return (unsigned)&((MusicEffect*)0)->n;}
OFF(mJumpHpf) OFF(mSecondaryStream) OFF(mEATraxStream) OFF(mMusicStreamMenu) OFF(mJunkyardStream) OFF(mEaTraxData) OFF(mpMixerControl) OFF(mfLastPublishedGuiVolume)
