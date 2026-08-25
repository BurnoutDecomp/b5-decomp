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
