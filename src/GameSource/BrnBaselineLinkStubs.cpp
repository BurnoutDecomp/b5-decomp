// ===========================================================================
// BrnBaselineLinkStubs.cpp -- FLAG (baseline link stubs).
//
// Out-of-line definitions the game exe needs to link whose real bodies are either
// unreconstructed or in TUs that cannot be mounted yet. Each block names what blocks it.
// None of these paths runs on the offline boot -> title -> driving slice.
// ===========================================================================

#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsEmergencyFragPoolModuleState.h"
#include "GameSource/Replays/BrnReplayModule.h"
#include "GameShared/GameClasses/Sound/CgsTestBedAllocator.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsFloatQuantiser.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"                      // BrnPhysics::Vehicle::RaceCarState
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"          // InGamePlayerStatusInterface (+ NetworkPlayerStats / LiveRevengeRelationship)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"                // DebugUI
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"                  // Palette / Variant
#include "SDKs/Realmc/RealmcLoadEntryInfo.h"                                                  // LoadEntryInfo (3-arg ctor stub)
#include "SDKs/Realmc/RealmcIfaceSaveCheckParams.h"                                           // SaveCheckParams (ctor/dtor stubs)

namespace CgsResource
{
    // The emergency defrag strategy. Its real bodies are in CgsEmergencyFragPoolModuleState.cpp,
    // which cannot be mounted: RunPoolDefragmentation calls Pool::BeginEmergencyDefragmentation,
    // and that needs CgsMemory::Relocator (no class in the tree, only a byte pad in the pool
    // module) plus the job-scheduler entry it submits. Delete both when the Relocator lands.
    // Returning false / doing nothing means the pool never falls back to an emergency compaction.
    bool EmergencyFragPoolModuleState::RunDefragAlgorithm(AllocListSet*, LinearHeapNode*, s32, s32)      { return false; }
    void EmergencyFragPoolModuleState::RunPoolDefragmentation(RelocateRequest*, RelocateSource*, u32, s32) {}
}

namespace BrnReplays
{
    // Link stub for the replay module ctor (BrnGameModule constructs mReplayModule). The real
    // ctor is in BrnReplayModule.cpp, unmounted: that TU also defines Update_Dispatch ->
    // GPUDiskWriteStream::Dispatch, and Stream/BrnReplayGPUDiskWriteStream.cpp does not compile
    // (two u64 -> CgsFileSystem::Handle casts at :186/:220). Delete this stub when that closure
    // mounts.
    ReplayModule::ReplayModule()
    {
        // The two members ReplayModule::Prepare / StoreSerialisers depend on; the real ctor
        // zeroes them too. An unzeroed mpLinearMalloc would be read as a live allocator.
        mbPrepared     = false;
        mpLinearMalloc = 0;
    }
}

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
    // Online scoring virtuals with no body anywhere in src (the scoring TUs are mounted and
    // define the rest). Each dies when its real body lands in its own TU.

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
}

namespace CgsSound
{
namespace TestBed
{
    // Testbed-allocator tail. DoAllocate is live on the boot path (rw::audio::core::System::
    // CreateInstance carves through it).
    //
    // FLAG [interim pass-through]: the real DoAllocate is the full tracked carve
    // (per-block Header + guard words + history ring + verbose log), its own recon slice. This
    // interim body forwards the carve straight to the backing allocator: behaviour-transparent
    // to every consumer; the debug surfaces (SanityCheck/SafeDump/IsValidMemoryAddress) see an
    // empty tracking list until the real body lands. DoFree mirrors it.
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
// Helpers that mounted TUs call and that are declared-only or live in TUs that drag XDK
// externals. None of these paths runs on the boot slice; replace each with the real body
// when its subsystem is wired into the build.
// ===========================================================================

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
    // NOT A STUB. Relocated from src/pc/gcm/renderengine/VertexBuffer.cpp, which stays unmounted
    // (29 externals / 23 unresolved plus an LNK2005 against the linked CgsIm2d.cpp). The two
    // bodies are kept identical; DELETE-WHEN VertexBuffer.cpp mounts. Slot 0 must not be
    // zero-size: LinearResourceAllocator::DoAllocate treats a zero lane as "nothing requested".
    // See VertexBuffer.cpp for why {m_size, m_alignment} are written by name.
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

// ===========================================================================
// XDK boundary shims: the XDK imports referenced by CgsSaveLoadPS3.cpp (SaveLoadSystem::
// Update's overlapped pump), CgsGuideIntegration.cpp (SystemUserProfile's XNotify/XUser
// watcher) and CgsXOverlapped. No game body to reconstruct; the PC has no XDK, so each
// returns the value that makes its caller take the no-device/no-user branch.
// ===========================================================================

// FLAG PC-platform leaf: XDK overlapped-result query; 0 (== ERROR_SUCCESS, not the
// 997/996 still-pending codes) tells SaveLoadSystem::Update the async op is finished,
// clearing its in-flight state -- no overlapped I/O ever starts on PC.
extern "C" unsigned long XGetOverlappedResult(void* /*lpOverlapped*/,
                                              unsigned long* /*lpdwResult*/,
                                              int /*bWait*/) { return 0; }

// FLAG PC-platform leaf: XDK overlapped EXTENDED-error query; only reached from
// CgsXOverlapped::GetResultString on a code its three named cases (0 / 996 / 997) did not
// cover. 0 on a platform where no overlapped I/O is ever started.
extern "C" unsigned long XGetOverlappedExtendedError(void* /*lpOverlapped*/) { return 0; }

// ---- StreetManagerDebugComponent vtable gate --------------------------------------------
// GameStateModule embeds StreetManager, whose embedded debug component's vtable is emitted by
// the module ctor chain, so these two virtuals must link. The real TUs
// (BrnStreetManagerDebugComponent.cpp + _wO_01.cpp) are unmounted: they close over
// StreetManager::SetChallengeUserScore, ScoreList::KAI_MIN/MAX_SCORES and the
// ProgressionManager trophy hooks (16 externals). GetName is the real one-line body;
// OnActivate is an inert gate (the console registers six debug-menu cheat callbacks here).
// Delete both when those TUs mount.
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

// RealmcIface record members whose owning TUs are not in the link (SaveLoadSystem::Save
// builds them; Save is never reached on this boot -- the PC backend is CgsSaveLoadPC):
//   * LoadEntryInfo's three-argument ctor is declared in RealmcLoadEntryInfo.h and not
//     reconstructed;
//   * SaveCheckParams' ctor/dtor are reconstructed in RealmcIfaceSaveCheckParams.cpp, but
//     that TU calls RealmcCore::AllocateMem / FreeMemSize / RealmcCopySaveReq, and
//     RealmcCore.cpp drags the vendor Message / RefCount / Response / RealmcString closure.
// DELETE-WHEN the Realmc core closure is added to the source list.
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
// Sound playback/logic engine surface the mounted TU group (CgsSoundLogicModule /
// CgsSoundPlaybackModule(+IO) / Playback CgsEnvironment / CgsVoice / CgsFactory /
// CgsGenericRwacFactory) references and no TU defines. Each real body lands with its own
// slice; all inert on the boot path (voice attach, registry dumps only run once content lands).
// ===========================================================================

#include "GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.h"  // the factory-shim decls + Playback surface
#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"                    // Logic Environment (Notify)

namespace CgsSound
{
namespace Playback
{
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

    // Real intern: the console static initializer (sub_82C654A8) stores
    // Name::MakeHash("~GenericRwacFactory::SK_NAME~") into dword_83008650.
    const Name& GenericRwacFactorySkName()
    {
        static const Name SK_NAME("~GenericRwacFactory::SK_NAME~");
        return SK_NAME;
    }
    void HACK_SetSnd9InitSubmix(Handle<Voice>* /*lphVoice*/)
    {
    }

    // The interned-name globals Environment::GetR keys on: gu32VoiceTypeTag is the console
    // dword_83008650 (GetR compares voice->mFactory.mName against it); dword_830080A8 is
    // PlayerVoice::SK_PLAYER_SLOT_NAME (used by GenericRwacVoice::CreateVoiceInstance and
    // every streaming voice attach/detach call).
    const u32 gu32VoiceTypeTag      = Name("~GenericRwacFactory::SK_NAME~").GetValue();
    const u32 gu32NamedSlotSentinel = Name("~PlayerVoice::SK_PLAYER_SLOT_NAME~").GetValue();

    // The serialised-entity type names Registry::GetEntity<T> compares slots
    // against -- interned from the type-name literals, the convention the
    // committed VoiceSchema::SK_TYPE_NAME("VoiceSchema") definition attests.
    const Name ContentSpec::SK_TYPE_NAME("~ContentSpec~");
    const Name VoiceSpec::SK_TYPE_NAME("~VoiceSpec~");
}
}

namespace CgsSound
{
namespace Playback
{
    // ---- the Voice base-virtual surface ----
    // playback_voice.obj emits the Voice vtable; the base slots below have no standalone
    // console dumps (every live vtable carries a subclass override). Each base declines/idles.
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

}
}

