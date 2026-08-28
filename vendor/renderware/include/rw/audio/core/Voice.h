#pragma once

// =====================================================================================
// rw::audio::core::Voice
//
// EARenderWare "rwaudio" mixing voice: the run-time object that owns a chain of PlugIn
// stages (a decoder/source stage plus effect stages), is inserted into the System's
// priority-sorted active-voice list, and is torn down through the System's deferred
// command ring. A Voice is a plain (non-polymorphic) object allocated as a single block
// that also carries, inline behind its fixed header, its per-stage PlugIn* pointer array,
// its per-stage record array, and the placement-constructed PlugIn instances themselves.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width and side-effect. There is NO matching TU in the Feb-2007 PS3 leak and no
// DecFIGS DWARF for this type. Sibling to the committed rw::audio::core homes (PlugIn,
// PlugInRegistry, Decoder, TimerManager, ...). NOTE: this is rw::audio::core::Voice, NOT
// the CgsSound::Logic::Voice in CgsVoice.h.
//
// LAYOUT NOTE (X360 byte offsets, from the asm). The compile target is x64 PC, so pointer
// members widen; the +0xNN annotations describe the X360 image and are reached only by
// named member -- only the member ORDER is load-bearing. The trailing arrays and the
// packed PlugIn instances form a hand-rolled sub-allocation inside the one block, so
// CreateInstance computes their positions arithmetically (offsetof/sizeof/alignment), the
// same layout the asm builds by hand.
//   +0x00  mfAverageCpuTicks + mafCpuTickHistory[2] (3x f32, init 800.0 -- the fresh-voice
//          deprioritisation seed; zeroed when the voice is silenced. Mixer::Execute
//          @0x82B6D900 grounds the semantics: +0x00 = the 3-sample moving average of the
//          voice's per-frame mix cost, +0x04/+0x08 = the two toggled history samples)
//   +0x0C  muCpuHistoryIndex     (Mixer::Execute's history-slot toggle, 0<->1)
//   +0x10  mpSystem              (owning System)
//   +0x14  mpName                ("Unknown" by default)
//   +0x18  mpStageData           (points at the inline per-stage record array in this block)
//   +0x1C  mExpelLink            (intrusive node in System::mpExpelledVoiceList)
//   +0x24  mfVolume              (f32, init 1.0)
//   +0x28  mfFadeStart           (f32, init 0.0)
//   +0x2C  mfParam2C             (f32, init 0.0)
//   +0x30  mfFadeEnd             (f32, init 0.0; ExpelAfterDecay copies mfFadeStart here)
//   +0x34  muCreateStamp         (snapshot of System::muFrameCounter at create time)
//   +0x38  mfPriority            (f32, init 100.0; SetPriorityHandler writes here)
//   +0x3C  miLastFrameCpuTicks   (init 0; zeroed on expel; Mixer::Execute's raw frame cost)
//   +0x40  muBlockSize           (total allocation size; also the active-node key)
//   +0x44  mucNumStages          (plug-in stage count)
//   +0x45  mucFlag45             (init 0)
//   +0x46  mcSourceStageIndex    (init -1; index of the source/decoder stage, or -1)
//   +0x47  mucState              (0=active, 1=decaying, 2=expelled)
//   +0x48  mucPriorityClass      (byte sort key for the active-voice list)
//   +0x49  mucExpelImmediate     (init 0)
//   +0x4C  mpPlugIns[mucNumStages] (trailing PlugIn* array -- the stage instances)
// =====================================================================================

#include "types.hpp" // f32, s8, u8, u16, u32, s32

#include "rw/audio/core/PlugIn.h" // System, PlugIn, PlugInDescRunTime (+ Voice/link fwd decls)

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// VoiceListLink -- the intrusive doubly-linked node embedded at Voice+0x1C, threading the
// System's "expelled / pending removal" voice list (head at System::mpExpelledVoiceList).
// New nodes insert at the head: node->mpPrev = 0; node->mpNext = oldHead; oldHead->mpPrev
// = node; head = node. (Grounded in ExpelImmediate / ReleaseImmediate / CreateInstanceHandler.)
// -------------------------------------------------------------------------------------
struct VoiceListLink
{
    VoiceListLink *mpNext; // +0x00 (== Voice +0x1C)
    VoiceListLink *mpPrev; // +0x04 (== Voice +0x20)
};

// -------------------------------------------------------------------------------------
// VoiceActiveNode -- one 8-byte entry (on X360) of System::mppVoiceListNodes, the priority-
// sorted array of live voices. muKey snapshots the voice's muBlockSize. Reached only by
// name / array index, so its (widening) size is not layout-critical.
// -------------------------------------------------------------------------------------
struct VoiceActiveNode
{
    Voice *mpVoice; // +0x00
    u32 muKey;      // +0x04 (== Voice::muBlockSize at insert time)
};

// -------------------------------------------------------------------------------------
// VoiceStageData -- one per-stage record, laid out inline (12-byte stride on X360) in the
// Voice block at mpStageData. CreateInstance copies the stage descriptor's pre-process /
// process hooks here and records the stage's sample count.
// -------------------------------------------------------------------------------------
struct VoiceStageData
{
    void *mpPreProcess;  // +0x00 (from PlugInDescRunTime +0x0C)
    void *mpProcess;     // +0x04 (from PlugInDescRunTime +0x10)
    u16 muSampleCount;   // +0x08 (low 16 bits of the stage's GetSize())
    u16 mPad0A;          // +0x0A
};

// -------------------------------------------------------------------------------------
// VoiceStageConfig -- one entry of the stage-configuration array handed to CreateInstance
// (12-byte stride on X360). Only mpDesc (+0x04) and the low byte at +0x08 are read; the
// whole entry is also passed by address to the descriptor's GetSize hook.
// -------------------------------------------------------------------------------------
struct VoiceStageConfig
{
    // ⭐ (phase-D probe crash root-cause, 2026-08-28): the +0x00 word is the stage's
    // CREATE CONTEXT POINTER -- the console PlugIn::CreateInstance reads this SAME
    // record as its `a4` (context at +0, init byte at +8); the splice staging's
    // config literals ({&1.0f,'SnP1',1}...) put the ctor-param float pointer here.
    // The former separate 'PlugInCreateDesc' host view diverged from this layout on
    // x64 (its +8 byte landed INSIDE the widened mpDesc pointer -- a garbage channel
    // count that overran the mixer), so the view is RETIRED and the create path
    // reads THIS record by name.
    void *mpContext;             // +0x00 -- the create-context pointer (0 when unused)
    PlugInDescRunTime *mpDesc;   // +0x04 -- describes this stage's plug-in
    u32 mFlagAndField8;          // +0x08 -- low byte = the `flag` passed to
                                 //   PlugIn::CreateInstance AND the init/channel byte
                                 //   copied into PlugIn::mOutputChannels
};

// -------------------------------------------------------------------------------------
// Voice -- see the file header for the byte layout. Plain object (no vtable).
// -------------------------------------------------------------------------------------
class Voice
{
public:
    // Default ctor = zero muCpuHistoryIndex only. Grounded in System::New2<Voice> @0x82B6E140, whose
    // inlined construction performs exactly this one store (every other field is written
    // by CreateInstance afterwards).
    Voice() : muCpuHistoryIndex(0) {}

    // @0x82B6EC50 -- allocate + lay out a voice for `numStages` stages, placement-construct
    // each stage's PlugIn, insert the create-handler command into the ring. Returns the
    // voice (and writes its plug-in array through outPlugIns) or null on failure.
    static Voice *CreateInstance(u8 priority, int numStages, VoiceStageConfig *configs,
                                 PlugIn ***outPlugIns, System *system);

    // @0x82B6C020 -- deferred handler: insert the voice into the priority-sorted active list
    // (growing it if full; else park the voice on the expelled list). Returns the record
    // size: X360 `li r3,8`, i.e. the CONSOLE sizeof -- on the host return
    // sizeof(VoiceCreateCommand) (16), which must equal the producer's cursor advance.
    static int CreateInstanceHandler(void *cmd);

    // @0x82B6BFD8 -- begin a fade-out: latch the decay target and queue the voice on the
    // System's expel-after-decay candidate list (no-op if not currently active).
    static Voice *ExpelAfterDecay(Voice *self);

    // @0x82B6E030 -- silence + expel the voice immediately: reset its stage CPU counters,
    // drop it from the active list, and re-link it onto the expelled list.
    static Voice *ExpelImmediate(Voice *self, u8 immediate);

    // @0x82B6EEA0 -- queue a release command (ReleaseHandler) into the System ring.
    static Voice *Release(Voice *self);

    // @0x82B6E0F8 -- deferred handler: ReleaseImmediate(voice, 0). Returns the record size:
    // X360 `li r3,8` is the CONSOLE sizeof -- on the host return sizeof(VoiceReleaseCommand)
    // (16), matching the producer's cursor advance.
    static int ReleaseHandler(void *cmd);

    // @0x82B6DF38 -- tear down every stage PlugIn, unlink from the active/expelled lists
    // (unless keepInList), and free the voice block through the System allocator.
    static Voice *ReleaseImmediate(Voice *self, u8 keepInList);

    // @0x82B6C1A8 -- remove the voice from the active list (compacting it) and from the
    // expulsion-candidate list. Returns 1 if it was present, 0 otherwise.
    static int RemoveActiveVoice(Voice *self);

    // @0x82B6EED0 -- queue a set-priority command (SetPriorityHandler) into the System ring.
    static Voice *SetPriority(Voice *self, f32 priority);

    // @0x82B6E128 -- deferred handler: write the queued priority into mfPriority. Returns the
    // record size: X360 `li r3,0xC` is the CONSOLE sizeof -- on the host return
    // sizeof(VoiceSetPriorityCommand) (24), matching the producer's cursor advance.
    static int SetPriorityHandler(void *cmd);

    // ---- layout (X360 byte offsets in the file-header comment; x64 widths, by-name access) ----
    // The per-voice mix-cost stat block (semantics grounded by Mixer::Execute
    // @0x82B6D900; formerly the mafBandFreq/mUnk0C guesses).
    f32 mfAverageCpuTicks;      // +0x00 -- 3-sample moving average of the frame cost
    f32 mafCpuTickHistory[2];   // +0x04 -- the two toggled history samples
    u32 muCpuHistoryIndex;      // +0x0C -- which history slot Execute overwrites (0<->1)
    System *mpSystem;      // +0x10
    const char *mpName;    // +0x14
    VoiceStageData *mpStageData; // +0x18
    VoiceListLink mExpelLink;    // +0x1C
    f32 mfVolume;          // +0x24
    f32 mfFadeStart;       // +0x28
    f32 mfParam2C;         // +0x2C
    f32 mfFadeEnd;         // +0x30
    u32 muCreateStamp;     // +0x34
    f32 mfPriority;        // +0x38
    s32 miLastFrameCpuTicks; // +0x3C -- raw cycles of the last mix frame
    u32 muBlockSize;       // +0x40
    u8 mucNumStages;       // +0x44
    u8 mucFlag45;          // +0x45
    s8 mcSourceStageIndex; // +0x46
    u8 mucState;           // +0x47
    u8 mucPriorityClass;   // +0x48
    u8 mucExpelImmediate;  // +0x49
    u8 mPad4A[2];          // +0x4A -- align the trailing pointer array
    PlugIn *mpPlugIns[1];  // +0x4C -- trailing flexible array: mucNumStages entries
};

} // namespace core
} // namespace audio
} // namespace rw
