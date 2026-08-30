// ============================================================================
// CgsAemsContent.cpp -- CgsSound::Playback::AemsContentSlot runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   AemsContentSlot::DoPlay(Slot&, PlayerVoice&, Content&, u32)        @ 0x826DAFF0
//   AemsContentSlot::DoStop(Slot&, PlayerVoice&, Content&)             @ 0x826DB008
//   AemsContentSlot::DoUpdatePlaying(System*, Slot&, PlayerVoice&, Content&, f32) @ 0x826DAFE8
//
// All three are thin AEMS-bank dispatchers: they downcast the PlayerVoice& base to
// AemsPlayerVoice& and forward to its Play/Stop/Update. DoPlay additionally raises
// the PLAYING bit (2) on the player voice's playback-flags byte (+0x80 X360)
// before starting playback. The X360 tail-calls (`b ...Play/Stop/Update`) keep the
// player-voice `this` in r3 and forward the trailing argument register/float, so
// the slot's own Slot/Content args are passed through unread (DoPlay/DoStop/
// DoUpdatePlaying ignore them), exactly as reconstructed below.
//
// FLAG: AemsPlayerVoice::Play/Stop/Update bodies live in their own (not-yet-done)
// AemsPlayerVoice TUs; they are declared in CgsAemsContent.h and linked at
// consolidation. The +0x80 playback-flags member's exact layout slot is DEFERRED
// to the AemsPlayerVoice keystone (see header FLAG).
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsContent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/Csis/CsisSystem.h"
#include "SDKs/EATech/include/snd/sndaems.h"

namespace CgsSound
{
namespace Playback
{

CsisContent::CsisContent(Factory& arFactory, const ContentSpec& akrSpec, u32 au32Ident)
    : Content(arFactory, akrSpec, au32Ident),
      mLoader(),
      mpCsisData(0)
{
}

CsisContent::~CsisContent()
{
}

bool CsisContent::DoLoad()
{
    return mLoader.Load(*this, GetContentSpec());
}

bool CsisContent::DoUnload()
{
    return mLoader.Unload(*this, GetContentSpec());
}

void CsisContent::DoUpdate(f32 /*af32Dt*/)
{
    mLoader.Update(*this, GetContentSpec());
}

void* CsisContent::DoGetData()
{
    void* lpData = mLoader.GetData();
    CGS_ASSERT(lpData != 0, "lpvData");
    return lpData;
}

// ARTIST @0x826D9CF0.
bool CsisContent::DoOnPostLoad()
{
    mpCsisData = Content::GetData(E_CONTENT_STATE_LOADING);
    CGS_ASSERT(mpCsisData != 0, "mpCsisData");
    Csis::System::Subscribe(static_cast<Csis::SystemContent*>(mpCsisData));
    return true;
}

// ARTIST @0x826D9C58.
bool CsisContent::DoOnPreUnload()
{
    CGS_ASSERT(mpCsisData != 0, "mpCsisData");
    Csis::System::Unsubscribe(static_cast<Csis::SystemContent*>(mpCsisData));
    mpCsisData = 0;
    return true;
}

AemsContent::AemsContent(Factory& arFactory, const ContentSpec& akrSpec, u32 au32Ident)
    : Content(arFactory, akrSpec, au32Ident),
      mLoader(),
      miAemsBankHandle(0),
      mbRemoveBegun(false),
      mpAemsData(0)
{
}

bool AemsContent::DoLoad()
{
    return mLoader.Load(*this, GetContentSpec());
}

bool AemsContent::DoUnload()
{
    return mLoader.Unload(*this, GetContentSpec());
}

void AemsContent::DoUpdate(f32 /*af32Dt*/)
{
    mLoader.Update(*this, GetContentSpec());
}

void* AemsContent::DoGetData()
{
    void* lpData = mLoader.GetData();
    CGS_ASSERT(lpData != 0, "lpvData");
    return lpData;
}

void* AemsContent::AddAemsBankCallback(void* apData, int /*aiResidentSize*/,
                                       int /*aiTotalSize*/)
{
    return apData;
}

bool AemsContent::DoOnPostLoad()
{
    void* lpData = Content::GetData(E_CONTENT_STATE_LOADING);
    CGS_ASSERT(mpAemsData == 0, "0 == mpAemsData");
    mpAemsData = lpData;
    CGS_ASSERT(mpAemsData != 0, "Ran out of memory trying to load some AEMS data.");

    miAemsBankHandle = SNDAEMS_addmodulebank(
        mpAemsData, 0, 0, &AemsContent::AddAemsBankCallback);
    CGS_ASSERT(miAemsBankHandle > 0, "miAemsBankHandle > 0");
    if (miAemsBankHandle <= 0)
    {
        mpAemsData = 0;
        return false;
    }
    mbRemoveBegun = false;
    return true;
}

bool AemsContent::DoOnPreUnload()
{
    if (!mbRemoveBegun)
    {
        const s32 liResult = Snd9::Aems::BeginRemoveModuleBank(miAemsBankHandle);
        CGS_ASSERT(liResult == 0, "Snd9::RESULT_OK == lResult");
        mbRemoveBegun = true;
        return false;
    }

    if (!Snd9::Aems::IsModuleBankRemoved(miAemsBankHandle))
    {
        return false;
    }

    CGS_ASSERT(mpAemsData != 0, "mpAemsData");
    mpAemsData = 0;
    return true;
}

// ---------------------------------------------------------------------------
// AemsContentSlot::DoPlay  @ 0x826DAFF0
//   playerVoice.mu8PlaybackFlags |= 2;  return playerVoice.Play(param);
// ---------------------------------------------------------------------------
bool AemsContentSlot::DoPlay(const Slot& /*aSlot*/, PlayerVoice& aVoice,
                             Content& /*aContent*/, u32 au32Param)
{
    aVoice.SetPlaybackState(E_PLAYBACK_STATE_PLAYING); // lbz/ori 2/stb 0x80(playerVoice)
    return static_cast<AemsPlayerVoice&>(aVoice).Play(au32Param);
}

// ---------------------------------------------------------------------------
// AemsContentSlot::DoStop  @ 0x826DB008
//   return playerVoice.Stop();
// ---------------------------------------------------------------------------
bool AemsContentSlot::DoStop(const Slot& /*aSlot*/, PlayerVoice& aVoice, Content& /*aContent*/)
{
    return static_cast<AemsPlayerVoice&>(aVoice).Stop();
}

// ---------------------------------------------------------------------------
// AemsContentSlot::DoUpdatePlaying  @ 0x826DAFE8
//   return playerVoice.Update(dt);
// (X360 `mr r3,r6` forwards the PlayerVoice& as `this`; the float dt rides in f1.)
// ---------------------------------------------------------------------------
bool AemsContentSlot::DoUpdatePlaying(System* /*apSystem*/, const Slot& /*aSlot*/,
                                      PlayerVoice& aVoice, Content& /*aContent*/, f32 af32Dt)
{
    return static_cast<AemsPlayerVoice&>(aVoice).Update(af32Dt);
}

} // namespace Playback
} // namespace CgsSound
