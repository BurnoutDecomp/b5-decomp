#include "GameShared/GameClasses/Sound/Playback/Splicer/internal/SpliceObjects.h"

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h" // RwacSystemLock, GetDefaultRwacSystem

#include <math.h>   // cos, fabsf
#include <stdlib.h> // rand

// The audio-core System unlock engine entry (X360 rw::audio::core::System::Unlock),
// declared BY NAME here exactly as CgsEnvironment.cpp does (its lock twin lives in
// CgsGenericRwacFactory.h); bodied in the RWAC System TU.
namespace rw { namespace audio { namespace core {
    void RwacSystemUnlock( System* apSystem );
} } }

// ============================================================================
// SpliceObjects.cpp -- runtime splice / splice-sample playback objects.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for
// every offset, side-effect and branch. Layout/type shape is DecFIGS DWARF gated on
// the X360 ledger (see SpliceObjects.h). The global SpliceManager instance is
// gpSpliceManager (X360 off_82FFB9F0); the process-wide RWAC System is fetched through
// CgsSound::Playback::GetDefaultRwacSystem (X360 off_83271928).
// ============================================================================

// SpliceObjects.cpp:8. A sample whose SPLICE_SampleRef azimuth equals this sentinel is a
// stereo sample (drawn from the stereo voice pool). X360 flt_820AA7B8.
static const f32 KI_STEREO_SAMPLE_AZIMUTH = -127.0f;

// The RWAC near-zero epsilon (2^-23 == FLT_EPSILON) the X360 uses to test a float against
// zero: |v| <= eps. Grounded in the sign-window compares in Play / GetEnvelopeVolume
// (flt_820AA114 = +eps, flt_82002514 = -eps).
static bool IsApproxZero( f32 afValue )
{
    return afValue <= 0.00000011920929f && afValue >= -0.00000011920929f;
}

// ============================================================================
// SpliceSample::IsPlaying @ 0x8268B128
//
// A sample is playing if it is still waiting to trigger (mtTimeToTrigger > 0) or, once a
// voice exists, that voice's state is not "expelled" (state != 2). Reconstructed store-
// for-store from the X360 branch lattice.
// ============================================================================
bool SpliceSample::IsPlaying()
{
    if ( mpVoicePluginPair == 0 )
    {
        // No voice yet: only the pending trigger keeps it alive.
        if ( mtTimeToTrigger < 0.0f )
            return false;
        if ( mtTimeToTrigger > 0.0f )
            return true;
        return false;
    }

    // A voice pair exists: a pending trigger still counts as playing.
    if ( mtTimeToTrigger > 0.0f )
        return true;

    rw::audio::core::Voice* lpVoice = mpVoicePluginPair->mpVoice;
    if ( lpVoice == 0 )
        return false;

    // Playing while the voice has not reached the expelled state (mucState == 2).
    return lpVoice->mucState != 2;
}

// ============================================================================
// SpliceSample::GetEnvelopeVolume @ 0x8268B000
//
// Apply the sample's raised-cosine attack/decay envelope to arfVol in place. During the
// attack window (0 .. mfEnvAttack) the volume ramps in; past mfEnvDecay it ramps out and
// is clamped at 0. A zero attack/decay time (within FLT_EPSILON) disables that phase.
// ============================================================================
void SpliceSample::GetEnvelopeVolume( f32& arfVol )
{
    SPLICE_SampleRef* lpData = mpData;

    if ( !IsApproxZero( lpData->mfEnvAttack ) && mtTimeIntoPlayback < lpData->mfEnvAttack )
    {
        // Attack ramp.
        arfVol = arfVol * ( 1.0f - (f32)cos( mtTimeIntoPlayback / lpData->mfEnvAttack ) );
    }
    else if ( !IsApproxZero( lpData->mfEnvDecay ) && mtTimeIntoPlayback > lpData->mfEnvDecay )
    {
        // Decay ramp, clamped to >= 0.
        f32 lfPhase = ( mtTimeIntoPlayback - lpData->mfEnvDecay )
                    / ( lpData->mfEnvLength - lpData->mfEnvDecay );
        arfVol = arfVol * ( 1.0f - (f32)cos( 1.0f - lfPhase ) );
        if ( arfVol < 0.0f )
            arfVol = 0.0f;
    }
}

// ============================================================================
// SpliceSample::Play @ 0x8268AE78
//
// Latch the randomised per-play volume and pitch, then either start the voice immediately
// or arm a trigger delay. Two rand() draws feed: a volume curve (biased by mfVolRandom)
// and a pitch offset (mfBasePitch + rand*mfPitchRandom). Only the incoming pitch (afPitch)
// scales mLastPitch; the other passed values are applied later in Update.
// ============================================================================
void SpliceSample::Play( f32 /*afVol*/, f32 afPitch, f32 /*afAzimuth*/, f32 /*afSpread*/ )
{
    SPLICE_SampleRef* lpData = mpData;

    // rand() normalised to [-1, 1] (rand/16384 - 1).
    f32 lfNormRand = (f32)rand() * 0.000061037019f - 1.0f;

    mLocVol = 1.0f;

    f32 lfVolRandom = lpData->mfVolRandom;
    f32 lfVol;
    if ( lfNormRand <= 0.0f )
    {
        lfVol = lfNormRand * ( 1.0f - lfVolRandom ) + 1.0f;
    }
    else if ( IsApproxZero( lfVolRandom ) )
    {
        lfVol = 4.0f;
    }
    else
    {
        lfVol = ( 1.0f / lfVolRandom - 1.0f ) * lfNormRand + 1.0f;
    }
    mLocVol = lfVol;

    // Pitch: base + rand[0,1)*random, then the last-applied pitch scaled by the caller pitch.
    f32 lfPitch = ( (f32)rand() * 0.000030518509f ) * lpData->mfPitchRandom + lpData->mfBasePitch;
    mLocPitch  = lfPitch;
    mLastPitch = lfPitch * afPitch;

    if ( IsApproxZero( lpData->mfTriggerDelay ) )
    {
        StartVoice();
        mtTimeToTrigger = -1.0f;   // -1 == already triggered
    }
    else
    {
        mtTimeToTrigger = lpData->mfTriggerDelay;
    }
}

// ============================================================================
// SpliceSample::FreeVoice @ 0x826A3870
//
// Release the sample's voice pair (if any) under the audio-core System lock. A
// dynamically-created voice is destroyed outright; a pooled voice is returned to its
// stereo or mono pool depending on whether the sample's azimuth is the stereo sentinel.
// ============================================================================
void SpliceSample::FreeVoice()
{
    if ( mpVoicePluginPair )
    {
        rw::audio::core::System* lpSystem = CgsSound::Playback::GetDefaultRwacSystem();
        rw::audio::core::RwacSystemLock( lpSystem );

        if ( mbUsingDynamicVoice )
        {
            gpSpliceManager->DestroyVoice( mpVoicePluginPair );
        }
        else if ( fabsf( mpData->mfAzimuth - KI_STEREO_SAMPLE_AZIMUTH ) < 0.000015258789f )
        {
            gpSpliceManager->FreeStereoVoicePlugInPair( mpVoicePluginPair );
        }
        else
        {
            gpSpliceManager->FreeMonoVoicePlugInPair( mpVoicePluginPair );
        }

        mpVoicePluginPair = 0;
        mbStartVoice      = false;
        mpSubMix          = 0;

        rw::audio::core::RwacSystemUnlock( lpSystem );
    }
}

// ============================================================================
// StereoSpliceSample::StartVoice @ 0x826A3F98
//
// Acquire a stereo voice pair for this sample: try the stereo pool first, and on
// exhaustion fall back to a dynamically-created stereo voice (under the System lock).
// Then cache the send / resample plug-in stages from the acquired chain (a stereo sample
// has no panning stage). Asserts, via the SpliceManager sink, on already-having-a-voice,
// dynamic-create failure, and a missing pair.
// ============================================================================
void StereoSpliceSample::StartVoice()
{
    mtTimeIntoPlayback = 0.0f;

    if ( mpVoicePluginPair )
    {
        SpliceManager::AssertCallbackFunc lpfnAssert = gpSpliceManager->GetAssertCallbackFunction();
        if ( lpfnAssert )
            lpfnAssert( "StereoSpliceSample already has a voice" );
    }

    mpVoicePluginPair = gpSpliceManager->AllocateStereoVoicePluginPair( this );

    if ( !mpVoicePluginPair )
    {
        rw::audio::core::System* lpSystem = CgsSound::Playback::GetDefaultRwacSystem();
        rw::audio::core::RwacSystemLock( lpSystem );

        mpVoicePluginPair = &mDynamicVoicePluginPair;
        gpSpliceManager->CreateStereoVoice( &mDynamicVoicePluginPair );

        if ( !mpVoicePluginPair->mpVoice )
        {
            SpliceManager::AssertCallbackFunc lpfnAssert = gpSpliceManager->GetAssertCallbackFunction();
            if ( lpfnAssert )
                lpfnAssert( "Failed to dynamically create stereo voice" );
        }

        mbUsingDynamicVoice = true;
        rw::audio::core::RwacSystemUnlock( lpSystem );
    }

    if ( !mpVoicePluginPair )
    {
        SpliceManager::AssertCallbackFunc lpfnAssert = gpSpliceManager->GetAssertCallbackFunction();
        if ( lpfnAssert )
            lpfnAssert( "No plugin pair" );
    }

    // Cache the plug-in stages from the acquired chain (send = [2], resample = [1]); a
    // stereo sample uses no panning stage.
    SpliceManager::VoicePluginPair* lpPair = mpVoicePluginPair;
    mpSendPlugIn     = lpPair->mppPlugIn[2];
    mpPanningPlugIn  = 0;
    mbStartVoice     = true;
    mpResamplePlugIn = lpPair->mppPlugIn[1];
}

// ============================================================================
// Splice::operator new @ 0x826C33A0
//
// Carve a Splice out of the SpliceManager's splice heap, tagged "Splice". Asserts through
// the manager's sink on allocation failure and returns the (possibly null) block.
// ============================================================================
void* Splice::operator new( usize auSize )
{
    void* lpMemory = gpSpliceManager->Allocate( (u32)auSize, "Splice" );
    if ( !lpMemory )
    {
        SpliceManager::AssertCallbackFunc lpfnAssert = gpSpliceManager->GetAssertCallbackFunction();
        if ( lpfnAssert )
            lpfnAssert( "Failed to dynamically new splice" );
    }
    return lpMemory;
}

// ============================================================================
// Splice::operator delete @ 0x826A3798
//
// Return a Splice block to the SpliceManager heap (X360 inlines SpliceManager::Free -- the
// heap's polymorphic free slot). No-op on a null pointer.
// ============================================================================
void Splice::operator delete( void* apMemory )
{
    if ( apMemory )
        gpSpliceManager->Free( apMemory );
}

// ============================================================================
// Splice::Play @ 0x826A3698
//
// Kick every live sample: the master volume scale is applied to the incoming volume and
// forwarded (with pitch/azimuth/spread) to each SpliceSample::Play. The live count is
// re-read from the SPLICE_Data record each iteration, exactly as the X360 loops.
// ============================================================================
void Splice::Play( f32 afVol, f32 afPitch, f32 afAzimuth, f32 afSpread )
{
    SPLICE_Data* lpData = GetData();
    f32 lfVol = lpData->mfVolumeScale * afVol;

    for ( int li = 0; li < lpData->mucNumSampleRefs; ++li )
        mSampleRefs[li]->Play( lfVol, afPitch, afAzimuth, afSpread );
}

// ============================================================================
// Splice::Update @ 0x826DB470
//
// Advance every live sample; the master volume scale is applied to the incoming volume.
// A sample that has stopped playing is destructed (its base ~SpliceSample releases the
// voice) and its block returned to the SpliceManager heap, and its slot cleared.
// ============================================================================
void Splice::Update( rw::audio::core::System* apRwacSystem, f32 afVol, f32 afPitch,
                     f32 afAzimuth, f32 afTimeDelta, f32 afSpread )
{
    SPLICE_Data* lpData = GetData();
    f32 lfVol = lpData->mfVolumeScale * afVol;

    for ( int li = 0; li < lpData->mucNumSampleRefs; ++li )
    {
        SpliceSample* lpSample = mSampleRefs[li];
        if ( lpSample )
        {
            lpSample->Update( apRwacSystem, lfVol, afPitch, afAzimuth, afTimeDelta, afSpread );
            if ( !lpSample->IsPlaying() )
            {
                // X360 inlines the base ~SpliceSample (vptr reset to SpliceSample + FreeVoice)
                // then frees the block through the SpliceManager heap.
                lpSample->SpliceSample::~SpliceSample();
                gpSpliceManager->Free( lpSample );
                mSampleRefs[li] = 0;
            }
        }
    }
}

// ============================================================================
// Splice::IsPlaying @ 0x826A3720
//
// The splice is playing while any live sample is playing.
// ============================================================================
bool Splice::IsPlaying()
{
    const int liCount = mpData->mucNumSampleRefs;
    if ( liCount <= 0 )
        return false;

    for ( int li = 0; li < liCount; ++li )
    {
        if ( mSampleRefs[li] && mSampleRefs[li]->IsPlaying() )
            return true;
    }
    return false;
}

// ============================================================================
// Splice::GetCpuTicks @ 0x826A3808
//
// Sum the CPU-tick cost of every live sample's voice (X360 inlines SpliceSample::
// GetCpuTicks -- the voice's per-frame tick counter, 0 when there is no voice).
// ============================================================================
u32 Splice::GetCpuTicks()
{
    u32 luTotal = 0;
    const int liCount = mpData->mucNumSampleRefs;
    for ( int li = 0; li < liCount; ++li )
    {
        if ( mSampleRefs[li] )
            luTotal += mSampleRefs[li]->GetCpuTicks();
    }
    return luTotal;
}

// ============================================================================
// BLOCKED (this TU): SpliceSample::Update @ 0x826A3A50
//
// PARTIAL UNBLOCK (re-attempt): the sample-source lookup that once blocked this func is now
// homed. The X360 inline table math at 0x826A3D84..0x826A3DC4 resolves against the (now
// committed) SpliceManager::m_Splices[8] bank array (SpliceContainer @ guest +0x614, 20-byte
// stride): bankIdx = (s8)mpData->mcSpliceType, sampleId = mpData->muSampleId, and the source
// pointer = m_Splices[bankIdx].mpSampleData + m_Splices[bankIdx].mpTableOfContents[sampleId]
// (the +0x614 load is field +0x00 = mpSampleData; the (bankIdx+78)*20 load is that same
// SpliceContainer's +0x04 = mpTableOfContents, indexed by sampleId*4). That part is fully
// groundable now.
//
// STILL BLOCKED (irreducible): the voice-start branch (mbStartVoice, 0x826A3D00..0x826A3F88)
// builds a ~40-byte rw::audio::core "SndPlayer" start-Event parameter record handed to
// PlugIn::Event, whose leading 8 bytes are seeded with the rodata double dbl_82001CA8 -- a
// constant with NO recovered value (no .ida-exports data segment covers 0x82001CA8; the only
// other reference is an unrelated ChallengeManager round-to-nearest idiom, which does not
// reveal the value). The record's remaining field semantics are likewise un-modelled, and
// the deferred SetAttributeHandler command-queue path it shares writes into the RwacSystem's
// private command buffer (guest System +0x20 base / +0x10B8 cursor) whose layout is not homed
// in the vendor rw::audio::core headers. Emitting that payload + constant would be inventing a
// layout and an attested constant -- a hard fidelity violation, worse than an honest gap. The
// declaration stays homed in SpliceObjects.h; the body remains a link-time trap until
// dbl_82001CA8 and the SndPlayer start-event parameter layout are recovered.
// ============================================================================
