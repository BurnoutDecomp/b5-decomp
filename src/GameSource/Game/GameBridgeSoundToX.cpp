// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeSoundToX.cpp
//
// BrnGame::BrnGameModule sound-output bridge family -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (signature/locals/callee names verbatim from the PS3
// DecFIGS BrnGameUnity.cpp dump; DWARF home GameSource/Game/GameBridgeSoundToX.cpp).
//
// Bodied here (1 ledger function):
//   BrnGameModule::BridgeSoundToTraining @ 0x823C63C0   (GameBridgeSoundToX.cpp:103)
// The family's other bridges (BridgeSoundToResource :41 / BridgeSoundToGuiPreUpdate
// :134 / ...) are their own ledger functions, reconstructed later into this TU.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"

#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"               // RootPreUpdateOutputBuffer / PreUpdateOutput
#include "GameSource/Sound/Module/SharedIO/BrnPreUpdateSharedIo.h"      // AudioEffectsMessageQueue
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"    // BrnGameState::TrainingManager
#include "GameSource/World/BrnWorldModuleIO.h"                          // BrnWorldIO::UpdateInputBuffer (BridgeSoundToWorld; phase C3)

namespace BrnGame
{
    namespace
    {
        // The audio-effects message id the bridge reacts to (queue message ids' own
        // enum home is not reconstructed; the id 2 is pinned by the X360 `cmpwi 2`).
        const s32 KI_AUDIO_EFFECTS_MESSAGE_VOICEOVER_FINISHED = 2;
    }

    // @ 0x823C63C0 -- walk the pre-update audio-effects message queue; each
    // voiceover-finished message notifies the training manager.
    //
    // NOTE (X360-faithful quirk): liEventType is assigned ONLY from GetFirstEvent --
    // the X360 discards GetNextEvent's returned id (`mr r27,r3` happens once; the
    // loop's `cmpwi cr6,r27,2` re-tests the FIRST event's id every iteration). The
    // original source evidently never re-assigned the type local, so the
    // voiceover-finished branch fires per-event only when the FIRST queued message
    // is a voiceover-finished one. Reproduced exactly; do NOT "fix" the loop.
    void BrnGameModule::BridgeSoundToTraining(
        BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundOutputBuffer)
    {
        const BrnSound::Module::Io::AudioEffectsMessageQueue& lQueue =                    // :105
            lpSoundOutputBuffer->GetPreUpdateOutput().GetAudioEffectsMessageQueue();

        const CgsModule::Event* lpEvent = 0;                                              // :108
        s32 liEventSize = 0;                                                              // :109
        const s32 liEventType = lQueue.GetFirstEvent(&lpEvent, &liEventSize);             // :110
        while (lpEvent)
        {
            if (liEventType == KI_AUDIO_EFFECTS_MESSAGE_VOICEOVER_FINISHED)
                mpTrainingManager->OnVoiceoverFinished();
            lQueue.GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }
    }

    // @ 0x823CDC98 (GameBridgeSoundToX.cpp:81; bodied 2026-08-25, faithful-audio-
    // engine phase C3). The sound -> world pre-update bridge: the world input's
    // audio-car-loaded queue takes the sound pre-update block's queue (+0x110 --
    // the loaded-car notifications the world raised last frame, echoed back
    // through the sound side's pre-update output). Caller holds the world input's
    // write lock + the sound output's read lock (the spine's bracket).
    void BrnGameModule::BridgeSoundToWorld(
        BrnWorldIO::UpdateInputBuffer* lpWorldInputBuffer,
        BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundOutputBuffer)
    {
        CGS_ASSERT(lpWorldInputBuffer != 0, "lpWorldInput");
        CGS_ASSERT(lpSoundOutputBuffer != 0, "lpSoundOutputBuffer");

        lpWorldInputBuffer->GetAudioCarDataLoadedQueue()->Append(
            lpSoundOutputBuffer->GetPreUpdateOutput().GetCarDataLoadedQueue());
    }

}
