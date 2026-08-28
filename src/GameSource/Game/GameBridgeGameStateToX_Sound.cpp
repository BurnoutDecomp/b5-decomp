// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX_Sound.cpp
//
// SIBLING SPLIT of GameBridgeGameStateToX.cpp (the established precedent --
// see GameBridgeGameStateToX_TrainingStringIds.cpp and the build bat's note:
// the parent TU is not mountable). BridgeGameStateToSound gained its caller
// with the faithful-audio-engine phase C4b in-game leg (DoUpdate_Sound
// @0x823DCEC0 is its ONLY console call site), so it is MOVED -- not copied --
// here so it can be mounted on its own. Folding it back later is a delete.
//
// Bodied here (1 ledger function):
//   BrnGame::BrnGameModule::BridgeGameStateToSound @ 0x823CDE50  (DWARF home
//   GameSource/Game/GameBridgeGameStateToX.cpp)
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"   // RootInputBuffer + the interface twins
#include "GameSource/GameState/BrnGameStateModuleIO.h"      // GameStateModuleIO::OutputBuffer accessors

namespace BrnGame
{
    // @ 0x823CDE50 (bodied 2026-08-25, faithful-audio-engine phase C3b). The
    // game-state -> sound input bridge (the caller holds the game-state output's
    // read lock + the sound input's write lock):
    //   [1] the 13312 game-action queue append (both ends are the same
    //       VariableEventQueue<13312,16> instantiation -- the console append
    //       symbol pins them; the root side's GetGameActionQueue write accessor).
    //   [2] the three interface installs -- game-mode (+176344), scoring
    //       (+173240), online-scoring (+175976), each opaque view cast onto its
    //       root-side twin (same console record, spans match).
    //   [3] the UpdateInfo byte: the two +1009411x game-module flags OR'd (their
    //       writers are un-decoded -- both stay false, publishing 0, the
    //       boot-state value).
    void BrnGameModule::BridgeGameStateToSound(
        BrnSound::Module::Io::RootInputBuffer* lpSoundInputBuffer,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutputBuffer)
    {
        typedef BrnSound::Module::Io::RootInputBuffer RootIn;

        lpSoundInputBuffer->GetGameActionQueue().Append(
            *lpGameStateOutputBuffer->GetGameActionQueue());

        lpSoundInputBuffer->SetGameModeInterface(
            reinterpret_cast<const RootIn::GameModeOutputInterface*>(
                lpGameStateOutputBuffer->GetGameModeOutputInterface()));
        lpSoundInputBuffer->SetScoringInterface(
            reinterpret_cast<const RootIn::ScoringOutputInterface*>(
                lpGameStateOutputBuffer->GetScoringOutputInterface()));
        lpSoundInputBuffer->SetOnlineScoringInterface(
            reinterpret_cast<const RootIn::OnlineScoringOutputInterface*>(
                lpGameStateOutputBuffer->GetOnlineScoringOutputInterface()));

        RootIn::UpdateInfo lUpdateInfo;
        lUpdateInfo.mData[0] = (mbField10094120 || mbField10094119) ? 1 : 0;
        lpSoundInputBuffer->SetUpdateInfo(&lUpdateInfo);
    }
}
