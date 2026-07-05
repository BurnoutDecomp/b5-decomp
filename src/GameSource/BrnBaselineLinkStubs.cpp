// ===========================================================================
// BrnBaselineLinkStubs.cpp -- FLAG (baseline link stubs).
//
// Minimal out-of-line definitions so the game exe LINKS without compiling the full
// IntelliFrag / EmergencyFrag defrag-pool-state TUs, whose real bodies call into a
// CgsResource::Pool defrag surface that is still declaration-only.
//
// These paths are NOT exercised on the current loading-screen -> EA -> Criterion boot.
//
// AUDIT (2026-07-02, vs the on-disk tree + the exe source list):
//   - The REAL CgsIntelliFragPoolModuleState.cpp / CgsEmergencyFragPoolModuleState.cpp
//     reconstructions now EXIST (RunDefragAlgorithm 0x828E3EB8/0x828E40C8,
//     RunPoolDefragmentation 0x828F80C0/0x828F80E0) but stay OUT of the exe build:
//     they call Pool::BeginDefragmentation and BaseDefragPoolModuleState::
//     AddAddressedAllocRequest, whose BODIES are still unreconstructed (declared in
//     CgsResourcePool.h / CgsBaseDefragPoolModuleState.h, marked deferred), so linking
//     the real TUs would leave unresolved externals. Swap them in (and delete the two
//     stub pairs below) when those Pool bodies land.
//   - The REAL ReplayModule ctor now EXISTS too (BrnReplayModule.cpp, X360 0x827E03D0,
//     boot-trace EXECUTED) but that TU also stays out of the exe build: it defines the
//     rest of the module group (Update_Dispatch -> GPUDiskWriteStream::Dispatch,
//     WaitForSerialiseJobs -> EA::Jobs, the command poster paths), so adding it pulls
//     the replay-stream closure (Stream/BrnReplayGPUDiskWriteStream.cpp + its deps)
//     into the link. When that closure is added to the source list, DELETE the ctor
//     stub below -- the two definitions must never coexist in one build.
// ===========================================================================

#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsIntelliFragPoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsEmergencyFragPoolModuleState.h"
#include "GameSource/Replays/BrnReplayModule.h"
#include "GameShared/GameClasses/Sound/CgsTestBedAllocator.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsFloatQuantiser.h"

namespace CgsResource
{
    // The concrete defrag strategies (the Base versions are stubbed in
    // CgsBaseDefragPoolModuleState.cpp). Returning false / doing nothing leaves the pool
    // un-defragmented, which is benign on the boot path.
    bool IntelliFragPoolModuleState::RunDefragAlgorithm(AllocListSet*, LinearHeapNode*, s32, s32)      { return false; }
    void IntelliFragPoolModuleState::RunPoolDefragmentation(RelocateRequest*, RelocateSource*, u32, s32) {}
    bool EmergencyFragPoolModuleState::RunDefragAlgorithm(AllocListSet*, LinearHeapNode*, s32, s32)      { return false; }
    void EmergencyFragPoolModuleState::RunPoolDefragmentation(RelocateRequest*, RelocateSource*, u32, s32) {}
}

namespace BrnReplays
{
    // Link stub for the replay module ctor (BrnGameModule constructs mReplayModule). The
    // member sub-objects default-construct; the real ctor body is in BrnReplayModule.cpp
    // (out of the exe build -- see the header audit note).
    ReplayModule::ReplayModule() {}
}

namespace BrnGameState
{
    // Link stub: ModeManager embeds a ScoringSystem by value (X360 ModeManager+0xDB0), so its
    // ctor is referenced by ModeManager::ModeManager(). The real ctor (BrnScoringSystem_
    // Lifecycle.cpp) + the online-mode-scoring subsystem it constructs are OFF the loading-screen
    // boot path and would pull the whole online/network/world closure into the link. Stub the ctor
    // here; constructing the embedded online-mode-scoring members needs their vtables, so the vtable
    // virtuals are stubbed below too. All inert (scoring is not exercised during loading).
    // Signatures MIRROR the class headers exactly so each lands in the right vtable slot.
    // Replace this whole block with the real scoring TUs when that subsystem is wired in.
    ScoringSystem::ScoringSystem() {}

    // --- BaseOnlineModeScoring: slots 0..8 + the two non-slot virtuals ---
    void BaseOnlineModeScoring::Construct() {}
    bool BaseOnlineModeScoring::Prepare()  { return false; }
    bool BaseOnlineModeScoring::Release()  { return false; }
    void BaseOnlineModeScoring::Destruct() {}
    void BaseOnlineModeScoring::ClearData() {}
    void BaseOnlineModeScoring::Update(const ScoringSystem*, s32) {}
    void BaseOnlineModeScoring::UpdatePlayerPoints(ScoringSystem*, s32) {}
    void BaseOnlineModeScoring::AwardNetworkRatings(const ScoringSystem*, u32) {}
    void BaseOnlineModeScoring::WriteDataToOutput(OnlineScoringOutputInterface*) {}
    GameStateModuleIO::EPlayerTeam BaseOnlineModeScoring::GetCurrentPlayerTeam(s32) { return static_cast<GameStateModuleIO::EPlayerTeam>(0); }

    // --- the four concrete online modes: identical override set (see each header) ---
#define BRN_STUB_ONLINE_MODE(CLS)                                                          \
    void CLS::Construct() {}                                                                \
    bool CLS::Prepare()  { return false; }                                                  \
    bool CLS::Release()  { return false; }                                                  \
    void CLS::Destruct() {}                                                                  \
    void CLS::ClearData() {}                                                                 \
    void CLS::Update(const ScoringSystem*, s32) {}                                           \
    void CLS::UpdatePlayerPoints(ScoringSystem*, s32) {}                                     \
    void CLS::WriteDataToOutput(GameStateModuleIO::OnlineScoringOutputInterface*) {}
    BRN_STUB_ONLINE_MODE(OnlineRaceModeScoring)
    BRN_STUB_ONLINE_MODE(OnlineRoadRageModeScoring)
    BRN_STUB_ONLINE_MODE(OnlineStuntRunModeScoring)
    BRN_STUB_ONLINE_MODE(OnlineBurningHomeRunModeScoring)
#undef BRN_STUB_ONLINE_MODE

    // --- StuntModeScoring extra virtuals the vtable references ---
    bool StuntModeScoring::HasStuntModeEnded(bool) { return true; }
    s32  StuntModeScoring::CalculateMultiplier(const StuntInfo*, StuntModeScoring::MultiplierOutInfo*) { return 0; }

    // --- CarScoreData ctor (embedded array element in the scoring records) ---
    GameStateModuleIO::CarScoreData::CarScoreData() {}
}

// Link stubs for the wave-30 MainGameFlowStateInGame virtuals: the reconstructed
// bodies (BrnGameMainFlowInGameState.cpp) call BrnGameModule::DoUpdate/DoDispatch,
// which are not reconstructed yet, so that TU is not in the exe source list. The
// boot/title slice never enters the in-game flow state; inert stubs satisfy the
// vtable link only. Replace by adding the real TU when DoUpdate/DoDispatch land.
#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"
void MainGameFlowStateInGame::OnEnter() {}
void MainGameFlowStateInGame::OnLeave() {}
void MainGameFlowStateInGame::Update() {}
void MainGameFlowStateInGame::Render() {}

namespace CgsSound
{
namespace Utils
{
    // Link stub for the curve-shape mapper (DWARF CgsSoundUtils.h:276) that the wave-26
    // PathLine<2>::Update stage interpolation calls. The real body (the per-ECurveType
    // fraction shaping) is its own recon slice; NOT exercised on the boot path (no
    // PathLine stage machine runs until the roadnoise/transition-envelope layers go
    // real). Inert linear-identity fallback: return the fraction unshaped.
    f32 Curve::GetOutput(f32 lfFraction, ECurveType /*leCurve*/)
    {
        return lfFraction;
    }
}
}

namespace CgsSound
{
namespace TestBed
{
    // Link stubs for the testbed-allocator tail. BrnRootSoundModule.cpp now instantiates the
    // four carve globals (gRwac/gCsis/gPlayback/gLogicTestBedAlloc), which pulls the Allocator
    // vtable + the SanityCheck/SafeDump paths into the link, but the DoAllocate/DoFree bodies
    // (the actual carve/track/free algorithms) and the per-block Header::SanityCheck body are
    // not reconstructed yet. NOT exercised on the boot path: nothing allocates through these
    // wrappers until the RWAC/PLAYBACK/LOGIC carve stages of RootSoundModule::Prepare go real
    // (they are gated on the same missing layers). Replace with the real bodies when the
    // testbed-allocator TU tail is reconstructed.
    //   NOTE: Header::Dump is NOT stubbed here -- its real body now lives in
    //   CgsTestBedAllocator.cpp (wired into the exe source list). A prior stub collided
    //   (LNK2005) with that body and was removed.
    rw::Resource Allocator::DoAllocate(const rw::ResourceDescriptor& /*lrDescriptor*/,
                                       const char* /*lpcName*/)
    {
        return rw::Resource();   // empty resource = "no allocation" until the real body lands
    }
    void Allocator::DoFree(const rw::Resource& /*lrResource*/) {}
    void Allocator::Header::SanityCheck(History& /*lrHistory*/, const char* /*lpcAllocatorName*/) {}
}
}

// ===========================================================================
// wave46 link-resolution stubs.
//
// Several TUs already in the exe source list had their bodies EXPANDED (wave46) to call
// helpers that are declared-only / reconstructed as isolated compile-gate TUs whose symbols
// do not link (they drag X360 XDK externals). None of these helper paths run on the
// title-screen boot slice, so inert stubs satisfy the link and stay behaviourally neutral.
// Replace each with the real body when its subsystem is wired into the build.
// ===========================================================================

// --- CgsUnicode::Copy / SafelyTerminate are NOT stubbed here -- their real asm-decoded bodies
// (X360 0x82834448 / 0x828345F0) now live in GameShared/GameClasses/Fonts/CgsUnicode.cpp (already in
// build_game_exe.bat), reached from UnicodeBuffer::Convert / LanguageManager::Format*String /
// GuiHudMessage::GetParam. The prior inert stubs (copy-nothing / terminate-at-[0]) were removed to
// avoid an LNK2005 double-definition with the real bodies.

// --- EA::GameTalk::GameTalkMessage accessors (BrnGameModule::RenderMetricsMessageHandler,
// a debug-metrics GameTalk handler -- not on the boot path). No keys / no key strings. ---
#include "SDKs/EA/GameTalk/GameTalk.h"
namespace EA { namespace GameTalk {
    s32         GameTalkMessage::GetNumKeys() const           { return 0; }
    const char* GameTalkMessage::GetKey(s32 /*liIndex*/) const { return 0; }
}}

// --- BrnHW::System360HW::HasGameBeenRebootedDueToInvite (real body is in BrnSystemHWX360.cpp,
// which is out of the PC exe build). Mirrors the BrnBootLegalBoundary.cpp fallback: false. ---
#include "GameSource/Game/X360/BrnSystemHWX360.h"
namespace BrnHW
{
    bool System360HW::HasGameBeenRebootedDueToInvite() { return false; }
}

// --- XShowDirtyDiscErrorUI: Xbox 360 XDK import (declared extern "C" in BrnGameModule.cpp's
// DiskErrorThreadProc). No PC equivalent; the disk-error thread never runs on the boot slice. ---
extern "C" unsigned long XShowDirtyDiscErrorUI(unsigned long /*dwUserIndex*/) { return 0; }

// --- RenderWare resource-descriptor helpers driving RwRenderableResourceType::
// GetSerialisedResourceDescriptor (a resource-SIZE query, not exercised while rendering the
// title Apt). RenderableMesh::GetResourceDescriptor has no linkable body; the renderengine
// IndexBuffer/VertexBuffer bodies live in TUs that drag undefined X360 XDK shims, so they are
// stubbed here rather than linked. All inert: empty/zero descriptors, pass-through pointers. ---
#include "GameShared/GameClasses/Graphics/Dispatch/renderablemesh.h"
#include "pc/gcm/renderengine/IndexBuffer.h"
#include "pc/gcm/renderengine/VertexBuffer.h"
CgsResource::ResourceDescriptor RenderableMesh::GetResourceDescriptor(uint32_t /*luNumVertexBuffers*/,
                                                                      uint32_t /*luNumVertexDescriptors*/)
{
    return CgsResource::ResourceDescriptor();
}
namespace renderengine
{
    IndexBufferHeader* IndexBuffer::GetParameters(IndexBufferHeader* lpBuffer, IndexBufferParamsOut* lpOut)
    {
        if (lpOut) { lpOut->muField00 = 0u; lpOut->muBits = 16u; lpOut->muCount = 0u; }
        return lpBuffer;
    }
    VertexBufferHeader* VertexBuffer::GetParameters(VertexBufferHeader* lpBuffer, u32* lpParamsOut)
    {
        if (lpParamsOut) { lpParamsOut[0] = 0u; lpParamsOut[1] = 0u; }
        return lpBuffer;
    }
    u64* VertexBuffer::GetResourceDescriptor(u64* lpDescriptorOut, int /*a2*/)
    {
        if (lpDescriptorOut) { for (int i = 0; i < 5; ++i) lpDescriptorOut[i] = 0ull; }
        return lpDescriptorOut;
    }
}

// CgsNetwork::FloatQuantiser::UnPack is NOT stubbed here -- the real asm-decoded body
// lives in GameShared/.../BitStream/CgsFloatQuantiser.cpp (wired into build_game_exe.bat).
// (A prior inert "reconstruct as the range minimum" stub was removed: it collided
// (LNK2005) with the real body and was itself an invented fallback.)
