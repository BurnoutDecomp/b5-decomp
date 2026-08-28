#pragma once

// =====================================================================================
// rw::audio::core::Mixer -- the audio-core mix executive: the per-audio-frame walk over
// the System's active voices that runs every voice's plug-in stage pipeline and lands
// the final planar mix in the master sample buffer the Dac interleaves for output.
//
// ⭐ THE MIXER *IS* THE PLUG-IN PROCESS CONTEXT (unified with the phase-D Dac slice
// 2026-08-28): the object every PlugIn::Process receives in r4 is the Mixer itself --
// the type Iir2Filters.h modelled as `AudioProcessContext` before this TU homed the
// whole layout (Iir2Filters.h now typedefs onto these types). The stage-visible fields
// keep their committed Iir2Filters.h spellings; the ProStreet08Milestone.pdb
// rw::audio::core::Mixer names ride in the comments (the PDB layout matches
// field-for-field; writer proof: progress/scratch_dossiers/dac_plugin_decode_codex.md
// section 3).
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm
// is authoritative for every member offset and store:
//   Mixer::Mixer (placement ctor)         @0x82B6D880
//   Mixer::Execute                        @0x82B6D900
//   Mixer::ProcessInputPlugIns            @0x82B6A048 (decode in flight -- see the
//                                          honest stub in Mixer.cpp)
//   Mixer::HandleBufferStatusUnavailable  @0x82B69F78 (same)
// No Feb-2007 leak source and no DecFIGS DWARF exist for this TU.
//
// The console Mixer is one 0x30080-byte block: 3 x 0x10000 bytes of channel-sample
// storage (64 channel slots x 256 f32 frames each) followed by a 0x80-byte header at
// +0x30000. Dac::CreateInstance @0x82BA24A0 allocates it (System::Alloc, align 128) and
// placement-constructs it; the +0xNN annotations are the X360 offsets and access is by
// name (only the ORDER is load-bearing on the host).
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable to match a third-party API).
// =====================================================================================

#include "types.hpp" // f32, f64, u8, u16, u32

namespace rw
{
namespace audio
{
namespace core
{

class System;
class Voice;
class PlugIn;
struct VoiceActiveNode;
struct VoiceStageData;

// -------------------------------------------------------------------------------------
// StackAllocator -- the audio-core scratch-stack record (rwaudio PDB name). ONE static
// instance exists (X360 dword_83271930, the 4-word table System_ctor points
// System::mpObjectTable at); Mixer::Mixer seeds it over the static scratch arena and
// links the owning System, and the mixer-scratch consumers (AiffWriter @0x82B95CC8,
// Delay::CreateInstance @0x82BA2790) read slot [3] (mpTop) through
// System::mpObjectTable. Defined in System.cpp (the console's owning TU).
// -------------------------------------------------------------------------------------
struct StackAllocator
{
    System *mpSystem;  // +0x00 (dword_83271930) -- the owning System (Mixer::Mixer links it)
    u8 *mpUpperLimit;  // +0x04 -- arena top, 128-aligned down
    u8 *mpLowerLimit;  // +0x08 -- arena bottom, 128-aligned up
    u8 *mpTop;         // +0x0C -- the live stack top (starts at mpUpperLimit)
};

// -------------------------------------------------------------------------------------
// MixerExecuteParams -- the per-frame parameter block Dac::Mix @0x82B96E80 fills from
// the System and hands to Mixer::Execute (r4); ALSO the record the filter stages read
// through Mixer::mpFormat (the Iir2Filters.h `AudioFormat` view -- its +0x0C sample
// rate IS this record's). ONE static instance exists (X360 0x8327EE40, defined in the
// Dac TU). PDB names in comments; X360 size 0x18.
// -------------------------------------------------------------------------------------
struct MixerExecuteParams
{
    f64 mfSystemTime;                    // +0x00 (PDB: systemTime; System::mfSystemTime snapshot)
    VoiceActiveNode *mpVoiceListNodes;   // +0x08 (PDB: pVoiceListNodes; System +0x58)
    f32 mfSampleRate;                    // +0x0C (PDB: outputSampleRate; the 48000.0f literal
                                         //   Dac::Mix stores -- the committed AudioFormat
                                         //   spelling, read by every filter Process)
    u16 muNumVoices;                     // +0x10 (PDB: numVoices; System::muActiveVoiceCount)
    u8 mu8NumPlugInsRegistered;          // +0x12 (PDB: numPlugInsRegistered; NOT written by
                                         //   Dac::Mix -- retains its prior value, faithful)
    u8 mbVerifyFloats;                   // +0x13 (PDB: verifyFloats; booleanized
                                         //   System::maucDebugFeatures[2])
};

// -------------------------------------------------------------------------------------
// SampleBuffer -- one channel-buffer descriptor (rwaudio PDB name; the committed
// Iir2Filters.h `AudioChannelBuffer` -- the record the filter Process loops and
// ReOrderRwAudioCoreToWave @0x82B6B590 read channels out of). THREE static instances
// exist (X360 unk_83271940, stride 0x114, defined in Mixer.cpp); Execute re-seeds all
// three every frame and publishes them at the Mixer's src/dst/aux slots. Channel k's
// samples start at mpSamples + muStride * k.
// -------------------------------------------------------------------------------------
struct SampleBuffer
{
    System *mpSystem;        // +0x00 -- Execute seeds the owning System (was the opaque
                             //   AudioChannelBuffer header word)
    f32 *mpSamples;          // +0x04 -- Execute seeds the region base (Mixer::mBuffer[i])
    u32 mUnk08;              // +0x08 (untouched by the decoded bodies)
    u16 muUnk0C;             // +0x0C -- Execute seeds 0
    u16 muStride;            // +0x0E -- Execute seeds 256 (samples per channel slot; the
                             //   committed spelling -- every filter reads it by this name)
    u8 mu8ChannelCapacity;   // +0x10 -- Execute seeds 64 (channel slots per region)
    // +0x11..0x113 -- opaque tail (the 0x114 console stride; per-channel state words
    // untouched by the ctor/Execute -- type it when a consumer body lands).
    u8 mau8Tail[0x114 - 0x11];
};

// -------------------------------------------------------------------------------------
// Mixer -- see the file header. 3 x 64-channel x 256-frame sample regions + the header.
// Field spellings: the stage-visible fields keep the committed Iir2Filters.h
// AudioProcessContext names (mdStreamTime / mpSrcBuffer / mpDstBuffer / mpFormat /
// mNumSamples / mfSampleRate / mfResampleGain / mbChannelCount); the PDB Mixer names
// ride in the comments.
// -------------------------------------------------------------------------------------
class Mixer
{
public:
    // The engine audio-frame quantum: 256 samples per channel per frame (the `li r4,
    // 0x100` Execute seeds every SampleBuffer stride with, and the 256.0f/rate period
    // Dac::CreateInstance publishes into the System).
    enum { KU_FRAME_SIZE = 256 };
    enum { KU_CHANNEL_CAPACITY = 64 };   // channel slots per sample region
    enum { KU_NUM_REGIONS = 3 };

    // @0x82B6D880 -- placement ctor over the Dac-allocated block: zero the four header
    // stat fields and seed the static StackAllocator over the static scratch arena
    // (linking the System singleton). Returns `self` (the console r3 passthrough).
    static Mixer *Mixer_ctor(Mixer *self);

    // @0x82B6D900 -- the per-audio-frame mix walk. Re-seeds the three SampleBuffer
    // descriptors over the sample regions (resetting the src/dst ping-pong), then
    // iterates params->mpVoiceListNodes [0..muNumVoices): per voice runs
    // ProcessInputPlugIns (unless the voice has no source stage), then each remaining
    // stage's process callback (VoiceStageData::mpProcess)(plugin, mixer,
    // stage > mucFlag45), falling back to HandleBufferStatusUnavailable when a stage
    // reports no buffer; folds the per-voice CPU cycle costs into the voice's stat
    // block. Returns the LAST stage status (1 = the src slot holds a valid frame;
    // 0 with zero voices -- the Dac then emits silence, the faithful idle state).
    static int Execute(Mixer *self, MixerExecuteParams *apParams);

    // @0x82B6A048 -- run a voice's source/input stages (decoder pull + buffer binding).
    // FLAG honest stub in Mixer.cpp: the register-level decode is in flight
    // (progress/scratch_dossiers/mixer_voicepath_decode_codex.md); UNREACHABLE until a
    // voice exists (phase E lights the first voices), and stubbed DECLINING (status 0)
    // so a premature voice mixes silence instead of an invented frame.
    static int ProcessInputPlugIns(Mixer *self, VoiceStageData *apStageData,
                                   VoiceActiveNode *apNode, Voice *apVoice);

    // @0x82B69F78 -- a stage returned buffer-unavailable: bind/clear the fallback
    // buffer so downstream stages see silence. FLAG honest stub (same decode in
    // flight, same unreachable-until-voices reasoning; declines with 0).
    static int HandleBufferStatusUnavailable(Mixer *self, Voice *apVoice,
                                             PlugIn *apPlugIn, int aiNumSamples);

    // ---- layout (X360 offsets in comments; x64 widths, by-name access) ----
    f32 mBuffer[KU_NUM_REGIONS][KU_CHANNEL_CAPACITY * KU_FRAME_SIZE]; // +0x00000 (PDB: mBuffer;
                                            //   the Iir2Filters.h "opaque graph buffers" span)
    f64 mdStreamTime;                       // +0x30000 (PDB: mCurrentMixTime; Execute refreshes
                                            //   it from the params clock around every voice --
                                            //   SinePlayer::Process reads it as the frame-start
                                            //   stream time)
    System *mpSystem;                       // +0x30008 (Dac::CreateInstance links it)
    SampleBuffer *mpSrcBuffer;              // +0x3000C (PDB: mpSampleBuffer[0]; the stage
                                            //   ping-pong source -- the latest stage output
                                            //   lands here, and the Dac interleaves it)
    SampleBuffer *mpDstBuffer;              // +0x30010 (PDB: mpSampleBuffer[1]; the ping-pong
                                            //   destination)
    SampleBuffer *mpAuxBuffer;              // +0x30014 (PDB: mpSampleBuffer[2]; the third
                                            //   region -- no decoded stage touches it yet)
    MixerExecuteParams *mpFormat;           // +0x30018 (PDB: the execute-params slot; the
                                            //   committed AudioFormat view -- filters read
                                            //   mpFormat->mfSampleRate)
    u32 muMixerCpuCycles;                   // +0x3001C (PDB: mMixerCpuCycles; ctor zeroes,
                                            //   Execute stamps; Dac::Mix copies it into
                                            //   System::muMixerCpuTicks)
    u32 mNumSamples;                        // +0x30020 (PDB: mSrcNumSamples; ctor zeroes; the
                                            //   frame's active sample count -- 0 == silent)
    f32 mfSampleRate;                       // +0x30024 (PDB: mSrcSampleRate; ctor 0.0f; the
                                            //   active src-buffer sample rate)
    f32 mfResampleGain;                     // +0x30028 (PDB: mTotalPitch; Execute stores 1.0f
                                            //   per voice before its stage walk)
    u8 mbChannelCount;                      // +0x3002C (PDB: mSrcNumChannels; ctor zeroes; the
                                            //   active src-buffer channel count)
};

} // namespace core
} // namespace audio
} // namespace rw
