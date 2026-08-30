// ============================================================================
// CgsAemsInterfaceImplementation.cpp -- CgsSound::Playback::AemsRWSamplePlayer dtor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826A2E80
//   (CgsSound::Playback::AemsRWSamplePlayer::`scalar deleting destructor')
//
//   *this = vtable;                  // off_820AB14C
//   if (a2 & 1) operator delete(this);
//
// The destructor reads NO data members -- every member of AemsRWSamplePlayer is a
// trivially-destructible pointer / float / double / enum / byte, and the base
// Snd9::IAemsSamplePlayer destructor is empty -- so the only work is the vtable store
// (which the compiler emits as part of running the destructor) plus the conditional
// operator delete (the scalar-deleting thunk). Defining the class destructor
// out-of-line emits exactly that sequence; the body is empty.
//
// NOTE: this TU owns the destructor only. The ctor and the Release/Pause/Unpause/
// SetInput/SetAzimuth/GetOutputs overrides are their own (not-yet-done) TUs and
// resolve at consolidation against this shared home.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsInterfaceImplementation.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsPlayerVoice.h"
#include "GameShared/GameClasses/Sound/Playback/CgsEnvironment.h"

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h" // GetDefaultRwacSystem + RwacLock
#include "rw/audio/core/PlugIn.h"           // System / PlugInRegistry (GetPlugInHandle)
#include "rw/audio/core/DecoderRegistry.h"  // DecoderRegistry (the Xas/Xas1 registrations)
#include "rw/audio/core/Xas1Dec.h"          // Xas1Dec::GetDecoderDesc
#include "rw/audio/core/XasDec.h"           // XasDec::GetDecoderDesc
#include "rw/audio/core/Gain.h"             // the four live descriptor registrations
#include "rw/audio/core/plugins/Pan2D.h"
#include "rw/audio/core/Rechannel.h"
#include "rw/audio/core/Resample.h"         // LIVE with the phase-E callback wave
#include "rw/audio/core/Route.h"
#include "rw/audio/core/Send.h"
#include "rw/audio/core/SubMix.h"
#include "rw/audio/core/plugins/SndPlayer1.h"
#include "rw/audio/core/Voice.h"

#include <algorithm>
#include <cstring>
#include <new>

namespace CgsSound
{
namespace Playback
{
    AemsRWSamplePlayer::~AemsRWSamplePlayer()
    {
        // All members are trivially destructible and the base dtor is empty: the only
        // emitted work is the vtable store + the scalar-deleting tail.
    }

    AemsRWSamplePlayer::AemsRWSamplePlayer(
        Environment& arEnvironment, AemsPlayerVoice* apPlayerVoice)
        : mEnvironment(arEnvironment),
          mpPlayerVoice(apPlayerVoice),
          mpNext(0),
          mpRwacSystem(0),
          mpVoice(0),
          mpSndPlayer(0),
          mpResample(0),
          mpSendWet(0),
          mpGain(0),
          mfPreviousPitch(1.0f),
          mfPreviousDry(1.0f),
          mfPreviousWet(0.0f),
          mPitch(1.0f),
          mVol(1.0f),
          mDryLevel(1.0f),
          mWetLevel(0.0f),
          mRequestHandle(-1.0f),
          mSampleLength(0.0),
          mPauseState(PAUSESTATE_UNPAUSED),
          mNumChannels(0),
          mNumPannerVoices(0)
    {
        std::memset(mpPannerVoice, 0, sizeof(mpPannerVoice));
        std::memset(mpPan2D, 0, sizeof(mpPan2D));
        std::memset(mafPreviousAzimuths, 0, sizeof(mafPreviousAzimuths));
        CGS_ASSERT(mpPlayerVoice, "mpPlayerVoice");
        mpPlayerVoice->AddSamplePlayer(this);
    }

    // ------------------------------------------------------------------------
    // AemsRWSampleFactory ctor @ 0x826C26B8  (AEMS-cascade slice 2; the full
    // store-order decode is progress/scratch_dossiers/aems_factory_cascade_codex.md).
    // Console order: provisional IAems vptr, Factory base at overall +4 (its
    // AddFactory registers the SUBOBJECT pointer), final vptr pair (host:
    // compiler-emitted), then -- under the locked global system -- the plug-in
    // registry cache, six descriptor registrations, two handle lookups, the
    // Xas/Xas1 decoder registrations, the unlock, and the three config records.
    //
    // FLAG [the descriptor-record deferral, the RWAC-ctor precedent]: the six
    // RegisterPlugInRunTime calls (orders 9-14: Gain @0x82B97350 -> +0x3C,
    // Pan2D @0x82B984E8 -> +0x40, Route @0x82B9B258 -> +0x44, SndPlayer1
    // @0x82B9BE60 -> +0x4C, Rechannel @0x82B9A718 -> +0x50, Resample
    // @0x82B9A850 -> +0x54) CANNOT run: every vendor getter still returns a
    // placeholder single-pointer static, and registering one scribbles the
    // record's link fields over neighbouring globals (measured on the RWAC
    // pass). The handle members stay 0 until the PlugInDescRunTime records are
    // built host-side; GetPlugInHandle on the empty registry likewise returns
    // null into the two lookup members -- every consumer null-checks through
    // its guarded failure path.
    // ------------------------------------------------------------------------
    AemsRWSampleFactory::AemsRWSampleFactory(Name aName, Environment& arEnvironment)
        : Factory(aName, arEnvironment)
    {
        rw::audio::core::System* lpSystem = GetDefaultRwacSystem();  // off_83271928, loaded directly
        {
            RwacLock lLock(lpSystem);   // console System::Lock before the registry work

            mpPlugInRegistry = rw::audio::core::System::GetPlugInRegistry(lpSystem);

            // Orders 9-14: the six descriptor registrations. FOUR are LIVE
            // (descriptor-record wave: real host records; RegisterPlugInRunTime
            // dupe-detects against the RWAC pass and hands back the existing
            // node, exactly the console behaviour). Route (record un-recovered)
            // and SndPlayer1 (no PC home) stay FLAG-deferred at 0.
            // Resample went LIVE with the phase-E callback wave 2026-08-28 (its
            // Process @0x82B9F3E8 and the raw-decoded LinearInterpolate
            // @0x82B9A918 are bodied); because the RWAC pass registers the same
            // record first, this call takes the console's dupe-detect path and
            // returns that existing node -- which is exactly the handle this
            // member is supposed to hold.
            mGainHandle = reinterpret_cast<uintptr_t>(
                rw::audio::core::PlugInRegistry::RegisterPlugInRunTime(mpPlugInRegistry,
                    reinterpret_cast<rw::audio::core::PlugInDescRunTime*>(
                        rw::audio::core::Gain::GetPlugInDescRunTime())));       // @0x82B97350 -> +0x3C
            mPan2DHandle = reinterpret_cast<uintptr_t>(
                rw::audio::core::PlugInRegistry::RegisterPlugInRunTime(mpPlugInRegistry,
                    reinterpret_cast<rw::audio::core::PlugInDescRunTime*>(
                        rw::audio::core::Pan2D::GetPlugInDescRunTime())));      // @0x82B984E8 -> +0x40
            mRouteHandle = reinterpret_cast<uintptr_t>(
                rw::audio::core::PlugInRegistry::RegisterPlugInRunTime(mpPlugInRegistry,
                    reinterpret_cast<rw::audio::core::PlugInDescRunTime*>(
                        rw::audio::core::Route::GetPlugInDescRunTime())));     // @0x82B9B258 -> +0x44
            mSndPlayer1Handle = reinterpret_cast<uintptr_t>(
                rw::audio::core::PlugInRegistry::RegisterPlugInRunTime(mpPlugInRegistry,
                    reinterpret_cast<rw::audio::core::PlugInDescRunTime*>(
                        rw::audio::core::SndPlayer1::GetPlugInDescRunTime()))); // @0x82B9BE60 -> +0x4C
            mRechannelHandle = reinterpret_cast<uintptr_t>(
                rw::audio::core::PlugInRegistry::RegisterPlugInRunTime(mpPlugInRegistry,
                    reinterpret_cast<rw::audio::core::PlugInDescRunTime*>(
                        rw::audio::core::Rechannel::GetPlugInDescRunTime())));  // @0x82B9A718 -> +0x50
            mResampleHandle = reinterpret_cast<uintptr_t>(
                rw::audio::core::PlugInRegistry::RegisterPlugInRunTime(mpPlugInRegistry,
                    reinterpret_cast<rw::audio::core::PlugInDescRunTime*>(
                        rw::audio::core::Resample::GetPlugInDescRunTime())));   // @0x82B9A850 -> +0x54

            // Orders 15-16: the two handle lookups ('Sen0' 0x53656E30 -> +0x48,
            // 'Sub0' 0x53756230 -> +0x58). Read-only walks; null on the
            // still-empty registry.
            mSendHandle = reinterpret_cast<uintptr_t>(
                rw::audio::core::PlugInRegistry::GetPlugInHandle(mpPlugInRegistry, 0x53656E30));
            mSubMixHandle = reinterpret_cast<uintptr_t>(
                rw::audio::core::PlugInRegistry::GetPlugInHandle(mpPlugInRegistry, 0x53756230));

            // The decoder registrations (real typed records): XasDec @0x82B91E80
            // then Xas1Dec @0x82B91E90 through RegisterDecoder @0x82B67CB0.
            rw::audio::core::DecoderRegistry* lpDecoderRegistry =
                rw::audio::core::System::GetDecoderRegistry(lpSystem);
            rw::audio::core::DecoderRegistry::RegisterDecoder(
                lpDecoderRegistry, rw::audio::core::XasDec::GetDecoderDesc());
            rw::audio::core::DecoderRegistry::RegisterDecoder(
                lpDecoderRegistry, rw::audio::core::Xas1Dec::GetDecoderDesc());
        }   // ~RwacLock == the console unlock

        // Orders 17-25: the three plug-in config records, exact console store
        // values -- {0, SubMix handle, 1ch}, {0, Pan2D handle, 6ch},
        // {0, Send handle, 6ch}. (The console's +0x18/+0x24/+0x30 handle loads
        // reload the members stored above.)
        maAemsSubMixPlugInConfig[0].mpInitialValue  = 0;
        maAemsSubMixPlugInConfig[0].muPlugInHandle  = mSubMixHandle;
        maAemsSubMixPlugInConfig[0].mu8ChannelCount = 1;
        maAemsSubMixPlugInConfig[1].mpInitialValue  = 0;
        maAemsSubMixPlugInConfig[1].muPlugInHandle  = mPan2DHandle;
        maAemsSubMixPlugInConfig[1].mu8ChannelCount = 6;
        maAemsSubMixPlugInConfig[2].mpInitialValue  = 0;
        maAemsSubMixPlugInConfig[2].muPlugInHandle  = mSendHandle;
        maAemsSubMixPlugInConfig[2].mu8ChannelCount = 6;
    }

    // ------------------------------------------------------------------------
    // FLAG (DEFER) -- the two IAems-surface virtuals (own console bodies:
    // CreateInstance @0x826C28A0, Release with it). Emitting the class vtable
    // (the ctor above) demands link homes; each is an honest null/no-op until
    // the sample-player creation slice lands (its consumer is the Snd9 AEMS
    // layer's per-sample voice bring-up -- content-gated, nothing reaches it
    // before the content campaign).
    // ------------------------------------------------------------------------
    Snd9::IAemsSamplePlayer* AemsRWSampleFactory::CreateInstance(
        void* apParams, int aiNumOutputs, const int* apOutputs,
        const char* apcName, int aiValue,
        const Snd9::AemsPlayerInputAccessor* apAccessor)
    {
        CGS_ASSERT(apParams, "apParams");
        CGS_ASSERT(apAccessor, "apAccessor");
        if (!apParams || !apAccessor)
            return 0;

        Handle<Voice> lhVoice = GetEnvironment().GetVoice(
            static_cast<u32>(apAccessor->GetValueByType(
                Snd9::IAemsSamplePlayer::PLAYER_INPUT_USER_FIRST)));
        Voice* lpBaseVoice = lhVoice.GetObject();
        if (!lpBaseVoice)
            return 0;
        AemsPlayerVoice* lpPlayerVoice =
            static_cast<AemsPlayerVoice*>(lpBaseVoice);

        rw::audio::core::System* lpSystem = GetDefaultRwacSystem();
        void* lpMemory = rw::audio::core::System::Alloc(
            lpSystem, static_cast<u32>(sizeof(AemsRWSamplePlayer)),
            "AemsRWSamplePlayer", 16, 0);
        if (!lpMemory)
        {
            lpBaseVoice->Release();
            return 0;
        }

        AemsRWSamplePlayer* lpPlayer = ::new (lpMemory)
            AemsRWSamplePlayer(GetEnvironment(), lpPlayerVoice);
        lpPlayer->mpRwacSystem = lpSystem;

        rw::audio::core::SndPlayer1::FileInfo lFileInfo = {};
        rw::audio::core::SndPlayer1::GetFileInfo(apParams, &lFileInfo);
        lpPlayer->mNumChannels = lFileInfo.numChannels;
        if (lFileInfo.sampleRate)
            lpPlayer->mSampleLength = static_cast<f64>(lFileInfo.numSamples) /
                                      static_cast<f64>(lFileInfo.sampleRate);
        CGS_ASSERT(lpPlayer->mNumChannels >= 1 &&
                   lpPlayer->mNumChannels <= AemsRWSamplePlayer::KU_MAX_SAMPLE_CHANNELS,
                   "mNumChannels");
        if (lpPlayer->mNumChannels < 1 ||
            lpPlayer->mNumChannels > AemsRWSamplePlayer::KU_MAX_SAMPLE_CHANNELS)
        {
            lpPlayer->Release();
            lpBaseVoice->Release();
            return 0;
        }

        f32 lfOne = 1.0f;
        rw::audio::core::VoiceStageConfig laMain[10] = {};
        u32 luStage = 0;
        const u32 luChannels = lpPlayer->mNumChannels;
        auto AddStage = [&](void* apContext, uintptr_t auHandle,
                            u32 auOutputChannels)
        {
            laMain[luStage].mpContext = apContext;
            laMain[luStage].mpDesc = reinterpret_cast<
                rw::audio::core::PlugInDescRunTime*>(auHandle);
            laMain[luStage].mFlagAndField8 = auOutputChannels;
            ++luStage;
        };

        AddStage(&lfOne, mSndPlayer1Handle, luChannels);
        AddStage(0, mRechannelHandle, luChannels);
        AddStage(0, mResampleHandle, luChannels);
        AddStage(0, mGainHandle, luChannels);
        if (luChannels == 1)
        {
            AddStage(0, mPan2DHandle, 6);
            AddStage(0, mSendHandle, 6);
        }
        else
        {
            for (u32 luChannel = 0; luChannel < luChannels; ++luChannel)
                AddStage(0, mRouteHandle, 1);
        }

        rw::audio::core::PlugIn** lppMainPlugins = 0;
        lpPlayer->mpVoice = rw::audio::core::Voice::CreateInstance(
            0, static_cast<int>(luStage), laMain, &lppMainPlugins, lpSystem);
        if (!lpPlayer->mpVoice)
        {
            lpPlayer->Release();
            lpBaseVoice->Release();
            return 0;
        }

        lpPlayer->mpSndPlayer = lppMainPlugins[0];
        lpPlayer->mpResample = lppMainPlugins[2];
        lpPlayer->mpGain = lppMainPlugins[3];
        rw::audio::core::PlugIn::SetAttribute(lpPlayer->mpResample, 0, 1.0f);
        rw::audio::core::PlugIn::SetAttribute(lpPlayer->mpGain, 0, 1.0f);
        rw::audio::core::Voice::SetPriority(
            lpPlayer->mpVoice, static_cast<f32>(aiNumOutputs) * 0.01f);

        rw::audio::core::SubMix* lpInternalSubmix =
            reinterpret_cast<rw::audio::core::SubMix*>(
                lpPlayerVoice->GetInternalSubmix());
        if (luChannels == 1)
        {
            lpPlayer->mNumPannerVoices = 1;
            lpPlayer->mpPan2D[0] = lppMainPlugins[4];
            lpPlayer->mpSendWet = lppMainPlugins[5];
            if (apOutputs)
            {
                const f32 lfAzimuth = static_cast<f32>(apOutputs[0]) *
                                      0.0054931641f;
                rw::audio::core::PlugIn::SetAttribute(
                    lpPlayer->mpPan2D[0], 0, lfAzimuth);
                lpPlayer->mafPreviousAzimuths[0] = lfAzimuth;
            }
            void* lapTarget[1] = { lpInternalSubmix };
            rw::audio::core::Send::EventEvent(
                reinterpret_cast<rw::audio::core::Send*>(lpPlayer->mpSendWet),
                0, lapTarget);
        }
        else
        {
            lpPlayer->mNumPannerVoices = static_cast<u8>(
                luChannels == 6 ? 5 : luChannels);
            for (u32 luChannel = 0;
                 luChannel < lpPlayer->mNumPannerVoices; ++luChannel)
            {
                rw::audio::core::VoiceStageConfig laPanner[3] = {};
                for (u32 luConfig = 0; luConfig < 3; ++luConfig)
                {
                    laPanner[luConfig].mpContext =
                        maAemsSubMixPlugInConfig[luConfig].mpInitialValue;
                    laPanner[luConfig].mpDesc = reinterpret_cast<
                        rw::audio::core::PlugInDescRunTime*>(
                            maAemsSubMixPlugInConfig[luConfig].muPlugInHandle);
                    laPanner[luConfig].mFlagAndField8 =
                        maAemsSubMixPlugInConfig[luConfig].mu8ChannelCount;
                }
                rw::audio::core::PlugIn** lppPannerPlugins = 0;
                lpPlayer->mpPannerVoice[luChannel] =
                    rw::audio::core::Voice::CreateInstance(
                        1, 3, laPanner, &lppPannerPlugins, lpSystem);
                if (!lpPlayer->mpPannerVoice[luChannel])
                    continue;
                lpPlayer->mpPan2D[luChannel] = lppPannerPlugins[1];
                if (apOutputs)
                {
                    const f32 lfAzimuth = static_cast<f32>(apOutputs[luChannel]) *
                                          0.0054931641f;
                    rw::audio::core::PlugIn::SetAttribute(
                        lpPlayer->mpPan2D[luChannel], 0, lfAzimuth);
                    lpPlayer->mafPreviousAzimuths[luChannel] = lfAzimuth;
                }
                void* lapTarget[1] = { lpInternalSubmix };
                rw::audio::core::Send::EventEvent(
                    reinterpret_cast<rw::audio::core::Send*>(lppPannerPlugins[2]),
                    0, lapTarget);

                rw::audio::core::RouteConnectEvent lRoute = {};
                lRoute.mpSubMix = reinterpret_cast<rw::audio::core::SubMix*>(
                    lppPannerPlugins[0]);
                lRoute.mfGain0 = static_cast<f32>(luChannel);
                lRoute.mfGain1 = 0.0f;
                lRoute.mfGain2 = 1.0f;
                rw::audio::core::Route::EventEvent(
                    reinterpret_cast<rw::audio::core::Route*>(
                        lppMainPlugins[4 + luChannel]), 0, &lRoute);
            }
            if (luChannels == 6)
            {
                rw::audio::core::RouteConnectEvent lRoute = {};
                lRoute.mpSubMix = lpInternalSubmix;
                lRoute.mfGain0 = 5.0f;
                lRoute.mfGain1 = 5.0f;
                lRoute.mfGain2 = 1.0f;
                rw::audio::core::Route::EventEvent(
                    reinterpret_cast<rw::audio::core::Route*>(lppMainPlugins[9]),
                    0, &lRoute);
            }
        }

        rw::audio::core::SndPlayer1::PlayLegacyParams lPlay = {};
        lPlay.startTime = 0.0;
        lPlay.streamFileOffset = static_cast<f64>((std::max)(aiValue, 0));
        lPlay.pStreamFilePath = apcName;
        lPlay.pRamData = apParams;
        lPlay.streamPoolGuid = 0;
        lPlay.expelMode = 1.0f;
        rw::audio::core::PlugIn::Event(lpPlayer->mpSndPlayer, 0, &lPlay);
        lpPlayer->mRequestHandle = lPlay.requestHandle;

        lpBaseVoice->Release();
        return lpPlayer;
    }
    // The console body @0x8284CB38 is a bare `blr` (raw 4E 80 00 20) -- an ICF-shared
    // empty function. This empty body is therefore the FAITHFUL one, not a stub:
    // verified 2026-08-28 by the AEMS control-surface decode, which resolved the
    // AemsRWSampleFactory vtable and read the slot's bytes directly.
    void AemsRWSampleFactory::Release()
    {
    }

    // ------------------------------------------------------------------------
    // FLAG (DEFER) -- the AemsRWSamplePlayer control-surface virtuals ("own
    // TUs" per the header). Mounting this TU emits the player vtable, which
    // demands link homes; no player is ever CONSTRUCTED until the AEMS
    // sample-player creation slice (CreateInstance above returns null), so each
    // is an honest unreachable no-op until its console body is reconstructed.
    //
    // ⭐ ALL SIX ARE NOW LOCATED (2026-08-28 AEMS control-surface decode). The
    // player vtable is off_820AD960 and every slot has a REAL console body:
    //     Release     @0x826A2EC8
    //     Pause       @0x8268A970
    //     Unpause     @0x8268AA18
    //     SetInput    @0x8268A180
    //     SetAzimuth  @0x8268A510
    //     GetOutputs  @0x8268A7E8
    // The load-bearing shape is also resolved: these drive the underlying voice's
    // plug-in graph through PlugIn::SetAttribute -- SetInput writes Resample /
    // Gain / Send attribute 0, SetAzimuth writes Pan2D attribute 0, and
    // GetOutputs reads SndPlayer1 attributes 0/1/2. (Notably there is NO Pan2D1
    // path, so an AEMS player pans through Pan2D, not the newer shape.)
    // They stay unbodied only because nothing constructs a player yet; body them
    // with the AEMS sample-player creation slice, when SndPlayer1 lands.
    // ------------------------------------------------------------------------
    void AemsRWSamplePlayer::Release()
    {
        if (mpPlayerVoice)
            mpPlayerVoice->RemoveSamplePlayer(this);
        if (mpVoice)
            rw::audio::core::Voice::Release(mpVoice);
        for (u32 luVoice = 0; luVoice < mNumPannerVoices; ++luVoice)
        {
            if (mpPannerVoice[luVoice])
                rw::audio::core::Voice::Release(mpPannerVoice[luVoice]);
        }
        rw::audio::core::System* lpSystem = mpRwacSystem;
        this->~AemsRWSamplePlayer();
        rw::audio::core::System::Free(lpSystem, this, 0);
    }

    void AemsRWSamplePlayer::Pause()
    {
        CGS_ASSERT(mPauseState == PAUSESTATE_UNPAUSED, "UNPAUSED == mPauseState");
        rw::audio::core::PlugIn::SetAttribute(mpGain, 0, 0.0f);
        if (mpSendWet)
            rw::audio::core::PlugIn::SetAttribute(mpSendWet, 0, 0.0f);
        rw::audio::core::PlugIn::SetAttribute(mpResample, 0, 0.0f);
        mPauseState = PAUSESTATE_PAUSED;
    }

    void AemsRWSamplePlayer::Unpause()
    {
        CGS_ASSERT(mPauseState == PAUSESTATE_PAUSED, "PAUSED == mPauseState");
        rw::audio::core::PlugIn::SetAttribute(mpGain, 0, mDryLevel * mVol);
        if (mpSendWet)
            rw::audio::core::PlugIn::SetAttribute(mpSendWet, 0, mWetLevel * mVol);
        rw::audio::core::PlugIn::SetAttribute(mpResample, 0, mPitch);
        mPauseState = PAUSESTATE_UNPAUSED;
    }

    void AemsRWSamplePlayer::SetInput(
        Snd9::IAemsSamplePlayer::InputSelector aeSelector, int aiValue)
    {
        switch (aeSelector)
        {
        case Snd9::IAemsSamplePlayer::PLAYER_INPUT_PITCHMULT:
            mPitch = static_cast<f32>(aiValue) * 0.00024414062f;
            rw::audio::core::PlugIn::SetAttribute(mpResample, 0, mPitch);
            break;
        case Snd9::IAemsSamplePlayer::PLAYER_INPUT_VOL:
            mVol = static_cast<f32>(aiValue) * 0.000030518509f;
            rw::audio::core::PlugIn::SetAttribute(mpGain, 0, mDryLevel * mVol);
            if (mpSendWet)
                rw::audio::core::PlugIn::SetAttribute(
                    mpSendWet, 0, mWetLevel * mVol);
            break;
        case Snd9::IAemsSamplePlayer::PLAYER_INPUT_AZIMUTH:
            CGS_ASSERT(false, "Azimuth uses SetAzimuth");
            break;
        case Snd9::IAemsSamplePlayer::PLAYER_INPUT_FXWET0:
            mWetLevel = static_cast<f32>(aiValue) * 0.000030518509f;
            if (mpSendWet)
                rw::audio::core::PlugIn::SetAttribute(
                    mpSendWet, 0, mWetLevel * mVol);
            break;
        case Snd9::IAemsSamplePlayer::PLAYER_INPUT_DRYLEVEL:
            mDryLevel = static_cast<f32>(aiValue) * 0.000030518509f;
            rw::audio::core::PlugIn::SetAttribute(
                mpGain, 0, mDryLevel * mVol);
            break;
        default:
            break;
        }
    }

    void AemsRWSamplePlayer::SetAzimuth(int aiAzimuth, int* apLegacyAzimuths)
    {
        if (!apLegacyAzimuths)
            return;
        int* lpAzimuth = apLegacyAzimuths + 3 * mNumChannels - 6;
        for (u32 luVoice = 0; luVoice < mNumPannerVoices; ++luVoice)
        {
            const f32 lfAzimuth = static_cast<f32>(
                static_cast<u32>(lpAzimuth[luVoice] + aiAzimuth) & 0xFFFFu) *
                0.0054931641f;
            if (lfAzimuth != mafPreviousAzimuths[luVoice])
            {
                rw::audio::core::PlugIn::SetAttribute(
                    mpPan2D[luVoice], 0, lfAzimuth);
                mafPreviousAzimuths[luVoice] = lfAzimuth;
            }
        }
    }

    void AemsRWSamplePlayer::GetOutputs(int aiNumOutputs, int* apValues)
    {
        if (!apValues || aiNumOutputs <= 0)
            return;
        std::memset(apValues, 0, sizeof(int) * aiNumOutputs);
        if (mpVoice && mpVoice->mucState != 2)
            apValues[0] = 1;
        if (aiNumOutputs < 3 || !mpSndPlayer)
            return;

        f32 lfCurrentRequest = 0.0f;
        rw::audio::core::PlugIn::GetAttribute(mpSndPlayer, 0,
                                               &lfCurrentRequest);
        f64 ldPosition = 0.0;
        f64 ldLength = mSampleLength;
        if (lfCurrentRequest >= mRequestHandle)
        {
            const rw::audio::core::SndPlayer1* lpSndPlayer =
                static_cast<const rw::audio::core::SndPlayer1*>(mpSndPlayer);
            ldPosition = lpSndPlayer->GetSamplePositionAttribute();
            ldLength = lpSndPlayer->GetSampleLengthAttribute();
        }
        apValues[1] = static_cast<int>((ldLength - ldPosition) * 1000.0);
        apValues[2] = static_cast<int>(ldPosition * 1000.0);
    }
}
}
