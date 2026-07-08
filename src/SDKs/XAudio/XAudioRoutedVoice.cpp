#include "SDKs/XAudio/XAudioRoutedVoice.h"

#include "SDKs/XAudio/XAudioXMemMemoryManager.h" // gXMemMemoryManager, KU_XMEM_FREE_ATTRIBUTES

// ===========================================================================
// XAUDIO::CRoutedVoice -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// See XAudioRoutedVoice.h for the layout and asm notes. No reference source and
// no DWARF exist for this TU; the PowerPC asm is authoritative.
//
// This wave lands the two self-contained members. The remaining CRoutedVoice
// methods depend on collaborators this build has not yet homed and are left for
// later waves:
//   - AttachOutputVoice / GetObjectAdditionalSize need the XAudio engine
//     singleton (`off_832BB944`, an un-homed XAUDIO::CEngine: default output at
//     +0x40, effect manager at +0x3C), the default-output-config global
//     (`unk_821B5764`), and the XAudioGetDefaultChannelMap /
//     XAudioEffectManager_QueryEffectSize helpers.
//   - Initialize / SetVoiceFormat dispatch through the CVoice allocator vtable
//     and the CRoutedVoice own-vtable slot 9, and call the still-todo
//     CVoice::Initialize / CreateEffect -- neither vtable order is reconstructed.
//   - DetachOutputVoices / ProcessEffects / SetVoiceOutputVolume / ~CRoutedVoice
//     dispatch through the destination-voice and per-effect vtables (CVoice /
//     CEffect vtable order deliberately deferred by those TUs).
//   - OnStopVoice uses the Xbox kernel spinlock/IRQL primitives
//     (KeRaiseIrqlToDpcLevel / KeAcquireSpinLockAtRaisedIrql / ...) and their
//     un-homed global lock state.
// SetVoiceOutput reaches none of that: it drives only its own (declared) Attach/
// Detach helpers plus named members, so it is fully groundable now.
// ===========================================================================

namespace XAUDIO
{

// @ 0x82C31C28:
//   v4 = 0;
//   DetachOutputVoices();
//   if (apConfig) {
//       if (apConfig->muCount) {
//           for (i = 0; ; ) {                       ; while (v4 >= 0)
//               v4 = AttachOutputVoice(&entries[i], &routes[i]);
//               if (++i >= muCount) break;
//           }
//           if (v4 < 0) return v4;                  ; loc_82C31CB0 (no count store)
//       }
//       muNumActiveOutputs = apConfig->muCount;     ; loc_82C31CA8
//       return v4;
//   }
//   if (!muNumRoutes) return v4;                    ; == 0
//   v4 = AttachOutputVoice(0, &routes[0]);          ; single default output
//   if (v4 >= 0) muNumActiveOutputs = 1;
//   return v4;
s32 CRoutedVoice::SetVoiceOutput(const SVoiceOutputConfig* apConfig)
{
    s32 liResult = 0;

    // Tear down whatever outputs are currently attached (return value ignored,
    // matching the asm which pre-seeds the result accumulator to 0).
    DetachOutputVoices();

    if (apConfig != nullptr)
    {
        if (apConfig->muCount != 0)
        {
            u32 luIndex = 0;
            while (liResult >= 0)
            {
                liResult = AttachOutputVoice(&apConfig->mpEntries[luIndex],
                                             &mpRoutes[luIndex]);
                ++luIndex;
                if (luIndex >= apConfig->muCount)
                    break;
            }

            // A failed attach returns without recording the active-output count.
            if (liResult < 0)
                return liResult;
        }

        // Reached with muCount == 0 (nothing attached) or after a fully-successful
        // attach loop: record the number of active outputs.
        muNumActiveOutputs = apConfig->muCount;
        return liResult;
    }

    // Null config: attach the single default output, if this voice has routes.
    if (muNumRoutes == 0)
        return liResult; // 0

    liResult = AttachOutputVoice(nullptr, &mpRoutes[0]);
    if (liResult >= 0)
        muNumActiveOutputs = 1;

    return liResult;
}

// @ 0x82C31EC8 (free arm): release `this` through the XAudio XMem manager
// singleton with the baked attribute word. The matching destruction (the
// `~CRoutedVoice` arm of the scalar deleting destructor) is deferred with the
// destructor body above.
void CRoutedVoice::operator delete(void* pMemory)
{
    gXMemMemoryManager.XMemFree(pMemory, KU_XMEM_FREE_ATTRIBUTES);
}

} // namespace XAUDIO
