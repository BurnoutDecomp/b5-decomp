// =====================================================================================
// rw::audio::core::Dac -- member-function bodies.
//
// EARenderWare "rwaudio" output plug-in. Reconstructed from BURNOUT_X360_ARTIST.XEX;
// the PowerPC asm is authoritative for every store, and the three exporter-gap bodies
// (StartImmediate / StartHandler / Mix) are decoded from the raw XEX instruction words
// (file_off = 0x3000 + vaddr - 0x82000000, big-endian). Full register-level decode +
// the rodata recovery tables: progress/scratch_dossiers/dac_plugin_decode_codex.md.
// Static names are the ProStreet08Milestone.pdb rw::audio::core::Dac names.
//
// ⭐ THE PC RUNS THE CONSOLE'S OWN EXTERNAL-DAC CONFIGURATION (sExternalDacMode below):
// every XAudio/worker-thread site in this TU is guarded by the SAME console flag
// (dword_8327EE58) -- with it set, the console itself creates no voice, no packet
// event and no "RWAudioCore Dac" thread, and start/stop skip the voice-volume calls.
// The PC's external device is CgsSystem::AudioOutputPC; its engine fill callback
// (CgsDacOutputPC.cpp) stands in for the XenonThread @0x82B96F40 worker loop, whose
// per-packet shape it inverts:
//   ExecuteCommandsLock -> { wait packet slot -> ExecuteCommands -> Mix -> ok ?
//   XMemCpy(packet, interleave, 0x1800) : XMemSet(packet, 0, 0x1800) -> submit }
//   -> ExecuteCommandsUnlock
// (the console holds the commands lock for the thread's whole life; the PC fill takes
// it per 256-frame callback -- the same mutual exclusion at the same granularity the
// packet ring imposed). XenonProcessCb / XenonPacketCompleteCb / XenonThreadFunc are
// console-only XAudio callbacks and are not ported.
// =====================================================================================

#include "rw/audio/core/plugins/Dac.h"
#include "rw/audio/core/Mixer.h"           // Mixer / MixerExecuteParams / SampleBuffer
#include "rw/audio/core/CpuLoadBalancer.h" // the CPU-load governor the Dac drives
#include "rw/audio/core/Profiler.h"        // Profiler (spProfiler)

#include <cstring> // memset (the fresh-page-parity mixer clear)
#include <new> // placement new (the vtable install in the create thunk)

namespace rw
{
namespace audio
{
namespace core
{

// The System singleton pointer (X360 off_83271928; defined in System.cpp).
extern "C" System *off_83271928;

// The PPC timebase read (System.cpp owns the one definition; local declaration --
// the Profiler.cpp / CpuLoadBalancer.cpp convention, link-merged).
u32 GetCpuCycle();

// =====================================================================================
// The Dac static cluster (the console .data/.bss block at 0x8327A580..0x8327EE8F and
// the rodata tables; PDB names). Console addresses in the comments.
// =====================================================================================
namespace
{
    // dword_8327A580 -- the Profiler CreateInstance caches (a null fetch fails create).
    Profiler *spProfiler = 0;

    // byte_8327A584 -- a start was requested: XenonDownMix applies the 128-frame
    // up-ramp once, then clears it.
    u8 sStartRequested = 0;

    // byte_8327A585 -- the live output channel count (6 == the 5.1 mode).
    u8 sChannels = 0;

    // byte_8327A586 / byte_8327A589 -- the compacted capability counts.
    u8 sCapNumSampleRates = 0;
    u8 sCapNumModes = 0;

    // byte_8327A587 -- started (the DAC is producing).
    u8 sStarted = 0;

    // byte_8327A588 -- the worker-loop run flag (console: XenonThread's lifetime; PC:
    // set/cleared at the same sites, read by the external fill as its "engine up" gate).
    u8 sThreadRunning = 0;

    // dword_8327EE58 -- the console's external-DAC flag: nonzero skips every XAudio /
    // worker-thread site. FLAG PC-platform leaf: the PC IS the external configuration
    // (CgsSystem::AudioOutputPC is the device), so the flag is DEFINED 1 here where the
    // console image boots it 0 -- every guarded site below is otherwise store-for-store.
    const int sExternalDacMode = 1;

    // dword_8327EE04 -- the supported-mode list (capacity 6; entry 0 = mode 3 == 5.1).
    int sCapModes[6] = { 0, 0, 0, 0, 0, 0 };

    // flt_8327EE1C -- the supported-rate list (capacity 7; entry 0 = 48000.0).
    f32 sCapSampleRates[7] = { 0, 0, 0, 0, 0, 0, 0 };

    // 0x8327EE40 -- the static MixerExecuteParams block Mix refills every frame.
    MixerExecuteParams sMixerExecuteParams = { 0.0, 0, 0.0f, 0, 0, 0 };

    // unk_8327EE6C -- the static CPU-load governor (Init at create, Reset at start,
    // Balance at mix/stop).
    CpuLoadBalancer sCpuLoadBalancer = { 0, 0, 0, 0, 0.0f, { 0.0f, 0.0f }, 0 };

    // unk_8327D600 -- the interleaved output frame (256 frames x 6 channels of f32 ==
    // 0x1800 bytes): XenonDownMix's destination, the external fill's source.
    f32 sInterleaveBuffer[Mixer::KU_FRAME_SIZE * 6];

    // unk_8215D010 -- the rate candidate table CreateInstance compacts (only 48 kHz is
    // populated on the ARTIST image; XEX-recovered).
    const f32 SKAF_RATE_CANDIDATES[7] = { 48000.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    // off_82F8C664 -> 0x8215D044.. -- the mode-name table event 0 indexes.
    const char *const SKPPC_MODE_NAMES[6] =
        { "Mono", "Stereo", "Quad", "5.1", "7.1", "Dolby Pro Logic II" };

    // byte_8215D078 -- mode index -> channel count (XEX-recovered {1,2,4,6,8,2}).
    const u8 SKAU8_MODE_CHANNELS[6] = { 1, 2, 4, 6, 8, 2 };

    // ---- the deferred-command records EventEvent enqueues (ring contract: the
    // producer's cursor advance == the handler's return == the HOST sizeof; the
    // console literals are 8 and 12) ----
    struct DacStartStopCommand
    {
        int (*mpHandler)(void *); // +0x00 -- &StartHandler / &StopHandler
        Dac *mpDac;               // +0x04 -- the plug-in (console: the r3 event object)
    };
    struct DacValueCommand
    {
        int (*mpHandler)(void *); // +0x00 -- &SetModeHandler / &SetSampleRateHandler
        Dac *mpDac;               // +0x04
        f32 mfValue;              // +0x08 -- the copied event parameter word
    };

    // The event-0 (mode query) parameter block the caller supplies: mode index in,
    // supported flag + name out (console stores at param +0/+4/+8).
    struct DacModeQuery
    {
        f32 mfModeIndex;      // +0x00 (in)
        f32 mfSupported;      // +0x04 (out: 0.0 / 1.0)
        const char *mpcName;  // +0x08 (out)
    };
}

// =====================================================================================
// ClipFloats @0x82B64B68 -- clamp `aiCount` floats into [afMin, afMax] in place. A
// core-namespace free function; its only ARTIST consumer is XenonDownMix, so it is
// homed here.
// =====================================================================================
f32 *ClipFloats(f32 *apValues, f32 afMin, f32 afMax, int aiCount)
{
    f32 *lpValue = apValues;
    f32 *lpEnd = apValues + aiCount;
    for (; lpValue < lpEnd; ++lpValue)
    {
        f32 lfSample = *lpValue;
        if (!(lfSample >= afMin))
            *lpValue = afMin;          // the console's `bge`-fails path -- a NaN is
                                       // unordered and lands here too (preserved by
                                       // the negated >= spelling)
        else if (lfSample > afMax)
            *lpValue = afMax;
    }
    return apValues;
}

// =====================================================================================
// ReOrderRwAudioCoreToWave @0x82B6B590 -- de-interleave the planar SampleBuffer into
// one interleaved frame in WAVE speaker order. Planar channel k lives at
// mpSamples + muStride * k; the 6-channel map is out {FL,FR,C,LFE,BL,BR} = planar
// {0,2,1,5,3,4} (the rw-core planar order is {FL,C,FR,BL,BR,LFE}). 4- and 2-channel
// maps and the 1-channel copy are the console's other switch arms. A core-namespace
// free function; only ARTIST consumer is XenonDownMix.
// =====================================================================================
f32 *ReOrderRwAudioCoreToWave(f32 *apOut, const SampleBuffer *apBuffer, int aiChannels,
                              int aiFrames)
{
    const u32 luStride = apBuffer->muStride;
    f32 *lpPlanar = apBuffer->mpSamples;

    switch (aiChannels)
    {
    case 6:
    {
        const f32 *lpCh0 = lpPlanar;                 // FL
        const f32 *lpCh1 = lpPlanar + luStride;      // C
        const f32 *lpCh2 = lpPlanar + luStride * 2;  // FR
        const f32 *lpCh3 = lpPlanar + luStride * 3;  // BL
        const f32 *lpCh4 = lpPlanar + luStride * 4;  // BR
        const f32 *lpCh5 = lpPlanar + luStride * 5;  // LFE
        const f32 *lpEnd = lpCh2 + aiFrames;         // the console's loop bound (ch2)
        while (lpCh2 < lpEnd)
        {
            apOut[2] = *lpCh1++;   // C
            apOut[1] = *lpCh2++;   // FR
            apOut[5] = *lpCh4++;   // BR
            apOut[4] = *lpCh3++;   // BL
            apOut[0] = *lpCh0++;   // FL
            apOut[3] = *lpCh5++;   // LFE
            apOut += 6;
        }
        break;
    }
    case 4:
    {
        const f32 *lpCh0 = lpPlanar;
        const f32 *lpCh1 = lpPlanar + luStride;
        const f32 *lpCh2 = lpPlanar + luStride * 2;
        const f32 *lpCh3 = lpPlanar + luStride * 3;
        const f32 *lpEnd = lpCh1 + aiFrames;         // the console's loop bound (ch1)
        while (lpCh1 < lpEnd)
        {
            apOut[1] = *lpCh1++;   // FR
            apOut[3] = *lpCh3++;   // BR
            apOut[2] = *lpCh2++;   // BL
            apOut[0] = *lpCh0++;   // FL
            apOut += 4;
        }
        break;
    }
    case 2:
    {
        const f32 *lpCh0 = lpPlanar;
        const f32 *lpCh1 = lpPlanar + luStride;
        const f32 *lpEnd = lpCh1 + aiFrames;
        while (lpCh1 < lpEnd)
        {
            apOut[1] = *lpCh1++;
            apOut[0] = *lpCh0++;
            apOut += 2;
        }
        break;
    }
    case 1:
        // The console tail-calls XMemCpy(out, planar, 4 * frames).
        for (int li = 0; li < aiFrames; ++li)
            apOut[li] = lpPlanar[li];
        break;
    }
    return apOut;
}

// =====================================================================================
// The 'Dac0' runtime descriptor (off_82F8C7A8; raw words + interpretation in
// plugindesc_layout_codex.md). The create slot is the host thunk below -- the console
// body's FIRST store is the Dac vtable install (`*a1 = off_8217F3C4`), which on the
// host IS the placement-construction of Dac over the generic stage memory (default-
// init: vptrs written, every data member left untouched -- the exact console store).
// Metadata pointers (console 0x82F8C67C/0x82F8C680/0x82F8C780) FLAG'd null per the
// descriptor-wave convention (no committed consumer reads them).
// =====================================================================================
static int DacCreateInstanceThunk(Dac *self, void * /*apContext*/)
{
    ::new (static_cast<void *>(self)) Dac;   // *a1 = off_8217F3C4
    return Dac::CreateInstance(self);
}

static PlugInDescRunTime g_DacDesc = {
    "Dac",                                            // 0x8215D080
    reinterpret_cast<void *>(&Dac::GetSize),          // @0x82B96CB0
    reinterpret_cast<void *>(&DacCreateInstanceThunk),// @0x82BA24A0
    0,
    reinterpret_cast<void *>(&Dac::Process),          // @0x82B97250
    0, 0, 0, 0,
    0,
    0x44616330u,       // 'Dac0'
    4, 0, 3, 5, 0, 0,
    0
};

// GetPlugInDescRunTime @0x82B96DB8 -- return &off_82F8C7A8.
char **Dac::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&g_DacDesc);
}

// GetSize @0x82B96CB0 -- console `li r3, 0x3108` (the console sizeof(Dac)); the host
// returns the naturally-widened host sizeof (the stage carve the host object is
// placement-constructed into -- the RawPuller2::GetSize convention).
int Dac::GetSize()
{
    return static_cast<int>(sizeof(Dac));
}

// =====================================================================================
// CreateInstance @0x82BA24A0 -- the descriptor's create body. Console store order
// preserved; the XAudio/worker block at the tail is the external-DAC-guarded console
// device creation (see the file header).
// =====================================================================================
int Dac::CreateInstance(Dac *self)
{
    // (The console's first store -- the Dac vtable install -- happened in the caller's
    // placement-construction; see DacCreateInstanceThunk.)
    self->mpMixer = 0;                        // stw 0 -> +0x24
    self->mpAttribute = self->maAttributes;   // stw a1+0x28 -> +0x0C (the inline table)

    sCpuLoadBalancer.muHistoryIndex = 0;      // dword_8327EE88 = 0
    CpuLoadBalancer::Init(&sCpuLoadBalancer, self->mpSystemUseGetSystemAccessor);

    spProfiler = System::GetProfiler(self->mpSystemUseGetSystemAccessor);
    if (!spProfiler)
        return 0;

    // The Mixer block: console 0x30080 bytes @128 == the host sizeof(Mixer) @128.
    System *lpSystem = self->mpSystemUseGetSystemAccessor;
    void *lpMixerBlock = System::Alloc(lpSystem, static_cast<u32>(sizeof(Mixer)), 0, 128, 0);
    self->mpMixer = static_cast<Mixer *>(lpMixerBlock);
    if (lpMixerBlock)
    {
        // FLAG PC-platform parity: the console block comes from freshly-mapped
        // physical pages, which the kernel hands over ZEROED -- the sample regions
        // start silent, and an interleave before any producer writes them emits
        // silence. The host allocator gives no such guarantee, so the zero is made
        // explicit here (same observable state, not an invented store).
        std::memset(lpMixerBlock, 0, sizeof(Mixer));
        self->mpMixer = Mixer::Mixer_ctor(static_cast<Mixer *>(lpMixerBlock));
    }
    if (!self->mpMixer)
        return 0;

    self->mpMixer->mpSystem = lpSystem;       // stwx -> mixer+0x30008

    // The capability/state seed block (console store order).
    sStarted = 0;
    sStartRequested = 0;
    self->maAttributes[2].mfValue = 0.0f;     // stfs flt_82001CC0 -> +0x38
    sCapNumModes = 0;
    sCapNumSampleRates = 0;
    self->maAttributes[1].mfValue = 48000.0f; // stfs flt_820AA808 -> +0x30 (the rate)
    self->maAttributes[0].mfValue = 3.0f;     // stfs flt_82004270 -> +0x28 (mode 3 == 5.1)
    sChannels = 6;
    sCapModes[sCapNumModes] = 3;              // the one supported mode
    sCapNumModes = static_cast<u8>(sCapNumModes + 1);
    for (u32 luRate = 0; luRate < 7; ++luRate)
    {
        // Compact the positive candidates (the console's pointer walk over
        // unk_8215D010..flt_8215D02C; only 48 kHz is populated).
        if (SKAF_RATE_CANDIDATES[luRate] > 0.0f)
        {
            sCapSampleRates[sCapNumSampleRates] = SKAF_RATE_CANDIDATES[luRate];
            sCapNumSampleRates = static_cast<u8>(sCapNumSampleRates + 1);
        }
    }

    // Publish the frame period into the System: rate at +0x10C4, 256/rate at +0x10C0
    // AND into the TimerManager's callback-time slot (+0x60 + 0x38 == the v8[38]
    // store -- the manager hands it to every due timer).
    f32 lfRate = self->maAttributes[1].mfValue;
    f32 lfPeriod = 256.0f / lfRate;                       // flt_820ADBFC / rate
    lpSystem->mfSampleRate = lfRate;                      // +0x10C4
    lpSystem->mfSystemTimerPeriod = lfPeriod;             // +0x10C0
    lpSystem->mTimerManager.mfCallbackTime = lfPeriod;    // +0x98

    sThreadRunning = 1;

    if (!sExternalDacMode)
    {
        // Console-only device creation, external-DAC-guarded (`if (!dword_8327EE58)`):
        // XAudioInitialize (submix param unk_82108F38); XAudioCreateSourceVoice (6ch,
        // 48000 Hz, 2 packets, XenonProcessCb / XenonPacketCompleteCb, ctx = the Dac)
        // -> +0x40; zero the packet-descriptor block (+0x3050, 0xB0) and wire packet i
        // {buffer = +0x44 + i*0x1800, size 0x1800, status word = +0x3100 + i*4}; zero
        // the PCM store (+0x44, 0x3000); submit/write cursors = 0; the packet event
        // (XAudioCreateEvent(0, 1)) -> +0x304C; spawn "RWAudioCore Dac"
        // (EA::Thread::Thread::Begin(XenonThreadFunc, this) with the System's
        // priority/stack/processor triple, SetRwAudioCoreThreadId) and
        // XAudioSourceVoice_Start. NOT PORTED -- the PC external device is
        // CgsSystem::AudioOutputPC and its engine fill drives the frame loop.
    }
    return 1;
}

// =====================================================================================
// Event (vt[1]) == EventEvent @0x82BA27F0 -- the engine event entry.
// =====================================================================================
int Dac::Event(int aiEventId, void *apParam)
{
    System *lpSystem = mpSystemUseGetSystemAccessor;   // lwz 4(r3)

    if (aiEventId == 0)
    {
        // Mode query (in place, no deferral): support flag + name for a mode index.
        DacModeQuery *lpQuery = static_cast<DacModeQuery *>(apParam);
        int liMode = static_cast<int>(lpQuery->mfModeIndex);   // fctiwz
        lpQuery->mfSupported = 0.0f;
        lpQuery->mpcName = SKPPC_MODE_NAMES[liMode];   // no bounds check -- console
        for (u32 luEntry = 0; luEntry < sCapNumModes; ++luEntry)
        {
            if (liMode == sCapModes[luEntry])
            {
                lpQuery->mfSupported = 1.0f;
                break;
            }
        }
        // (The console leaves its r3 passthrough; every caller discards it.)
        return 0;
    }

    if (aiEventId == 1 || aiEventId == 2)
    {
        // Deferred value command (console 12 bytes): {handler, this, *param}.
        DacValueCommand *lpCommand = reinterpret_cast<DacValueCommand *>(
            lpSystem->mpDeferredRingBase + lpSystem->muDeferredRingCursor);
        lpSystem->muDeferredRingCursor += static_cast<u32>(sizeof(DacValueCommand));
        lpCommand->mpHandler = (aiEventId == 1) ? &Dac::SetModeHandler
                                                : &Dac::SetSampleRateHandler;
        lpCommand->mpDac = this;
        lpCommand->mfValue = *static_cast<f32 *>(apParam);   // the copied word
        return 0;
    }

    // Every other id (console: 3 == start, else stop): deferred 8-byte command.
    DacStartStopCommand *lpCommand = reinterpret_cast<DacStartStopCommand *>(
        lpSystem->mpDeferredRingBase + lpSystem->muDeferredRingCursor);
    lpSystem->muDeferredRingCursor += static_cast<u32>(sizeof(DacStartStopCommand));
    lpCommand->mpDac = this;
    lpCommand->mpHandler = (aiEventId == 3) ? &Dac::StartHandler : &Dac::StopHandler;
    return 0;
}

// =====================================================================================
// StartImmediate @0x82B96DC8 (raw-XEX recovery) / StopImmediate @0x82B96E38.
// =====================================================================================
Dac *Dac::StartImmediate(Dac *self)
{
    if (!sStarted)
    {
        sStartRequested = 1;   // arm the 128-frame up-ramp
        if (!sExternalDacMode)
        {
            // Console-only: XAudioSourceVoice_SetVolume(+0x40 voice, 1.0).
        }
        sStarted = 1;
    }
    return self;
}

Dac *Dac::StopImmediate(Dac *self)
{
    if (sStarted)
    {
        sStarted = 0;
        if (!sExternalDacMode)
        {
            // Console-only: XAudioSourceVoice_SetVolume(+0x40 voice, 0.0).
        }
    }
    return self;
}

// =====================================================================================
// The deferred-command handlers (ExecuteCommands replays; each returns its record's
// HOST sizeof, matching its producer's advance -- console literals 8 / 12).
// =====================================================================================
int Dac::StartHandler(void *apCommand)   // @0x82B9DCF0 (raw-XEX recovery)
{
    DacStartStopCommand *lpCommand = static_cast<DacStartStopCommand *>(apCommand);
    if (!sStarted)
    {
        StartImmediate(lpCommand->mpDac);
        CpuLoadBalancer::Reset(&sCpuLoadBalancer);
        sCpuLoadBalancer.muLastStamp = GetCpuCycle();
    }
    return static_cast<int>(sizeof(DacStartStopCommand));   // console li r3, 8
}

int Dac::StopHandler(void *apCommand)    // @0x82B9DD48
{
    DacStartStopCommand *lpCommand = static_cast<DacStartStopCommand *>(apCommand);
    if (sStarted)
    {
        StopImmediate(lpCommand->mpDac);
        CpuLoadBalancer::Balance(&sCpuLoadBalancer);
    }
    return static_cast<int>(sizeof(DacStartStopCommand));   // console li r3, 8
}

// SetModeHandler @0x82B9DB78 -- pick the best supported mode for the request, walking
// the preference order {3, 2, 1, 0} from the requested mode's own preference slot;
// republish the channel count; bounce the output (stop + start) if live.
int Dac::SetModeHandler(void *apCommand)
{
    DacValueCommand *lpCommand = static_cast<DacValueCommand *>(apCommand);
    Dac *lpDac = lpCommand->mpDac;

    if (!sCapNumModes)
    {
        lpDac->maAttributes[0].mfValue = 1.0f;   // no capabilities: park stereo
    }
    else
    {
        const f32 lfRequested = lpCommand->mfValue;   // lfs 8(r3)
        const int KAI_PREFERENCE[4] = { 3, 2, 1, 0 };

        // The requested mode's slot in the preference order (0 when absent).
        int liStart = 0;
        for (int liSlot = 0; liSlot < 4; ++liSlot)
        {
            if (lfRequested == static_cast<f32>(KAI_PREFERENCE[liSlot]))
            {
                liStart = liSlot;
                break;
            }
        }

        // First preference from that slot onward that the capability list carries.
        bool lbMatched = false;
        for (int liSlot = liStart; liSlot < 4 && !lbMatched; ++liSlot)
        {
            for (u32 luEntry = 0; luEntry < sCapNumModes; ++luEntry)
            {
                if (KAI_PREFERENCE[liSlot] == sCapModes[luEntry])
                {
                    lpDac->maAttributes[0].mfValue =
                        static_cast<f32>(KAI_PREFERENCE[liSlot]);
                    lbMatched = true;
                    break;
                }
            }
        }

        sChannels = SKAU8_MODE_CHANNELS[static_cast<int>(lpDac->maAttributes[0].mfValue)];
        if (sStarted)
        {
            StopImmediate(lpDac);
            StartImmediate(lpDac);
        }
    }
    return static_cast<int>(sizeof(DacValueCommand));   // console li r3, 0xC
}

// SetSampleRateHandler @0x82B9DCE8 -- consume the record (only 48 kHz exists).
int Dac::SetSampleRateHandler(void * /*apCommand*/)
{
    return static_cast<int>(sizeof(DacValueCommand));   // console li r3, 0xC
}

// =====================================================================================
// RampOutput @0x82B96CB8 -- linear gain ramp over the first `aiFrames` frames: up
// (counter 0,1,..) after a start, down (counter N,N-1,..) toward a stop; each frame's
// gain = counter / aiFrames. The console re-reads the live channel count every sample
// (it can change under SetModeHandler) -- reproduced. (The console r3 return is
// address-materialisation residue; dropped.)
// =====================================================================================
void Dac::RampOutput(Dac * /*self*/, f32 *apFrame, int aiFrames, u8 au8RampUp)
{
    u8 lu8Channels = sChannels;
    f32 *lpEnd = apFrame + static_cast<int>(lu8Channels) * aiFrames;
    const f32 lfInvFrames = 1.0f / static_cast<f32>(aiFrames);
    f32 lfCounter = au8RampUp ? 0.0f : static_cast<f32>(aiFrames);

    while (apFrame < lpEnd)
    {
        const f32 lfGain = lfCounter * lfInvFrames;
        for (int liChannel = 0; liChannel < lu8Channels; ++liChannel)
        {
            apFrame[liChannel] *= lfGain;
            lu8Channels = sChannels;   // the console's per-sample re-read
        }
        lfCounter += au8RampUp ? 1.0f : -1.0f;
        apFrame += lu8Channels;
    }
}

// =====================================================================================
// XenonDownMix @0x82B97178 -- interleave the mixer's src slot (the latest stage
// output) into the static output frame, apply the armed start ramp once, clip. The
// name is the console's; despite it, NO 6->2 fold happens here -- the fold belongs to
// the platform output leaf.
// =====================================================================================
void Dac::XenonDownMix(Dac *self)
{
    ReOrderRwAudioCoreToWave(sInterleaveBuffer, self->mpMixer->mpSrcBuffer,
                             sChannels, Mixer::KU_FRAME_SIZE);
    if (sStartRequested)
    {
        RampOutput(self, sInterleaveBuffer, 128, 1);
        sStartRequested = 0;
    }
    // The console clips ALL 0x600 floats regardless of the live channel count.
    ClipFloats(sInterleaveBuffer, -1.0f, 1.0f, 0x600);
}

// Process @0x82B97250 -- the descriptor's pProcess (the DAC voice's terminal stage).
int Dac::Process(Dac *self)
{
    XenonDownMix(self);
    return 1;
}

// =====================================================================================
// Mix @0x82B96E80 (raw-XEX recovery) -- one engine mix frame; see Dac.h.
// =====================================================================================
int Dac::Mix(Dac *self)
{
    CpuLoadBalancer::Balance(&sCpuLoadBalancer);

    System *lpSystem = self->mpSystemUseGetSystemAccessor;
    sMixerExecuteParams.mfSystemTime = lpSystem->mfSystemTime;         // lfd +0x08
    sMixerExecuteParams.mpVoiceListNodes = lpSystem->mppVoiceListNodes; // lwz +0x58
    sMixerExecuteParams.mfSampleRate = 48000.0f;                       // flt_820AA808
    sMixerExecuteParams.muNumVoices = lpSystem->muActiveVoiceCount;    // lhz +0x10F4
    // (mu8NumPlugInsRegistered is NOT written -- faithful.)
    sMixerExecuteParams.mbVerifyFloats =
        (off_83271928->maucDebugFeatures[2] != 0) ? 1 : 0;             // lbz +0x10FD fold

    int liStatus = Mixer::Execute(self->mpMixer, &sMixerExecuteParams);

    lpSystem->muMixerCpuTicks = self->mpMixer->muMixerCpuCycles;       // +0x3001C -> +0x10D0
    lpSystem->mfSystemTime = lpSystem->mfSystemTimerPeriod + lpSystem->mfSystemTime;
    sCpuLoadBalancer.muLastStamp = GetCpuCycle();
    return liStatus;
}

// =====================================================================================
// ~Dac (vt[0]) == ReleaseEvent @0x82B9DAE0 -- teardown.
// =====================================================================================
Dac::~Dac()
{
    sThreadRunning = 0;
    if (!sExternalDacMode)
    {
        // Console-only: XAudioVoice_Release(+0x40) + null it; XAudioFreeEvent(+0x304C)
        // + null it; XAudioShutDown.
    }
    u32 luZeroThreadId = 0;
    System::SetRwAudioCoreThreadId(mpSystemUseGetSystemAccessor, &luZeroThreadId);
    StopImmediate(this);
    if (mpMixer)
        System::Free(mpSystemUseGetSystemAccessor, mpMixer, 0);
}

// =====================================================================================
// The external-fill support surface (PC; consumed by CgsDacOutputPC.cpp). These are
// PC-home accessors over this TU's statics -- the console equivalents are the direct
// static reads XenonThread performs in-TU.
// =====================================================================================
u8 DacIsEngineRunning() { return sThreadRunning; }
u8 DacIsStarted() { return sStarted; }
const f32 *DacGetInterleaveBuffer() { return sInterleaveBuffer; }
u8 DacGetChannelCount() { return sChannels; }

} // namespace core
} // namespace audio
} // namespace rw
