#define private public
#include "GameSource/Sound/Global/BrnMusicEffect.h"
#undef private
using namespace BrnSound::Logic;
unsigned BaseSize(){return sizeof(BrnEffectObject);}
unsigned EffectSize(){return sizeof(MusicEffect);}
unsigned StreamSize(){return sizeof(MusicStream);}
#define OFF(n) unsigned off_##n(){return (unsigned)&((MusicEffect*)0)->n;}
OFF(mJumpHpf) OFF(mSecondaryStream) OFF(mEATraxStream) OFF(mMusicStreamMenu) OFF(mJunkyardStream) OFF(mEaTraxData) OFF(meEventEndResult) OFF(mbEventWon) OFF(mbEventEndPending) OFF(mCarUnlockName) OFF(mMenuStreamName) OFF(meMusicType) OFF(mePrevMusicType) OFF(mpStreamingStateManager) OFF(mpMixerControl) OFF(mePendingMusicType) OFF(meJunkyardAmbience) OFF(mfLastPublishedGuiVolume) OFF(miPicParadiseMusic) OFF(mbHoldVolumes) OFF(mbMenuStreamIsVideo) OFF(mbMenuStreamOverCustom) OFF(meLanguage)

unsigned off_mbPendingChange(){return (unsigned)&((MusicEffect*)0)->mEaTraxData.mbPendingChange;}
unsigned off_mbPlayFromEATraxSlider(){return (unsigned)&((MusicEffect*)0)->mEaTraxData.mbPlayFromEATraxSlider;}
