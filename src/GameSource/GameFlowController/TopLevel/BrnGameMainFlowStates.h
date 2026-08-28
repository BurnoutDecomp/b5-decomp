#pragma once

#include "types.hpp"
#include "SharedClasses/BrnSharedConstants.h"   // BrnUpdateSet (the loading update set)

// Top-level game-flow states. Hierarchy reconstructed from the X360 ARTIST build
// (GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h):
//
//   MainGameFlowState (abstract base: OnEnter/OnLeave/Update/Render)
//     +-- LoadingScriptedState (scripted module-by-module load; adds FinishLoading)
//     |     +-- MainGameFlowStateInitialLoadingScreen   (the boot loading screen)
//     |     +-- MainGameFlowStateStartScreen
//     |     +-- MainGameFlowStateMarketingScreens
//     |     +-- MainGameFlowStateCheckDiskSpace
//     |     +-- MainGameFlowStateMemoryCard
//     |     +-- MainGameFlowStateCompleteLoading
//     +-- MainGameFlowStateInGame
//
// The per-frame update spine (BrnGameModule::GameMain) drives the active state's Update()
// (vtable +8) each simulation sub-step and its Render() (vtable +12) once per frame. The
// scripted-load helper methods (LoadXxxModule / RenderGUI / Suspend/ResumeLoadingWorld) are
// non-virtual and omitted here (they don't affect layout/vtable); they are reconstructed
// with each state's OnEnter/Update body.
//
// The boot loading-screen commands ride the REAL dispatch-thread input pair now: the
// flow states and BridgeGuiToGame post onto the game module's write buffer
// (GetDispatchThreadInputBufferManager().GetWriteBuffer()->Show/HideLoadingScreen()),
// OnEndOfUpdateFrame swaps it, and BrnRendererModule::Render forwards the read buffer's
// one-shot command into LoadingScreenRenderer::AddCommand -- see
// GameSource/Game/BrnDispatchThreadInputBuffer.h.

namespace rw { namespace core { struct GeneralResourceAllocator; } }

// The GameData per-frame IO payloads the scripted-load helpers thread through (pointer-only
// here; the helper bodies include the real home, GameSource/Resource/BrnGameDataModuleIO.h).
namespace BrnResource { namespace GameDataIO { struct InputBuffer; struct OutputBuffer; } }

// The sound pre-update payload the spine threads into the world drive for BridgeSoundToWorld
// (pointer-only here; real home GameSource/Sound/Module/BrnRootSoundModuleIo.h).
namespace BrnSound { namespace Module { namespace Io { struct RootPreUpdateOutputBuffer; } } }

namespace BrnGameMainFlowController
{
    // The GameData IO pair the scripted-load spine brackets every frame.
    //
    // [PC placement] the X360 keeps this pair as GAME-MODULE members (gm+10055440 =
    // the input, gm+10055444 = the output; BrnGameModule::GamePrepare @0x823EFBD0 opens
    // with LockForWrite(gm+10055440) / LockForRead(gm+10055444) and the scripted-load
    // spine @0x823F22D8 brackets the same pair). The PC host owns one constructed-once
    // pair here instead. GamePrepare needs the SAME pair -- its LoadBundle requests have
    // to reach the one GameDataModule pump -- so it is published through these accessors
    // rather than duplicated. Both accessors bring the pair up on first use (the
    // construction has to be idempotent because GamePrepare runs on the update thread
    // BEFORE the first flow-state Update of the boot frame).
    BrnResource::GameDataIO::InputBuffer*  GetScriptedLoadGameDataInput();
    BrnResource::GameDataIO::OutputBuffer* GetScriptedLoadGameDataOutput();
}

// --- base -------------------------------------------------------------------------------
struct MainGameFlowState
{
    MainGameFlowState();

    virtual void OnEnter();   // vtable +0
    virtual void OnLeave();   // vtable +4
    virtual void Update();    // vtable +8
    virtual void Render();    // vtable +12
};

// --- scripted-load intermediate ---------------------------------------------------------
struct LoadingScriptedState : public MainGameFlowState
{
    enum ELoadingStateStage
    {
        E_LOADINGSTATESTAGE_START = 0,
        E_LOADINGSTATESTAGE_SOUND_AGAIN = 1,
        E_LOADINGSTATESTAGE_EFFECTS = 2,
        E_LOADINGSTATESTAGE_WORLD = 3,
        E_LOADINGSTATESTAGE_SOUND_BINDPROPS = 4,
        E_LOADINGSTATESTAGE_WORLD_COLLISION = 5,
        E_LOADINGSTATESTAGE_DONE = 6,
        E_LOADINGSTATE_NUM = 7,
    };

    LoadingScriptedState();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void Render();
    virtual void FinishLoading();   // vtable +16

protected:
    // X360 0x823E75A8 (DWARF BrnGameMainFlowStates.h:58 / BrnLoadingScriptedState.cpp:733).
    // One frame of the sound-module load: create the Root sound IO buffer pair on the game
    // module's update IO stacks, drive RootSoundModule::Prepare (vtable +64) with the
    // GameData allocator list, and -- while it reports "still preparing" -- forward the
    // module's resource requests out of the RootOutputBuffer into the GameData input buffer.
    // Returns true once the module reports prepared. The sibling LoadXxxModule helpers
    // (DWARF :43-:74) are reconstructed with the stages that drive them.
    bool LoadSoundModule(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                         const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer);

    // X360 0x823E72F0 -- one frame of the world-module load: create a scratch
    // BrnWorldIO::UpdateOutputBuffer on the update output stack, drive WorldModule::
    // Prepare (vtable +68) with the update IO stacks + the GameData allocator list, and
    // -- while it reports "still preparing" -- forward the world's staged resource
    // requests (RequestInterface<4096>) and AttribSys vault requests (<2048> queue) into
    // the GameData input buffer. Returns true once the module reports prepared.
    bool LoadWorldModule(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                         const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer);

    // X360 0x823E73E0 -- one frame of the WORLD COLLISION load (scripted stage 7): create a
    // scratch BrnWorldIO::UpdateOutputBuffer on the update output stack, drive
    // WorldModule::PrepareWorldCollision @0x827C9478, and -- while it reports "still
    // preparing" -- forward the world's staged resource requests into the GameData input.
    // Returns true once the whole world collision is prepared. (The console's true arm also
    // runs EffectsModule::PostWorldPreparePrepare @0x822902F0; deferred -- see the .cpp.)
    bool LoadWorldCollision(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                            const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer);

    // ⭐ X360 0x823EF4D8 -- one frame of the GAME-STATE SECOND PREPARE (scripted stage 3):
    // drive GameStateModule::Prepare2 @0x8239ED10 and -- while it reports "still preparing" --
    // forward the module's staged resource requests into the GameData input
    // (AppendRequestInterface<3072>). This is the console's PROGRESSION.DAT load path.
    // The second parameter is the GameData OUTPUT buffer. The console passes it and does
    // not read it (@0x823EF4D8); it had been dropped from our signature entirely, which is
    // the kind of quiet divergence that makes a later diff against the asm confusing.
    // Restored 2026-08-16 (boot audit F-P6-15).
    bool LoadGameState2(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                        const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer);

    // The per-frame world UPDATE leg of the spine (X360 0x823F22D8's `stage > 5` block,
    // inlined there): FreeAll the world frame allocator, drive WorldModule::Update
    // (vtable +76) with the frame's world IO pair and the loading update set, then run
    // BridgeWorldToResource @0x823E5300 -- the forward that carries the world streamer's
    // per-frame LoadBundle requests (TRK / PVS / prop graphics) into the GameData pump.
    // The sound pre-update buffer (phase C4) rides through to DriveWorldUpdateFrame's
    // BridgeSoundToWorld staging; null keeps the pre-C4 no-sound behaviour.
    void UpdateWorldModule(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                           BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundPreUpdateOutput = 0);

    // X360 0x823E74C0 -- the E_LOADINGSTAGE_DIRECTORMODULE leg. Creates this frame's director
    // OUTPUT buffer on the update output stack, runs the module's staged
    // DirectorModule::Prepare @0x822712D8 once with the GameData allocator list, and -- while
    // it reports "still preparing" -- read-locks the output buffer and forwards its staged
    // requests into the GameData input buffer (AppendRequestInterface<512>). Same shape as
    // LoadSoundModule/LoadWorldModule.
    bool LoadDirectorModule(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                            const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer);

    // The update set the loading spine drives the world with: ConstructUpdateSetFromFsm
    // @0x823BD420's base value 128 (frustum testing on; no in-game / boot-video / paused
    // bits while the scripted load runs).
    static const BrnUpdateSet KU_LOADING_UPDATE_SET = 0x80;

    // X360 0x823A85B0 -- the title-screen pre-accept resume: assert the scripted load is
    // still parked at START and clear the pause flag (byte_82FAE28E) so the load runs.
    void ResumeLoadingWorld();

    ELoadingStateStage meLoadingStateStage;
    bool               mbLoadingPaused;
};

// --- leaves -----------------------------------------------------------------------------
struct MainGameFlowStateInitialLoadingScreen : public LoadingScriptedState
{
    enum ELoadingScreenStage
    {
        E_LOADINGSTAGE_START = 0,
        E_LOADINGSTAGE_CONTROLLERMODULE = 1,
        E_LOADINGSTAGE_GUIMODULE = 2,
        E_LOADINGSTAGE_DIRECTORMODULE = 3,
        E_LOADINGSTAGE_SOUND_MODULE = 4,
        E_LOADINGSTAGE_NETWORK = 5,
        E_LOADINGSTAGE_JUICE = 6,
        E_LOADINGSTAGE_MASSIVE = 7,
        E_LOADINGSTAGE_REPLAYS = 8,
        E_LOADINGSTAGE_DONE = 9,
    };

    MainGameFlowStateInitialLoadingScreen();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void FinishLoading();

private:
    // Advance to + log the next load stage (interim helper; each real LoadXxxModule will call this
    // on its own completion). Non-virtual; does not affect layout/vtable.
    void AdvanceLoadingStage(ELoadingScreenStage leNextStage);

protected:
    ELoadingScreenStage                 meLoadingScreenStage;
    rw::core::GeneralResourceAllocator* mpInputModuleAllocator;
    bool                                mbGuiPreloadDone;
};

struct MainGameFlowStateStartScreen : public LoadingScriptedState
{
    enum EStartScreenLoadingStage
    {
        E_STARTSCREEN_START = 0,
        E_STARTSCREEN_WORLD = 1,
        E_STARTSCREEN_SOUND_BINDPROPS = 2,
        E_STARTSCREEN_EFFECTS = 3,
        E_STARTSCREEN_DONE = 4,
        E_STARTSCREEN_NUM = 5,
    };

    MainGameFlowStateStartScreen();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void Render();

private:
    EStartScreenLoadingStage meLoadingStage;
};

struct MainGameFlowStateMarketingScreens : public LoadingScriptedState
{
    MainGameFlowStateMarketingScreens();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void Render();
};

struct MainGameFlowStateCheckDiskSpace : public LoadingScriptedState
{
    MainGameFlowStateCheckDiskSpace();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void Render();
};

struct MainGameFlowStateMemoryCard : public LoadingScriptedState
{
    MainGameFlowStateMemoryCard();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void Render();
};

struct MainGameFlowStateCompleteLoading : public LoadingScriptedState
{
    MainGameFlowStateCompleteLoading();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void Render();

private:
    bool mbIsCollisionWorldPrepared;
};

// ---------------------------------------------------------------------------------------------
// KU_INGAME_UPDATE_SET IS DELETED (pause wave, 2026-08-26). It read
//     const BrnUpdateSet KU_INGAME_UPDATE_SET = 0x88;
// and its banner made two claims, BOTH FALSE, and the pair of them is why the in-game world
// could never be paused:
//   1. "Mirrored here because that method is private to BrnGameModule."
//      ConstructUpdateSetFromFsm is PUBLIC -- BrnGameModule.hpp:327, inside the public block
//      that opens at :150. Nothing ever required a mirror.
//   2. "WorldModule::Update only tests 0x1 / 0x80 / 0x100, so the 0x08 bit is inert there --
//      this cannot change the world's behaviour."
//      The reasoning is about the wrong bit. 0x08 is indeed inert; what the frozen constant
//      actually did was make bit 0x1 -- THE SIM-PAUSE BIT, the one WorldModule::Update very much
//      does test -- permanently UNREACHABLE in game. ConstructUpdateSetFromFsm ORs 0x1 when
//      mbSimPaused (BrnGameModule.cpp), so hardcoding 0x88 pinned the game to "never paused"
//      no matter what the pause FSM decided.
// A hardcoded mirror of a FUNCTION OF STATE is not a constant. DriveInGameWorldUpdate now calls
// the method, exactly as the LOADING spine at BrnGameMainFlowStates.cpp:323 already did.
// ---------------------------------------------------------------------------------------------

// The per-frame world UPDATE leg, shared by the scripted-load spine and the in-game state.
// See the commentary on DriveWorldUpdateFrame in the .cpp for why the world previously
// stopped updating the moment the flow left the loading screen.
void DriveWorldUpdateFrame(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                           BrnUpdateSet lUpdateSet,
                           BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundPreUpdateOutput = 0);
// The sound pre-update buffer (phase C4b) rides through to DriveWorldUpdateFrame's
// BridgeSoundToWorld staging, exactly as on the loading spine; null = no sound this frame.
void DriveInGameWorldUpdate(BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundPreUpdateOutput = 0);

struct MainGameFlowStateInGame : public MainGameFlowState
{
    MainGameFlowStateInGame();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void Render();
};
