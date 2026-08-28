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

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h" // GetDefaultRwacSystem + RwacLock
#include "rw/audio/core/PlugIn.h"           // System / PlugInRegistry (GetPlugInHandle)
#include "rw/audio/core/DecoderRegistry.h"  // DecoderRegistry (the Xas/Xas1 registrations)
#include "rw/audio/core/Xas1Dec.h"          // Xas1Dec::GetDecoderDesc
#include "rw/audio/core/XasDec.h"           // XasDec::GetDecoderDesc

namespace CgsSound
{
namespace Playback
{
    AemsRWSamplePlayer::~AemsRWSamplePlayer()
    {
        // All members are trivially destructible and the base dtor is empty: the only
        // emitted work is the vtable store + the scalar-deleting tail.
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

            // Orders 9-14: the six descriptor registrations -- FLAG-deferred (banner).
            mGainHandle       = 0;
            mPan2DHandle      = 0;
            mRouteHandle      = 0;
            mSndPlayer1Handle = 0;
            mRechannelHandle  = 0;
            mResampleHandle   = 0;

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
        void* /*apParams*/, int /*aiNumOutputs*/, const int* /*apOutputs*/,
        const char* /*apcName*/, int /*aiValue*/,
        const AemsPlayerInputAccessor* /*apAccessor*/)
    {
        return 0;
    }
    void AemsRWSampleFactory::Release()
    {
    }

    // ------------------------------------------------------------------------
    // FLAG (DEFER) -- the AemsRWSamplePlayer control-surface virtuals ("own
    // TUs" per the header). Mounting this TU emits the player vtable, which
    // demands link homes; no player is ever CONSTRUCTED until the AEMS
    // sample-player creation slice (CreateInstance above returns null), so each
    // is an honest unreachable no-op until its console body is reconstructed.
    // ------------------------------------------------------------------------
    void AemsRWSamplePlayer::Release() {}
    void AemsRWSamplePlayer::Pause() {}
    void AemsRWSamplePlayer::Unpause() {}
    void AemsRWSamplePlayer::SetInput(Snd9::IAemsSamplePlayer::InputSelector /*aeSelector*/,
                                      int /*aiValue*/) {}
    void AemsRWSamplePlayer::SetAzimuth(int /*aiAzimuth*/, int* /*apLegacyAzimuths*/) {}
    void AemsRWSamplePlayer::GetOutputs(int /*aiNumOutputs*/, int* /*apValues*/) {}
}
}
