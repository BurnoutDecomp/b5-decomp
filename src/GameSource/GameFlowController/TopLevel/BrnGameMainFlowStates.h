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
namespace BrnGame { class BrnGameModule; }
namespace CgsGui {
namespace CgsGuiModuleIO { struct InputBuffer; struct OutputBuffer; }
namespace ModelIO { struct OutputBuffer; }
namespace ViewIO { struct InputBuffer; }
}

// The GameData per-frame IO payloads the scripted-load helpers thread through (pointer-only
// here; the helper bodies include the real home, GameSource/Resource/BrnGameDataModuleIO.h).
namespace BrnResource { namespace GameDataIO { struct InputBuffer; struct OutputBuffer; } }

// The sound pre-update payload the spine threads into the world drive for BridgeSoundToWorld
// (pointer-only here; real home GameSource/Sound/Module/BrnRootSoundModuleIo.h).
namespace BrnSound { namespace Module { namespace Io { struct RootPreUpdateOutputBuffer; } } }

// The scripted world-load stage every loading-scripted state shares (defined in
// BrnGameMainFlowStates.cpp). At 8 the load is done and a loading-scripted state's Update runs
// the full per-frame module cascade instead of its partial one; BrnGameModule reads it to
// decide whether this sub-step runs that cascade's network legs.
extern s32 gBrnScriptedLoadStage;

namespace BrnGameMainFlowController
{
    // The same persistent pair BrnGameModule::Prepare creates on the resource
    // stacks (ARTIST gm+996F10/14), also used by GamePrepare.
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
    friend class BrnGame::BrnGameModule;
    // ARTIST 823CE098; original callers supply these real GUI IO buffers.
    void RenderGUI(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
                   CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput,
                   CgsGui::ModelIO::OutputBuffer* lpGuiModelOutput,
                   CgsGui::ViewIO::InputBuffer* lpGuiViewInput,
                   bool lbSkipGui);
    // X360 0x823E75A8 (DWARF BrnGameMainFlowStates.h:58 / BrnLoadingScriptedState.cpp:733).
    // One frame of the sound-module load: create the Root sound IO buffer pair on the game
    // module's update IO stacks, drive RootSoundModule::Prepare (vtable +64) with the
    // GameData allocator list, and -- while it reports "still preparing" -- forward the
    // module's resource requests out of the RootOutputBuffer into the GameData input buffer.
    // Returns true once the module reports prepared. The sibling LoadXxxModule helpers
    // (DWARF :43-:74) are reconstructed with the stages that drive them.
    bool LoadSoundModule(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                         const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer);

    // ARTIST823E7700, DecFIGS3119A8. The second sound prepare owns a fresh
    // Root input/output pair and forwards requests while gameplay setup waits.
    bool LoadSoundModuleAgain(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                              const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer);

    // X360 0x823E72F0 -- one frame of the world-module load: create a scratch
    // BrnWorldIO::UpdateOutputBuffer on the update output stack, drive WorldModule::
    // Prepare (vtable +68) with the update IO stacks + the GameData allocator list, and
    // -- while it reports "still preparing" -- forward the world's staged resource
    // requests (RequestInterface<4096>) and AttribSys vault requests (<2048> queue) into
    // the GameData input buffer. Returns true once the module reports prepared.
    // @0x823E7820 -- one frame of the EFFECTS-module load (scripted-load stage 2). Same shape
    // as LoadWorldModule below: carve a scratch EffectsIO::OutputBuffer on the update output
    // stack, drive EffectsModule::Prepare (vtable +64) with the GameData allocator list, and --
    // while it reports "still preparing" -- forward the effects output's staged AttribSys vault
    // requests and resource requests into the GameData input buffer. That forward is what
    // carries ParticleModule::LoadFXBundle's particles.bundle load and its texture acquires
    // into the GameData pump; without it the FX-bundle ladder never advances.
    bool LoadEffectsModule(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                           const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer);

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

    // X360 0x823E74C0 -- the E_LOADINGSTAGE_DIRECTORMODULE leg. Creates this frame's director
    // OUTPUT buffer on the update output stack, runs the module's staged
    // DirectorModule::Prepare @0x822712D8 once with the GameData allocator list, and -- while
    // it reports "still preparing" -- read-locks the output buffer and forwards its staged
    // requests into the GameData input buffer (AppendRequestInterface<512>). Same shape as
    // LoadSoundModule/LoadWorldModule.
    bool LoadDirectorModule(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                            const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer);

    // ARTIST823EF310: caller-owned GUI buffers across the actual prepare/retry path.
    bool LoadGUIModule(BrnResource::GameDataIO::InputBuffer* lpGameDataInput,
                      CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
                      CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput,
                      CgsGui::ModelIO::OutputBuffer* lpModelOutput,
                      CgsGui::ViewIO::InputBuffer* lpViewInput,
                      const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput);

    bool LoadNetworkModule(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                           const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer);

    // The controller stage: carve the input module's general allocator once (8 KB main memory,
    // 16-aligned, from GameData heap 0x26) and prepare the input module with it. Body in
    // BrnGameMainFlowStates_wBT_01.cpp.
    bool LoadControllerModule(BrnResource::GameDataIO::InputBuffer* lpGDMInput,
                              const BrnResource::GameDataIO::OutputBuffer* lpGDMOutput,
                              rw::core::GeneralResourceAllocator** lppInputModuleAllocator);

    // The update set the loading spine drives the world with: ConstructUpdateSetFromFsm
    // @0x823BD420's base value 128 (frustum testing on; no in-game / boot-video / paused
    // bits while the scripted load runs).
    static const BrnUpdateSet KU_LOADING_UPDATE_SET = 0x80;

    // X360 0x823A85B0 -- the title-screen pre-accept resume: assert the scripted load is
    // still parked at START and clear the pause flag (byte_82FAE28E) so the load runs.
    void ResumeLoadingWorld();

    // ARTIST stores the shared stage/pause in file-static state, not each instance.
};

// --- leaves -----------------------------------------------------------------------------
struct MainGameFlowStateInitialLoadingScreen : public LoadingScriptedState
{
    // Original own-stage GUI helper gate at the native caller boundary.
    friend class BrnGame::BrnGameModule;
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

struct MainGameFlowStateInGame : public MainGameFlowState
{
    MainGameFlowStateInGame();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void Render();
};
