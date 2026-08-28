// =====================================================================================
// rw::audio::core::Voice -- member-function bodies.
//
// EARenderWare "rwaudio" mixing voice. Reconstructed from BURNOUT_X360_ARTIST.XEX; the
// PowerPC asm is authoritative for every store, offset and side-effect (see the byte-layout
// comment in rw/audio/core/Voice.h). No Feb-2007 leak source and no DecFIGS DWARF exist for
// this type. Each body is a store-for-store translation of the cited disassembly, with the
// hand-rolled in-block sub-allocation restated through offsetof/sizeof/alignment.
// =====================================================================================

#include "rw/audio/core/Voice.h"

#include <cstddef> // offsetof, size_t
#include <cstdint> // uintptr_t
#include <cstring> // std::memmove

// XMemCpy -- the Xbox 360 platform block copy (defined in the platform CRT; declared here,
// as in Decoder.cpp). Byte-wise copy, dst/src/count.
extern "C" void *XMemCpy(void *pDest, const void *pSrc, unsigned int uiCount);

namespace rw
{
namespace audio
{
namespace core
{

// Round `v` up to a multiple of `a` (a power of two).
static inline uintptr_t AlignUp(uintptr_t v, uintptr_t a)
{
    return (v + (a - 1)) & ~(a - 1);
}

// (The former VoiceStageDesc local view over the then-opaque PlugInDescRunTime is
// RETIRED -- descriptor-record wave 2026-08-28: PlugIn.h types the record in full,
// and the console-offset pad the view carried would have misread a host-width
// record past the pointer block. Reads below go through the real type BY NAME;
// the GetSize dispatch cast happens at the one call site, exactly where the
// console's generic dispatch is.)
typedef u32 (*VoiceStageGetSizeFn)(const VoiceStageConfig *config);

// The deferred-command records the Voice methods push into the System ring buffer. Each is
// { handler, voice[, payload] }; replayed later by the matching *Handler.
//
// RECORD-STRIDE RULE (X360-literal trap): for every record below the producer's cursor
// advance and the handler's return are BOTH that record's size. The X360 immediates
// (8 / 8 / 0xC) are the CONSOLE sizeofs, computed with 4-byte pointers; the host records
// are 16 / 16 / 24 because the handler and voice pointers widened. Both sides therefore
// use `sizeof(...)`, never the console literal -- otherwise each enqueue overruns its
// reserved stride and the replay walk dispatches through the previous record's tail.
struct VoiceCreateCommand
{
    int (*mpHandler)(void *); // +0x00 -- &Voice::CreateInstanceHandler
    Voice *mpVoice;           // +0x04
};
struct VoiceReleaseCommand
{
    int (*mpHandler)(void *); // +0x00 -- &Voice::ReleaseHandler
    Voice *mpVoice;           // +0x04
};
struct VoiceSetPriorityCommand
{
    int (*mpHandler)(void *); // +0x00 -- &Voice::SetPriorityHandler
    Voice *mpVoice;           // +0x04
    f32 mfPriority;           // +0x08
};

// -------------------------------------------------------------------------------------
// Voice::CreateInstance @0x82B6EC50
// -------------------------------------------------------------------------------------
Voice *Voice::CreateInstance(u8 priority, int numStages, VoiceStageConfig *configs,
                             PlugIn ***outPlugIns, System *system)
{
    // First pass: total block size = header + pointer array (8-aligned) + stage records,
    // then each stage's plug-in instance packed 16-aligned.
    u32 blockSize =
        static_cast<u32>(AlignUp(offsetof(Voice, mpPlugIns) +
                                     static_cast<uintptr_t>(numStages) * sizeof(PlugIn *),
                                 8)) +
        static_cast<u32>(numStages) * sizeof(VoiceStageData);
    for (int i = 0; i < numStages; ++i)
    {
        PlugInDescRunTime *desc = reinterpret_cast<PlugInDescRunTime *>(configs[i].mpDesc);
        u32 stageSize = reinterpret_cast<VoiceStageGetSizeFn>(desc->pGetSize)(&configs[i]);
        blockSize = stageSize + static_cast<u32>(AlignUp(blockSize, 16));
    }

    Voice *voice = 0;
    System::New2<Voice>(system, &voice, 0, blockSize, 16, 0);
    if (!voice)
        return 0;

    voice->muBlockSize = blockSize;

    // Zero the trailing stage-instance pointer array.
    for (int i = 0; i < numStages; ++i)
        voice->mpPlugIns[i] = 0;

    // The stage-record array sits 8-aligned immediately after the pointer array.
    VoiceStageData *stageData = reinterpret_cast<VoiceStageData *>(
        AlignUp(reinterpret_cast<uintptr_t>(&voice->mpPlugIns[numStages]), 8));

    // Fixed-header init (matches the store block at 0x82B6ED18..0x82B6EDB4).
    voice->mpName = "Unknown";
    voice->mpSystem = system;
    voice->mucNumStages = static_cast<u8>(numStages);
    voice->mucFlag45 = 0;
    voice->mucState = 0;
    voice->mucPriorityClass = priority;
    voice->mfVolume = 1.0f;
    voice->mfFadeStart = 0.0f;
    voice->mfParam2C = 0.0f;
    voice->mfFadeEnd = 0.0f;
    voice->muCreateStamp = system->muFrameCounter;
    voice->mfPriority = 100.0f;
    voice->mcSourceStageIndex = -1;
    voice->miLastFrameCpuTicks = 0;
    voice->mfAverageCpuTicks = 800.0f;      // the fresh-voice deprioritisation seed
    voice->mafCpuTickHistory[0] = 800.0f;
    voice->mafCpuTickHistory[1] = 800.0f;
    voice->mpStageData = stageData;
    voice->mucExpelImmediate = 0;

    // Second pass: placement-construct each stage's PlugIn into the packed 16-aligned
    // region after the stage-record array.
    uintptr_t placementCursor = reinterpret_cast<uintptr_t>(stageData) +
                                static_cast<uintptr_t>(numStages) * sizeof(VoiceStageData);
    u8 flag = 0;
    for (int i = 0; i < numStages; ++i)
    {
        PlugInDescRunTime *desc = reinterpret_cast<PlugInDescRunTime *>(configs[i].mpDesc);
        VoiceStageData *stage = &voice->mpStageData[i];

        if (desc->mu8PlugInType <= 3)
            voice->mcSourceStageIndex = static_cast<s8>(i);

        u16 sampleCount = static_cast<u16>(
            reinterpret_cast<VoiceStageGetSizeFn>(desc->pGetSize)(&configs[i]));
        placementCursor = AlignUp(placementCursor, 16);
        stage->muSampleCount = sampleCount;
        PlugIn *placement = reinterpret_cast<PlugIn *>(placementCursor);
        placementCursor += sampleCount;

        PlugIn *instance = PlugIn::CreateInstance(placement, voice, configs[i].mpDesc,
                                                  &configs[i], static_cast<char>(flag));
        voice->mpPlugIns[i] = instance;
        if (!instance)
        {
            Voice::ReleaseImmediate(voice, 1);
            return 0;
        }
        stage->mpProcess = desc->pProcess;
        stage->mpPreProcess = desc->pPreProcess;
        flag = static_cast<u8>(configs[i].mFlagAndField8);
    }

    // Success: hand back the plug-in array and enqueue the create-handler command.
    *outPlugIns = voice->mpPlugIns;
    u32 cursor = system->muDeferredRingCursor;
    VoiceCreateCommand *cmd =
        reinterpret_cast<VoiceCreateCommand *>(system->mpDeferredRingBase + cursor);
    // X360 `addi r11,r11,8` @0x82B6EE74 == console sizeof(VoiceCreateCommand); host = 16.
    system->muDeferredRingCursor = cursor + static_cast<u32>(sizeof(VoiceCreateCommand));
    cmd->mpHandler = &Voice::CreateInstanceHandler;
    cmd->mpVoice = voice;
    return voice;
}

// -------------------------------------------------------------------------------------
// Voice::CreateInstanceHandler @0x82B6C020
// -------------------------------------------------------------------------------------
int Voice::CreateInstanceHandler(void *cmd)
{
    Voice *voice = static_cast<VoiceCreateCommand *>(cmd)->mpVoice;
    System *system = voice->mpSystem;

    if (system->muActiveVoiceCount >= system->muActiveVoiceCapacity)
    {
        // Active-voice array is full: grow it by 32 slots.
        u16 oldCapacity = system->muActiveVoiceCapacity;
        u16 newCapacity = static_cast<u16>(oldCapacity + 32);
        u32 oldBytes = static_cast<u32>(oldCapacity) * sizeof(VoiceActiveNode);
        VoiceActiveNode *newNodes = static_cast<VoiceActiveNode *>(system->mpAllocator->Alloc(
            static_cast<size_t>(newCapacity) * sizeof(VoiceActiveNode),
            "rw::audio::core::System::mpVoiceListNodes", 1, 16, 0));
        if (!newNodes)
        {
            // Allocation failed: mark the voice expelled and park it on the expelled list.
            voice->mucState = 2;
            voice->mucExpelImmediate = 1;
            VoiceListLink *node = &voice->mExpelLink;
            VoiceListLink *head = system->mpExpelledVoiceList;
            node->mpPrev = 0;
            node->mpNext = head;
            if (head)
                head->mpPrev = node;
            system->mpExpelledVoiceList = node;
            // Same record size as the success path (the asm branches back to the shared
            // `li r3,8` @0x82B6C158): console sizeof(VoiceCreateCommand), host 16.
            return static_cast<int>(sizeof(VoiceCreateCommand));
        }
        XMemCpy(newNodes, system->mppVoiceListNodes, oldBytes);
        system->mpAllocator->Free(system->mppVoiceListNodes, 0);
        system->mppVoiceListNodes = newNodes;
        system->muActiveVoiceCapacity = newCapacity;
    }

    // Insert the voice in priority-class order (skip past nodes with a lower class).
    u32 index = 0;
    if (system->muActiveVoiceCount)
    {
        VoiceActiveNode *nodes = system->mppVoiceListNodes;
        while (voice->mucPriorityClass > nodes[index].mpVoice->mucPriorityClass)
        {
            ++index;
            if (index >= system->muActiveVoiceCount)
                break;
        }
    }

    VoiceActiveNode *nodes = system->mppVoiceListNodes;
    std::memmove(&nodes[index + 1], &nodes[index],
                 sizeof(VoiceActiveNode) * (system->muActiveVoiceCount - index));
    nodes[index].mpVoice = voice;
    nodes[index].muKey = voice->muBlockSize;
    ++system->muActiveVoiceCount;
    return static_cast<int>(sizeof(VoiceCreateCommand)); // X360: li r3,8 @0x82B6C158
}

// -------------------------------------------------------------------------------------
// Voice::ExpelAfterDecay @0x82B6BFD8
// -------------------------------------------------------------------------------------
Voice *Voice::ExpelAfterDecay(Voice *self)
{
    if (self->mucState)
        return self;

    System *system = self->mpSystem;
    self->mfFadeEnd = self->mfFadeStart;
    self->mucState = 1;
    system->mpExpelAfterDecayList[system->muExpelAfterDecayCount] = self;
    ++system->muExpelAfterDecayCount;
    return self;
}

// -------------------------------------------------------------------------------------
// Voice::ExpelImmediate @0x82B6E030
// -------------------------------------------------------------------------------------
Voice *Voice::ExpelImmediate(Voice *self, u8 immediate)
{
    if (self->mucState == 2)
        return self;

    u8 numStages = self->mucNumStages;
    self->mucExpelImmediate = immediate;
    self->mfAverageCpuTicks = 0.0f;
    self->miLastFrameCpuTicks = 0;
    self->mafCpuTickHistory[0] = 0.0f;
    self->mucState = 2;
    self->mafCpuTickHistory[1] = 0.0f;

    for (u8 i = 0; i < numStages; ++i)
        self->mpPlugIns[i]->mCpuTicks = 0;

    // Drop it from the active list (side effects only; the asm leaves RemoveActiveVoice's
    // int result in r3 as a dead value that every caller ignores).
    Voice::RemoveActiveVoice(self);

    // Re-link the voice at the head of the System's expelled list.
    System *system = self->mpSystem;
    VoiceListLink *head = system->mpExpelledVoiceList;
    self->mExpelLink.mpPrev = 0;
    self->mExpelLink.mpNext = head;
    if (head)
        head->mpPrev = &self->mExpelLink;
    system->mpExpelledVoiceList = &self->mExpelLink;
    return self;
}

// -------------------------------------------------------------------------------------
// Voice::Release @0x82B6EEA0 -- queue the release-handler command (one
// VoiceReleaseCommand slot -- 8 bytes on the X360).
// -------------------------------------------------------------------------------------
Voice *Voice::Release(Voice *self)
{
    System *system = self->mpSystem;
    u32 cursor = system->muDeferredRingCursor;
    VoiceReleaseCommand *cmd =
        reinterpret_cast<VoiceReleaseCommand *>(system->mpDeferredRingBase + cursor);
    // X360 `addi r10,r10,8` == console sizeof(VoiceReleaseCommand); host = 16.
    system->muDeferredRingCursor = cursor + static_cast<u32>(sizeof(VoiceReleaseCommand));
    cmd->mpHandler = &Voice::ReleaseHandler;
    cmd->mpVoice = self;
    return self;
}

// -------------------------------------------------------------------------------------
// Voice::ReleaseHandler @0x82B6E0F8 -- deferred: ReleaseImmediate(voice, 0).
// -------------------------------------------------------------------------------------
int Voice::ReleaseHandler(void *cmd)
{
    Voice *voice = static_cast<VoiceReleaseCommand *>(cmd)->mpVoice;
    Voice::ReleaseImmediate(voice, 0);
    return static_cast<int>(sizeof(VoiceReleaseCommand)); // X360: li r3,8
}

// -------------------------------------------------------------------------------------
// Voice::ReleaseImmediate @0x82B6DF38
// -------------------------------------------------------------------------------------
Voice *Voice::ReleaseImmediate(Voice *self, u8 keepInList)
{
    for (u8 i = 0; i < self->mucNumStages; ++i)
    {
        PlugIn *stage = self->mpPlugIns[i];
        if (stage)
        {
            stage->~PlugIn();  // vt[0]
            stage->Destroy(0); // vt[3]
        }
    }

    // If the voice was not on the active list (and we are not asked to keep it linked),
    // unlink it from the expelled list.
    if (!keepInList && !Voice::RemoveActiveVoice(self))
    {
        System *system = self->mpSystem;
        VoiceListLink *node = &self->mExpelLink;
        if (node == system->mpExpelledVoiceList)
            system->mpExpelledVoiceList = node->mpNext;
        if (node->mpPrev)
            node->mpPrev->mpNext = node->mpNext;
        if (node->mpNext)
            node->mpNext->mpPrev = node->mpPrev;
    }

    // Free the voice block back through the System allocator (the X360 r3 return is the
    // untouched allocator pointer, unused by every caller).
    self->mpSystem->mpAllocator->Free(self, 0);
    return self;
}

// -------------------------------------------------------------------------------------
// Voice::RemoveActiveVoice @0x82B6C1A8
// -------------------------------------------------------------------------------------
int Voice::RemoveActiveVoice(Voice *self)
{
    System *system = self->mpSystem;
    if (!system->muActiveVoiceCount)
        return 0;

    u32 index = 0;
    VoiceActiveNode *nodes = system->mppVoiceListNodes;
    while (nodes[index].mpVoice != self)
    {
        if (++index >= system->muActiveVoiceCount)
            return 0;
    }

    System::RemoveVoiceFromExpulsionCandidateList(system, self);

    --system->muActiveVoiceCount;
    VoiceActiveNode *base = system->mppVoiceListNodes;
    std::memmove(&base[index], &base[index + 1],
                 sizeof(VoiceActiveNode) * (system->muActiveVoiceCount - index));
    return 1;
}

// -------------------------------------------------------------------------------------
// Voice::SetPriority @0x82B6EED0 -- queue the set-priority command (one
// VoiceSetPriorityCommand slot -- 12 bytes on the X360).
// -------------------------------------------------------------------------------------
Voice *Voice::SetPriority(Voice *self, f32 priority)
{
    System *system = self->mpSystem;
    u32 cursor = system->muDeferredRingCursor;
    VoiceSetPriorityCommand *cmd =
        reinterpret_cast<VoiceSetPriorityCommand *>(system->mpDeferredRingBase + cursor);
    // X360 `addi r9,r9,0xC` == console sizeof(VoiceSetPriorityCommand) ({4,4,4}); the host
    // record is 24 (widened pointers + tail padding to the 8-byte alignment).
    system->muDeferredRingCursor = cursor + static_cast<u32>(sizeof(VoiceSetPriorityCommand));
    cmd->mfPriority = priority;
    cmd->mpHandler = &Voice::SetPriorityHandler;
    cmd->mpVoice = self;
    return self;
}

// -------------------------------------------------------------------------------------
// Voice::SetPriorityHandler @0x82B6E128 -- deferred: write the queued priority.
// -------------------------------------------------------------------------------------
int Voice::SetPriorityHandler(void *cmd)
{
    VoiceSetPriorityCommand *c = static_cast<VoiceSetPriorityCommand *>(cmd);
    c->mpVoice->mfPriority = c->mfPriority;
    return static_cast<int>(sizeof(VoiceSetPriorityCommand)); // X360: li r3,0xC
}

} // namespace core
} // namespace audio
} // namespace rw
