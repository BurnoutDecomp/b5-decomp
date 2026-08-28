#pragma once

// =====================================================================================
// rw::audio::core::Dac -- the engine's output (digital-to-analogue converter) plug-in:
// the terminal stage that interleaves the Mixer's planar master buffer into the
// speaker-ordered output frame the platform device consumes. On the X360 it owns the
// XAudio source voice, the double packet ring and the "RWAudioCore Dac" worker thread;
// on the PC the console's own EXTERNAL-DAC configuration is used (see sExternalDacMode
// in Dac.cpp) and CgsSystem::AudioOutputPC is the external device, its fill callback
// standing in for the Xenon worker loop.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the
// asm is authoritative for every member offset and store. Full register-level decode:
// progress/scratch_dossiers/dac_plugin_decode_codex.md (cross-checked in-session
// against the dossiers and the raw XEX bytes for the two exporter-gap bodies).
// Member/static names are the ProStreet08Milestone.pdb rw::audio::core::Dac names.
//   GetSize               @0x82B96CB0     GetPlugInDescRunTime @0x82B96DB8
//   CreateInstance        @0x82BA24A0     EventEvent (vt[1])   @0x82BA27F0
//   StartImmediate        @0x82B96DC8*    StopImmediate        @0x82B96E38
//   StartHandler          @0x82B9DCF0*    StopHandler          @0x82B9DD48
//   SetModeHandler        @0x82B9DB78     SetSampleRateHandler @0x82B9DCE8
//   RampOutput            @0x82B96CB8     Process              @0x82B97250
//   XenonDownMix          @0x82B97178     Mix                  @0x82B96E80*
//   ReleaseEvent (vt[0])  @0x82B9DAE0     (* = recovered from raw XEX bytes; these
//                                          three sit in IDA-exporter gaps)
// Console-only, documented in Dac.cpp but NOT ported (the external-DAC configuration
// skips them on the console too): XenonThread @0x82B96F40 / XenonThreadFunc @0x82B97150
// / XenonProcessCb / XenonPacketCompleteCb and every XAudio* call.
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable to match a third-party API).
// =====================================================================================

#include "types.hpp"
#include "rw/audio/core/PlugIn.h" // PlugIn (the base header the console instance shares)

namespace rw
{
namespace audio
{
namespace core
{

class Mixer;

// -------------------------------------------------------------------------------------
// Dac -- console instance sizeof 0x3108 (GetSize's literal): the PlugIn header, the
// Mixer pointer, three inline attributes, then the console packet-output block. The
// class DERIVES PlugIn on the host because the console instance is genuinely
// polymorphic: CreateInstance installs the Dac vtable (off_8217F3C4: [0] ReleaseEvent,
// [1] EventEvent, [3] the deleting dtor; 2/4-7 are COMDAT-folded no-ops) over the
// generic PlugIn header -- the host placement-construction of Dac IS that store.
// -------------------------------------------------------------------------------------
class Dac : public PlugIn
{
public:
    // vt[1] == EventEvent @0x82BA27F0 -- the engine event entry (PlugIn::Event
    // dispatches here). Event 0 = mode query (writes support + name through apParam);
    // 1/2 = deferred SetMode/SetSampleRate commands; 3/4 = deferred Start/Stop
    // commands -- each enqueued into the System's command ring and replayed by
    // ExecuteCommands. (The console leaves its r3 passthrough in the return; the
    // callers all discard it, so the host returns 0.)
    virtual int Event(int aiEventId, void *apParam);

    // vt[0] == ReleaseEvent @0x82B9DAE0 -- teardown: stop the worker flag, (console:
    // XAudio release/event-free/shutdown -- skipped in external-DAC mode), clear the
    // System's rwaudio thread id, StopImmediate, free the Mixer block.
    virtual ~Dac();

    // @0x82B96CB0 -- the descriptor's allocation stride. Console `li r3, 0x3108` ==
    // the console sizeof(Dac); the host returns the naturally-widened host sizeof
    // (the RawPuller2::GetSize convention -- the callback sizes the stage carve the
    // HOST object is placement-constructed into).
    static int GetSize();

    // @0x82B96DB8 -- return the 'Dac0' runtime descriptor record.
    static char **GetPlugInDescRunTime();

    // @0x82BA24A0 -- the descriptor's pCreateInstance body: install the Dac vtable
    // (host: the caller's placement-construction), allocate + construct the Mixer,
    // seed the static capability tables (mode 3 == 5.1, six channels, 48 kHz) and the
    // System's frame-period fields, then (console, non-external only) create the
    // XAudio voice + packet ring + worker thread. Returns 1 on success, 0 on failure.
    static int CreateInstance(Dac *self);

    // @0x82B97250 -- the descriptor's pProcess body (the DAC voice's terminal stage):
    // XenonDownMix, return 1.
    static int Process(Dac *self);

    // @0x82B96E80 (raw-XEX recovery) -- one engine mix frame: balance the CPU
    // governor, fill the static MixerExecuteParams from the System, Mixer::Execute,
    // publish the mixer cycle cost, advance the System sample clock by the frame
    // period, stamp the governor. Returns Execute's status (1 = the interleave buffer
    // will hold a valid frame after the stage walk ran Process; 0 = emit silence).
    static int Mix(Dac *self);

    // @0x82B97178 -- interleave the Mixer's master SampleBuffer into the static
    // output frame (WAVE speaker order), apply the 128-frame start ramp once after
    // each start, clip to [-1, 1].
    static void XenonDownMix(Dac *self);

    // @0x82B96CB8 -- linear gain ramp over the first `aiFrames` frames of `apFrame`
    // (all live channels): up (0/N..) after a start, down (N/N..) toward a stop. (The
    // console r3 return is address-materialisation residue -- dropped.)
    static void RampOutput(Dac *self, f32 *apFrame, int aiFrames, u8 au8RampUp);

    // @0x82B96DC8 (raw-XEX recovery) -- if stopped: arm the start ramp, (console,
    // non-external: voice volume 1.0), mark started.
    static Dac *StartImmediate(Dac *self);

    // @0x82B96E38 -- if started: mark stopped, (console, non-external: voice volume
    // 0.0).
    static Dac *StopImmediate(Dac *self);

    // The four deferred-command handlers (replayed by System::ExecuteCommands; each
    // returns its own record's size -- the HOST sizeof, matching the producer's
    // cursor advance per the ring contract).
    static int StartHandler(void *apCommand);         // @0x82B9DCF0 (raw-XEX recovery)
    static int StopHandler(void *apCommand);          // @0x82B9DD48
    static int SetModeHandler(void *apCommand);       // @0x82B9DB78
    static int SetSampleRateHandler(void *apCommand); // @0x82B9DCE8 (return-size no-op)

    // ---- layout (X360 offsets in comments; x64 widths, by-name access) ----
    Mixer *mpMixer;               // +0x24 -- the 0x30080 mix block (CreateInstance allocates)
    Attribute_t maAttributes[3];  // +0x28 -- the inline attribute table CreateInstance
                                  //   repoints PlugIn::mpAttribute at: [0] +0x28 the
                                  //   output mode (3.0 == 5.1), [1] +0x30 the sample
                                  //   rate (48000.0), [2] +0x38 (0.0)
    void *mpXAudioVoice;          // +0x40 -- console XAudio source voice; PC: never
                                  //   created (external-DAC mode), stays null
    u8 mau8PacketPcm[2][0x1800];  // +0x44 -- the console double packet PCM store
    s32 miSubmitPacketIndex;      // +0x3044 -- console packet-ring submit cursor
    s32 miWritePacketIndex;       // +0x3048 -- console packet-ring write cursor
    void *mpPacketEvent;          // +0x304C -- console XAudio packet event; PC null
    u8 mau8PacketDesc[2][0x58];   // +0x3050 -- the console XAUDIOPACKET pair (opaque;
                                  //   only the console submit path reads it)
    u32 mauPacketState[2];        // +0x3100 -- console packet states (0 free / 1
                                  //   ready / 2 submitted)
};

// The core free functions homed in Dac.cpp (their only ARTIST consumer is
// XenonDownMix): the planar->WAVE-order interleave and the in-place clamp.
struct SampleBuffer;
f32 *ReOrderRwAudioCoreToWave(f32 *apOut, const SampleBuffer *apBuffer, int aiChannels,
                              int aiFrames);                        // @0x82B6B590
f32 *ClipFloats(f32 *apValues, f32 afMin, f32 afMax, int aiCount);  // @0x82B64B68

// The external-fill support surface (PC-home accessors over the Dac TU statics --
// the console equivalents are XenonThread's direct in-TU static reads). Consumed by
// the PC output leaf (CgsDacOutputPC.cpp).
u8 DacIsEngineRunning();            // byte_8327A588 (the worker-loop run flag)
u8 DacIsStarted();                  // byte_8327A587
const f32 *DacGetInterleaveBuffer(); // unk_8327D600 (256 frames x 6 ch, f32)
u8 DacGetChannelCount();            // byte_8327A585

} // namespace core
} // namespace audio
} // namespace rw
