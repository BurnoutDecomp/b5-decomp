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
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"                      // BrnPhysics::Vehicle::RaceCarState
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"          // InGamePlayerStatusInterface (+ NetworkPlayerStats / LiveRevengeRelationship)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"                // DebugUI deferred accessors (see the block at the end)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"                  // Palette / Variant
#include "SDKs/Realmc/RealmcLoadEntryInfo.h"                                                  // LoadEntryInfo (3-arg ctor stub)
#include "SDKs/Realmc/RealmcIfaceSaveCheckParams.h"                                           // SaveCheckParams (ctor/dtor stubs)

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

// RETIRED 2026-08-01 (camera wave). BrnPhysics::Vehicle::RaceCarState::operator= used to be
// an inert `{}` here, on the reasoning quoted in its own comment: "Only the Director camera
// path -- OFF the boot/title/menu path -- reaches it." That path went live with
// BridgeWorldToDirector, and the empty body then discarded EVERY RaceCarState copy in the
// tree with no diagnostic (the world published a car at (3008.17, -1.16, -1874.30); the
// director's camera received one at the origin). The real bitwise body now lives in its home,
// GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp.

namespace BrnNetwork
{
    // Link stubs: the controller-bridge closure links BrnGameStateModuleIO.cpp ->
    // BrnNetworkModuleIO.cpp -> the in-game-player-status interface TU, whose record
    // Clear/operator= call these "own TU" (unreconstructed) leaves. All are network/
    // multiplayer state -- OFF the boot/title/menu path. Inert until the real bodies
    // land; DELETE each when its TU is reconstructed.
    void NetworkPlayerStats::Clear() {}
    void LiveRevengeRelationship::Construct() {}

namespace BrnNetworkModuleIO
{
    s32 InGamePlayerStatusInterface::GetNumPlayers() const { return 0; }
    const InGamePlayerStatusData* InGamePlayerStatusInterface::GetPlayerStatusData(s32) const { return 0; }
}
}

namespace BrnGameState
{
    // [stuntrace wave B mount, 2026-08-26] PARTIAL RETIREMENT of the old scoring stub block.
    // The scoring subsystem is mounted now (BrnScoringSystem_*.cpp + the offline StuntModeScoring
    // set + the four Online*ModeScoring TUs), so every stub with a real body DIED here -- retired:
    // ScoringSystem::ScoringSystem, BaseOnlineModeScoring::GetCurrentPlayerTeam,
    // OnlineRaceModeScoring::{ClearData,Update,UpdatePlayerPoints},
    // OnlineRoadRageModeScoring::{Construct,Prepare,ClearData,UpdatePlayerPoints,WriteDataToOutput},
    // OnlineStuntRunModeScoring::UpdatePlayerPoints,
    // OnlineBurningHomeRunModeScoring::{UpdatePlayerPoints,WriteDataToOutput},
    // StuntModeScoring::{HasStuntModeEnded,CalculateMultiplier} (the return-true HasStuntModeEnded
    // stub would have ended every stunt run on frame 1).
    // The stubs BELOW have NO body anywhere in src (measured, seam audit S7 2026-08-26): deleting
    // any one is an LNK2019. Each dies only when its real body lands in its own TU.

    // --- BaseOnlineModeScoring: the 9 bodiless virtuals ---
    void BaseOnlineModeScoring::Construct() {}
    bool BaseOnlineModeScoring::Prepare()  { return false; }
    bool BaseOnlineModeScoring::Release()  { return false; }
    void BaseOnlineModeScoring::Destruct() {}
    void BaseOnlineModeScoring::ClearData() {}
    void BaseOnlineModeScoring::Update(const ScoringSystem*, s32) {}
    void BaseOnlineModeScoring::UpdatePlayerPoints(ScoringSystem*, s32) {}
    void BaseOnlineModeScoring::AwardNetworkRatings(const ScoringSystem*, u32) {}
    void BaseOnlineModeScoring::WriteDataToOutput(OnlineScoringOutputInterface*) {}

    // --- OnlineRaceModeScoring: 5 bodiless ---
    void OnlineRaceModeScoring::Construct() {}
    bool OnlineRaceModeScoring::Prepare()  { return false; }
    bool OnlineRaceModeScoring::Release()  { return false; }
    void OnlineRaceModeScoring::Destruct() {}
    void OnlineRaceModeScoring::WriteDataToOutput(GameStateModuleIO::OnlineScoringOutputInterface*) {}

    // --- OnlineRoadRageModeScoring: 3 bodiless ---
    bool OnlineRoadRageModeScoring::Release()  { return false; }
    void OnlineRoadRageModeScoring::Destruct() {}
    void OnlineRoadRageModeScoring::Update(const ScoringSystem*, s32) {}

    // --- OnlineStuntRunModeScoring: 7 bodiless ---
    void OnlineStuntRunModeScoring::Construct() {}
    bool OnlineStuntRunModeScoring::Prepare()  { return false; }
    bool OnlineStuntRunModeScoring::Release()  { return false; }
    void OnlineStuntRunModeScoring::Destruct() {}
    void OnlineStuntRunModeScoring::ClearData() {}
    void OnlineStuntRunModeScoring::Update(const ScoringSystem*, s32) {}
    void OnlineStuntRunModeScoring::WriteDataToOutput(GameStateModuleIO::OnlineScoringOutputInterface*) {}

    // --- OnlineBurningHomeRunModeScoring: 6 bodiless ---
    void OnlineBurningHomeRunModeScoring::Construct() {}
    bool OnlineBurningHomeRunModeScoring::Prepare()  { return false; }
    bool OnlineBurningHomeRunModeScoring::Release()  { return false; }
    void OnlineBurningHomeRunModeScoring::Destruct() {}
    void OnlineBurningHomeRunModeScoring::ClearData() {}
    void OnlineBurningHomeRunModeScoring::Update(const ScoringSystem*, s32) {}

    // --- CarScoreData ctor: RETIRED 2026-08-01 (BridgeGameStateToWorld wave) ---
    // The real body (X360 0x822A45A8, zero-inits the whole 296-byte record) has been sitting
    // in BrnGameStateSharedIO.cpp:393 the whole time; that TU was simply never mounted, so this
    // empty stub was what every scoring record actually got constructed with -- a 296-byte
    // record left at whatever the allocation held. It is mounted now (for
    // RaceCarRaceDistanceInterface::Clear, which OutputBuffer::Construct needs), and the stub
    // would be a duplicate symbol.
}

// The wave-30 MainGameFlowStateInGame virtual stubs that used to live here are GONE:
// DoUpdate/DoDispatch landed, so the real TU (BrnGameMainFlowInGameState.cpp) is in
// the exe source list now. Its OnEnter requests GUI FSM stage 5 (the front-end/
// freeburn handoff) -- the inert stubs silently swallowed that request, which was
// the post-intro handoff stall.

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
    // Testbed-allocator tail. (2026-08-25, faithful-audio-engine phase A4: the RWAC/LOGIC
    // carve stages of RootSoundModule::Prepare went REAL, so DoAllocate is now LIVE on the
    // boot path -- rw::audio::core::System::CreateInstance carves through it. The old inert
    // empty-Resource stub made CreateInstance fail and left mpSystem null.)
    //
    // FLAG [interim pass-through]: the real DoAllocate @0x826AE420 is the full 441-line
    // TRACKED carve (per-block Header + guard words + history ring + verbose log) -- its own
    // recon slice, ledgered. This interim body forwards the carve straight to the backing
    // allocator: behaviour-transparent to every consumer (they only see the returned
    // Resource); the debug surfaces (SanityCheck/SafeDump/IsValidMemoryAddress) see an empty
    // tracking list until the real body lands. DoFree mirrors it.
    //   NOTE: Header::Dump is NOT stubbed here -- its real body now lives in
    //   CgsTestBedAllocator.cpp (wired into the exe source list). A prior stub collided
    //   (LNK2005) with that body and was removed.
    rw::Resource Allocator::DoAllocate(const rw::ResourceDescriptor& lrDescriptor,
                                       const char* lpcName)
    {
        if (mpAllocator == 0)
            return rw::Resource();   // un-backed wrapper (e.g. gCsisTestBedAlloc): no carve
        return mpAllocator->DoAllocate(lrDescriptor, lpcName);
    }
    void Allocator::DoFree(const rw::Resource& lrResource)
    {
        if (mpAllocator != 0)
            mpAllocator->DoFree(lrResource);
    }
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
    // NOT A STUB. Relocated from src/pc/gcm/renderengine/VertexBuffer.cpp (X360 0x82B63778), which
    // stays unmounted because mounting that TU was MEASURED WORSE (29 externals / 23 unresolved plus
    // an LNK2005 against the linked CgsIm2d.cpp -- see the sky-wave note in build_game_exe.bat).
    // The two bodies are kept identical; delete this one the day VertexBuffer.cpp mounts.
    //
    // The previous inert stub zeroed all five entries, which made slot0 m_size == 0. A zero-size lane
    // is exactly the "nothing requested" case, so LinearResourceAllocator::DoAllocate skipped the
    // Alloc and handed back a null lane 0, tripping BrnSkyDomeManager::CreateGeometry's
    // CGS_ASSERT(vbResource.GetMemoryResource()) on both dome builds. (The sky still drew only
    // because the PC leaf's VertexBuffer::Initialize falls back to ArenaAlloc.)
    //
    // See VertexBuffer.cpp for the endianness derivation: the console's merged 64-bit stores encode
    // {m_size, m_alignment} in big-endian dword order, so they are written by NAME here rather than
    // replayed as literals.
    ::rw::BaseResourceDescriptors<5>* VertexBuffer::GetResourceDescriptor(
        ::rw::BaseResourceDescriptors<5>* lpDescriptorOut, const VertexBuffer::Parameters* lpParams)
    {
        for (int liIndex = 0; liIndex < 5; ++liIndex)
        {
            lpDescriptorOut->m_baseResourceDescriptors[liIndex].m_size      = 0u;
            lpDescriptorOut->m_baseResourceDescriptors[liIndex].m_alignment = 1u;
        }
        lpDescriptorOut->m_baseResourceDescriptors[0].m_size      = 0x28u;   // sizeof(VertexBufferHeader)
        lpDescriptorOut->m_baseResourceDescriptors[0].m_alignment = 4u;
        lpDescriptorOut->m_baseResourceDescriptors[2].m_size      = lpParams->muLength;
        lpDescriptorOut->m_baseResourceDescriptors[2].m_alignment = 4u;
        return lpDescriptorOut;
    }
}

// CgsNetwork::FloatQuantiser::UnPack is NOT stubbed here -- the real asm-decoded body
// lives in GameShared/.../BitStream/CgsFloatQuantiser.cpp (wired into build_game_exe.bat).
// (A prior inert "reconstruct as the range minimum" stub was removed: it collided
// (LNK2005) with the real body and was itself an invented fallback.)

// ===========================================================================
// XDK boundary shims (profile link-closure wave, 2026-07-12): the Xbox 360 XDK
// imports referenced by CgsSaveLoadPS3.cpp (SaveLoadSystem::Update's overlapped
// pump) and CgsGuideIntegration.cpp (SystemUserProfile's XNotify/XUser watcher),
// both now in the exe source list. These are XDK IMPORTS on the X360 (no game
// body to reconstruct); the PC has no XDK, so each returns the value that makes
// its caller take the no-device/no-user branch. Same precedent as
// XShowDirtyDiscErrorUI above.
// ===========================================================================

// FLAG PC-platform leaf: XDK overlapped-result query; 0 (== ERROR_SUCCESS, not the
// 997/996 still-pending codes) tells SaveLoadSystem::Update the async op is finished,
// clearing its in-flight state -- no overlapped I/O ever starts on PC.
extern "C" unsigned long XGetOverlappedResult(void* /*lpOverlapped*/,
                                              unsigned long* /*lpdwResult*/,
                                              int /*bWait*/) { return 0; }

// FLAG PC-platform leaf: XDK overlapped EXTENDED-error query -- the twin of the call
// above, and the only other XDK import in CgsSystem::CgsXOverlapped (achievement-manager
// wave, 2026-08-11: CgsXOverlappedX360.cpp joins the exe so AchievementManagerX360::
// Prepare/Release can construct the module's embedded overlapped). Its ONLY caller is
// CgsXOverlapped::GetResultString @0x823557F0, which only reaches it on a code the three
// named cases (0 / 996 / 997) did not cover; 0 keeps that diagnostic string honest on a
// platform where no overlapped I/O is ever started.
extern "C" unsigned long XGetOverlappedExtendedError(void* /*lpOverlapped*/) { return 0; }

// ---- StreetManagerDebugComponent vtable gate (street wave, 2026-08-11) ------------------
// GameStateModule now embeds StreetManager (X360 this+284520) whose embedded debug
// component's vtable is emitted by the module ctor chain -- so its two out-of-line
// virtuals must link. The component's REAL TUs (BrnStreetManagerDebugComponent.cpp +
// _wO_01.cpp, bodies on disk) stay unmounted: they close over the road-rules cheat set
// (StreetManager::SetChallengeUserScore / ScoreList::KAI_MIN/MAX_SCORES /
// ProgressionManager trophy hooks -- 16 link-measured externals). Until that wave:
// GetName is the real one-line body (@0x823175F0, same string); OnActivate is an inert
// gate (the console registers the six debug-menu cheat callbacks here; activating the
// menu on PC logs instead of registering dead pointers).
#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"
namespace BrnGameState
{
    const char* StreetManagerDebugComponent::GetName() const { return "Street Manager"; }
    void StreetManagerDebugComponent::OnActivate()
    {
        *CgsDev::Log::gpDebugPrint
            << "StreetManagerDebugComponent::OnActivate: inert [FLAG PC boot gate]\n";
    }
}

// ---- ScoringSystemDebugComponent vtable gate (stuntrace waveB mount closure, 2026-08-26) ----
// EXACTLY the StreetManagerDebugComponent case above, one class over. ModeManager now embeds
// ScoringSystemDebugComponent BY VALUE (BrnModeManager.h:614, X360 ModeManager+28136), and
// GameStateModule embeds ModeManager by value, so the ctor chain emits this component's vtable
// and its two out-of-line virtuals must link.
//
// The component's REAL TU EXISTS AND COMPILES (BrnScoringSystemDebugComponent.cpp, 256 lines --
// GetName / OnActivate / GetChainableTableEntry / DebugRenderChainableStunts, X360 0x82312470 /
// 0x82312490 / 0x82329D60 / 0x82337C38). It stays UNMOUNTED on purpose: its two table bodies
// carry self-declared UNRECOVERED PLACEHOLDERS whose rodata is not in the exports -- the per-row
// stunt-multiplier bit table (dword_82020F54[]) and the three cell colours (dword_82CDB878 /
// _87C / _880). Mounting it would put invented constants on a live vtable for no gain: the debug
// UI never constructs on this build (same reason the CgsDev::DebugUI block further down exists).
//
//   GetName    IS THE REAL BODY. @0x82312470 is a two-instruction leaf returning the literal
//              "Scoring System" (lis/addi aScoringSystem; blr), dumped this session. No state.
//   OnActivate is an INERT GATE. The console body @0x82312490 is a single tail call,
//              sub_8282D800(this, this + 0x10, "Show chainable stunts") -- the debug-menu bool
//              tweakable registration pointing at mbShowChainableStunts (+0x10). Registering a
//              tweakable against a component whose render half is not mounted would only park a
//              live pointer; the log makes the gap visible instead.
//
// DELETE-WHEN BrnScoringSystemDebugComponent.cpp joins the exe source list (i.e. when its
// placeholder rodata is recovered). Both definitions in one build is an LNK2005.
#include "GameSource/GameState/ModeManager/Debug/BrnScoringSystemDebugComponent.h"
namespace BrnGameState
{
    const char* ScoringSystemDebugComponent::GetName() const { return "Scoring System"; }
    void ScoringSystemDebugComponent::OnActivate()
    {
        *CgsDev::Log::gpDebugPrint
            << "ScoringSystemDebugComponent::OnActivate: inert [FLAG PC boot gate]\n";
    }
}

// ---- DeveloperChallengeManager::OnEventEnd (stuntrace waveB mount closure, 2026-08-26) ----
// FLAG link gate -- NOT a reconstruction.
//
// The wave's ModeManager::ShowModeResults path calls it (BrnModeManager_Finish.cpp:62 includes
// BrnDeveloperChallengeManager.h for exactly this), so the mount needs the symbol.
//
// A REAL BODY EXISTS IN THE TREE -- BrnDeveloperChallengeManager.cpp:394 (the full 14-body TU) --
// and it COMPILES STANDALONE (selfcheck pass). It is not mounted because MOUNTING IT WAS MEASURED
// WORSE: cl /c of that TU + dumpbin /SYMBOLS of its obj, diffed against the defined-symbol set of
// every obj in build\game\obj, leaves SEVEN unresolved externals -- it would close one hole and
// open seven:
//     BrnGameState::StuntModeScoring::GetBestStuntScore() const
//     BrnGameState::ScoringSystem::GetCarCount() const
//     BrnGameState::CarData::GetFinishScore() const
//     BrnGameState::CarData::IsFlawless() const
//     BrnGameState::GameStateModuleIO::OutputBuffer::GetGuiOutputQueue()
//     BrnGameState::GameStateModule::IsActiveRaceCarStillPresent(EActiveRaceCarIndex) const
//     BrnProgression::Profile::IsDeveloperChallengeComplete(int) const
// (GetFinishScore / IsFlawless are the two BrnScoringSystem.h itself flags as an "ADDITIVE GROW
// (declare-only) for the BrnGameState::DeveloperChallengeManager TU", offsets still FLAG'd.)
//
// WHY INERT IS SAFE TODAY: the developer-challenge subsystem is not merely off the offline path,
// it is NOT CONSTRUCTED AT ALL. BrnGameStateModule.h marks mDeveloperChallengeManager
// "NOT Construct()ed yet -- see the named deferral in GameStateModule::Construct", so every
// member the real body reads (its progression manager / street manager / challenge tables) is
// null. The real OnEventEnd opens with a progression-profile lookup; running it against an
// unconstructed manager is strictly worse than not running it.
//
// DELETE-WHEN BrnDeveloperChallengeManager.cpp joins the exe source list -- which needs the seven
// symbols above first. LNK2005 otherwise.
#include "GameSource/GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.h"
namespace BrnGameState
{
    void DeveloperChallengeManager::OnEventEnd(s32 /*liGameModeType*/, bool /*lbWon*/)
    {
    }
}

// FLAG PC-platform leaf: XDK notification-listener creation; a null handle makes
// SystemUserProfile::Update early-return (no sign-in/storage/invite events on PC).
extern "C" void* XNotifyCreateListener(unsigned long long /*qwAreas*/) { return 0; }

// FLAG PC-platform leaf: XDK notification poll; 0 == "no notification pending"
// (unreached while XNotifyCreateListener hands out no listener).
extern "C" int XNotifyGetNext(void* /*hListener*/, unsigned long /*dwMsgFilter*/,
                              unsigned long* /*pdwId*/, unsigned long* /*pParam*/) { return 0; }

// FLAG PC-platform leaf: XDK sign-in-state query; 0 == eXUserSigninState_NotSignedIn,
// so SystemUserProfile::UpdateUserSigninState derives "not signed in" for any user.
extern "C" u32 XUserGetSigninState(u32 /*luUserIndex*/) { return 0; }

// FLAG PC-platform leaf: XDK user-name query; success + empty name (unreached: no
// user ever signs in without XNotify events, so the no-user sentinel holds).
extern "C" u32 XUserGetName(u32 /*luUserIndex*/, char* lpszUserName, u32 luCchUserName)
{
    if (lpszUserName != 0 && luCchUserName != 0)
    {
        lpszUserName[0] = 0;
    }
    return 0;
}

// FLAG PC-platform leaf: XDK profile-settings read; a non-zero error return makes the
// caller treat the read as failed rather than parse an unfilled results buffer
// (unreached: SystemUserProfile only reads settings for a signed-in user).
extern "C" u32 XUserReadProfileSettings(u32 /*luTitleId*/, u32 /*luUserIndex*/,
                                        u32 /*luNumSettingIds*/, unsigned long* /*lpaSettingIds*/,
                                        unsigned long* /*lpcbResults*/, void* /*lpResults*/,
                                        void* /*lpOverlapped*/) { return 87u; /* ERROR_INVALID_PARAMETER */ }

// ===========================================================================
// DebugUI deferred accessors (2026-07-28).
//
// CgsWindow.cpp and CgsVariable.cpp were bodied against four DebugUI members that
// CgsDebugUI.h/CgsTypes.h declare but no TU defines yet -- CgsDebugUI.h says so at
// its head ("the deferred-member accessors ... are declared but defined in" the
// unreconstructed manager TU) and CgsTypes.cpp repeats it for Variant. Both callers
// are in the exe source list, so the link needs definitions.
//
// The debug UI is never constructed on this boot (DebugManager's UI is inert), so
// none of these runs; each returns the value that makes its caller take the
// no-debug-UI branch. DELETE the whole block when the real DebugUI manager /
// Variant conversion TUs land -- the definitions must never coexist.
// ===========================================================================
namespace CgsDev
{
    namespace DebugUI
    {
        // No window is ever active while the UI is inert, so Window::IsActiveWindow
        // is false for every window.
        const Window* DebugUI::GetActiveWindow() const { return 0; }

        // Window::Prepare uses this only to place an unpositioned window; the origin
        // is the neutral answer with no cascade state to read.
        void DebugUI::GetCascadePosition(const Window* /*lpWindow*/, f32& lrfX, f32& lrfY)
        {
            lrfX = 0.0f;
            lrfY = 0.0f;
        }

        // Window::GetPalette forwards straight to this; a zeroed palette keeps every
        // colour lookup in range for a UI that never draws.
        const Palette& DebugUI::GetPalette() const
        {
            static const Palette lsEmpty = Palette();
            return lsEmpty;
        }

        // Variable::SetValueFromString feeds this; leaving the variant untouched
        // means a console "set" is ignored rather than writing a parsed-from-nothing
        // value into a live game variable.
        void Variant::ConvertFromString(const char* /*lpcString*/) {}
    }
}

// ===========================================================================
// RealmcIface record members whose owning TUs are not in the link (2026-07-28).
//
// CgsSaveLoadPS3.cpp's SaveLoadSystem::Save now builds real Realmc records.
// RealmcLoadEntryInfo.cpp and RealmcTitleInfo.cpp are in the exe source list and
// supply the rest, but:
//   * LoadEntryInfo's three-argument ctor (X360 0x82B51A08) is declared in
//     RealmcLoadEntryInfo.h and not yet reconstructed;
//   * SaveCheckParams' ctor/dtor (0x82B51E38 / 0x82B51F88) ARE reconstructed, in
//     RealmcIfaceSaveCheckParams.cpp, but that TU calls RealmcCore::AllocateMem /
//     FreeMemSize / RealmcCopySaveReq, and RealmcCore.cpp drags the whole vendor
//     Message / RefCount / Response / RealmcString closure into the link.
// Save() is never reached on this boot (the PC profile backend is CgsSaveLoadPC and
// nothing writes a card save during a world drive). DELETE these three when the
// Realmc core closure is added to the source list.
// ===========================================================================
namespace RealmcIface
{
    // Empty record: the same zeroed state the default ctor leaves, so a caller that
    // built one of these hands the interface a no-entry request rather than garbage.
    LoadEntryInfo::LoadEntryInfo(const char* /*pName*/, const DataBuffer* /*pA*/,
                                 const DataBuffer* /*pB*/)
        : maTrailing(), mpData(0), muDataSize(0)
    {
        for (unsigned lu = 0u; lu < sizeof(maHead); ++lu)
        {
            maHead[lu] = 0u;
        }
    }

    // Zero slots: the dtor's own "nothing allocated" case, and the value that makes
    // every consumer loop over the request array run zero iterations.
    SaveCheckParams::SaveCheckParams(s32 /*nCount*/, SaveReq* const* /*paSources*/)
        : mCount(0), mppReqs(0)
    {
    }

    SaveCheckParams::~SaveCheckParams()
    {
    }
}

// ===========================================================================
// Faithful-audio-engine phase B5 mount-closure (2026-08-25).
//
// The playback/logic engine TU group entered the exe source list (CgsSoundLogic-
// Module / CgsSoundPlaybackModule(+IO) / Playback CgsEnvironment / CgsVoice /
// CgsFactory / CgsGenericRwacFactory), which makes the linker want the symbols
// below. Each is a declared-only surface whose real body lands with its own
// ledgered slice; every one is INERT on the boot path today -- the paths that
// reach them (voice attach, content lookup by factory name, registry dumps, the
// DAC plug-in events) only run once item-3 content / the phase-D DAC land.
// ===========================================================================

#include "GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.h"  // the factory-shim decls + Playback surface
#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"                    // Logic Environment (Notify)

namespace CgsSound
{
namespace Playback
{
    // Voice::Attach (DWARF CgsVoice.h:501; real body = the slot-resolve +
    // Slot::Attach walk, its own slice). Reached only from Module::AttachVoice
    // (voice-attach traffic -- none until content). False = "did not attach".
    bool Voice::Attach(Name /*akName*/, Handle<Content>& /*arhContent*/)
    {
        return false;
    }

    // Environment::GetFactory (DWARF CgsEnvironment.h:280; the by-name factory
    // lookup, its own slice). Reached from Module::CreateVoice/CreateContent.
    // Empty handle = "no factory registered under that name".
    Handle<Factory> Environment::GetFactory(Name /*aName*/)
    {
        return Handle<Factory>(0);
    }

    // Environment::Allocate (DWARF CgsEnvironment.h; the environment's carve
    // helper). Same allocator route the dispose walk attests (the env's rw
    // allocator), expressed through the committed <5>-descriptor idiom.
    void* Environment::Allocate(u32 lu32Size, u32 lu32Alignment, const char* lpcName)
    {
        rw::BaseResourceDescriptors<5> lDescriptor;
        for (u32 luEntry = 0u; luEntry < 5u; ++luEntry)
        {
            lDescriptor.m_baseResourceDescriptors[luEntry].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[luEntry].m_alignment = 1u;
        }
        lDescriptor.m_baseResourceDescriptors[0].m_size      = lu32Size;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = lu32Alignment;
        rw::Resource lResource = GetAllocator()->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), lpcName);
        return lResource.m_baseResources[0];
    }

    // Environment::operator delete(void*) (DWARF h:167): demanded by the
    // compiler-emitted scalar deleting destructor; the console never scalar-
    // deletes an Environment (disposal is DoDispose -> the allocator-keyed
    // operator delete), so this plain form has no carve to hand back.
    void Environment::operator delete(void* /*lpMemory*/)
    {
    }

    // Registry::Dump (DWARF CgsRegistry.h:137; the debug registry printer, its
    // own slice). Reached from Module::DumpRegistries (a debug-page action).
    void Registry::Dump()
    {
    }

    // The per-factory registry accessors + the reserved-name/init-submix hooks
    // (declared in CgsSoundPlaybackModule.h, each FLAG'd DEFER there -- the
    // AEMS-keystone surface). Null/empty until the factory slices land.
    Registry* GetRwacFactoryRegistry(Factory* /*lpRwacFactory*/)
    {
        return 0;
    }
    Registry* GetAemsFactoryRegistry(Factory* /*lpAemsFactory*/)
    {
        return 0;
    }
    const Name& GenericRwacFactorySkName()
    {
        // The interned source of the console dword_83008650 is not decoded yet
        // (the writer is the RWAC factory bring-up); an uninterned Name cannot
        // match any real request name, so the init-submix assert path stays cold.
        static const Name SK_NAME;
        return SK_NAME;
    }
    void HACK_SetSnd9InitSubmix(Handle<Voice>* /*lphVoice*/)
    {
    }

    // The two interned-name globals Environment::GetR keys on (X360
    // dword_83008650 / dword_830080A8 -- both written by the un-decoded RWAC
    // bring-up interns; zero matches nothing, so GetR returns empty until then).
    const u32 gu32VoiceTypeTag      = 0;
    const u32 gu32NamedSlotSentinel = 0;

    // The serialised-entity type names Registry::GetEntity<T> compares slots
    // against -- interned from the type-name literals, the convention the
    // committed VoiceSchema::SK_TYPE_NAME("VoiceSchema") definition attests.
    const Name ContentSpec::SK_TYPE_NAME("ContentSpec");
    const Name VoiceSpec::SK_TYPE_NAME("VoiceSpec");
}
}

namespace CgsSound
{
namespace Logic
{
    // Logic Environment::Notify (DWARF CgsEnvironment.h; the per-message state-
    // manager dispatch, its own phase-C slice). Reached from the engine
    // Module::ProcessMessageQueue -- which only runs once the per-frame pump
    // (phase C) drives Module::Update.
    void Environment::Notify(const CgsSound::Io::MessageHeader* /*apkMessage*/) const
    {
    }
}
}

namespace rw
{
namespace audio
{
namespace core
{
    // The 3-arg engine event entry Environment::StartDac/StopDac dispatch
    // (events 3/4 at the DAC plug-in). The vendor PlugIn TU models the 1-arg
    // command-ring Event only; the 3-arg form lands with the phase-D Dac slice,
    // and mpDacPlugin is null until then (StartDac asserts it first).
    void RwacPlugInEvent(PlugIn* /*apPlugIn*/, int /*aiEvent*/, int /*aiArg*/)
    {
    }
}
}
}

namespace CgsSound
{
namespace Playback
{
    // ---- the Voice base-virtual surface (phase B5 mount closure) ----
    // playback_voice.obj emits the Voice vtable; the base slots below have no
    // standalone X360 dumps (every live vtable carries a subclass override).
    // Each base declines/idles -- the only reachable behaviour until the
    // concrete voice slices land (no Voice object exists before item-3 content).
    f32 Voice::GetCpuTicks()
    {
        return 0.0f;
    }
    void Voice::DisplayVoiceCpu(f32* /*lpfX*/, f32* /*lpfY*/, f32 /*lfScale*/, bool /*lbDetail*/)
    {
    }
    Voice::EProfileVoiceType Voice::GetProfileVoiceType()
    {
        return static_cast<EProfileVoiceType>(0);
    }
    void Voice::DoDispose()
    {
        // The real disposer is the environment-allocator carve return (the
        // wave-3 dispose pattern); lands with the Voice keystone slice.
    }
    void Voice::DoUpdate(System* /*apSystem*/, f32 /*af32DeltaTime*/)
    {
    }
    bool Voice::DoConnectSend(u32 /*au32Index*/, SubmixVoice* /*apSubmix*/)
    {
        return false;
    }
    bool Voice::DoRemove()
    {
        // "Removal work complete" -- lets Voice::Update advance REMOVING ->
        // REMOVED immediately, the degenerate base behaviour.
        return true;
    }

    // ---- the Slot per-frame surface (Voice::Update's callees; their own
    //      ledgered slices -- CgsVoice.h:344 marks them DEFER) ----
    void Slot::Update(System* /*apSystem*/, Voice& /*arVoice*/,
                      PlayerVoice& /*arPlayerVoice*/, f32 /*af32DeltaTime*/)
    {
    }
    void Slot::Detach(Voice& /*arVoice*/)
    {
    }

    // Content::OnAttach (CgsContent.h marks it DEFER; the attach-side load
    // reference). Inert until voice-attach traffic exists.
    void Content::OnAttach(Voice& /*arVoice*/, Slot& /*arSlot*/)
    {
    }
}
}

namespace CgsSound
{
namespace Playback
{
    // ContentType::GetContentClass @ 0x82691778 -- an exact copy of the
    // canonical body in CgsDataStructures.cpp, carried here because that TU is
    // NOT in the exe source list yet (its EntityFixer fix hooks need the
    // declared-only Entity member-pointer fixup templates -- their own slice);
    // the resolved-pointer accessor is all the mounted playback module needs.
    // Remove this copy when CgsDataStructures.cpp mounts.
    const ContentClass& ContentType::GetContentClass() const
    {
        CGS_ASSERT(mpContentClass != 0, "mpContentClass");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(mpContentClass) & 1) == 0,
                   "This Data Structure is not resolved. (Name ");
        return *mpContentClass;
    }
}
}

// (The FriendsListEntry::Select link gate that lived here died 2026-08-26: the friends-list
// tranche landed its own gates TU, BrnFriendsListLinkGates.cpp, which carries Select --
// the DELETE-WHEN fired; two definitions would be LNK2005.)
