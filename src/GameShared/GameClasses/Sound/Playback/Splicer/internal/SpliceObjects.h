#ifndef SPLICE_OBJECTS_H
#define SPLICE_OBJECTS_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/Splicer/SpliceManager.h" // SpliceManager, VoicePluginPair, SPLICE_Data, SPLICE_TYPE
#include "rw/audio/core/Voice.h" // rw::audio::core::Voice (mucState) + PlugIn (via PlugIn.h)

// ============================================================================
// GameShared/GameClasses/Sound/Playback/Splicer/internal/SpliceObjects.h
//
// The runtime objects the sound-playback Splicer plays: a Splice owns up to 15
// SpliceSamples (one voice each), each of which is driven from a SPLICE_SampleRef
// descriptor. StereoSpliceSample is a SpliceSample that starts a stereo voice.
//
// Shape is DecFIGS DWARF ground truth (SpliceObjects.h) gated on the X360 ledger; the
// member layout is reproduced BY NAME and in DWARF declaration ORDER. The +0xNN comments
// are the X360 (32-bit) guest offsets from the ARTIST asm and are documentary only --
// on the x64 host the vptr and pointer members widen, so only member ORDER is
// load-bearing and every access is by name. The X360 bodies that ground the offsets:
//     SpliceSample::Play             @ 0x8268AE78
//     SpliceSample::Update           @ 0x826A3A50
//     SpliceSample::IsPlaying        @ 0x8268B128
//     SpliceSample::GetEnvelopeVolume@ 0x8268B000
//     SpliceSample::FreeVoice        @ 0x826A3870
//     StereoSpliceSample::StartVoice @ 0x826A3F98
//     Splice::Play / Update / IsPlaying / GetCpuTicks / operator new / operator delete
// ============================================================================

// SPLICE_SampleRef -- the per-sample descriptor a SpliceSample plays from (SpliceSample::
// mpData). It contains no pointers, so the x64 layout equals the X360 layout.  The float
// fields are recovered by offset/usage from the X360 bodies; the two trailing bytes are
// named by the DecFIGS DWARF.  The complete 44-byte shape is load-bearing because
// SpliceManager walks an array of these records.
struct SPLICE_SampleRef
{
    u16 muSampleId;      // +0x00  sample index into the splice bank's table of contents
    s8  mcSpliceType;    // +0x02  SPLICE_TYPE selecting the source bank (used by Update)
    u8  mPad03;          // +0x03
    f32 mfField04;       // +0x04  base volume (Update)
    f32 mfBasePitch;     // +0x08  base playback pitch (Play)
    f32 mfTriggerDelay;  // +0x0C  time-to-trigger; ~0 => start immediately (Play)
    f32 mfAzimuth;       // +0x10  pan azimuth; == KI_STEREO_SAMPLE_AZIMUTH marks a stereo sample
    f32 mfEnvLength;     // +0x14  envelope total length (GetEnvelopeVolume)
    f32 mfEnvAttack;     // +0x18  envelope attack time (GetEnvelopeVolume)
    f32 mfEnvDecay;      // +0x1C  envelope decay start time (GetEnvelopeVolume)
    f32 mfVolRandom;     // +0x20  volume-randomisation amount (Play)
    f32 mfPitchRandom;   // +0x24  pitch-randomisation amount (Play)
    u8  muPriority;      // +0x28  Priority (DecFIGS SpliceStructs.h:58)
    u8  muRollOffType;   // +0x29  eRollOffType (DecFIGS SpliceStructs.h:59)
    u8  maPad2A[2];      // +0x2A  natural tail padding; sizeof == 44
};

static_assert( sizeof(SPLICE_SampleRef) == 44,
               "serialized splice sample references must retain their 44-byte stride" );

// SpliceObjects.h:16 (DWARF). One playing splice sample (owns a single RWAC voice).
struct SpliceSample
{
    // vtable slot 0 == StartVoice (virtual; StereoSpliceSample overrides it).
    virtual void StartVoice();                                   // cpp:345 (base body: own TU)

    // ---- layout (DWARF order; X360 offsets documentary; x64 widths, by-name access) ----
    SpliceManager::VoicePluginPair  mDynamicVoicePluginPair;     // +0x04  h:51 (dynamic-voice fallback pair)
    SpliceManager::VoicePluginPair* mpVoicePluginPair;           // +0x0C  h:52
    rw::audio::core::PlugIn*         mpSubMix;                    // +0x10  h:53
    rw::audio::core::PlugIn*         mpSendPlugIn;                // +0x14  h:54
    rw::audio::core::PlugIn*         mpResamplePlugIn;            // +0x18  h:55
    rw::audio::core::PlugIn*         mpPanningPlugIn;             // +0x1C  h:56
    f32 mLocPitch;                                               // +0x20  h:59
    f32 mLocVol;                                                 // +0x24  h:60
    f32 mtTimeToTrigger;                                         // +0x28  h:61
    f32 mtTimeIntoPlayback;                                      // +0x2C  h:62
    SPLICE_SampleRef* mpData;                                    // +0x30  h:63
    f32 mLastPitch;                                              // +0x34  h:64
    f32 mfPreviousAzimuth;                                       // +0x38  h:65
    f32 mfPreviousVol;                                           // +0x3C  h:66
    f32 mfPreviousPitch;                                         // +0x40  h:67
    bool mbUsingDynamicVoice;                                    // +0x44  h:69
    bool mbStartVoice;                                           // +0x45  h:71

    // ---- own ledger funcs (declared here; bodies in this TU or their own) ----
    SpliceSample( SPLICE_SampleRef* apData, rw::audio::core::PlugIn* apSubMix ); // cpp:228 (own TU)
    ~SpliceSample();                                             // cpp:244 (own TU): resets vptr + FreeVoice

    void Play( f32 afVol, f32 afPitch, f32 afAzimuth, f32 afSpread );            // cpp:292
    void Update( rw::audio::core::System* apRwacSystem, f32 afVol, f32 afPitch,
                 f32 afAzimuth, f32 afTimeDelta, f32 afSpread );                 // cpp:394 (own TU)
    u32  GetCpuTicks();                                          // cpp:643 (own TU)
    bool IsPlaying();                                            // cpp:616
    const SPLICE_SampleRef* GetData() { return mpData; }         // h:49

protected:
    void GetEnvelopeVolume( f32& arfVol );                       // cpp:590
    void FreeVoice();                                            // cpp:253
};

// SpliceObjects.h:75 (DWARF). A SpliceSample that plays a stereo voice.
struct StereoSpliceSample : public SpliceSample
{
    StereoSpliceSample( SPLICE_SampleRef* apData, rw::audio::core::PlugIn* apSubMix ); // cpp:660 (own TU)
    virtual void StartVoice();                                   // cpp:669 (override)
};

// SpliceObjects.h:92 (DWARF). A splice: an array of up to 15 SpliceSamples plus the
// SPLICE_Data record that describes how many are live and their master volume scale.
struct Splice
{
    // ---- layout (DWARF order) ----
    SpliceSample* mSampleRefs[15];   // +0x00  h:130 (15 sample slots)
    SPLICE_Data*  mpData;            // +0x3C  h:131

    // ---- own ledger funcs ----
    Splice( SPLICE_TYPE aeType, int aiIndex, rw::audio::core::PlugIn* apSubMix ); // h:94 (own TU)
    Splice( SPLICE_Data* apData, rw::audio::core::PlugIn* apSubMix );             // h:96 (own TU)
    ~Splice();                                                                    // h:97 (own TU)

    void Play( f32 afVol, f32 afPitch, f32 afAzimuth, f32 afSpread );             // h:103
    void Update( rw::audio::core::System* apRwacSystem, f32 afVol, f32 afPitch,
                 f32 afAzimuth, f32 afTimeDelta, f32 afSpread );                  // h:111
    bool IsPlaying();                                                             // h:113

    static void* operator new( usize auSize );                                   // h:116
    static void  operator delete( void* apMemory );                              // h:119

    u32  GetCpuTicks();                                                           // h:123

private:
    SPLICE_Data* GetData() { return mpData; }                                     // h:128
};

#endif // SPLICE_OBJECTS_H
