#include "GameSource/Gui/BrnGuiPerfmons.h"
#include "GameSource/Gui/BrnGuiModule.h"
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h"
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h" // original movie acquire request
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h" // collision-world event 493
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h" // collision-world swap requests/replies
#include "GameSource/Resource/BrnResourceAllocator.h" // console-memory markers
#include "SDKs/EA/GameTalk/GameTalk.h"
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"   // [H3b] MapTransform (the sat-nav view-rect install)

#include <cstdio>                                                         // std::snprintf (log formatting)
#include <windows.h>                                                      // [gate] GetEnvironmentVariableA (BRN_FLAPT_AFTER_DISPATCH)
#include <chrono>   // the PC frame clock for the view time-step event (FLAG: wall clock)
#include <cstddef>  // offsetof for the native event-22 payload
#include <cstring>  // std::strcmp (ARTIST GUI-audio action-name table)

#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h" // CgsDev::Message::gxMessageFilterFlags
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h"// CgsResource::BundleLoader ([PC IO] FSM loads)
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"// CgsResource::ResolveResourceType
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"    // E_RESOURCETYPE_LUACODE / E_MEMTYPE_*
#include "GameShared/GameClasses/Fsm/Resources/CgsLuaCodeResource.h"      // CgsResource::LuaCodeResource
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> (channel command records)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::GuiEventPlayAptMovie (channel-41 payload)
#include "GameShared/GameClasses/Gui/Model/CgsEventInterpreterModule.h"   // priority removal/blocking event ids
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::GuiEventLoadNotification / GuiEventLoadRequest
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // BrnGui::GuiAudioTriggerEvent
#include "GameSource/GameState/Progression/BrnProfile.h"                   // BrnProgression::Profile::Construct (the PC progression block seed)
#include "GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.h"        // AlwaysAvailableComponentsManager + free accessor (bodied below)
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers (flow interface wiring)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"       // CgsGui::AptAuxPointer (the AptAux singleton)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // CgsGui::AptCommunicator (the per-frame trigger publish)
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"              // CgsSound::Playback::Name::MakeHash (event-155 keys)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"                // FLApt live-instance allocator
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "rw/rwcore_general_alloc.h"                                   // actual public allocator derivation
#include "GameSource/Resource/BrnGameDataModuleIO.h"                      // BrnResource::GameDataIO::Input/OutputBuffer (the colour-calibration screen's IO)
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"        // AllocatorList::GetRWLinearResourceAllocator (mGuiConfig.mpTextureAllocator)
#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h" // GetScriptedLoadGameData{Input,Output} (the PC stand-in for the scheduler's GameData IO pair)
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h"      // BrnNetwork::LiveRevengeProfile (the PC live-revenge block)

// Existing PC platform providers in System/PC/CgsXboxLivePC.cpp.
extern "C" void* XNotifyCreateListener(unsigned long long luAreas);
extern "C" int XNotifyGetNext(void* lhListener, unsigned long luFilter,
    unsigned long* lpuId, unsigned long* lpuParam);

// DecFIGS types GuiModule::Construct's alternate-text palette as const RGBA*.
// ARTIST's eight packed words at 0x82F27F84 are the complete table.
struct RGBA
{
    u32 mPacked;
};

// BrnGui::GuiModule owns the original staged lifecycle and contained Model/View.
// The selected PC entries borrow the current caller's real IO buffers. Request
// servicing stays at the PC boundary of that actual Model. Existing raw game and
// harness consumers receive one mirror of the completed canonical GUI output.

// The loading-screen visual signal (BrnRendererModule::Render shows the loading screen
// while it's set). Driven by the REAL protocol now: the states post the loading-screen
// commands 19/20 on the GUI-out channel and the game module's BridgeGuiToGame writes the
// dispatch write buffer's command (BrnDispatchThreadInputBuffer.h).
extern bool gBrnInitialLoadingComplete;    // set by the game-flow when the load stages finish
extern bool gBrnGuiDrivesLoadingScreen;    // we set this while the HUD flow FSM is live
// The in-game flow-state latch (BrnGameMainFlowInGameState.cpp) -- gates the PC
// ignition-event stand-in in the per-frame view drive below.
namespace BrnGameMainFlowController { extern bool gBrnInGameStateActive; }
extern bool gbLogGuiAudioTriggers; // X360 byte_82FB5098; defined by CgsAptCommunicator.cpp.

namespace
{
    // ARTIST event 22's unwrapped inner body is 100 bytes, with no GuiEvent header.
    // Keep a payload-only view here because the current CgsGui::GuiEventSoundTrigger
    // carries a 12-byte host header ahead of these fields.
    struct GuiEventSoundTriggerPayload22
    {
        char macTypeName[32];
        char macActionName[32];
        char macLabel[32];
        s32 miLayer;
    };
    static_assert(sizeof(GuiEventSoundTriggerPayload22) == 100 &&
                  offsetof(GuiEventSoundTriggerPayload22, miLayer) == 96,
                  "ARTIST event-22 payload reads its layer at byte 96");

    // ARTIST file-global Tool.GUI request owners at82FB5060/5088/5089.
    // The native image starts these bytes at zero. Construct clears only the
    // start flag; native Update consumes start before stop. Names are host projections.
    char gacGuiToolHookName[32] = {};
    bool gbGuiToolStartHookRequested = false;
    bool gbGuiToolStopHookRequested = false;

    // Host extraction of the callback's duplicated assert/copy block. Keep
    // assertion continuation and strncpy's original fixed32 behavior.
    void CopyGuiToolHookName(const char* lpcContent, s32 liOriginalLine)
    {
        if (!lpcContent)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpcContent != NULL",
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", liOriginalLine);
            CgsDev::Assert::EndAssert();
        }
        if (std::strlen(lpcContent) >= 32)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lMessage;
            CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
            lStream << "String too long: " << (lpcContent ? lpcContent : "<NULLSTRING>");
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h", 55);
            CgsDev::Assert::EndAssert();
        }
        std::strncpy(gacGuiToolHookName, lpcContent, 32);
    }

    // [FLAG PC witness] BRN_HUD_VIS gate for the `[hud-vis] cmd` lines in the channel-42
    // drain (lane `hud`, bug-test wave 2026-09-06). NOT IN THE X360 BINARY.
    // DELETE-WHEN tools\tests\cases\hud_visibility.ps1 is retired.
    bool HudVisWitnessEnabled()
    {
        static s32 siState = -1;
        if (siState < 0)
        {
            char lacValue[8] = { 0 };
            const DWORD luLen = ::GetEnvironmentVariableA("BRN_HUD_VIS", lacValue,
                                                          sizeof(lacValue));
            siState = (luLen > 0 && lacValue[0] != '0') ? 1 : 0;
        }
        return siState != 0;
    }

    // [FLAG PC witness] BRN_FREEROAM_DIAG: one `[freeroam-gui] routed <id> -> <consumer>` line
    // per free-roam GUI event the inbound dispatch hands to the cache or the tracker, first 8
    // per id (165 repeats every frame the car sits in a landmark area).
    void FreeRoamRouteWitness(s32 liId, const char* lpcConsumer)
    {
        static const bool sbDiag = (getenv("BRN_FREEROAM_DIAG") != 0);
        if (!sbDiag || CgsDev::Log::gpDebugPrint == 0)
            return;

        static const s32 KAI_WITNESSED_IDS[] = { 64, 79, 165, 201, 211, 232, 233 };
        const s32 KI_NUM_WITNESSED_IDS =
            static_cast<s32>(sizeof(KAI_WITNESSED_IDS) / sizeof(KAI_WITNESSED_IDS[0]));
        static s32 saiLinesLeft[sizeof(KAI_WITNESSED_IDS) / sizeof(KAI_WITNESSED_IDS[0])] =
            { 8, 8, 8, 8, 8, 8, 8 };

        for (s32 liSlot = 0; liSlot < KI_NUM_WITNESSED_IDS; ++liSlot)
        {
            if (KAI_WITNESSED_IDS[liSlot] == liId)
            {
                if (saiLinesLeft[liSlot] > 0)
                {
                    --saiLinesLeft[liSlot];
                    *CgsDev::Log::gpDebugPrint
                        << "[freeroam-gui] routed " << liId << " -> " << lpcConsumer << "\n";
                }
                return;
            }
        }
    }

    // Backing for the per-flow FSM bundle pools (3 mem types each; the boot FSM scripts
    // are tiny single-state bundles ~1.5 KB -- BRNSCREENFSM is the largest at ~68 KB --
    // and each pool reserves 64 KB for its own management structures). One backing per
    // flow slot: each flow's ScriptedFsm holds its LuaCode resource while live.
    const u32 KU_FSM_POOL_BYTES = 256u * 1024u;
    u8 s_fsmPoolBacking[BrnGui::E_GUIFLOW_COUNT][CgsResource::E_MEMTYPE_NUMTYPES][KU_FSM_POOL_BYTES];
    // FLAG PC-platform leaf: ARTIST allocator 38 ("Gui_FSM_Lang_Alloc") has
    // 0x80000 bytes for 32-bit Lua. Native Lua tables, closures and allocator
    // links contain host pointers. Scale this PC backing with pointer width so
    // the three VMs retain their working space while another flow compiles.
    // Keeping the console byte budget on x64 exhausts it after GUI input
    // traffic, so the next HUD script never defines GetCurrentState.
    const u32 KU_FSM_LUA_HEAP_BYTES = 0x80000u * sizeof(void*) / sizeof(u32);
    u8 s_fsmLuaHeapBuffer[KU_FSM_LUA_HEAP_BYTES];

    // Backing for the HUD flow's 14-state pool (BrnHudFlow::Prepare carves the state
    // objects out of this linear region; BootLegal is the largest at a few KB).
    u8 s_hudStatePoolBacking[512u * 1024u];

    // Backing for the overlay flow's 15-popup-state pool (X360 sizes 0x40/0x50/13x0x148;
    // sized generously for the x64 member inflation).
    u8 s_overlayStatePoolBacking[256u * 1024u];

    // Backing for the SCREEN flow's 61-state pool (X360 total ~638 KB with 4-byte
    // pointers; the big real states -- ON_GAME_ROOM 86 KB, ON_CUST_MAT 57 KB -- widen
    // on x64, so the region carries 2x headroom).
    u8 s_screenStatePoolBacking[2u * 1024u * 1024u];

    // The view's FLApt timeline tree is one linear allocation, reset when the GUI
    // module is rebuilt. ARTIST receives the GUI module's LinearAllocator here;
    // the PC owner supplies an equivalent dedicated region.
    alignas(16) u8 s_flaptLinearBacking[16u * 1024u * 1024u];
    CgsMemory::LinearMalloc s_flaptLinear;

    // Backing for the profile manager's allocators (FLAG PC stand-in: the console hands
    // the 0x26 game-data heap + a module linear; the PC module owns dedicated regions).
    // The heap carves the 3x9608 mugshot circular buffer + the SLS callback block; the
    // LINEAR carves the save/load system's mugshot image buffer
    // (miExtraFilesSizeBytes 9600 * mugshotsPerType 20 * types 5 == 960000 bytes) + the
    // content-info file buffer -- so it must clear ~1 MB (the 64 KB it had returned null
    // from SaveLoadSystem::Prepare's Malloc and tripped the mpMugshotBufferData assert).
    u8 s_profileHeapBacking[192u * 1024u];
    u8 s_profileLinearBacking[2u * 1024u * 1024u];

    // FLAG PC-platform leaf: the live-revenge profile block the console's NETWORK module
    // installs on the ProfileManager via GUI event 351 (SetLiveRevengeProfile). That
    // subsystem is not wired on PC, so mpLiveRevengeProfile stays null and
    // ProfileManager::Bootup->ReadProfileData faults (memcpy from mpLiveRevengeProfile).
    // This stand-in is a Clear()ed profile -- the current version and an empty relationship
    // table, what the console's network module installs before any save loads -- installed in
    // Prepare below. Its size is the real segment width (BrnGuiProfile.h: live-revenge is
    // memcpy'd 30016 B), asserted below.
    //
    // ⛔⛔ [one-profile wave 2026-08-28] s_pcProgressionProfileBacking (sizeof(Profile)) and
    // s_pcProgressionManifestBacking (4096 "ExpectedManifest") ARE DELETED, not moved. They
    // were a SECOND BrnProgression::Profile: the save deserialised into them while every
    // gameplay reader used GameStateModule::mProgressionManager::mProfile. The console's own
    // installer -- GuiModule::Update's case-350 arm consuming GameStateModule::PreWorldUpdate's
    // action-193 record -- now runs (see the arm and Prepare below), so both pointers are the
    // live game-state objects and neither stand-in has anything to stand in for.
    // ⛔ DO NOT RE-ADD either one to "guard" a null: the console's ProfileManager::Construct
    // zeroes both members too, and nothing dereferences them before BF_PROFILE -- which runs
    // hundreds of log lines after the first sub-step's event-350 post.
    BrnNetwork::LiveRevengeProfile s_pcLiveRevengeProfile;
    static_assert(sizeof(BrnNetwork::LiveRevengeProfile) == 30016,
                  "the live-revenge block is the 30016-byte segment the profile manager copies");

    // The subscription record the states post through StateInterface::RegisterForEvents
    // (X360 wire records 34/35: { s32 miEventType; EventObserver* } -- only the leading
    // event-type word matters to the dispatch table; the pointer is the posting observer).
    struct RegisterEventRecord
    {
        s32 miEventType;
    };

    // StateInterface::PriorityRegisterForEvent's ARTIST wire prefix. The observer
    // pointer follows at +0x968 on PPC; GuiModule already knows the posting flow, so
    // the native pointer is deliberately not part of this parser.
    struct PriorityRegisterRecordPrefix
    {
        s32 miEventType;
        s32 maiEventTypeOverridden[600];
        u32 muOverrideCount;
    };

    // The event-64 record: the module posts the GuiCache pointer each frame (the X360
    // AddEvent(&cachePtr, 64, ptr-size) in GuiModule::Update -- the states' "cache" feed).
    struct GuiEventCache : public CgsModule::Event
    {
        BrnGui::GuiCache* mpGuiCache;
    };
}

// ============================================================================
// THE APT BRING-UP (transplanted from the retired BrnGuiAptRuntime.cpp): the
// GuiModule owns the Apt bring-up driver; render callbacks use the frame owner's buffers, as the
// console GuiModule::Prepare owns the view/apt prepare chain. Every FLAG below
// is carried over unchanged; the remaining stand-in is the [PC IO] language
// string-table load (its console replacement, the CgsLanguage::Sku pump, is
// reconstructed and awaits wiring -- see the retirement plan).
// ============================================================================

#include <cstdio>    // std::snprintf (probe logging)
#include <cstring>   // std::strncpy
#include <chrono>    // steady_clock (the faithful timeline pacing's elapsed-ms source)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::WriteToLog

// ---- the Apt text render-data hooks (dynamic-text draw/release) --------------
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCallbackRender.h"   // AptCallbackRender::DrawString / DeallocateString + AptMaskRenderOperation

// ---- the Apt host adaptor + render handler (the render bridge) --------------
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"          // CgsGui::AptAux / AptAuxPointer
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"// CgsGui::AptImRendererSet / AptIm2dRenderBuffer
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"                // CgsGui::ViewModule (real Apt/text owner)
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"                       // CgsGuiModuleIO::ImRendererSet
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameSource/Gui/BrnGuiPerfmons.h"

// ---- the Apt engine leaves that ARE bodied (the bring-up drives these) ------
#include "SDKs/EATech/Apt/DogmaAllocator.h"                   // DOGMA_PoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"            // AptValueGC_PoolManager
#include "SDKs/EATech/include/Apt/AptDefine.h"                // gpGCPoolManager / gpNonGCPoolManager
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"     // AptActionInterpreter + AptInitParmsT
#include "SDKs/EATech/include/Apt/AptTarget.h"                // AptTarget + gpAptTarget singletons
#include "SDKs/EATech/include/Apt/AptString/EAString.h"       // EAStringC (loader file name)
#include "SDKs/EATech/include/Apt/AptLoader.h"                // AptLoader / GetTarget / AptFilePtr

// ---- the original 2D command-buffer types used by the Apt render callbacks ----
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // ImRenderBuffer<V> (AptAux render-set only)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"  // CgsGraphics::Im2d (the PROVEN immediate render path)
#include "pc/gcm/renderengine/texture.h"                      // renderengine::Texture (mesh texture binding -> mpD3DTexture)
#include "rw/rwcore_structs.h"                                // rw::ResourceAllocatorRegistry::GetDefaultAllocator

// ---- the synchronous bundle load + the Apt-data resource -> geometry path ----
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"         // CgsResource::Pool / Entry
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h" // CgsResource::BundleLoader
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h" // CgsResource::ResolveResourceType
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistration.h" // RegisterAllResourceTypes
#include "GameShared/GameClasses/System/Resource/CgsSmallResourcePS3.h"     // E_MEMTYPE_* / E_MEMTYPE_NUMTYPES
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h"  // CgsGui::AptDataHeader (relocated movie header)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // AptCommunicator (the framework-bootstrap ordering gate)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h" // CgsResource::GuiGeometryObject / File
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"  // CgsGui::GuiEventLoadNotification (the load-notification records)

// ---- STEP 1 FAITHFUL INSTANTIATION (gated; the direct-geometry render stays the fallback) ----
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"    // AptCharacterAnimation::Fixup (FixupTranscode/InPlace)
#include "SDKs/EATech/include/Apt/AptConstFile.h"             // AptConstFile (pointer-size dispatch)
#include "SDKs/EATech/include/Apt/AptFile.h"                  // AptFile (synthesised loaded-file handle for instantiation)
#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"       // AptGetAnimationAtLevel (the root level-0 CIH)
#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"// MakeCharacterAnimationInst (loaded movie -> live inst)
#include "SDKs/EATech/include/Apt/AptCIH.h"                   // AptCIH (the root display-object node)
#include "SDKs/EATech/include/Apt/AptMovie.h"                 // AptMovie::doFrameControls / resolve (timeline driver)
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h" // mDisplayList (the root CIH's child display list)
#include "SDKs/EATech/include/Apt/AptCharacterTextInst.h"     // SetTextValue/ClearStateFlags (the apt_labeltxt bridge)
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"       // AptAnimationTarget::GetRootDisplayList (the director's root list)
#include "SDKs/EATech/include/Apt/AptDisplayList.h"           // AptDisplayList::AsState
#include "SDKs/EATech/include/Apt/AptDisplayListState.h"      // AptDisplayListState::mpFirst (the placed-node chain)
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"         // AptCurrentRenderTreeManager / gnCurrUpdateTick (StopMovie's removal op)
#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"     // AptRenderTreeManager::Update_ItemRemoved (StopMovie's park)

// ---- the Apt TEXT system (fonts + glyph batcher + localisation) --------------
// The PC stand-in for CgsGui::ViewModule's owned text sub-objects (GetFontCollection() /
// GetTextRenderer() / GetLanguageManager()): the bring-up constructs + loads them itself and
// hands them to AptAux::Construct so the Apt dynamic-text path lays out + draws visible glyphs.
#include "GameShared/GameClasses/Gui/View/CgsGuiFontCollection.h"   // CgsGui::FontCollection (typeface set)
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"   // CgsGraphics::TextRenderer (glyph batcher)
#include "GameShared/GameClasses/Fonts/CgsFont.h"                   // CgsResource::Font (loaded typeface)
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h" // E_RESOURCETYPE_FONT
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"     // CgsLanguage::LanguageManager
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"            // CgsMemory::HeapMalloc (language allocator)
#include "pc/gcm/renderengine/device.h"                             // renderengine::gDevice (font atlas gate)

#include <cstdlib>   // malloc (font pool backing, language file block)



// ---- the Apt allocator-pool globals (X360 off_8324D808 aliases) -------------
// Declared extern in the Apt TUs (AptGlobals.cpp defines them, null). Re-declared
// here (legal redundant declarations) so this TU can WIRE them off our pools -- the
// X360 AptInit that wires them is un-reconstructed, so the bring-up does its job.
// (gpNonGCPoolManager / gpGCPoolManager come from AptDefine.h, already included.)
extern DOGMA_PoolManager* gpAptOperandStackPool;   // off_8324D808 (operand-stack arrays)
extern DOGMA_PoolManager* gpAptPseudoDataPool;     // off_8324D808
extern DOGMA_PoolManager* gpAptRenderManagerPool;  // off_8324D808
extern DOGMA_PoolManager* gpAptSharedPtrPool;      // off_8324D808
extern DOGMA_PoolManager* gpAptSingleListPool;     // off_8324D808
// gpAptValueGCPool extern RETIRED (2026-08-11): off_8324D834's three homes were
// unified onto AptDefine.h's gpGCPoolManager; this never-referenced alias
// declaration pointed at the deleted void* view.

// The interpreter VM singleton (X360 &dword_8324E760) -- defined in AptGlobals.cpp.
extern AptActionInterpreter gAptActionInterpreter;

// The faithful per-frame dynamic-text refresh pass (AptCIHBehaviour.cpp): install
// AptCIH::ProcessTextInst as the generalised-process callback + walk the root subtree,
// so every dynamic-text node re-resolves its bound text + (re)lays it out through
// EnsureStringAllocated. The console runs this inside AptUpdate (sub_82B0D608).
struct AptCIH;
void AptRunGeneralisedTextProcess(AptCIH* pRoot);

// The Apt input-recorder/replay gate (X360 byte_82F733F7; defined in AptGlobals.cpp, default 0).
// When SHUT (0) a freshly-placed clip's play-head does NOT auto-advance (deterministic replay);
// when OPEN (1) fresh clips step + run doFrameControls, placing their nested content. Normal play
// runs with the gate OPEN -- the console opens it outside replay recording. The bring-up opens it
// so the imported sprite CONTAINERS recurse + place their nested shapes/images. // FLAG: set to the
// normal-play value (the replay-recorder subsystem that would toggle it is out of this slice).
extern unsigned char gbAptRecorderGate;   // byte_82F733F7

namespace
{
    using namespace BrnGui;

    // Current-owner readiness comes from its real staged Prepare chain.
    // The process-global ready flags, private language/Apt heaps and synthetic
    // pending-language queue retired with the selected manual resource facade.

    // (The PER-MOVIE SLOTS -- framework/flow/persist/aux AptMovieSlot machinery, the
    // BF_LEGAL park latch and the per-slot tick pacing -- are RETIRED, slice 2 of the
    // runtime retirement: every movie mounts through the ENGINE chain now. BootPreload
    // plays "main" @ level 0 over channel-41 exactly like the console; AptLoadAnimation
    // -> AptLinker::Load / AptLinker::Update own the load + mount + unload, and the
    // PERSISTENTAPT component library stays a registered-data import library (the
    // GuiResourceModule's up-front bank), not a mounted movie.)

    // PHASE 2: the per-slot movie pool backings + the host bundle-IO leaf (AptLoadMovieSlot)
    // are RETIRED -- every movie slot's bundle now loads through the real GuiResourceModule's
    // [PC] servicer (CgsGuiResourceModulePC.cpp owns the streamed-apt bank), so the host no
    // longer carves its own resident movie pools here.

    // The Apt-data resource type id (X360 0x1E == 30; CgsResource::AptDataHeaderType::GetTypeID).

    // ====================================================================================
    // FAITHFUL LOAD-PATH STATE.
    //
    // The load flow is the console notification chain with only the bundle IO host-side:
    //   AptLoadMovieSlot ([PC IO] BundleLoader) -> queued GuiEventLoadNotification(s) ->
    //   GuiModule's bridge drains them as view events (14) -> the REAL ViewModule::
    //   ProcessIncomingLoadNotification @0x8285BD30 -> AptDataHandler::AddAptData; then
    //   the play-movie event drives DriveFaithfulLoad: FindAptData -> AptLoader::Load ->
    //   AptCallbackFile::LoadAnimation -> AptCompleteAnimationAsyncLoad -> CompleteLoad
    //   -> Resolve -> Fixup -> AptGetAnimationAtLevel -> MakeCharacterAnimationInst.
    // The movie root locates inside CompleteLoad (const chunk movieOffset @+0x18, the
    // XB1-attested native-8 formula); the resolve64 bounds derive inside LoadAnimation
    // from the header's size slot. The old byte-pattern root locator, the
    // gAptLoadAnimRootOverride / span pokes, and the direct AddAptData host calls are
    // RETIRED (2026-07-09, the step-5 load-ownership move).
    //
    // DEFENSIVE: the whole path is heavily guarded; ANY failure leaves the slot's
    // mbInstantiated false and the game up.
    // ====================================================================================

    // (The faithful-load state -- char-anim def base, AptFile handle, root CIH,
    //  attempted/instantiated flags -- moved into AptMovieSlot above: each of the two
    //  movies owns its own copy, keyed off the slot the load path is driven with.)
    // STAGED VM EXECUTION (2026-07-01): the deferred-action DRAIN (AptAnimationTarget::
    // RunActions after each tick). The full chain is wired + verified live -- with this
    // false the queue fills faithfully (289 enqueues to frame 100, boot green); with it
    // true the first real bytecode EXECUTES (45 ops traced: pushes, Stop, a taken
    // BranchIfTrue) but the run dies at frame ~2 in the byte-push family: a sub-clip
    // stream indexes the movie string DICTIONARY (interp mpRegisters) before any
    // DefineDictionary (0x88) stream has installed it. On the console the ordering
    // guarantee comes from the clip-event queue chain (AptCIH_queueClipEvents -- still
    // the deferred link-cluster stub) + init-action sequencing. Flip to true once that
    // cluster is homed. // (RESOLVED 2026-07-01 -- see below; the switch is retired.)
    //
    // STATUS 2026-07-01 (FINAL): the drain is PERMANENTLY ON. The frame-3 crash was a
    // straddled (never-relocated) tag-1 stream slot from a deep-nested movie (the
    // converter 4-byte-straddle family) dereferenced into a wild runStream PC -- now
    // guarded at both the enqueue (queueFrameActions) and the drain (RunActions). With
    // the CONVERTER-FIXED bundles (branch offsets corrected, 2026-07-01) the full VM
    // runs steady-state: 1243 tick+drain frames, 0 asserts, the executed stop()s hold
    // the clips faithfully.
    bool   s_bDisableRunActions    = false;

    // ---- the faithful timeline PACING (console AptUpdate/sub_82B0D608) ------------------
    // The console accumulates the ELAPSED MILLISECONDS into the root anim inst (charInst+36)
    // and ticks the display list once per movie msPerFrame (character+44 -- TITLE_SCREEN02
    // authors 33ms == 30fps), catch-up looping while the accumulator holds a full frame;
    // RunActions runs per TICK and the generalised text/mask process per UPDATE. Without
    // this the movie ticked once per RENDER frame (~170fps) and every transition played
    // ~5.7x too fast (a blink instead of an animation). The pacing state (msPerFrame,
    // accumulator, last-update clock, tick counter) is PER SLOT (AptMovieSlot above):
    // each movie ticks on its OWN authored clock, exactly as the console paces each
    // level's root anim inst separately.

    // (The import content-load registry + AptLoaderStartAsyncLoad -- the PC body
    // of the console's async .apt stream -- are RE-HOMED to the marked PC leaf TU
    // GameShared/GameClasses/Gui/PC/CgsAptStreamLoaderPC.cpp.)

    // NOTE: the Apt allocator/interpreter/target bring-up (the invented Steps 1/2/5 that
    // used to live here -- a local AptAllocatorInitialize + WireAllocatorGlobals + the
    // interpreter init + AptCreateTargetInstance) is RETIRED: it is now the faithful
    // CgsGui::AptAux::InitializeApt @0x82848E50 (CgsAptAux.cpp), which chains the homed
    // AptAllocatorInitialize/AptUpdateInitialize/AptRenderInitialize/AptCreateTargetInstance/
    // AptChangeTargetInstance (SDKs/EATech/Apt/AptInit.cpp). PrepareRuntime() below drives
    // it after AptAux::Construct has bound the canonical view renderer set.
}


namespace BrnGui
{
    // Existing flow presentation probes may read engine context only while the
    // selected facade still publishes this owner's prepared View/Movie lifetime.
    bool AptFlowMovieLive()
    {
        if (gpActiveGuiModule == nullptr)
            return false;
        // FLOW semantics: BootLegal drives this query off its channel-41 movie at
        // display level 1. Engine-native since the AptLoadAnimation retirement: the
        // linker mounts the flow movie's anim inst onto the level-1 node -- live ==
        // that node exists with a bound character inst (SetCharacterInst ran).
        AptCIH* lpNode = AptFindAnimationAtLevel(1);
        return lpNode != nullptr && lpNode->GetCharacterInst() != nullptr;
    }

    // True once the movie has COMPOSED: the root clip's first paced tick has run its
    // frame-0 place commands, so the child display list is populated (the PLACE-named
    // clips the view-state bridge targets exist). The console's equivalent gate is the
    // GuiCache apt-component handshake (AreAllAptComponentsInitialised): a component
    // reports initialised only once its clip is placed.
    bool AptFlowMovieComposed()
    {
        if (gpActiveGuiModule == nullptr)
            return false;
        // FLOW semantics (BootLegal's compose gate): composed == the mounted level-1
        // movie's first paced tick ran its frame-0 place commands (child display list
        // non-empty). Engine-native read of the same node IsRuntimeMovieLive probes.
        AptCIH* lpRoot = AptFindAnimationAtLevel(1);
        if (lpRoot == nullptr)
            return false;
        AptCharacterInst* lpCI = lpRoot->GetCharacterInst();
        if (lpCI == nullptr || (lpCI->GetTypeTag() != 5 && lpCI->GetTypeTag() != 9))
            return false;
        AptDisplayListState* lpState =
            static_cast<AptCharacterSpriteInstBase*>(lpCI)->mDisplayList.AsState();
        return lpState != nullptr && lpState->mpFirst != nullptr;
    }
}


namespace BrnGui
{
    GuiModule*      gpActiveGuiModule      = 0;

    // The exact eight packed colours passed to ViewModule::Construct by ARTIST
    // (0x82F27F84, count 8).
    const RGBA KA_ALTERNATE_TEXT_COLOURS[8] =
    {
        { 0xFF000000u },
        { 0xFF00CCFFu },
        { 0xFFFFFFFFu },
        { 0xFF2864B7u },
        { 0xFFA68C4Au },
        { 0xFF0F0F9Cu },
        { 0xFF6B8A57u },
        { 0xFF33B6E6u },
    };

    // X360 GuiModule::Construct (0x82518028) builds the whole GUI subsystem. This slice
    // constructs the view module, the movie manager, and the real flow-controller chain
    // (cache + HUD flow + FSM controller).
    void GuiModule::Construct(const BrnResource::HudMessageController* lpHudMessageController,
                              const BrnResource::PopupController* lpPopupController,
                              bool lbHighDef)
    {
        // ARTIST 0x82518054: register GUI profiling handles before any view updates.
        // The registry is static; the initializer does not use its receiver.
        GuiPerfmons().Initialise();

        // X360 GuiModule::Construct @0x82518028, pseudocode lines 327-332 -- the console's
        // own argument assert, fired before anything is built.
        CGS_ASSERT(lpHudMessageController != 0, "lpHudMessageController");   // BrnGuiModule.cpp:229
        CGS_ASSERT(lpPopupController != 0, "lpPopupController");

        // Route through the real BrnGui::ViewModule::Construct @0x824F13B8 with the X360
        // caller's recovered args: the view flapt count (7), a 16:9 aspect, and the real
        // alternate-text-colours table + count 8 (see the static above). The
        // X360 passes a null debug name here; the descriptive "BrnGuiView" is a harmless
        // non-null label the base accepts.
        mViewModule.Construct(this, "BrnGuiView", 7, 1280.0f / 720.0f,
                              KA_ALTERNATE_TEXT_COLOURS, 8);
        // ARTIST825184E4: construct the actual shared Model and stored-event
        // owner. Native preparation/update selection remains with its caller.
        CgsGui::GuiModule::Construct(&mViewModule, lbHighDef);
        mViewModule.GetFlaptManager()->SetSoundTriggerHandler(
            &GuiModule::FlaptSoundTriggerCallback, this);
        // X360 GuiModule::Construct @0x82518B18: EffectsArbitrator::Construct just ahead of
        // MovieManager::Construct + ColourCalibrationScreen::Construct.
        muFrameUpdateSet = 0;
        mEffectsArbitrator.Construct();
        mMovieManager.Construct();
        // ARTIST82518B88 initializes the actual boot-movie owner byte.
        mbBootMovieMode = true;
        // ARTIST82518858: distinct from primary Prepare and release stages.
        mePrepareStage = E_PREPARE_START;
        muPrepare2State = 0;
        meReleaseStage = E_RELEASE_DONE;
        // X360 GuiModule::Construct @0x82518B18-24: MovieManager::Construct is immediately
        // followed by ColourCalibrationScreen::Construct (gm+301600 then gm+306752), with
        // EffectsArbitrator::Construct just ahead of the pair.
        mColourCalibrationScreen.Construct();
        // ARTIST82518028: these values seed the collision-world and car-pool validators.
        meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE;
        mbCollisionWorldInvalid = false;
        meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_IDLE;
        mbCollisionWorldInvalidateForNetworking = false;
        mbCollisionWorldValidateForNetworking = false;

        // ---- [profile-save] THE AUTOSAVE LATCH'S OWN INITIALISERS -------------------------
        // X360 GuiModule::Construct @0x82518028, the run that immediately follows
        // ColourCalibrationScreen::Construct (pseudocode lines 413..421):
        //     *(gm + 1628216..1628220) = 0;      // the four sibling bytes (not landed here)
        //     *(gm + 1628224)          = 0.0;    // mfLastProfileAutosaveTime
        //     *(gm + 1628228)          = 0;      // mbProfileAutosavePending
        // ⛔ These two were MISSING from the pair's landing commit, which left the tail's
        // `if (mbProfileAutosavePending)` reading an indeterminate byte on the first Update.
        mfLastProfileAutosaveTime = 0.0f;
        mbProfileAutosavePending  = false;
        // The console keeps the FORCE flag in a per-Update stack byte it zeroes at the top of
        // Update (@0x82527A58, `v142 = 0` before the event walk). This build carries it as a
        // member (the walk and the tail are two functions), so it gets the same defined start
        // here; the tail clears it again at the end of every Update.
        mbForceProfileAutosave    = false;

        mAlwaysAvailableComponentsManager.Construct();

        // X360 GuiModule::GuiModule @0x827E5B28 constructs the custom-renderer manager as a
        // by-value member; GuiModule::Construct @0x82518028 then drives its Construct.
        // ⭐ This is the object the Apt custom-control render callback reaches through
        // AptRenderHandler::mpCustomRendererManager -- until this leg NOTHING in the PC
        // build instantiated it, so `_type='PlayerImage'` on the licence card (and every
        // other custom control) had nowhere to resolve to.
        // ARTIST82518BB0..BD0: bind this receiver's own backing with 1024/16, then Clear.
        mEventReceiverQueue.Construct();
        // ARTIST82518BD4..C0C: the collision validator has its own 1024/16 receiver.
        mCollisonPoolValidationReceiverQueue.Construct();
        // ARTIST82518BF0..C0C: the real receiver owns its backing storage.
        mGDMReceiverQueue.Construct();
        gbGuiToolStartHookRequested = false; // ARTIST82518C38..82518C44
        mhSysListenerHandle = 0; // ARTIST82518C48, before serialiser Construct
        mGuiModuleSerialiser.Construct(); // ARTIST82518C4C, before manager Construct
        mCustomRendererManager.Construct();

        // ⭐ X360 GuiModule::Construct's LAST TWO ACTIONS, @0x82518D2C-54:
        //     addis r30, r31, 5 ; addi r30, r30, -0x3D70   ; r30 = gm + 311952 (the manager)
        //     mr r3, r30 ; lwz r11,0(r30) ; lwz r11,0(r11) ; mtctr r11 ; bctrl   ; its Construct
        //     addis r4, r31, 5 ; mr r3, r30 ; addi r4, r4, -0x6E80
        //     bl BrnGui__CustomRendererManager__SetFlaptRenderer
        // The argument is gm + 327680 - 28352 - 64... i.e. +299392, which is
        // mViewModule (gm+132224) + mFlaptManager (view +0x28CC0 == +167104) + mRenderer
        // (+0x40): 132224 + 167104 + 64 == 299392 exactly. (The sibling constant at
        // @0x82518C6C, gm+299328, is the FlaptManager itself -- the `this` the
        // SetSoundTriggerHandler call above uses.)
        // ⛔ THIS CALL WAS MISSING, and the setter it drives was writing the wrong field
        // (see BrnCustomRenderer.cpp), so NetworkPlayerImageRenderer::mpFlaptRenderer had
        // ZERO writers in this build. It is read by SwapBuffers, which the newly-wired
        // GuiModule::EndOfFrame runs every frame -- without this line that is a
        // "mpFlaptRenderer" assert per frame, not a fix.
        mCustomRendererManager.SetFlaptRenderer(&mViewModule.GetFlaptManager()->mRenderer);

        mpGuiEventInputBuffer = 0;
        mbRenderInputCompletedPC = false;
        mCompletedRenderInputPC.Construct();
        mpOutputBuffer = 0;
        mGuiConfig.mpTextureAllocator = 0;   // retained selected PC initialization
        mGuiConfig.mpRendererAllocator = 0;  // retained selected PC initialization
        mbCustomRenderersPrepared = false;   // [licence-icon] the manager staged-prepare latch

        // The flow-controller chain (the X360 Construct's flow set): the cache Construct
        // (the watcher reset @0x82505860 -> 0x824FD978), then the profile manager (X360
        // GuiModule::Construct @0x82518028 hands it the cache, the sign-in watcher, and
        // the view module's language manager), then the flows against the cache; the
        // controller starts UNLOADED on every flow slot.
        // ⭐⭐ THE SAT-NAV TRACKER, Constructed BEFORE the cache exactly as the console does.
        // X360 GuiModule::Construct @0x82518028 emits the tracker's inlined Construct at
        // @0x82518874..@0x825188C8 (off r29 == gm+1131872) and only then calls
        // GuiCache::Construct @0x82518948 with `mr r4, r29`. Order matters here for the same
        // reason it does on the console: the cache's own "Invalid tracker pointer" assert
        // (BrnGuiCache.cpp:1226) fires on a pointer that is handed over before its object is
        // initialised, and every reader downstream (GetGuiTracker) gets the live one.
        mGuiTracker.Construct();

        // X360 @0x8251893C..@0x82518948, verbatim:
        //     mr r5, r26   ; the module's CgsGui::SystemUserProfile @gm+949152
        //     mr r4, r29   ; the module's GuiTracker              @gm+1131872
        //     mr r3, r27   ; the GuiCache                         @gm+1005376
        //     bl BrnGui::GuiCache::Construct
        // ⭐ This is the WRITER of GuiCache::mpGuiTracker (+0x4054) the tree never had.
        mGuiCache.Construct(&mGuiTracker, &mSystemUserProfile);

        // X360 GuiModule::Construct @0x82518028, the two lines that follow GuiCache::Construct
        // and MapIconManager::Construct: Construct the module's own WorldDataController (the
        // seven inlined stores at gm+307836..+309028 + the receiver-queue Clear) and bind it
        // into the cache (`*(gm + 1021860) = gm + 307836`, guarded by the "lpController"
        // assert at BrnGuiCache.h:2310). This is the pointer every GUI component that resolves
        // a car / landmark / event through GuiCache::GetWorldDataController reaches.
        mWorldDataController.Construct();
        mGuiCache.SetWorldDataController(&mWorldDataController);

        // [H3b] X360 GuiModule::Construct: MapIconManager::Construct(gm+1088304, cache)
        // right after the cache/world-data wiring, then the cache binding + the sat-nav
        // view-rect pick + the mask matrix build (the @0x82518A2C..@0x82518A64 run).
        mMapIconManager.Construct(&mGuiCache);
        mGuiCache.SetMapIconManager(&mMapIconManager);
        // ⭐ THE HIGH-DEFINITION BYTE (issue #11). X360 @0x82518A24: `*(gm + 1024649) = a6`
        // -- 1024649 - 1005376 == 19273 == GuiCache +0x4B49, the byte BootLegal::Update,
        // MainMapComponent::Construct, CrashNavMapMain, CrashNavDriverDetails and
        // RoadSignIconManager all read to choose between their HD and SD constants. This
        // store is the byte's ONLY writer in the image; without it the cache said SD.
        mGuiCache.SetIsHighDefinition(lbHighDef);
        {
            // [FLAG PC witness] once, unconditional -- tools/tests/cases/hd_ui_flag.ps1 reads it.
            char lacLine[160];
            std::snprintf(lacLine, sizeof(lacLine),
                          "[GuiModule] high-definition=%d (GuiCache +0x4B49; display %dx%d)\n",
                          lbHighDef ? 1 : 0,
                          static_cast<int>(renderengine::gDisplayWidth),
                          static_cast<int>(renderengine::gDisplayHeight));
            CgsDev::Log::WriteToLog(lacLine);
        }
        // @0x82518A2C..A64: the sat-nav view rect pick, `a6 ? unk_82FB30A0 : unk_82FB3130`
        // stored to the live rect @0x82FB36A0.
        MapTransform::SetSatNavRect(lbHighDef ? MapTransform::GetSatNavViewRectHD()
                                              : MapTransform::GetSatNavViewRectSD());
        SetMaskAspectCorrectionMatrix(&mGuiCache);

        // ⭐ [stuntrace] THE FREEBURN-CHALLENGE MANAGER HAND-OFF -- the missing writer of
        // GuiCache::mpChallengeManager. X360 GuiModule::Construct @0x82518028, the two lines
        // that follow the mask-matrix build and the BurnoutSkillsManager pair, verbatim:
        //     BrnGui::FreeburnChallengeManager::Construct(gm + 309584, gm + 1005376);
        //     if (gm == -309584) { ... FireAssert("lpChallengeManager",
        //                          "..\\..\\..\\GameSource\\Gui/BrnGuiCache.h", 2374); ... }
        //     *(gm + 1021868) = gm + 309584;          // == GuiCache +0x406C
        // (1021868 - 1005376 == 16492 == 0x406C, the member BrnGuiCache.h:1105 pins.) The
        // setter carries the console's own assert, so nothing is restated here.
        //
        // This is the GUI-OBJECT half of the freeburn-challenge chain and is SEPARATE from the
        // challenge-LIST data half (GameDataModule streams OnlineChallenges.bndl and
        // WorldDataController publishes the list GuiCache::GetFreeburnChallengeList returns).
        // The list landed first; this pointer had no producer at all, which is what made
        // RaceMainHudState::UpdateRunning's case-6 arm (BrnRaceMainHudState_wS3.cpp:261, an
        // UNCONDITIONAL GetFreeburnChallengeManager per controller-input event) fire
        // "mpChallengeManager" once per input frame for the whole of a live stunt race.
        //
        // The console's BurnoutSkillsManager pair sits between the mask matrix and this one:
        // the module's skills manager is constructed against the cache and bound as
        // GuiCache::mpSkillsManager (the "lpSkillsManager" assert is the setter's).
        mBurnoutSkillsManager.Construct(&mGuiCache);
        mGuiCache.SetSkillsManager(&mBurnoutSkillsManager);
        mFreeburnChallengeManager.Construct(&mGuiCache);
        mGuiCache.SetChallengeManager(&mFreeburnChallengeManager);

        // X360 GuiModule::Construct wires the shared state-access bundle here, before
        // any flow is allowed to run: ViewModule::GetAptAux/GetLanguageManager,
        // ViewModule::GetFlaptManager, and this GuiCache are the four live owners.
        // AptAux itself is installed during Prepare below because the PC runtime host
        // constructs that singleton there; the other three owners already exist.
        mGuiAccessPointers.Construct();
        mGuiAccessPointers.mpAptAux = mViewModule.GetAptAux();
        mGuiAccessPointers.mpLanguageManager = mViewModule.GetLanguageManager();
        mGuiAccessPointers.SetFlaptManager(mViewModule.GetFlaptManager());
        mGuiAccessPointers.SetGuiCache(&mGuiCache);
        // ARTIST82518CE8/D24: publish the actual contained receiver and serialiser.
        // The serialiser's static buffer is allocated by the replay owner later.
        mGuiAccessPointers.SetGDMReceiverQueue(&mGDMReceiverQueue);
        mGuiAccessPointers.SetSerialiser(&mGuiModuleSerialiser);

        // ARTIST82518798/A0/A8: the profile watcher owns these three initial stores.
        mSystemUserProfile.Construct();
        mProfileHost.GetProfileManager().Construct(mGuiCache, mSystemUserProfile,
                                  mViewModule.GetLanguageManager());
        // ARTIST825187CC/D0/D4: real contained keyboard and listener host.
        mGuiKeyboard.Construct();
        mKeyboardHost.Construct();
        mScreenFlow.Construct(&mGuiCache);
        mHudFlow.Construct(&mGuiCache);
        mOverlayFlow.Construct(&mGuiCache);
        // ARTIST82518570/580/590/5A0: the four real observer owners, event0.
        mGuiModel.AddGuiEventObserver(&mHudFlow, 0);
        mGuiModel.AddGuiEventObserver(&mScreenFlow, 0);
        mGuiModel.AddGuiEventObserver(&mOverlayFlow, 0);
        mGuiModel.AddGuiEventObserver(&mAlwaysAvailableComponentsManager, 0);
        mFsmController.Construct();

        // ARTIST82518720 and the inlined overlay Construct: both directors receive the real contained
        // Model. Their original publishers are restored together (CP16).
        mHudMessageDirector.Construct(&mGuiModel, &mGuiCache);
        mHudMessageAnalyzer.Construct(&mHudMessageDirector);
        mOverlaysDirector.Construct(&mGuiModel);

        // ARTIST825187D8..82518818: actual debug subobjects, after their real director owner.
        // Their C++ constructors installed only vtables/queue flags; this is their data phase.
        mDebugComponent.Construct(this);
        mHudMessageDebugComponent.Construct(&mHudMessageDirector);
        mSatNavDebugComponent.Construct(this);
        mGuiCacheDebugComponent.Construct(this);

        // ⭐ [gateui r4] THE CONTROLLER HAND-OFF -- the missing writer of
        // GuiCache::mpHudMessageController. X360 GuiModule::Construct @0x82518028
        // pseudocode lines 362-368, IMMEDIATELY before the director store:
        //     if ( !a2 ) { BeginAssert; FireAssert("lpController",
        //                  "..\\..\\..\\GameSource\\Gui/BrnGuiCache.h", 2405); EndAssert; }
        //     *(a1 + 1021872) = a2;                     // == GuiCache +0x4070, mpHudMessageController
        //     if ( a1 == -639264 ) { ... "lpDirector", BrnGuiCache.h, 2433 ... }
        //     *(a1 + 1021876) = a1 + 639264;            // == GuiCache +0x4074, mpHudMessageDirector
        // (1021872 - 1005376 == 16496 == 0x4070 and 1021876 - 1005376 == 16500 == 0x4074,
        // the two members BrnGuiCache.h:917/918 pin -- so the console's order really is
        // controller THEN director.) The setter carries the console's assert already, so the
        // CGS_ASSERT below is the ARGUMENT one (BrnGuiModule.cpp:229) re-stated at the point
        // of use rather than a second copy of BrnGuiCache.h:2405.
        CGS_ASSERT(lpHudMessageController != 0,
                   "lpHudMessageController (GuiCache::mpHudMessageController would stay null "
                   "and every HUD message would die at FilterAndSendOffMessage's mpController "
                   "assert)");
        mGuiCache.SetHudMessageController(lpHudMessageController);

        mGuiCache.SetHudMessageDirector(&mHudMessageDirector);
        // The popup controller: the console's third argument, stored right after the
        // director (GuiCache +0x4078).
        mGuiCache.SetPopupController(lpPopupController);

        // The REAL GUI resource-loading module + its persistent IO pair (replaces the
        // host FSM-bundle stand-in). Construct the IO buffers (their embedded queues come
        // up here) and the module. The HD flag is the console's own argument (the same a6
        // the cache byte above took): it picks GuiApt\ vs GuiAptSD\, FLAPTHUD vs FLAPTHUDSD
        // and Fonts\ vs FontsSD\ (the FSM bundle path itself is HD-independent). Construct
        // seeds the module counters/stages + marks it a new-module type (its base Prepare
        // then skips the old-module IO-structure lock path -- no assert).
        // The shared Model constructed its one actual Resource owner above.
        // Existing legacy members remain unused to avoid unrelated layout churn.

        for (s32 lf = 0; lf < KI_NUM_EVENT_OBSERVERS; ++lf)
        {
            for (s32 li = 0; li < KI_MAX_OBSERVED_EVENT_ID; ++li)
                mabObservedEventIds[lf][li] = false;
            mabPriorityBlocking[lf] = false;
            for (s32 lc = 0; lc < KI_MAX_PRIORITY_CLAIMS_PER_FLOW; ++lc)
            {
                PriorityClaim& lrClaim = maPriorityClaims[lf][lc];
                lrClaim.mbActive   = false;
                lrClaim.miEventType = -1;
                for (s32 li = 0; li < KI_MAX_OBSERVED_EVENT_ID; ++li)
                    lrClaim.mabOverriddenEventIds[li] = false;
            }
        }
        // ⭐ THE EVENT-64 CACHE HAND-OFF -- the console's own tail of GuiModule::Construct,
        // two lines before its GuiAccessPointers block:
        //     v102[0] = a1 + 1005376;                              // the GuiCache
        //     BrnGui::GuiTracker::RecEvent(a1 + 1131872, v102, 64, 4);
        // (The 4 is the X360's 32-bit pointer size; RecEvent never reads the size argument,
        // so the host passes sizeof the record it actually built.) The arm latches
        // mpGuiCache and seeds mPlayersTrackerInfo from the cache's world-camera lane
        // (GuiCache +0x4AE0), which is the tracker's own view of where the player is.
        // ⚠️ The console re-posts 64 on every cache update; this build posts it once, at
        // construction -- so the tracker's player record does not follow the camera yet.
        // Named as the known gap (see the RecEvent banner in BrnGuiTracker.h), not papered
        // over: the 232 publish path that draws the route line does not depend on it.
        {
            GuiEventCachePointer lCachePointerEvent;
            lCachePointerEvent.mpCachePointer = &mGuiCache;
            mGuiTracker.RecEvent(
                reinterpret_cast<const CgsModule::Event*>(&lCachePointerEvent), 64,
                static_cast<s32>(sizeof(lCachePointerEvent)));
        }

        // FLAG PC-platform leaf: retain the existing View input until Render.
        // The real Model input and View output are fresh IO-stack owners per Update.
        mViewInputBuffer.Construct();
        mGuiOutQueue.Construct();
        mbResourcesReadyFed = false;
        mbPrepared = false;
    }

    // ARTIST82518E9C..82519024, extracted from the original inline memory
    // stage. No selected host caller invokes this helper yet.
    void GuiModule::PopulateNativeGuiConfig(
        const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput)
    {
        if (!lpGameDataOutput->GetAllocatorList())
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lMessage;
            CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
            lStream << "Invalid allocator list passed into BrnGui::GuiModule::Prepare";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 367);
            CgsDev::Assert::EndAssert();
        }
        mGuiConfig.miAptPersistentBunldePoolId = 8;
        mGuiConfig.miAptStreamedbundlePoolId = 9;
        mGuiConfig.miFontBundlePoolId = 0;
        mGuiConfig.miGlobalTexturePoolId = 10;
        mGuiConfig.miFSMBunldePoolId = 11;
        mGuiConfig.miLanguageBundlePoolId = 11;
        mGuiConfig.miTexturesBundlePoolId = 12;
        mGuiConfig.mpAptAllocator = lpGameDataOutput->GetAllocatorList()->GetHeapAllocator(32);
        mGuiConfig.mpFSMAllocator = lpGameDataOutput->GetAllocatorList()->GetHeapAllocator(38);
        mGuiConfig.mpRendererAllocator =
            lpGameDataOutput->GetAllocatorList()->GetRWGeneralResourceAllocator(31);
        mGuiConfig.mpLanguageAllocator = lpGameDataOutput->GetAllocatorList()->GetHeapAllocator(38);
        mGuiConfig.mpGuiStateAllocator = lpGameDataOutput->GetAllocatorList()->GetLinearAllocator(44);
        mGuiConfig.mpTextureAllocator =
            lpGameDataOutput->GetAllocatorList()->GetRWLinearResourceAllocator(42);
        mGuiConfig.mpFlaptAllocator = lpGameDataOutput->GetAllocatorList()->GetLinearAllocator(66);
        mGuiConfig.mpGuiStateAllocator->SetAlignment(16);
        mGuiConfig.mpFlaptAllocator->SetAlignment(16);
    }

    // ARTIST825193C8..82519444. Counts and tuple words are recovered
    // directly from the three original tables; enum ordinals are target values.
    // This extracted block has no selected host caller.
    bool GuiModule::EnsureNativeGuiResources()
    {
        static const CgsGui::sResourceTuple maEuropeanFontResourcesToLoad[3] =
        {
            { 17u, static_cast<CgsGui::ResourceRequestTypes>(16) },
            { 18u, static_cast<CgsGui::ResourceRequestTypes>(16) },
            { 19u, static_cast<CgsGui::ResourceRequestTypes>(16) },
        };
        static const CgsGui::sResourceTuple maJapaneseFontResourcesToLoad[2] =
        {
            { 20u, static_cast<CgsGui::ResourceRequestTypes>(16) },
            { 21u, static_cast<CgsGui::ResourceRequestTypes>(16) },
        };
        static const CgsGui::sResourceTuple maBasicResourcesToLoad[2] =
        {
            { 125u, static_cast<CgsGui::ResourceRequestTypes>(7) },
            { 196u, static_cast<CgsGui::ResourceRequestTypes>(10) },
        };
        const CgsGui::sResourceTuple* lpFonts;
        u32 luFontCount;
        if (mGuiCache.GetLanguage() == CgsLanguage::E_LANGUAGE_JAPANESE)
        {
            lpFonts = maJapaneseFontResourcesToLoad;
            luFontCount = 2;
        }
        else
        {
            lpFonts = maEuropeanFontResourcesToLoad;
            luFontCount = 3;
        }
        if (!mGuiCache.EnsureResourcesAreLoaded(lpFonts, luFontCount))
            return false;
        return mGuiCache.EnsureResourcesAreLoaded(maBasicResourcesToLoad, 2);
    }

    // ARTIST824F10D8. GameTalk's actual MessageHandler contract is void.
    // Registration belongs to native Prepare and remains unselected here.
    void GuiModule::GameTalkMsgHandler(EA::GameTalk::GameTalkMessage* lpMessage)
    {
        const char* lpcKey = lpMessage->GetKey(0);
        if (_strnicmp(lpcKey, "StartHook", 9) == 0)
        {
            CopyGuiToolHookName(lpMessage->GetKeyContent("StartHook"), 3554);
            gbGuiToolStartHookRequested = true;
        }
        else if (_strnicmp(lpcKey, "StopHook", 8) == 0)
        {
            CopyGuiToolHookName(lpMessage->GetKeyContent("StopHook"), 3564);
            gbGuiToolStopHookRequested = true;
        }
        else if (_strnicmp(lpcKey, "StopMenuHook", 12) == 0)
        {
            std::strncpy(gacGuiToolHookName, "", 32);
            gbGuiToolStopHookRequested = true;
        }
    }

    bool GuiModule::Prepare()
    {
        // FLAG PC-platform leaf: borrow the caller's actual GUI/GameData buffers.
        return Prepare(BrnGame::GetMainGameModule()->GetGuiOutputBuffer(),
            BrnGameMainFlowController::GetScriptedLoadGameDataInput(),
            BrnGameMainFlowController::GetScriptedLoadGameDataOutput());
    }

    // ARTIST82518D68: r3=this, r4=GUI output, r5=GameData input,
    // r6=GameData output. The ULONGLONG pseudotype packs two real arguments.
    bool GuiModule::Prepare(CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput,
        BrnResource::GameDataIO::InputBuffer* lpGameDataInput,
        const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput)
    {
        switch (mePrepareStage)
        {
        case E_PREPARE_START:
        {
            EA::GameTalk::GameTalkManager::RegisterMessageHandler(
                EA::GameTalk::GameTalkManager::GetInstance(),
                &GuiModule::GameTalkMsgHandler, "Tool.GUI");
            lpGuiOutput->LockForWrite();
            GuiReplayRegisterSerialiser lRegistration;
            lRegistration.mpSerialiser = &mGuiModuleSerialiser;
            lpGuiOutput->AddGuiOutEvent(lRegistration);
            lpGuiOutput->UnlockForWrite();
        }
            [[fallthrough]];
        case E_PREPARE_MEMORY:
            mePrepareStage = E_PREPARE_MEMORY;
            meReleaseStage = E_RELEASE_MEMORY;
            PopulateNativeGuiConfig(lpGameDataOutput);
            [[fallthrough]];
        case E_PREPARE_PARENT:
            mePrepareStage = E_PREPARE_PARENT;
            meReleaseStage = E_RELEASE_PARENT;
            if (!CgsGui::GuiModule::Prepare(mGuiConfig))
                return false;
            [[fallthrough]];
        case E_PREPARE_HUD:
            mePrepareStage = E_PREPARE_HUD;
            meReleaseStage = E_RELEASE_HUD;
            if (!mHudFlow.Prepare(&mGuiAccessPointers, mGuiConfig.mpRendererAllocator,
                mGuiConfig.mpGuiStateAllocator, &mProfileHost.GetProfileManager()))
                return false;
            mHudMessageAnalyzer.SetAccessPointers(&mGuiAccessPointers);
            [[fallthrough]];
        case E_PREPARE_SCREEN:
            mePrepareStage = E_PREPARE_SCREEN;
            meReleaseStage = E_RELEASE_SCREEN;
            if (!mScreenFlow.Prepare(&mGuiAccessPointers, mGuiConfig.mpRendererAllocator,
                mGuiConfig.mpGuiStateAllocator, mProfileHost.GetProfileManager()))
                return false;
            mScreenFlow.SetStreamingMode(BrnBaseFlow::E_STREAMING_CURRENT);
            [[fallthrough]];
        case E_PREPARE_OVERLAY:
            mePrepareStage = E_PREPARE_OVERLAY;
            meReleaseStage = E_RELEASE_OVERLAY;
            if (!mOverlayFlow.Prepare(&mGuiAccessPointers, mGuiConfig.mpRendererAllocator,
                mGuiConfig.mpGuiStateAllocator))
                return false;
            [[fallthrough]];
        case E_PREPARE_FSM_CONTROLLER:
            mePrepareStage = E_PREPARE_FSM_CONTROLLER;
            meReleaseStage = E_RELEASE_FSM_CONTROLLER;
            mFsmController.Prepare(&mGuiModel, mGuiConfig.mpFSMAllocator);
            // Existing selected projection of the original contained flow slots.
            mFsmController.AddFlow(E_GUIFLOW_SCREEN, &mScreenFlow);
            mFsmController.AddFlow(E_GUIFLOW_HUD, &mHudFlow);
            mFsmController.AddFlow(E_GUIFLOW_OVERLAY, &mOverlayFlow);
            [[fallthrough]];
        case E_PREPARE_CUSTOMRENDERER_PREPARE:
        {
            mePrepareStage = E_PREPARE_CUSTOMRENDERER_PREPARE;
            meReleaseStage = E_RELEASE_CUSTOMRENDERER_RELEASE;
            const bool lbPrepared = mCustomRendererManager.Prepare(
                mGuiConfig.mpRendererAllocator, mGuiConfig.mpTextureAllocator);
            mViewModule.SetCustomRendererManager(&mCustomRendererManager, 10,
                &mGuiModuleSerialiser);
            mbCustomRenderersPrepared = lbPrepared;
            if (!lbPrepared)
                return false;
        }
            [[fallthrough]];
        case E_PREPARE_MOVIE_MANAGER:
            mePrepareStage = E_PREPARE_MOVIE_MANAGER;
            meReleaseStage = E_RELEASE_MOVIE_MANAGER;
            // FLAG PC-platform leaf: the selected movie player acquires from
            // its existing private pool. Retain its real synchronous transport
            // until that separate owner is migrated to the GameData bank.
            if (!mMovieManager.Prepare(static_cast<s32>(mGuiCache.GetLanguage())))
                return false;
            // FLAG PC-platform leaf: the selected render facade drives both
            // View and MovieManager. Publish only after both actual owners prepared.
            gpActiveGuiModule = this;
            gpActiveMovieManager = &mMovieManager;
            mbPrepared = true;
            [[fallthrough]];
        case E_PREPARE_COLOUR_CALIBRATION_SCREEN:
            mePrepareStage = E_PREPARE_COLOUR_CALIBRATION_SCREEN;
            meReleaseStage = E_RELEASE_COLOUR_CALIBRATION_SCREEN;
            [[fallthrough]];
        case E_PREPARE_GUI_DEBUG_COMPONENT:
            mePrepareStage = E_PREPARE_GUI_DEBUG_COMPONENT;
            meReleaseStage = E_RELEASE_GUI_DEBUG_COMPONENT;
            PrepareNativeDebugComponents();
            if (!mSystemUserProfile.Prepare())
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mSystemUserProfile.Prepare()",
                    "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 519);
                CgsDev::Assert::EndAssert();
            }
            if (!mGuiKeyboard.Prepare(&mSystemUserProfile))
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mGuiKeyboard.Prepare(&mSystemUserProfile )",
                    "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 523);
                CgsDev::Assert::EndAssert();
            }
            if (!mKeyboardHost.Prepare(&mGuiKeyboard))
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mKeyboardHost.Prepare(&mGuiKeyboard)",
                    "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 525);
                CgsDev::Assert::EndAssert();
            }
            mbCollisionWorldInvalidateForNetworking = false;
            mbCollisionWorldValidateForNetworking = false;
            [[fallthrough]];
        case E_PREPARE_PROFILE:
            mePrepareStage = E_PREPARE_PROFILE;
            meReleaseStage = E_RELEASE_PROFILE;
            if (!mProfileHost.GetProfileManager().Prepare(mGuiConfig.mpFSMAllocator,
                lpGameDataOutput->GetAllocatorList()->GetLinearAllocator(65)))
                return false;
            [[fallthrough]];
        case E_PREPARE_SYSTEM_NOTIFICATIONS:
            mePrepareStage = E_PREPARE_SYSTEM_NOTIFICATIONS;
            meReleaseStage = E_RELEASE_SYSTEM_NOTIFICATIONS;
            PrepareNativeSystemNotifications();
            [[fallthrough]];
        case E_PREPARE_WF_RESOURCES:
            mePrepareStage = E_PREPARE_WF_RESOURCES;
            if (!EnsureNativeGuiResources())
                return false;
            [[fallthrough]];
        case E_PREPARE_WORLD_DATA_CONTROLLER:
            mePrepareStage = E_PREPARE_WORLD_DATA_CONTROLLER;
            if (!mWorldDataController.Prepare(lpGameDataInput))
                return false;
            [[fallthrough]];
        case E_PREPARE_DONE:
            mePrepareStage = E_PREPARE_DONE;
            meReleaseStage = E_RELEASE_START;
            gBrnGuiDrivesLoadingScreen = true; // existing selected renderer publication
            return true;
        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Invalid prepare state",
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 608);
            CgsDev::Assert::EndAssert();
            return false;
        }
    }

    // ARTIST825194B8: the enclosing second-prepare owner. Keep the unsigned
    // stage tests and original wait/continuation behavior; no readiness surrogate.
    bool GuiModule::Prepare2(BrnResource::GameDataIO::InputBuffer* lpGameDataInput)
    {
        if (muPrepare2State < 2)
        {
            muPrepare2State = 1;
            if (!mWorldDataController.Prepare2(lpGameDataInput))
                return false;
        }
        else if (muPrepare2State >= 3)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Invalid prepare state", "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 658);
            CgsDev::Assert::EndAssert();
            return false;
        }
        muPrepare2State = 2;
        return true;
    }

    // X360 GuiModule::Prepare @0x82518D68 STAGE 14, split out to its own entry point because the
    // PC has no module scheduler to hand GuiModule::Prepare the GameData IO pair. See the FLAG on
    // the declaration; the machine itself is the console's, unaltered.
    bool GuiModule::PrepareWorldData(BrnResource::GameDataIO::InputBuffer* lpGameDataInput)
    {
        if (lpGameDataInput == 0)
            return false;
        return mWorldDataController.Prepare(lpGameDataInput);
    }

    // ⭐ [event-starts wave 2026-08-27] X360 GuiModule::Prepare2 @0x825194B8's WorldDataController
    // leg, split out for exactly the same reason as PrepareWorldData above (the PC has no module
    // scheduler to hand GuiModule::Prepare2 the GameData IO pair). The machine itself is the
    // console's, unaltered -- see WorldDataController::Prepare2.
    //
    // ⛔ THE CALLER MUST NOT PUMP THIS AND PrepareWorldData IN THE SAME PHASE. Both machines drain
    // the SAME mReceiverQueue and neither checks what the one queued reply belongs to, so
    // overlapping them makes each eat the other's answer -- and on this build that is not
    // hypothetical: Prepare parks forever at stage 9 waiting on a GetFreeburnChallengeList reply
    // that no PC producer sends, so a Prepare2 reply arriving while Prepare is still pumped would
    // be consumed as the challenge list AND falsely advance meState to WFPLAYERCARCOLOURS.
    // BrnGameModule's driver enforces the exclusion; see the ⛔ at its call site.
    bool GuiModule::PrepareWorldData2(BrnResource::GameDataIO::InputBuffer* lpGameDataInput)
    {
        if (lpGameDataInput == 0)
            return false;
        return mWorldDataController.Prepare2(lpGameDataInput);
    }

    // Original stage12 has no failure return after this assertion.
    // Selecting the full typed Prepare will own the stage transition/call.
    void GuiModule::PrepareNativeSystemNotifications()
    {
        mhSysListenerHandle = XNotifyCreateListener(1);
        if (!mhSysListenerHandle)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lMessage;
            CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
            lStream << "Failed to create an X360 system listener";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 557);
            CgsDev::Assert::EndAssert();
        }
    }

    // ARTIST82529CA8..82529DD0: dequeue at most one notification,
    // then publish the actual four-byte event516 to the fresh Model input.
    void GuiModule::UpdateNativeSystemNotifications(
        CgsGui::ModelIO::InputBuffer* lpInputBuffer)
    {
        unsigned long luNotificationId;
        unsigned long luNotificationParam;
        if (mhSysListenerHandle &&
            XNotifyGetNext(mhSysListenerHandle, 9, &luNotificationId, &luNotificationParam))
        {
            if (luNotificationId == 0x02000002)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::MessageBufferPC lMessage;
                CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
                lStream << "Invite notification processed by GuiModule. This could break cross game invites\n";
                CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                    "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 2918);
                CgsDev::Assert::EndAssert();
            }
            const u32 luUiVisible = luNotificationParam != 1;
            if (!lpInputBuffer)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpInputBuffer",
                    "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleUtils.h", 238);
                CgsDev::Assert::EndAssert();
            }
            lpInputBuffer->LockForWrite();
            lpInputBuffer->GetEventQueueNonConst()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&luUiVisible), 516, sizeof(luUiVisible));
            if (!lpInputBuffer)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpInputBuffer",
                    "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleUtils.h", 248);
                CgsDev::Assert::EndAssert();
            }
            lpInputBuffer->UnlockForWrite();
        }
    }

    // Original Release closes before its stage switch, including failed close.
    // The assertion continues and clears the original member in either case.
    void GuiModule::ReleaseNativeSystemNotifications()
    {
        if (mhSysListenerHandle)
        {
            if (!CloseHandle(mhSysListenerHandle))
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CloseHandle( mhSysListenerHandle )",
                    "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 681);
                CgsDev::Assert::EndAssert();
            }
            mhSysListenerHandle = 0;
        }
    }

    // ARTIST824F9158. Release follows the last entered prepare seat in
    // descending order. Only the three flows and the shared base can wait.
    bool GuiModule::Release()
    {
        // FLAG PC-platform leaf: stop the selected renderer before any owner
        // phase can destroy a target or allocator. GameModule owns Release retries.
        mbRenderInputCompletedPC = false;
        mbPrepared = false;
        if (gpActiveGuiModule == this)
            gpActiveGuiModule = 0;
        if (gpActiveMovieManager == &mMovieManager)
            gpActiveMovieManager = 0;
        ReleaseNativeSystemNotifications();
        mbCollisionWorldInvalidateForNetworking = false;
        mbCollisionWorldValidateForNetworking = false;
        switch (meReleaseStage)
        {
        case E_RELEASE_START:
            meReleaseStage = E_RELEASE_START;
            [[fallthrough]];
        case E_RELEASE_SYSTEM_NOTIFICATIONS:
            meReleaseStage = E_RELEASE_SYSTEM_NOTIFICATIONS;
            [[fallthrough]];
        case E_RELEASE_PROFILE:
            meReleaseStage = E_RELEASE_PROFILE;
            if (!mProfileHost.Release())
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mProfileHost.Release()",
                    "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 703);
                CgsDev::Assert::EndAssert();
            }
            [[fallthrough]];
        case E_RELEASE_GUI_DEBUG_COMPONENT:
            meReleaseStage = E_RELEASE_GUI_DEBUG_COMPONENT;
            mEventReceiverQueue.Clear();
            if (!mSystemUserProfile.Release())
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mSystemUserProfile.Release()",
                    "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 710);
                CgsDev::Assert::EndAssert();
            }
            mGuiKeyboard.Release();
            if (!mKeyboardHost.Release())
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mKeyboardHost.Release()",
                    "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 712);
                CgsDev::Assert::EndAssert();
            }
            [[fallthrough]];
        case E_RELEASE_WORLD_DATA_CONTROLLER:
            meReleaseStage = E_RELEASE_WORLD_DATA_CONTROLLER;
            // ARTIST824F9320 inlines the original WorldData Release state store.
            // Do not invent an unobserved standalone boolean result for that fold.
            mWorldDataController.meState = WorldDataController::E_WORLDDATACONTROLLERSTATE_DESTRUCTED;
            [[fallthrough]];
        case E_RELEASE_COLOUR_CALIBRATION_SCREEN:
            meReleaseStage = E_RELEASE_COLOUR_CALIBRATION_SCREEN;
            [[fallthrough]];
        case E_RELEASE_MOVIE_MANAGER:
            meReleaseStage = E_RELEASE_MOVIE_MANAGER;
            [[fallthrough]];
        case E_RELEASE_CUSTOMRENDERER_RELEASE:
            meReleaseStage = E_RELEASE_CUSTOMRENDERER_RELEASE;
            if (!mCustomRendererManager.Release())
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("mCustomRenderManager.Release()",
                    "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 748);
                CgsDev::Assert::EndAssert();
            }
            [[fallthrough]];
        case E_RELEASE_CUSTOMRENDERER_DESTRUCT:
            meReleaseStage = E_RELEASE_CUSTOMRENDERER_DESTRUCT;
            mCustomRendererManager.Destruct();
            [[fallthrough]];
        case E_RELEASE_FSM_CONTROLLER:
            meReleaseStage = E_RELEASE_FSM_CONTROLLER;
            [[fallthrough]];
        case E_RELEASE_OVERLAY:
            meReleaseStage = E_RELEASE_OVERLAY;
            if (!mOverlayFlow.Release())
                return false;
            [[fallthrough]];
        case E_RELEASE_SCREEN:
            meReleaseStage = E_RELEASE_SCREEN;
            if (!mScreenFlow.Release())
                return false;
            [[fallthrough]];
        case E_RELEASE_HUD:
            meReleaseStage = E_RELEASE_HUD;
            if (!mHudFlow.Release())
                return false;
            [[fallthrough]];
        case E_RELEASE_PARENT:
            meReleaseStage = E_RELEASE_PARENT;

            // FLAG PC-platform leaf: retire the existing host decoder/arena/pool.
            // Keep it alive through every flow OnLeave, as the selected host did,
            // then honor its retry before the base destroys Apt and its allocators.
            if (!mMovieManager.Release())
                return false;
            if (!CgsGui::GuiModule::Release())
                return false;
            [[fallthrough]];
        case E_RELEASE_MEMORY:
            meReleaseStage = E_RELEASE_MEMORY;
            [[fallthrough]];
        case E_RELEASE_DONE:
            meReleaseStage = E_RELEASE_DONE;
            return true;
        default:
            return false;
        }
    }

    // Extracted ARTIST8251A4D4..8251A538. Only the future typed PreWorld phase calls this block.
    void GuiModule::PublishNativeMovieResourceRequest(CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput)
    {
        const CgsResource::ID lResourceId = mMovieManager.PendingVideoDataResourceRequest();
        if (lResourceId.GetHash() == 0)
            return;

        CgsResource::Events::AcquireResourceRequest lRequest;
        lRequest.mpUser = mMovieManager.GetRecieverQueue();
        lRequest.miEventId = MovieManager::KI_VIDEO_DATA_RESOURCE_EVENT_ID;
        lRequest.miPoolId = 11;
        lRequest.mResourceId = lResourceId;
        lpGuiOutput->LockForWrite();
        lpGuiOutput->GetGuiResourceRequestQueue()->AddEvent(&lRequest, 4);
        lpGuiOutput->UnlockForWrite();
    }

    // ARTIST8251A3D0; original call order and locks are retained. The active PC GameMain
    // still does not select this virtual phase; that remains a separate frame-wiring increment.
    void GuiModule::PreWorldUpdate(CgsModule::IOBufferStack* lpInputBufferStack,
                                   CgsModule::IOBufferStack* lpOutputBufferStack,
                                   CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput,
                                   CgsGui::ModelIO::OutputBuffer* lpModelOutputBuffer)
    {
        CgsDev::PerfMonCpu::StartMonitor(GuiPerfmons::miGuiModulePreWorldUpdate);
        CGS_ASSERT(lpGuiOutput != 0, "lpGuiOutput != NULL");
        CGS_ASSERT(lpModelOutputBuffer != 0, "lpModelOutputBuffer != NULL");
        CGS_ASSERT(lpInputBufferStack != 0, "lpInputBufferStack != NULL");
        CGS_ASSERT(lpOutputBufferStack != 0, "lpOutputBufferStack != NULL");

        lpGuiOutput->LockForWrite();
        UpdateCollisionWorldValidation(lpGuiOutput);
        UpdateCarPoolValidation(lpGuiOutput);
        mProfileHost.GetProfileManager().LoadProfileUpgradeData(lpGuiOutput, &mEventReceiverQueue);
        lpGuiOutput->UnlockForWrite();

        PublishNativeMovieResourceRequest(lpGuiOutput);
        CgsGui::GuiModule::PreWorldUpdate(lpInputBufferStack, lpOutputBufferStack,
                                          lpGuiOutput, lpModelOutputBuffer);
        CgsDev::PerfMonCpu::StopMonitor(GuiPerfmons::miGuiModulePreWorldUpdate);
    }

    // ARTIST82519578. Pending* calls consume their flags/state even when an
    // assertion is disabled, so keep every call outside its assertion expression.
    void GuiModule::UpdateCollisionWorldValidation(CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput)
    {
        const auto ConsumeNetworkingInvalidate = [this]() {
            const bool lbPending = mbCollisionWorldInvalidateForNetworking;
            mbCollisionWorldInvalidateForNetworking = false;
            return lbPending;
        };
        const auto ConsumeNetworkingValidate = [this]() {
            const bool lbPending = mbCollisionWorldValidateForNetworking;
            mbCollisionWorldValidateForNetworking = false;
            return lbPending;
        };
        const auto NoNetworkingInvalidate = [&]() {
            const bool lbPending = ConsumeNetworkingInvalidate();
            CGS_ASSERT(!lbPending, "!PendingCollisionWorldInvalidateForNetworking()");
        };
        const auto NoMovieInvalidate = [this]() {
            const bool lbPending = mMovieManager.PendingCollisionWorldInvalidate();
            CGS_ASSERT(!lbPending, "!mMovieManager.PendingCollisionWorldInvalidate()");
        };
        const auto NoProfileInvalidate = [this]() {
            const bool lbPending = mProfileHost.GetProfileManager().PendingCollisionWorldInvalidate();
            CGS_ASSERT(!lbPending, "!mProfileHost.GetProfileManager().PendingCollisionWorldInvalidate()");
        };
        const auto SwapOut = [this, lpOutput]() {
            BrnResource::GameDataIO::SwapOutCollisionWorldRequest lRequest;
            lRequest.miEventId = 0;
            lRequest.mpReceiverQueue = &mCollisonPoolValidationReceiverQueue;
            lpOutput->GetGuiResourceRequestQueue()->AddEvent(&lRequest, 67);
        };
        const auto SwapIn = [this, lpOutput]() {
            BrnResource::GameDataIO::SwapInCollisionWorldRequest lRequest;
            lRequest.miEventId = 0;
            lRequest.mpReceiverQueue = &mCollisonPoolValidationReceiverQueue;
            lpOutput->GetGuiResourceRequestQueue()->AddEvent(&lRequest, 68);
        };
        const auto AskWorld = [lpOutput](GuiEventRequestCollisionWorldEvent::EEventType leType) {
            GuiEventRequestCollisionWorldEvent lRequest;
            lRequest.meEventType = leType;
            lpOutput->AddGuiOutEvent(lRequest);
        };
        const auto ReadResponse = [this](s32 liExpectedType) {
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            const s32 liType = mCollisonPoolValidationReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
            CGS_ASSERT(mCollisonPoolValidationReceiverQueue.GetLength() == 1,
                "mCollisonPoolValidationReceiverQueue.GetLength() == 1");
            if (liExpectedType == 67)
                CGS_ASSERT(liType == 67, "liEventId == BrnResource::GameDataIO::EVENT_SWAP_OUT_COLLISION_WORLD");
            else
                CGS_ASSERT(liType == 68, "liEventId == BrnResource::GameDataIO::EVENT_SWAP_IN_COLLISION_WORLD");
            return lpEvent;
        };

        switch (meCollisionWorldStateValidationState)
        {
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE:
        {
            const bool lbMovie = mMovieManager.PendingCollisionWorldInvalidate();
            const bool lbNetworking = ConsumeNetworkingInvalidate();
            if (mbBootMovieMode)
            {
                const bool lbProfile = mProfileHost.GetProfileManager().PendingCollisionWorldInvalidate();
                if (lbMovie || lbProfile)
                {
                    BrnResource::PrintConsoleMemory(0);
                    mCollisonPoolValidationReceiverQueue.Clear();
                    SwapOut();
                    meCollisionWorldStateValidationState = lbMovie
                        ? E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_BOOT
                        : E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_SAVELOAD;
                }
            }
            else if (lbMovie || lbNetworking)
            {
                CGS_ASSERT(!mbCollisionWorldInvalid, "!mbCollisionWorldInvalid");
                CGS_ASSERT((lbMovie && !lbNetworking) || (!lbMovie && lbNetworking),
                    "( lbMoviePendingInvalidate && !lbNetworkingPendingInvalidate) || (!lbMoviePendingInvalidate && lbNetworkingPendingInvalidate)");
                AskWorld(GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_INVALIDATE);
                meCollisionWorldStateValidationState = lbMovie
                    ? E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_MOVIE
                    : E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_NETWORKING;
            }
            break;
        }
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_BOOT:
            NoNetworkingInvalidate();
            if (mCollisonPoolValidationReceiverQueue.GetLength() > 0)
            {
                const auto* lpResponse = static_cast<const BrnResource::GameDataIO::SwapOutCollisionWorldResponse*>(ReadResponse(67));
                mMovieManager.FreeCollisionPoolMemory(lpResponse);
                BrnResource::PrintConsoleMemory(0);
                mbCollisionWorldInvalid = true;
                mMovieManager.SetCollisionWorldValid(false);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_BOOT;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_BOOT:
            NoNetworkingInvalidate();
            CGS_ASSERT(mbCollisionWorldInvalid, "mbCollisionWorldInvalid");
            if (mMovieManager.PendingCollisionWorldValidate())
            {
                SwapIn();
                mCollisonPoolValidationReceiverQueue.Clear();
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_BOOT;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_BOOT:
            NoNetworkingInvalidate();
            if (mCollisonPoolValidationReceiverQueue.GetLength() > 0)
            {
                ReadResponse(68);
                BrnResource::PrintConsoleMemory(0);
                BrnResource::PrintConsoleMemory(0);
                mbCollisionWorldInvalid = false;
                mMovieManager.SetCollisionWorldValid(true);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_SAVELOAD:
            NoMovieInvalidate();
            NoNetworkingInvalidate();
            if (mCollisonPoolValidationReceiverQueue.GetLength() > 0)
            {
                ReadResponse(67);
                BrnResource::PrintConsoleMemory(0);
                mbCollisionWorldInvalid = true;
                mProfileHost.GetProfileManager().SetCollisionWorldValid(false);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_SAVELOAD;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_SAVELOAD:
            NoMovieInvalidate();
            NoNetworkingInvalidate();
            CGS_ASSERT(mbCollisionWorldInvalid, "mbCollisionWorldInvalid");
            if (mProfileHost.GetProfileManager().PendingCollisionWorldValidate())
            {
                SwapIn();
                mCollisonPoolValidationReceiverQueue.Clear();
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_SAVELOAD;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_SAVELOAD:
            NoMovieInvalidate();
            NoNetworkingInvalidate();
            if (mCollisonPoolValidationReceiverQueue.GetLength() > 0)
            {
                ReadResponse(68);
                BrnResource::PrintConsoleMemory(0);
                mbCollisionWorldInvalid = false;
                mProfileHost.GetProfileManager().SetCollisionWorldValid(true);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_MOVIE:
            NoProfileInvalidate();
            NoNetworkingInvalidate();
            if (mbCollisionWorldInvalid)
            {
                mMovieManager.SetCollisionWorldValid(false);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_MOVIE;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_MOVIE:
            NoProfileInvalidate();
            NoNetworkingInvalidate();
            CGS_ASSERT(mbCollisionWorldInvalid, "mbCollisionWorldInvalid");
            if (mMovieManager.PendingCollisionWorldValidate())
            {
                AskWorld(GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_VALIDATE);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_MOVIE;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_MOVIE:
            NoProfileInvalidate();
            NoNetworkingInvalidate();
            if (!mbCollisionWorldInvalid)
            {
                mMovieManager.SetCollisionWorldValid(true);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_NETWORKING:
            NoProfileInvalidate();
            NoMovieInvalidate();
            if (mbCollisionWorldInvalid)
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_NETWORKING;
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_NETWORKING:
            NoProfileInvalidate();
            NoMovieInvalidate();
            if (ConsumeNetworkingValidate())
            {
                AskWorld(GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_VALIDATE);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_NETWORKING;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_NETWORKING:
            NoProfileInvalidate();
            NoMovieInvalidate();
            if (!mbCollisionWorldInvalid)
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE;
            break;
        default:
            CGS_ASSERT(false, "Unhandled state");
            break;
        }
    }

    // ARTIST82514228. Pool15 is positively attested here. Invalidate IN_USE
    // retries at WAITING_FOR_POOL_TO_EMPTY; it never fabricates completion.
    void GuiModule::UpdateCarPoolValidation(CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput)
    {
        switch (meCarPoolStateValidationState)
        {
        case E_CARPOOLSTATEVALIDATIONSTATE_IDLE:
            if (mMovieManager.PendingCarPoolInvalidate())
            {
                if (mbBootMovieMode)
                    mbTrafficPoolEmptied = true;
                else
                {
                    const bool lbEmpty = true;
                    lpOutput->GetGameActionQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lbEmpty), 13, sizeof(lbEmpty));
                    mbTrafficPoolEmptied = false;
                }
                meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_EMPTY;
            }
            break;
        case E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_EMPTY:
            if (mbTrafficPoolEmptied)
            {
                CgsResource::Events::InvalidatePoolRequest lRequest;
                lRequest.mpUser = &mEventReceiverQueue;
                lRequest.miEventId = BrnResource::E_POOL_TRAFFIC;
                lRequest.miPoolId = BrnResource::E_POOL_TRAFFIC;
                lpOutput->GetGuiResourceRequestQueue()->AddEvent(&lRequest, 7);
                meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_INVALIDATING_FOR_MOVIE;
            }
            break;
        case E_CARPOOLSTATEVALIDATIONSTATE_INVALIDATING_FOR_MOVIE:
            if (mEventReceiverQueue.GetLength() > 0)
            {
                CGS_ASSERT(mEventReceiverQueue.GetLength() == 1, "mEventReceiverQueue.GetLength() == 1");
                const CgsModule::Event* lpEvent = 0;
                s32 liSize = 0;
                const s32 liEventId = mEventReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
                CGS_ASSERT(liEventId == 7, "liEventId == CgsResource::ResourceIO::EVENT_INVALIDATEPOOL");
                const auto* lpResponse = reinterpret_cast<const CgsResource::Events::InvalidatePoolResponse*>(lpEvent);
                CGS_ASSERT(lpResponse->miEventId == BrnResource::E_POOL_TRAFFIC,
                    "lpInvalidatePoolResponse->GetEventId() == BrnResource::E_POOL_TRAFFIC");
                mEventReceiverQueue.Clear(); // Original Clear preserves payload storage.
                if (lpResponse->GetResult() != CgsResource::Events::InvalidatePoolResponse::E_RESULT_SUCCESS)
                    meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_EMPTY;
                else
                {
                    mMovieManager.SetCarPoolValid(false, lpResponse->GetResource(), lpResponse->GetDescriptor());
                    meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_INVALID_FOR_MOVIE;
                }
            }
            break;
        case E_CARPOOLSTATEVALIDATIONSTATE_INVALID_FOR_MOVIE:
            if (mMovieManager.PendingCarPoolValidate())
            {
                CgsResource::Events::ValidatePoolRequest lRequest;
                lRequest.mpUser = &mEventReceiverQueue;
                lRequest.miEventId = BrnResource::E_POOL_TRAFFIC;
                lRequest.miPoolId = BrnResource::E_POOL_TRAFFIC;
                lpOutput->GetGuiResourceRequestQueue()->AddEvent(&lRequest, 8);
                meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_VALIDATING_FOR_MOVIE;
            }
            break;
        case E_CARPOOLSTATEVALIDATIONSTATE_VALIDATING_FOR_MOVIE:
            if (mEventReceiverQueue.GetLength() <= 0)
                break;
            {
                CGS_ASSERT(mEventReceiverQueue.GetLength() == 1, "mEventReceiverQueue.GetLength() == 1");
                const CgsModule::Event* lpEvent = 0;
                s32 liSize = 0;
                const s32 liEventId = mEventReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
                CGS_ASSERT(liEventId == 8, "liEventId == CgsResource::ResourceIO::EVENT_VALIDATEPOOL");
                const auto* lpResponse = reinterpret_cast<const CgsResource::Events::ValidatePoolResponse*>(lpEvent);
                CGS_ASSERT(lpResponse->miEventId == BrnResource::E_POOL_TRAFFIC,
                    "lpValidatePoolResponse->GetEventId() == BrnResource::E_POOL_TRAFFIC");
                mEventReceiverQueue.Clear();
                if (mbBootMovieMode)
                    mbTrafficPoolEmptied = false;
                else
                {
                    const bool lbEmpty = false;
                    lpOutput->GetGameActionQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lbEmpty), 13, sizeof(lbEmpty));
                    CGS_ASSERT(mbTrafficPoolEmptied, "mbTrafficPoolEmptied");
                }
                meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_FILL;
            }
            [[fallthrough]];
        case E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_FILL:
            if (!mbTrafficPoolEmptied)
            {
                CgsResource::SmallResource lResource;
                CgsResource::Entry::ResourceDescriptor lDescriptor;
                for (s32 liType = 0; liType < 3; ++liType)
                {
                    lResource.m_baseResources[liType] = 0;
                    lDescriptor.m_baseResourceDescriptors[liType].m_size = 0;
                    lDescriptor.m_baseResourceDescriptors[liType].m_alignment = 1;
                }
                mMovieManager.SetCarPoolValid(true, lResource, lDescriptor);
                meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_IDLE;
            }
            break;
        default:
            CGS_ASSERT(false, "Unhandled state");
            break;
        }
    }

    // ARTIST GuiModule::Prepare stage 10 (0x82519218..0x82519254): register the four
    // native debug components and clear the event receiver queue at the original stage.
    void GuiModule::PrepareNativeDebugComponents()
    {
        mDebugComponent.Register();
        mHudMessageDebugComponent.Register();
        mSatNavDebugComponent.Register();
        mGuiCacheDebugComponent.Register();
        mEventReceiverQueue.Clear();
    }

void GuiModule::Destruct()
    {
        mbRenderInputCompletedPC = false;
        mMovieManager.Destruct();
        // ARTIST825076D0..825077A4: four actual derived teardown owners, original order.
        mDebugComponent.Destruct();
        mHudMessageDebugComponent.Destruct();
        mSatNavDebugComponent.Destruct();
        mGuiCacheDebugComponent.Destruct();
        // X360 GuiModule::Destruct @0x82507690: ColourCalibrationScreen::Destruct is the LAST
        // sub-object destruct, immediately before the base CgsGui::GuiModule::Destruct.
        // ARTIST825077D0/D4: retain the original host/profile pointer teardown.
        mKeyboardHost.Destruct();
        mGuiKeyboard.Destruct();
        mColourCalibrationScreen.Destruct();
        // ARTIST825077F4: the shared queue/base lifecycle follows the final
        // game subobject. Existing Movie/APT lifetime owners above are retained.
        CgsGui::GuiModule::Destruct();
    }

    // Post one event into each subscribing observer's in-queue (the EventInterpreterModule
    // observer-subscription filter the console applies in ProcessInEvents before handing
    // an observer its per-frame queue). The observer slots are the three flows plus the
    // always-available components manager.


    // ARTIST @0x825112B0 converts the FLApt action string to a
    // GuiAudioTriggerEvent and publishes it through the module output buffer.
    void GuiModule::FlaptSoundTriggerCallback(void* lpUserData,
                                              const char* lpcComponentName,
                                              const char* lpcSwfName,
                                              const char* lpcActionName,
                                              const char* lpcLabel)
    {
        CGS_ASSERT(lpUserData != 0, "lpUserData");
        CGS_ASSERT(lpcComponentName != 0, "lpcComponentName");
        CGS_ASSERT(lpcSwfName != 0, "lpcSwfName");
        CGS_ASSERT(lpcActionName != 0, "lpcActionName");
        CGS_ASSERT(lpcLabel != 0, "lpcLabel");

        GuiModule* lpThis = static_cast<GuiModule*>(lpUserData);
        CGS_ASSERT(lpThis->mpOutputBuffer != 0, "mpOutputBuffer");

        // off_82F277A8..off_82F277E0: the fourteen authored presentation actions.
        static const char* const KAPC_ACTION_NAMES[14] = {
            "ON_ENTER", "ON_LEAVE", "ON_FOCUS", "ON_LOSE_FOCUS",
            "ON_ACCEPT", "ON_CANCEL", "ON_TICK", "ON_CHANGE",
            "ON_UP", "ON_DOWN", "ON_LEFT", "ON_LEFT_SWEEP",
            "ON_RIGHT", "ON_RIGHT_SWEEP"
        };

        s32 liAction = 14;
        for (s32 li = 0; li < 14; ++li)
        {
            if (std::strcmp(lpcActionName, KAPC_ACTION_NAMES[li]) == 0)
            {
                liAction = li;
                break;
            }
        }

        GuiAudioTriggerEvent lAudioEvent;
        lAudioEvent.Construct(liAction, lpcComponentName, lpcLabel, lpcSwfName);
        // ARTIST sub_824FB9A8 emits this diagnostic after Construct for any action >= 14.
        if (liAction >= 14 && ::gbLogGuiAudioTriggers &&
            (CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            const char* lpcLogComponent = lpcComponentName != 0 ? lpcComponentName : "<NULLSTRING>";
            const char* lpcLogAction = lpcActionName != 0 ? lpcActionName : "<NULLSTRING>";
            const char* lpcLogLabel = lpcLabel != 0 ? lpcLabel : "<NULLSTRING>";
            const char* lpcLogMovie = lpcSwfName != 0 ? lpcSwfName : "<NULLSTRING>";
            *CgsDev::Log::gpDebugPrint
                << "\nGUI AUDIO TRIGGERS: "
                << "Received invalid sound action. Component: " << lpcLogComponent
                << " Action: " << lpcLogAction
                << " Label: " << lpcLogLabel
                << " Movie: " << lpcLogMovie
                << "\n";
        }
        lpThis->mpOutputBuffer->GetOutEventQueueNonConst()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAudioEvent),
            lAudioEvent.GetEventType(), static_cast<s32>(sizeof(lAudioEvent)));
    }

    namespace
    {
        // The four player-name string ids UpdatePlayerName reads/writes. The read side
        // ("DEFAULTPLAYERNAME"/"DEFAULTPLAYERNAMEQUOTED", X360 off_82F278B4/off_82F278B8)
        // are language-DATABASE keys whose entries ship in the LANGUAGE bundles ("You" /
        // '"You"'); the write side (off_82F278AC/off_82F278B0) are the LIVE ids every
        // GUI consumer resolves (GuiCache::GetPlayerName returns the first verbatim).
        const char* const KAPC_UPN_PLAYER_NAME_ID        = "PLAYER_NAME_STRING_ID";      // @0x8206E7DC
        const char* const KAPC_UPN_PLAYER_NAME_QUOTED_ID = "PLAYER_NAME_STRING_ID_Q";    // @0x8206E7C4
        const char* const KAPC_UPN_DEFAULT_NAME_ID       = "DEFAULTPLAYERNAME";          // off_82F278B4
        const char* const KAPC_UPN_DEFAULT_QUOTED_ID     = "DEFAULTPLAYERNAMEQUOTED";    // off_82F278B8
    }

    // @ 0x824F0D30 -- publish the live player name into the language database (see the
    // header note). Runs when GuiModule::Update sees event 507 -- the command record
    // BootProfile::OnLeave posts as the profile boot completes -- so every subsequent
    // "PLAYER_NAME_STRING_ID" lookup (the licence card's playerName field, the HUD
    // message analyzer's name substitution) resolves to a real name instead of the id.
    //
    // [PC] XUserGetName has no host equivalent and no user is ever signed in, so this
    // always takes the console's XUserGetName-FAILED branch: copy the database's own
    // DEFAULT entries ("You" / '"You"') under the live ids -- byte-identical to an X360
    // with no profile signed in. The signed-in branch (gamertag + the "''%s''" quoted
    // form) needs the platform user account and stays X360-only.
    void GuiModule::UpdatePlayerName()
    {
        CgsLanguage::LanguageManager* lpLanguageManager = mViewModule.GetLanguageManager();
        if (lpLanguageManager == 0)
            return;   // [PC] Prepare not finished yet; the console cannot reach 507 this early.

        const u8* lpcDefaultName = lpLanguageManager->FindString(KAPC_UPN_DEFAULT_NAME_ID);
        CGS_ASSERT(lpcDefaultName != 0,
                   "Couldn't find default player name in string database.");             // cpp:1216
        if (lpcDefaultName == 0)
            return;   // [PC] the console strncpy's from NULL here; entries ship in every LANGUAGE bundle
        CGS_ASSERT(std::strlen(reinterpret_cast<const char*>(lpcDefaultName)) < 0x1B,
                   "String too long: ");                                                  // CgsStringUtils.h:55
        char lacName[32];
        std::strncpy(lacName, reinterpret_cast<const char*>(lpcDefaultName), 27);
        lacName[27] = 0;
        lpLanguageManager->AddString(KAPC_UPN_PLAYER_NAME_ID,
                                     reinterpret_cast<const u8*>(lacName));

        const u8* lpcDefaultQuoted = lpLanguageManager->FindString(KAPC_UPN_DEFAULT_QUOTED_ID);
        CGS_ASSERT(lpcDefaultQuoted != 0,
                   "Couldn't find default player name quoted in string database.");      // cpp:1222
        if (lpcDefaultQuoted == 0)
            return;   // [PC] see above
        CGS_ASSERT(std::strlen(reinterpret_cast<const char*>(lpcDefaultQuoted)) < 0x1F,
                   "String too long: ");                                                  // CgsStringUtils.h:55
        char lacQuoted[32];
        std::strncpy(lacQuoted, reinterpret_cast<const char*>(lpcDefaultQuoted), 31);
        lacQuoted[31] = 0;   // the console's explicit v35 = 0 store
        lpLanguageManager->AddString(KAPC_UPN_PLAYER_NAME_QUOTED_ID,
                                     reinterpret_cast<const u8*>(lacQuoted));
    }

    // X360 0x8251A568: preserve an image file when autosave is disabled, otherwise autosave
    // it as a one-record ImageFileInfo request. The current PC Update keeps its documented
    // sign-in-gate deviation, but retains the original autosave-enabled and image-copy branches.
    void GuiModule::AutosaveProfile(const CgsGui::ImageFileInfo* lpImageFileInfo)
    {
        if (lpImageFileInfo != 0 && !mProfileHost.GetProfileManager().AutosaveIsEnabled())
        {
            mProfileHost.GetProfileManager().CopyImageToBuffer(lpImageFileInfo);
        }

        if (mProfileHost.GetProfileManager().AutosaveIsEnabled())
        {
            mProfileHost.GetProfileManager().Autosave(&mProfileHost,
                                     lpImageFileInfo != 0 ? 1 : 0, lpImageFileInfo);
            mbProfileAutosavePending  = false;
            mfLastProfileAutosaveTime = mGuiCache.GetTime();
        }
    }

    // The real GuiModule::Update event dispatch (X360 0x82527A58's switch): consume the
    // module-level events, forward the load notifications, and fan the rest to the flow.
    const CgsModule::Event* GuiModule::DispatchInboundGuiEvents(
        bool& rbHasSaveImageEvent, CgsGui::ImageFileInfo& lrSaveImageInfo,
        const CgsModule::Event* lpAfterEvent,
        CgsGui::ModelIO::OutputBuffer* lpModelOutput)
    {
        if (mpGuiEventInputBuffer == 0)
            return lpAfterEvent;

        mpGuiEventInputBuffer->LockForRead();
        const CgsModule::VariableEventQueue<32768, 16>* lpInQueue =
            static_cast<const CgsGui::CgsGuiModuleIO::InputBuffer*>(mpGuiEventInputBuffer)->GetGuiEvents();

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        const CgsModule::Event* lpLastDispatchedEvent = lpAfterEvent;
        s32 liId = lpAfterEvent != 0
            ? lpInQueue->GetNextEvent(lpAfterEvent, &lpEvent, &liSize)
            : lpInQueue->GetFirstEvent(&lpEvent, &liSize);
        while (liId >= 0 && lpEvent != 0)
        {
            switch (liId)
            {
                case 141:   // ARTIST82528ACC..AE0: reply with this module's actual keyboard.
                {
                    // FLAG PC-platform leaf: the existing no-argument GUI pump publishes
                    // through mGuiOutQueue and its subscription-filtered observer loop-back.
                    GuiEventKeyboardResponse lResponse;
                    lResponse.lpKeyboard = &mGuiKeyboard;
                    mpOutputBuffer->GetOutEventQueueNonConst()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lResponse),
                        lResponse.GetEventType(), static_cast<s32>(sizeof(lResponse)));
                    break;
                }

                case 143:   // ARTIST 82528AE8..AF8: user-index word, then sign-in refresh.
                    mSystemUserProfile.SetUserIndex(*reinterpret_cast<const u32*>(lpEvent));
                    mSystemUserProfile.UpdateUserSigninState();
                    break;

                case 144:   // GuiEventRunFsm -- the flow-change request (BridgeGameToGui)
                    mFsmController.RunFsm(reinterpret_cast<const GuiEventRunFsm*>(lpEvent));
                    break;

                case 481:   // HUD-state load complete: notify the controller + forward
                    mFsmController.HandleHudStateLoadComplete();
                    // fall through -- the record also lands on the notification queue
                case 14:    // load notification    -> the ModelIO output notification queue
                case 16:    // unload notification  -> (the controller's Update consumes them)
                {
                    lpModelOutput->LockForWrite();
                    lpModelOutput->GetLoadNotificationsNonConst()->AddEvent(lpEvent, liId, liSize);
                    lpModelOutput->UnlockForWrite();
                    break;
                }

                case 221:  // ARTIST 82528B28: the same bar size also gates HUD messages.
                    mHudMessageDirector.SetBlackBarSize(*reinterpret_cast<const f32*>(lpEvent));
                    break;

                case 22:   // ARTIST82528444..82528468: stored Apt sound trigger.
                {
                    const GuiEventSoundTriggerPayload22* lpSound =
                        reinterpret_cast<const GuiEventSoundTriggerPayload22*>(lpEvent);
                    const char* lpcMovieName = mViewModule.GetMovieNameByLevel(lpSound->miLayer);
                    FlaptSoundTriggerCallback(this, lpSound->macTypeName, lpcMovieName,
                        lpSound->macActionName, lpSound->macLabel);
                    break;
                }

                case 26:    // GuiEventTimeInfo -- the per-frame { delta, now } pair
                    // The cache LEADS with this pair and every GUI-side timer reads it
                    // straight off mpGuiCache+0 (see GuiCache::RecTimeInfo's note). Without
                    // this latch mfTimeStep stays 0 and every GUI dwell/tick in the game is
                    // frozen -- which is exactly what parked BrnGui::Intro in WELCOMETEXT.
                    mGuiCache.RecTimeInfo(
                        reinterpret_cast<const CgsGui::GuiEventTimeInfo*>(lpEvent));
                    break;

                case 507:
                    // Preserve the existing selected name publication when its
                    // ordinary channel40 event returns through original stored input.
                    UpdatePlayerName();
                    break;

                case 504:   // localized audio ready
                case 508:   // play video
                case 513:   // (movie family)
                    mMovieManager.RecvEvent(lpEvent, liId);
                    break;

                case 493:   // World collision invalidate/validate completion
                {
                    const auto* lpCollisionWorldEvent =
                        reinterpret_cast<const GuiEventRequestCollisionWorldEvent*>(lpEvent);
                    if (lpCollisionWorldEvent->meEventType ==
                        GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_INVALIDATED)
                        mbCollisionWorldInvalid = true;
                    else if (lpCollisionWorldEvent->meEventType ==
                             GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_VALIDATED)
                        mbCollisionWorldInvalid = false;
                    break;
                }

                case 512:   // Traffic-pool emptied/refilled latch
                {
                    const bool lbTrafficPoolEmpty =
                        reinterpret_cast<const GuiEventTrafficPoolEmptied*>(lpEvent)->mbTrafficPoolEmpty;
                    if (lbTrafficPoolEmpty)
                    {
                        CGS_ASSERT(!mbTrafficPoolEmptied, "!mbTrafficPoolEmptied");
                        mbTrafficPoolEmptied = true;
                    }
                    else
                    {
                        CGS_ASSERT(mbTrafficPoolEmptied, "mbTrafficPoolEmptied");
                        mbTrafficPoolEmptied = false;
                    }
                    break;
                }

                case 350:   // GuiEventProgressionProfileData -- the live-profile handoff
                    // ⭐⭐⭐ [one-profile wave 2026-08-28] THE MISSING HALF OF CASE 350, and
                    // the reason this build owned TWO BrnProgression::Profile objects where the
                    // console owns one. X360 BrnGui::GuiModule::Update @0x82527A58:
                    //     case 350:
                    //       BrnGui::ProfileManager::SetProgressionProfile(a1 + 681696, *v61, v61[1]);
                    // i.e. the event's two payload words ARE the manager's mpProgressionProfile /
                    // mpProgressionData, and this is the ONLY writer of either on the console
                    // (SetProgressionProfile @0x824ECC98 has exactly one xref, and it is this).
                    // We routed 350 to the GuiCache only, so the manager never learned the live
                    // profile's address and BrnGuiModule::Prepare installed a module-static
                    // stand-in instead -- a SECOND Profile that Deserialise wrote into and no
                    // gameplay code ever read. The static and its install are deleted with this
                    // arm; the console's own chain is now the only installer.
                    {
                        const BrnGui::GuiEventProgressionProfileData* lpProfileData =
                            reinterpret_cast<const BrnGui::GuiEventProgressionProfileData*>(lpEvent);
                        mProfileHost.GetProfileManager().SetProgressionProfile(lpProfileData->mpProfile,
                                                              lpProfileData->mpProgressionData);
                    }
                    // fall through -- the record also feeds GuiCache::mpProfile (case 350 below)
                case 27:    // Actual Sku notification -> the cache enum owner.
                case 29:    // Actual language notification -> the cache enum owner.
                case 501:   // Original PFX enumeration -> actual cache owner.
                case 330:   // GuiEventSetRoadRuleScoreMode -- the road-rule panel mode word
                case 335:   // GuiEventRoadRuleBegin -- the rule's score type marked active
                case 336:   // GuiEventRoadRuleEnd   -- the rule's score type marked inactive
                case 169:   // GuiEventChangeDistrict -- the district-marker source words
                case 147:   // [H3b] GuiEventUpdateHud -- the player {speed,rpm,gear} words
                case 199:   // [H3b] GuiEventUpdateSatNav -- the icon array (player position arm)
                case 204:   // [H3b] the sat-nav event-filter pair (the ch40 mirror)
                case 207:   // [H3b] GuiRaceCarInfoEvent -- the mRaceCarInfo SoA feed
                case 208:   // Traffic score targets for AboveCarRenderer.
                case 210:   // Visible overhead-sign score targets.
                case 376:   // [H3b] GuiPlayerRaceCarIdEvent -- the player index pair (case-199 gate)
                case 377:   // GuiPlayerCrashingStateChangeEvent -- the crash-bar state latch + the
                            // gameplay-HUD-active drop (RecEvent @0x8250F58C; was misfiled as 132)
                case 379:   // [reveal gate] GuiPlayerEngineEvent -- the ignition latch (+0x4B20)
                case 238:   // Race positions, finished flags and per-slot validity.
                case 380:   // Player entered or left a shortcut.
                case 492:   // [E1] GuiEventCurrentStatus  -- distance driven + player-team table
                case 424:   // [E1] GuiEventScoreUpdate    -- THE EVENT TIMER (mfEventTime/mfTargetTime)
                case 428:   // [E1] GuiAttackScoreUpdate   -- THE STUNT SCORE (current/target/combo/multiplier)
                case 434:   // [showtime] GuiCrashScoreUpdate -- THE SHOWTIME SCORE (cars crashed + distance)
                case 426:   // [road-rage] GuiRoadRageScoreUpdate -- THE TAKEDOWN COUNT (current/target)
                case 194:   // Specific preset races; the cache adopts the per-mode table.
                case 203:   // [event-starts] GuiEventUpdateEventStarts -- THE EVENT-START TABLE
                case 93:    // [A9] GuiEventPrepareForModeStart -- THE MODE-TYPE SEED (meGameModeType)
                case 159:   // Pre-race messages and the in-event map colouring gate.
                case 162:   // Clear the pre-race message count.
                case 170:   // Landmark race list and its navigation tracker.
                case 289:   // [results] GuiEventOfflinePostEvent -- THE OFFLINE RESULT RECORD
                case 318:   // Online post-event record, adopted by the existing cache arm.
                case 292:   // [results] post-event teardown (clears the record + suppress byte)
                case 307:   // Medal totals and wins remaining on the licence.
                case 304:   // [results] raises the presentation-suppressed byte
                case 237:   // [event-start 2026-09-10] GuiGameModeStarted -- drops mbEventPreparedForModeStart
                case 321:   // [event-end 2026-09-10] GuiEventFinishedModeResults -- GuiCache's shared LABEL_232 tail
                case 322:   // [event-end 2026-09-10] GuiEventStopMode -- game mode back to NONE, tracker cleared
                case 556:   // [event-state 2026-09-10] GuiEventEventStateResponse -- the DISCOVERED-event array
                case 373:   // [FX-FLOW 2026-09-24, G13-X5] GuiShutdownEvent -- mShutdownCarID (RecEvent 0x8250FFA8)
                case 374:   // [FX-FLOW 2026-09-24, G13-X5] GuiShutdownFinishedEvent -- mbCarUnlockPending (0x8250FFC4)
                case 44:    // [net] GuiEventNetworkDisconnected -- the local player marked disconnected
                case 58:    // [net] the game launched -- disconnected / eliminated tables cleared
                case 108:   // [net] GuiEventOnlineTimeout -- the online timeout pending byte
                case 239:   // [net] GuiEventRaceDistanceRemaining -- the eliminated bytes
                case 240:   // [net] GuiEventRaceDistanceToCheckpoint -- the checkpoint distance
                case 243:   // [net] GuiEventNetworkPlayerList -- the pre-game roster
                case 244:   // [net] GuiEventNetworkLobbyPlayerList -- the lobby roster
                case 245:   // [net] GuiEventNetworkPlayerStatus -- the in-game roster + host byte
                case 257:   // [net] GuiEventNetworkGameParams -- the params mirror (online game mode)
                case 273:   // [net] GuiEventNetworkLeftGame -- the params security reset
                case 320:   // [net] online showtime completed
                case 79:    // GuiEnteredJunkyard -- the in-junkyard byte
                case 201:   // GuiEventHideDriveThru -- a closed drive-thru's map row hidden
                case 375:   // GuiEventTrophyCarUnlock -- the award screen's unlock type and car
                case 415:   // GuiChangeCarEvent -- current and original car ids for event icons
                    FreeRoamRouteWitness(liId, "cache");
                    // [H1 wave 2026-08-25] On the console EVERY module-input event reaches
                    // GuiCache::RecEvent (its ~180-case switch consumes what it wants);
                    // this build's pump routes selectively, so the two cache-consumed ids
                    // this wave landed handlers for are forwarded here explicitly. 350 is
                    // what fills GuiCache::mpProfile (the odometer asserts it per frame);
                    // 169 is what fills the district-marker words. ⚠️ Do NOT blanket-route
                    // the whole stream: 14/16 are already re-consumed off the notification
                    // queue below and would double-deliver.
                    // [E1 event-status wave 2026-08-26] 492/424/428 join the list on the same
                    // terms: BridgeGameStateToGui's stunt slice posts them and GuiCache::RecEvent
                    // now has the matching three arms. None of the three is re-consumed off the
                    // notification queue, so forwarding them here delivers exactly once.
                    // ⭐⭐ [results-screen wave 2026-08-29] 289/292/304 join on identical terms,
                    // and they were THE last link in the offline results chain. Measured: with
                    // GuiCache::RecEvent's case-289 arm landed but this forward missing, the
                    // bridge posted the 192-byte record (its action-37 arm builds and pushes it
                    // unconditionally on the offline path), the cache never saw it, and
                    // InstantResultsState copied 192 ZERO bytes -- so its own reconstructed
                    // SetupComponents printed the console's "Unhandled game mode! (0)" and the
                    // screen stayed blank. A selective pump means a landed producer AND a landed
                    // consumer still add up to nothing until the id is on this list; that is
                    // three separate places to check, and the log looked identical at each.
                    // None of the three is re-consumed off the notification queue.
                    // [showtime score wave 2026-08-29] 434 joins on IDENTICAL terms and completes
                    // that set for modes 2/16: its only producer is the same bridge's showtime arm
                    // (@0x823EEE18), GuiCache::RecEvent's case-434 arm is its only consumer, and
                    // nothing else in the pump touches it. It is the SOLE writer of
                    // miShowTimeCarsCrashed / miShowTimeComboMultiplier /
                    // mfShowTimeDistanceTravelled -- the three words
                    // EventInfoComponent::UpdateCrash renders.
                    // ⚠️ THE ID IS 434 (AddGuiEvent<GuiCrashScoreUpdate> @0x823D10D0 `li r5, 0x1B2`),
                    // NOT the DecFIGS DWARF's 429. Forwarding 429 here would forward an id nothing
                    // posts, and RecEvent has no arm for it either.
                    // [A9 mode-type arm 2026-08-27] 93 joins on exactly the same terms and it is
                    // the one that makes the other three mean anything: BrnGame::
                    // TranslateEventFlowGameActionToGuiEvent's case-23 arm posts it (mounted +
                    // called), GuiCache::RecEvent's case-93 arm above is its only consumer, and
                    // it is the SOLE writer of meGameModeType -- the switch variable case 424
                    // and case 428 assert against and EventInfoComponent::Update switches on.
                    // ⭐ SELECTIVE, NOT BLANKET (the gateui-campaign house rule): 93 is not
                    // observed by any flow (RouteEventToFlow drops it) and is not re-consumed
                    // off the notification queue, so this forward delivers it exactly once.
                    // [event-starts wave 2026-08-27] 203 joins on identical terms: its only
                    // producer is BridgeGameStateToGui's event-start arm (landed as
                    // GameBridgeGameStateToX_EventStartsGuiEvents.cpp), GuiCache::RecEvent's
                    // case-203 arm is its only consumer, and nothing else in the pump touches
                    // it -- so this forward delivers it exactly once. It is the SOLE writer of
                    // GuiCache::maEventStarts, the table GetProfileEventDisplayInfo walks.

                    break;

                case 343:   // GuiEventRoadRuleChangeMode -- the active road rule
                    // The console's module switch hands the new rule to the skills manager and,
                    // like every event, then gives the record to GuiCache::RecEvent, which keeps
                    // it for GetActiveRoadRule (the ticker's road-rules mode reads that).
                    mBurnoutSkillsManager.SetRoadRuleMode(
                        reinterpret_cast<const GuiEventRoadRuleChangeMode*>(lpEvent)->meScoreType);

                    break;

                // The free-burn challenge tracker's event arms, in the console's case order.
                // Only the online free-burn lobby produces these records (the action->GUI
                // translator's 153/154/155/157 arms and the network bridge's 583).
                case 544:
                    mFreeburnChallengeManager.SelectNext();
                    break;
                case 574:
                    mFreeburnChallengeManager.StartChallenge(
                        reinterpret_cast<const GuiChallengeStartEvent*>(lpEvent));
                    break;
                case 576:
                    mFreeburnChallengeManager.TriggerChallenge(
                        reinterpret_cast<const GuiChallengeTriggerResponse*>(lpEvent));
                    break;
                case 577:
                    mFreeburnChallengeManager.HandleNewData(
                        reinterpret_cast<const GuiChallengeUpdateEvent*>(lpEvent));
                    break;
                case 578:
                    mFreeburnChallengeManager.StartResults();
                    break;
                case 579:
                    mFreeburnChallengeManager.FinishChallenge();
                    break;
                case 581:
                    mFreeburnChallengeManager.HandleCompletionStatus(
                        reinterpret_cast<const GuiEventFburnChallengeEveryPlayerStatus*>(lpEvent));
                    break;
                case 583:
                    mFreeburnChallengeManager.StartNotActiveChallenge(
                        reinterpret_cast<const GuiChallengeNotActiveStartEvent*>(lpEvent));
                    break;
                case 584:
                    mFreeburnChallengeManager.EndNotActiveChallenge();
                    break;

                case 358:
                    // X360 copies the four image-save words into a frame-local record, raises
                    // the force/pending latches, then lets Update call AutosaveProfile. The host
                    // record is 24 bytes because the buffer pointer widened; the unwritten
                    // trailing word remains byte-for-byte residue.
                    std::memcpy(&lrSaveImageInfo, lpEvent, sizeof(lrSaveImageInfo));
                    rbHasSaveImageEvent = true;
                    mbForceProfileAutosave = true;
                    mbProfileAutosavePending = true;
                    break;

                case 359:   // Image-file records loaded by ProfileManager (ARTIST GuiModule::Update case 359)
                    mProfileHost.GetProfileManager().LoadImageFiles(
                        &mProfileHost,
                        reinterpret_cast<const CgsGui::ImageFileInfo*>(lpEvent));
                    break;

                case 356:   // [profile-save] GuiAutosaveRequestEvent -- THE AUTOSAVE LATCH
                {
                    // ⭐⭐ X360 GuiModule::Update's event walk @0x82529054 (jumptable
                    // 0x82528C5C case 0 == GUI id 356), instruction for instruction:
                    //     lbz  r11, 0(r30)          ; the request's one payload byte
                    //     lbz  r10, var_430(r1)     ; v142, the per-Update FORCE byte
                    //     or   r11, r11, r10
                    //     stb  r11, var_430(r1)     ; v142 |= payload
                    //     li   r11, 1
                    //     stbx r11, r26, r15        ; *(module + 0x18D844) = 1  (pending)
                    //
                    // ⛔⛔ THIS ARM IS THE ONE LINK THE AUTOSAVE COMMIT DID NOT LAND. Its
                    // message claims "case 356 raises the pending byte", but the diff only
                    // added the CONSUMER (Update's tail) -- `mbProfileAutosavePending` had a
                    // reader and a clearer and NO WRITER, so the latch could never rise and
                    // ProfileManager::Autosave was unreachable. Measured 2026-08-27: a full
                    // junkyard->car-select->driving run logged
                    // `[profile-save] action 55 -> gui 356 (flag 0)` and then never wrote
                    // Memcard\Profile.sav, because the byte on the other side of this arm
                    // stayed down. The 1-byte record is the AutosaveRequestWire356 both
                    // GameBridgeGameStateToX_{Stunt,EventFlow}GuiEvents.cpp post.
                    //
                    // ⚠️ The producers in this build post a payload of ZERO (CarSelectManager's
                    // exit post, BrnCarSelectManager.cpp:1185), so the force byte stays clear
                    // and the tail's 60-second throttle is what actually times the save --
                    // exactly as on the console. Only the paused-game arm (case 505) and the
                    // mugshot arm (case 358) force it.
                    mbForceProfileAutosave =
                        mbForceProfileAutosave || (*reinterpret_cast<const u8*>(lpEvent) != 0);
                    mbProfileAutosavePending = true;
                    break;
                }

                default:
                    // The other module-level consumers (profile/skills/overlays/keyboard/
                    // language...) are subsystem follow-ons; their events pass through to
                    // the flow filter below.
                    break;
            }

            // The console's per-event tail, after GuiCache::RecEvent: the set-tracker, landmark
            // reached, route-information, cache-pointer and regenerate-route ids go to the
            // tracker (GuiCache ignores all five).
            // ARTIST82529558: the wrapper forwards every input to its cache once.
            // This also owns genuine completions and stored trigger21 delivery.
            mGuiCache.RecEvent(lpEvent, liId);
            if (liId == 232 || liId == 165 || liId == 211 || liId == 64 || liId == 233)
            {
                FreeRoamRouteWitness(liId, "tracker");
                mGuiTracker.RecEvent(lpEvent, liId, liSize);
            }

            // ⭐ [tut-ticker] THE CUSTOM-RENDERER MANAGER FEED (2026-08-24). On the console
            // every module-input GUI event reaches BrnGui::CustomRendererManager::RecvEvent
            // through the view hop (BridgeFromInputToView -> ViewModule::
            // ProcessIncomingViewEvents' per-event hook @0x8285FCE8 tail). This build feeds
            // the view queue selectively, so the manager forward sits HERE, on the full
            // module-input stream -- exactly-once for everything that transits this queue
            // (537 the tutorial ticker, 64 the GuiCache bind, 258/571 the player image...).
            // The seat deviation is flagged at the console hook in CgsGuiViewModule.cpp.
            // The actual Input -> View bridge forwards this event once.

            // The observer-subscription fan-out (EventInterpreterModule::ProcessInEvents).
            // The actual Input -> Model bridge dispatches observers once.

            lpLastDispatchedEvent = lpEvent;
            const CgsModule::Event* lpNext = 0;
            liId = lpInQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
        mpGuiEventInputBuffer->UnlockForRead();
        return lpLastDispatchedEvent;
    }

    // The REAL GuiResourceModule dispatch (replaces ServiceFsmBundleRequests). Runs the
    // reconstructed CgsGui::GuiResourceModule each frame against its own persistent IO
    // pair, and bridges the two queue ends to the flow controller's ModelIO buffers --
    // the same 39-in / 14-out contract the host stand-in served, now through the module:
    //   1. feed this frame's controller load requests (GuiEventLoadRequest, id 39) into
    //      the module input, then clear the controller queue (consumed);
    //   2. run the module -- it drains the requests into its bundle-load queue, advances
    //      the acquire state machine, and at its Update tail runs the [PC] platform
    //      servicer that loads FSM\<NAME>.BUNDLE synchronously; completed loads post
    //      GuiEventLoadNotification (14) into the module output;
    //   3. clear the module input's now-consumed request queue (the persistent PC buffer
    //      is not recreated per frame as the console's transient one is);
    //   4. bridge the module's load notifications into the ModelIO output notification
    //      queue the controller reads (what the host stand-in posted on completion);
    //   5. clear the module output's bridged notifications.
    // The module's acquire machine takes several frames per bundle (acquire-miss -> load
    // -> re-acquire -> notify); the controller's WFLOAD stage polls for the notification,
    // so the completion arriving 1+ frames after the request is safe. On the console the
    // module runs under the model scheduler between the controller's request-post and
    // notification-read; here it runs in that same slot, before the controller Update.


    // (RequestAptMovieLoad RETIRED, slice 2: no more host movie-slot requests --
    // the engine's AptLoader owns movie data acquisition.)

    // (RequestAptMovieLoadThroughModule RETIRED, slice 2: the engine's AptLoader
    // requests movie data itself -- registered-data first, bundle-IO fallback --
    // through the real AptLoaderStartAsyncLoad platform hook.)

    // [PC IO] the ORIGINAL host FSM-bundle stand-in: serviced the controller's FSM-bundle
    // load requests synchronously and posted the load notification it waits for. SUPERSEDED
    // by DispatchGuiResourceModule (the real CgsGui::GuiResourceModule now owns this path);
    // retained unused this phase -- /OPT:REF strips the unreferenced body from the exe.
    // (On the console the request queue reaches CgsGui::GuiResourceModule through the
    // module scheduler; ProcessIncomingLoadRequests + LoadBundle then post the
    // notification -- those bodies are reconstructed, but the module dispatch that runs
    // them was not, so the IO leaf lived here.)


    // ARTIST82507800; the native caller invokes this immediately after
    // CgsGui::GuiModule::Update, while the same module OutputBuffer is still live.
    void GuiModule::HandleEventsPostBaseModuleUpdate(
        CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput)
    {
        lpOutput->LockForRead();

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventType = lpOutput->GetOutEventQueue()->GetFirstEvent(&lpEvent, &liEventSize);
        while (lpEvent != 0)
        {
            switch (liEventType)
            {
            case 493:
            {
                const GuiEventRequestCollisionWorldEvent* lpRequest =
                    reinterpret_cast<const GuiEventRequestCollisionWorldEvent*>(lpEvent);
                switch (lpRequest->meEventType)
                {
                case GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_INVALIDATE:
                    if (meCollisionWorldStateValidationState !=
                        E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE)
                    {
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert(
                            "meCollisionWorldStateValidationState == E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE",
                            "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 1729);
                        CgsDev::Assert::EndAssert();
                    }
                    // ARTIST stbx to GuiModule +0x18D83B.
                    mbCollisionWorldInvalidateForNetworking = true;
                    break;

                case GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_VALIDATE:
                    if (meCollisionWorldStateValidationState !=
                        E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_NETWORKING)
                    {
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert(
                            "meCollisionWorldStateValidationState == E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_NETWORKING",
                            "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 1737);
                        CgsDev::Assert::EndAssert();
                    }
                    // ARTIST stbx to GuiModule +0x18D83C.
                    mbCollisionWorldValidateForNetworking = true;
                    break;

                default:
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "Invalid collsion world request output event type",
                        "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 1745);
                    CgsDev::Assert::EndAssert();
                    break;
                }
                break;
            }

            case 508:
            case 509:
                mMovieManager.RecvEvent(lpEvent, liEventType);
                break;

            case 514:
            case 515:
                mColourCalibrationScreen.RecvEvent(lpEvent, liEventType);
                break;

            default:
                break;
            }

            liEventType = lpOutput->GetOutEventQueue()->GetNextEvent(
                lpEvent, &lpEvent, &liEventSize);
        }

        lpOutput->UnlockForRead();
    }

    // Drain one observer's StateInterface output queue -- the per-frame dispatch point for
    // everything its states post (the ModelModule bridge + EventInterpreter
    // ProcessOutEvents roles). The 34/35 subscription records key into THAT observer's
    // observed-id table. The fourth slot is the always-available components manager,
    // whose Prepare posts its real 19-id registration through the same records.


    // Selected no-argument PC entry for the GUI port. Original GuiModule::Update
    // (0x82527A58) receives its IO set from its frame caller.
    void GuiModule::Update()
    {
        // FLAG PC-platform leaf: the no-argument host entry owns its scripted
        // GameData pair. Loading callers already own the pair and use the typed
        // overload below. The original native Update receives its IO set from
        // its caller; this boundary does not recover that complete signature.
        BrnResource::GameDataIO::InputBuffer* lpGameDataInput =
            BrnGameMainFlowController::GetScriptedLoadGameDataInput();
        const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput =
            BrnGameMainFlowController::GetScriptedLoadGameDataOutput();
        lpGameDataInput->LockForWrite();
        lpGameDataOutput->LockForRead();
        Update(lpGameDataInput, lpGameDataOutput);
        lpGameDataOutput->UnlockForRead();
        lpGameDataInput->UnlockForWrite();
    }

    // The original call at ARTIST 82529AE4..82529B04 borrows the caller's
    // GameData pair. Keep its selected-port call together without an inner lock.
    void GuiModule::UpdateCalibrationScreen(
        BrnResource::GameDataIO::InputBuffer* lpGameDataInput,
        const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput)
    {
        if (lpGameDataInput != 0)
        {
            mColourCalibrationScreen.Update(lpGameDataInput, lpGameDataOutput,
                                            mpOutputBuffer->GetOutEventQueueNonConst(), mGuiConfig.mpTextureAllocator);
        }
    }

    // FLAG PC-platform leaf: selected GUI port with an explicitly borrowed
    // GameData pair, already locked by this frame's owner.
    void GuiModule::Update(BrnResource::GameDataIO::InputBuffer* lpGameDataInput,
        const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput)
    {
        // FLAG PC-platform leaf: selected caller borrows its actual IO owners.
        BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
        Update(muFrameUpdateSet, lpGameModule->GetUpdateInputBufferStack(),
            lpGameModule->GetUpdateOutputBufferStack(), mpGuiEventInputBuffer,
            lpGameModule->GetGuiOutputBuffer(), lpGameModule->GetGuiModelOutputBuffer(),
            &mViewInputBuffer, lpGameDataInput, lpGameDataOutput);
    }

    void GuiModule::Update(BrnUpdateSet leUpdateSet,
        CgsModule::IOBufferStack* lpInputStack, CgsModule::IOBufferStack* lpOutputStack,
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
        CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput,
        CgsGui::ModelIO::OutputBuffer* lpModelOutput,
        CgsGui::ViewIO::InputBuffer* lpViewInput,
        BrnResource::GameDataIO::InputBuffer* lpGameDataInput,
        const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput)
    {
        if (!lpGuiInput)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpInput != NULL",
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 1801);
            CgsDev::Assert::EndAssert();
        }
        if (!lpGuiOutput)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutput != NULL",
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 1802);
            CgsDev::Assert::EndAssert();
        }
        if (!lpInputStack)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpInputBufferStack != NULL",
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 1803);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputStack)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBufferStack != NULL",
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 1804);
            CgsDev::Assert::EndAssert();
        }
        muFrameUpdateSet = leUpdateSet;
        mpGuiEventInputBuffer = lpGuiInput;
        mGuiAccessPointers.SetGDMInput(lpGameDataInput);
        CGS_ASSERT(mpOutputBuffer == 0, "mpOutputBuffer == NULL");
        lpGuiOutput->LockForWrite();
        mpOutputBuffer = lpGuiOutput;
        // FLAG PC-platform leaf: preserve any current-frame host/harness staging.
        lpGuiOutput->AddGuiOutEvents(&mGuiOutQueue);
        mGuiOutQueue.Clear();
        lpGuiInput->LockForWrite();
        lpGuiInput->GetGuiEvents()->Append(mStoredGuiEvents);
        mStoredGuiEvents.Clear();
        lpGuiInput->UnlockForWrite();
        lpViewInput->LockForWrite();
        lpViewInput->GetViewStateQueue().CgsModule::VariableEventQueue<65536, 16>::Clear();
        lpViewInput->UnlockForWrite();
        CgsModule::IOHelper<CgsGui::ModelIO::InputBuffer> lModelInput(lpInputStack, "GUIModel");
        CgsModule::IOHelper<CgsGui::ViewIO::OutputBuffer> lViewOutput(lpOutputStack, "GUIView");
        CgsGui::ModelIO::InputBuffer* lpModelInput = lModelInput;
        CgsGui::ViewIO::OutputBuffer* lpViewOutput = lViewOutput;
        bool lbHasSaveImageEvent = false;
        CgsGui::ImageFileInfo lSaveImageInfo;
        // ARTIST 0x82527FD8 calls UpdateWorstCaseHUD(this, lpGuiInputBuffer) before
        // GetFirstEvent drains the queue, so its generated event is consumed this frame.
        mDebugComponent.UpdateWorstCaseHUD(mpGuiEventInputBuffer);
        const CgsModule::Event* lpLastInboundEvent =
            DispatchInboundGuiEvents(lbHasSaveImageEvent, lSaveImageInfo, 0, lpModelOutput);

        // ---- 2. the per-frame cache event (64): the real Update posts the GuiCache
        //          pointer into the event queue each frame; the states read it as their
        //          "cache ready" feed (BootPreload/BootVideos/BootProfile all key on it).
        {
            // The per-frame "gameplay HUD data ready" trio the gameplay HUD components gate on
            // (BoostBarRenderer::Update @0x82451CA4 reads cache+0x4B54/0x4B56/0x4B58 as one
            // gate). The console publishes +0x4B54 from the frame's update set
            // (`(lUpdateSet & 8) != 0`, PS3 GuiModule::Update); the other two bytes' producers
            // are unrecovered. [FLAG PC stand-in] the update set is not threaded into this
            // PC-shaped Update and the two producers are unknown, so all three follow the
            // gameplay-HUD flag the cache already maintains -- the same "in gameplay" condition
            // the update-set bit models. DELETE-WHEN the two producers are recovered.
            // 2026-09-17: the update set IS threaded now (SetFrameUpdateSet); the first byte
            // follows the console's `(a2 & 8) != 0` (@0x82527A58 -> +0xFA294). The other two
            // ride along as before.
            mGuiCache.SetGameplayHudReady((muFrameUpdateSet & 8) != 0);
            GuiEventCache lCacheEvent;
            lCacheEvent.mpGuiCache = &mGuiCache;
            // ⭐ [licence-icon] the CUSTOM-RENDERER manager is the event's other console
            // consumer (its RecvEvent case 64 fans the GuiCache to every component --
            // NetworkPlayerImageRenderer latches mpGuiCache off it, the pointer its staged
            // Prepare's resource wait needs). The queue record below is excluded from
            // suffix dispatch; this direct handoff remains its sole manager delivery.
            // Cache64 reaches the custom renderer through actual View input.

            // FLAG PC-platform leaf: ARTIST82529848 publishes cache64 before
            // GuiDebugComponent::Update reads it (82529C04). Publish the same
            // native record here so its read pass refreshes the PFX enumeration.
            // Original callbacks flush before base/model dispatch; this PC pump
            // dispatches/cache-updates earlier, so flush before those consumers.
            if (mpGuiEventInputBuffer != 0)
            {
                mpGuiEventInputBuffer->LockForWrite();
                CgsGui::CgsGuiModuleIO::InputBuffer::GuiEventInputQueue* lpGuiEvents =
                    mpGuiEventInputBuffer->GetGuiEvents();
                if (lpGuiEvents->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCacheEvent),
                                          64, static_cast<s32>(sizeof(lCacheEvent))))
                {
                    s32 liCacheEventSize = 0;
                    if (lpLastInboundEvent != 0)
                        lpGuiEvents->GetNextEvent(lpLastInboundEvent, &lpLastInboundEvent, &liCacheEventSize);
                    else
                        lpGuiEvents->GetFirstEvent(&lpLastInboundEvent, &liCacheEventSize);
                }
                mpGuiEventInputBuffer->UnlockForWrite();

                mDebugComponent.Update(mpGuiEventInputBuffer);
                mSatNavDebugComponent.Update(mpGuiEventInputBuffer);
                mGuiCacheDebugComponent.Update(mpGuiEventInputBuffer);
                // ARTIST 0x82529C40..44 copies the debug-selected mode each frame.
                mScreenFlow.SetStreamingMode(mDebugComponent.GetScreenFlowStreamingMode());
                // Fixed queue storage retains the cursor across Append. Skip
                // all prior input and cache64; retain initial image-save state.
                DispatchInboundGuiEvents(lbHasSaveImageEvent, lSaveImageInfo, lpLastInboundEvent, lpModelOutput);
            }

            // ⭐ [licence-icon] THE PREPARE PUMP (PC seat of the console's staged-Prepare
            // re-entry -- see the mbCustomRenderersPrepared note in the header). Advances the
            // manager's staged prepare each frame until the PlayerImage component reaches
            // E_PREPARESTAGE_DONE (its GuiCache wait needs the cache bind above first, which
            // is why the pump sits after it). One-shot log on completion.

        }

        // ---- 2b. boot-resources-ready feedback (event 567; bring-up FLAG) -------------
        // The console GUI cache posts 567 when the title's expected apt components have
        // initialised, which arms BootLegal's press-start path. The cache watcher isn't
        // reconstructed; post it once when the apt movie is live.


        // ---- 3. the FSM-bundle load service (the REAL GuiResourceModule) then the
        //          controller update ---------------------------------------------------


        // ---- 3b. the profile manager pump (X360 GuiModule::Update: the SLS Update at
        //          module+685896, the collision-world validate/invalidate swap
        //          @0x82519578, and the manager out-queue drained into the GUI out
        //          channel). The world-side pool free/restore around the swap is the
        //          un-reconstructed world collision pool -- FLAG'd absent (no world on
        //          the PC boot path); the manager's own state machine is driven fully. --
        // ARTIST8252A088/A094: system notifications and keyboard completion precede SLS.
        mSystemUserProfile.Update();
        mGuiKeyboard.Update();
        mProfileHost.GetProfileManager().Update();
        if (mProfileHost.GetProfileManager().PendingCollisionWorldInvalidate())
        {
            // FLAG PC-platform leaf: the console frees the world collision pool here.
            mProfileHost.GetProfileManager().SetCollisionWorldValid(false);
        }
        if (mProfileHost.GetProfileManager().PendingCollisionWorldValidate())
        {
            // FLAG PC-platform leaf: the console restores the world collision pool here.
            mProfileHost.GetProfileManager().SetCollisionWorldValid(true);
        }
        {
            CgsGui::GuiEventQueueSmall& lrProfileOut = mProfileHost.GetProfileManager().GetOutEventQueue();
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            s32 liId = lrProfileOut.GetFirstEvent(&lpEvent, &liSize);
            while (liId >= 0 && lpEvent != 0)
            {
                mpOutputBuffer->GetOutEventQueueNonConst()->AddEvent(lpEvent, liId, liSize);
                // The console publishes these on the module bus (AddGuiOutEvents onto the
                // out buffer's gui-events channel), where they come back around as in
                // events and reach every registered observer through the interpreter's
                // subscription filter (that is how the autosave-icon flag, id 355, reaches
                // the always-available manager). Model the loop with the same filter.
                // Base output -> stored input owns observer loopback.
                const CgsModule::Event* lpNext = 0;
                liId = lrProfileOut.GetNextEvent(lpEvent, &lpNext, &liSize);
                lpEvent = lpNext;
            }
            lrProfileOut.Clear();
        }

        // ---- 3b'. THE PROFILE AUTOSAVE TAIL --------------------------------------------
        // ⭐⭐ [profile-save 2026-08-27] X360 GuiModule::Update @0x825295CC..0x825296A4,
        // store for store (r26 == the module, r30 == module+0x18D844, r31 == 0x18D840):
        //     if (*(module + 1628228) == 1) {                     // the autosave-pending latch
        //         r11 = 1;
        //         if (*(module + 1024648) != 0) r11 = 0;          // the cache's suppress byte
        //         else if (!(GuiCache::GetTime() > *(module + 1628224) + 60.0f)) r11 = 0;
        //         if ((v142 | r11) == 1) {                        // v142 == the FORCE flag
        //             if (v143 == 1) GuiModule::AutosaveProfile(v186);   // with an image record
        //             else if (*(module + 949156) == 1 && *(module + 686280)) {
        //                 ProfileManager::Autosave(&mProfileHost.GetProfileManager(), &host, 0, 0);
        //                 *(module + 1628228) = 0;
        //                 *(module + 1628224) = GuiCache::GetTime();
        //             }
        //         }
        //     }
        // flt_82004C6C == 60.0f. GUI358 now reaches AutosaveProfile with the complete
        // host-native ImageFileInfo record; absent that event, this block saves the profile
        // without image files.
        //
        // The two gates, and what each is on PC:
        //  * module+1024648 == GuiCache+19272 -- GuiCache::Construct @0x82505860 zeroes it
        //    (`*(a1 + 19272) = 0;`) and its ONLY writer is a GuiCache::RecEvent @0x8250DDF0 arm
        //    for an event id this build does not route, so it is 0 for the whole run: no
        //    suppression. Reproduced as the console's initial value rather than as a fake read.
        //  * module+686280 == mProfileHost.GetProfileManager().mSaveLoadSystem's autosave-enabled word, which is
        //    exactly ProfileManager::AutosaveIsEnabled() (the committed accessor over the same
        //    word) -- tested below.
        //  * module+949156 == mSystemUserProfile.miState (1 == E_USERSIGNINSTATE_SIGNEDIN).
        //    Restored: PC now drives miState -- binding player 0 (a START press) hands its
        //    port to SystemUserProfile as the user index and UpdateUserSigninState signs that
        //    user in ("Signin state changed: signed-in" in the log) -- so the console's gate
        //    is back. Passing the title with A binds no controller,
        //    no user signs in and, as on ARTIST, the session does not autosave.
        if (mbProfileAutosavePending)
        {
            // flt_82004C6C == 60.0f (the throttle window, X360 `fadds f0,f13,f0`).
            const f32 KF_AUTOSAVE_THROTTLE_SECONDS = 60.0f;
            const f32 lfNow = mGuiCache.GetTime();
            const bool lbThrottleElapsed = lfNow > (mfLastProfileAutosaveTime + KF_AUTOSAVE_THROTTLE_SECONDS);

            if (mbForceProfileAutosave || lbThrottleElapsed)
            {
                if (lbHasSaveImageEvent)
                {
                    AutosaveProfile(&lSaveImageInfo);
                }
                else if (mSystemUserProfile.miState == 1 &&
                         mProfileHost.GetProfileManager().AutosaveIsEnabled())
                {
                    // The console's 4-arg call with count 0 / records NULL: save the profile
                    // image with no image files attached. The handler is the ProfileHost the
                    // module embeds; this build drives the manager directly, so it is the
                    // manager's own task-result handler that the boot states already use.
                    mProfileHost.GetProfileManager().Autosave(&mProfileHost, 0, 0);
                    mbProfileAutosavePending  = false;
                    mfLastProfileAutosaveTime = mGuiCache.GetTime();

                    if (CgsDev::Log::gpDebugPrint != 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "[profile-save] autosave requested (t=" << lfNow << ")\n";
                    }
                }
            }
        }
        // ⭐⭐ [drive-thru wave 2026-08-27] THE `else` ARM: THE 300-SECOND PERIODIC AUTOSAVE.
        // The block above is only the CONSUMER of the latch; this is the second PRODUCER, and it
        // was missing entirely -- so until now the game only ever autosaved on the event-driven
        // path (the id-356 GuiAutosaveRequestEvent arm at :1974). X360 @0x825296AC..0x82529700,
        // store for store (r26 == the module, r30 == module+0x18D844 == the pending latch):
        //     ori   r11, r11, 0x979C   ; 0xF979C == 1021852 == mGuiCache + 0x405C
        //     lwzx  r11, r26, r11      ; GuiCache::mpProfile
        //     cmpwi r11, 0 / beq       ; null-guarded
        //     ori   r10, r10, 0xA615   ; 42517
        //     lbzx  r11, r11, r10      ; Profile+42517, a BYTE
        //     cmplwi r11, 1 / bne      ; == 1 exactly, not "non-zero"
        //     bl    GuiCache::GetTime  ; module+0xF5740 == 1005376 == mGuiCache
        //     lfsx  f13, r26, 0x18D840 ; mfLastProfileAutosaveTime
        //     lfs   f0, flt_8201A258   ; 300.0f
        //     fadds f0, f13, f0 / fcmpu f1, f0 / ble
        //     li    r11, 1 / stb r11, 0(r30)   ; mbProfileAutosavePending = true
        // It ARMS the latch only; mbForceProfileAutosave is untouched, so the actual save
        // happens on the NEXT Update through the arm above -- whose own 60 s throttle is
        // trivially satisfied once 300 s have passed. That is why the two windows differ.
        //
        // ⚠️ THE GATE IS THE GOLD-CARS FLAG, AND THAT IS NOT A TYPO. Profile+42517 is the
        // 100%-completion flag (see the ⭐⭐ note on the pair in BrnProfile.h: the two flags were
        // named in DWARF declaration order and were swapped; CheckForSpecialCarUnlocks
        // @0x82396058 prints +42517 as "mProfile.AreGoldCarsUnlocked() : " and sets it from
        // ComputeCompletionPercentage() >= 100.0f). So the console's periodic autosave only runs
        // for a completed profile. Reproduced as-is -- it is a fixed byte compare against 1, not
        // an inference -- and flagged rather than "corrected" into a nicer-looking gate.
        // Consequence worth knowing when testing: on a fresh profile this arm never fires, and
        // the event-driven path above remains the only autosave. That matches the measured
        // behaviour (a fresh boot writes its container from the id-356 arm, not from here).
        else
        {
            const BrnProgression::Profile* lpCacheProfile = mGuiCache.GetProfile();
            if (lpCacheProfile != 0 && lpCacheProfile->GetGoldCarsUnlocked())
            {
                const f32 KF_AUTOSAVE_PERIOD_SECONDS = 300.0f;   // flt_8201A258
                if (mGuiCache.GetTime() > (mfLastProfileAutosaveTime + KF_AUTOSAVE_PERIOD_SECONDS))
                {
                    mbProfileAutosavePending = true;
                }
            }
        }
        mbForceProfileAutosave = false;   // the console's per-Update stack byte

        // Pump the always-available components manager (the top-left save-icon spinner + the
        // in-game EATrax/achievement/showtime overlays). The console GuiModule::Update
        // (@0x82527A58) advances its Prepare state machine each frame, and the interpreter's
        // UpdateObservers runs its Update against the queue the subscription filter filled
        // (RouteEventToFlow above delivers the ids its Prepare registered -- 64, 355, ...).
        // 0x8252A278. Prepare's BOOL RETURN IS A GATE, not a status byte to drop: the
        // console skips the whole overlay tick below until the prepare state machine reaches
        // DONE (`clrlwi r11, r3, 0x18 ; cmplwi 0 ; beq 0x8252A2CC`).


        // ---- 3b'. the overlays director's controller (console: inlined
        //          GuiOverlaysDirector::SetController, just before the calibration screen).
        //          The cache word is tested for null first; the setter then stores it and
        //          asserts it.
        if (mGuiCache.GetPopupController() != 0)
            mOverlaysDirector.SetController(mGuiCache.GetPopupController());

        // ---- 3c. the colour-calibration screen (X360 GuiModule::Update @0x82529B04) -----
        // The console ticks it here after the profile/overlay pumps and before
        // the flow update. The selected PC port uses this caller's borrowed
        // GameData pair and mGuiOutQueue as its GUI output boundary.
        // [FLAG PC bring-up TEST HOOK -- OFF BY DEFAULT] BRN_POSTFX_CALIB_SCREEN_TEST=<n>: 300
        // updates before the n-th GuiModule::Update ensure the CN_COLOUR state's two APT resources
        // are loaded (the same GuiCache::EnsureResourcesAreLoaded call BrnBaseFlow::UpdateStreaming
        // makes on state entry, over the same {143 APT, 34 APT} list CrashNavColourCalibrate::
        // maResourcesToLoad carries -- 143 = BRNCRASHNAVCOLOURCALIBRATE, the bundle that holds the
        // calibration card raster the screen acquires from pool 9), on the n-th feed the screen a
        // 514 (show), and 600 updates later a 515 (hide). Stands in for the flow entering CN_COLOUR
        // + CrashNavColourCalibrate::SetupComponents / OnLeave, all reconstructed but UNREACHABLE on
        // this build (the FSM's only inbound edge to CN_COLOUR is "TO_COLOUR", posted by
        // CrashNavSettings, which has no TU). Exercises the whole chain: APT load -> screen acquire
        // -> 546 -> BridgeGuiToGame -> dispatch buffer -> the composite's calibration ramp override.
        // Without the resource load the acquire returns the null handle and the screen's own
        // "!= NULLResourceHandle" assert (cpp:147) fires -- measured. DELETE-WHEN CrashNavSettings lands.
        {
            static s32  siTestShowAt = -2;   // -2 = not read yet, -1 = knob absent
            static u32  suUpdates    = 0u;
            if (siTestShowAt == -2)
            {
                const char* lpcEnv = std::getenv("BRN_POSTFX_CALIB_SCREEN_TEST");
                siTestShowAt = (lpcEnv != 0 && lpcEnv[0] != '\0') ? std::atoi(lpcEnv) : -1;
                if (siTestShowAt >= 0)
                    CgsDev::Log::WriteToLog("[calib-screen] TEST HOOK armed: load APT at N-300, show at update N, hide at N+600\n");
            }
            ++suUpdates;
            if (siTestShowAt >= 300)
            {
                CgsModule::Event lEvent;
                if (suUpdates == static_cast<u32>(siTestShowAt) - 300u)
                {
                    static const CgsGui::sResourceTuple kaCalibrateResources[] =
                        { { 143u, CgsGui::E_GUI_RESOURCETYPE_APT }, { 34u, CgsGui::E_GUI_RESOURCETYPE_APT } };
                    mGuiCache.EnsureResourcesAreLoaded(kaCalibrateResources, 2u);
                    CgsDev::Log::WriteToLog("[calib-screen] TEST HOOK: CN_COLOUR APT resources {143,34} requested\n");
                }
                else if (suUpdates == static_cast<u32>(siTestShowAt))
                    mColourCalibrationScreen.RecvEvent(&lEvent, 514);
                else if (suUpdates == static_cast<u32>(siTestShowAt) + 600u)
                    mColourCalibrationScreen.RecvEvent(&lEvent, 515);
            }
        }
        UpdateCalibrationScreen(lpGameDataInput, lpGameDataOutput);
        // The console's next call after ColourCalibrationScreen::Update, unconditional: the
        // sat-nav tracker's route request (GUI out 494).
        mGuiTracker.Update(mpOutputBuffer->GetOutEventQueueNonConst());

        // ---- 3c'. THE SCREEN-FILTER ARBITRATOR (X360 GuiModule::Update @0x82527A58) --------
        // EffectsArbitrator::EventUpdate @0x825125C0: the 495..500 hook events this frame's
        // bridge posted (start / stop / background / enumeration request -> the 501 reply on
        // the out queue), then UpdateHooks. The GuiCache hand-over is the console's event-64
        // case; this build posts 64 outside the module queue, so the cache is handed over by
        // name. The update set is the scheduler's own (SetFrameUpdateSet), as on the console.
        mEffectsArbitrator.SetGuiCache(&mGuiCache);
        if (mpGuiEventInputBuffer != 0)
        {
            const BrnUpdateSet leUpdateSet = muFrameUpdateSet;
            mpGuiEventInputBuffer->LockForRead();
            mEffectsArbitrator.EventUpdate(leUpdateSet, mpGuiEventInputBuffer, mpOutputBuffer->GetOutEventQueueNonConst());
            mpGuiEventInputBuffer->UnlockForRead();
        }

        // ---- 3d. THE HUD-MESSAGE PUMP (gateui wave, round 2) --------------------------
        // X360 GuiModule::Update @0x82527A58, reproduced in the console's own order:
        //
        //   910  v153 = gm + 1005376;
        //   911  VariableEventQueue<32768,16>::AddEvent(v156, &v153, 64, 4);
        //          -- v156 == sub_8284F238(a5) == CgsGuiModuleIO::InputBuffer::GetGuiEvents(),
        //             i.e. the console PUSHES the GuiCache pointer (event 64) back into the
        //             very queue the analyzer is about to drain. That record is how
        //             HudMessageAnalyzer::mpGuiCache gets set: Construct @0x82509060 leaves
        //             it NULL and Update's case 64 latches it (_wB_12). Without this push
        //             the analyzer's first `mpGuiCache->...` is a null deref.
        //   912-929 the mpHudMessageController assert (BrnGuiCache.h:2418) + SetController.
        //   1015 LockForWrite(a8)                        -- the VIEW input buffer
        //   1022 v114 = ViewIO::InputBuffer::GetViewStateQueue(a8)
        //   1024 HudMessageAnalyzer::Update(gm + 660992, v115, v114)
        //   1031 UnlockForWrite(a8)
        //   1035 HudMessageDirector::Update(gm + 639264, v150)   -- v150 == the ModelIO
        //          input buffer, the same one GuiCache::Update is handed at line 1048.
        //
        // The console runs the analyzer between ColourCalibrationScreen::Update (1007) and
        // GuiOverlaysDirector::Update (1032), both inside the GUI input buffer's write lock;
        // the director follows the analyzer below, in the same lock.
        if (mpGuiEventInputBuffer != 0)
        {
            mpGuiEventInputBuffer->LockForWrite();
            {
                CgsGui::CgsGuiModuleIO::InputBuffer::GuiEventInputQueue* lpGuiEvents =
                    mpGuiEventInputBuffer->GetGuiEvents();

                // The native cache64 record is already in this queue, published
                // before the debug read/flush pass at the PC dispatch boundary.

                // console lines 911-928, and the SHAPE below is the CONSOLE'S OWN, not a
                // PC gate: it fires a NON-GATING `mpHudMessageController` assert
                // (BrnGuiCache.h:2418) and then wraps the SetController call in
                // `if ( *(a1 + 1021872) )` -- i.e. it too declines to publish a null
                // controller.
                //
                // ⭐ [gateui r4] THE ROUND-3 PARK IS RETIRED. Its DELETE-WHEN ("a
                // HudMessageController producer lands") is met twice over:
                // GameDataModule::PrepareHudMessages fills mHudMessageController, and
                // GuiModule::Construct now receives it as the console's `a2` and stores it
                // through GuiCache::SetHudMessageController. So this gate is expected to be
                // TRUE from the first frame and the assert below is a tripwire, not a
                // standing gap report.
                //
                // The assert fires ONCE rather than per frame: a per-pump assert storm is
                // what the 440-assert perf incident was, and the console reaches this line
                // once per frame too.
                if (!mGuiCache.HasHudMessageController())
                {
                    static bool sbNoControllerAssertFired = false;
                    if (!sbNoControllerAssertFired)
                    {
                        sbNoControllerAssertFired = true;
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert(
                            "mpHudMessageController",
                            "..\\..\\..\\GameSource\\Gui/BrnGuiCache.h", 2418);
                        CgsDev::Assert::EndAssert();
                    }
                }
                else
                {
                    // ARTIST82529894..825298A8 lazily binds the debug component too.
                    if (!mHudMessageDebugComponent.ControllerIsValid())
                        mHudMessageDebugComponent.SetController(mGuiCache.GetHudMessageController());
                    mHudMessageDirector.SetController(mGuiCache.GetHudMessageController());
                }

                // console lines 1015-1031: the analyzer runs with the VIEW input buffer
                // write-locked across it (it appends view-state records through
                // mpViewOutputQueue).
                lpViewInput->LockForWrite();
                mHudMessageAnalyzer.Update(lpGuiEvents, &lpViewInput->GetViewStateQueue());
                lpViewInput->UnlockForWrite();

                // [FLAG PC seam] the previous frame's flow-posted overlay traffic. On the
                // console it rode the GUI out-event loop-back into this queue at the top of
                // the update; here it joins the queue only now, after every other reader of
                // it has run, so the director is the one consumer that sees it (the flows got
                // the same records at the drain). See mOverlayLoopBackQueue.
                // Full stored input was replayed once at Update entry.

                // console line 1032: the overlays director drains the queue.
                mOverlaysDirector.Update(mpGuiEventInputBuffer);
            }
            mpGuiEventInputBuffer->UnlockForWrite();
        }

        // console line 1035. The director drains its own published messages (event 154)
        // into the model module's input GUI-event queue.
        mHudMessageDirector.Update(lpModelInput);

        // The overlays director hands its 185/187/188 records to the model input buffer
        // (console: after the debug components, before GuiCache::Update). The walk below
        // fans them out to the overlay flow's states.
        mOverlaysDirector.BridgeOutEvents(lpModelInput);

        // FLAG PC bring-up (dispatch seam, NOT a behaviour change): on the console the
        // records the director just appended are consumed by the embedded
        // CgsGui::ModelModule's own update -- the EventInterpreterModule fans them out to
        // every registered observer, which is how event 154 reaches
        // BrnFBurnMainHudState::RecvEvent. This build models that fan-out with
        // RouteEventToFlow (see DispatchInboundGuiEvents), so the same walk is run here
        // over the model input buffer's queue. It is ALSO what keeps that queue bounded:
        // nothing else drains or clears it, and a GuiHudMessage is ~840 bytes against a
        // 32768-byte queue -- roughly 39 messages to an overflow assert.
        // DELETE-WHEN CgsGui::ModelModule is reconstructed and owns this dispatch.


        // ⭐ [stuntrace] the freeburn-challenge tracker's per-frame tick. X360
        // GuiModule::Update @0x82527A58 runs it in exactly this seat -- the two manager ticks
        // immediately before the base module update that drives the flows:
        //     BrnGui::BurnoutSkillsManager::Update(gm + 309032);
        //     BrnGui::FreeburnChallengeManager::Update(gm + 309584);
        //     CgsGui::GuiModule::Update(gm, ...);      // == the flow ticks below
        // The body is the AUTO_ROTATE page timer and is entirely behind
        // `meInternalState != E_INTERNAL_STATE_OFF`, so on an offline event it costs one
        // compare per frame. The skills-manager tick above it rotates the Burnout Skillz page
        // only in game mode 15.
        // The manager's event arms (544/574/576..579/581/583/584) are in
        // DispatchInboundGuiEvents; only free-burn challenge records reach them.

        // FLAG PC-platform leaf: this port fans the current frame's model/input
        // events out directly. ARTIST 82529848 appends cache 64 after the existing
        // GUI input; base Update 828602C8 then bridges that stream to the model.
        // Publish it here, after current inbound events (including profile 350),
        // before observer updates. A post-update handoff leaves stale 64 ahead of
        // the next frame's profile and violates Intro's SetProfilePointer contract.


        // ---- 6. the MovieManager pump (receiver -> RecvEvent -> Update -> 510 back) ---
        {
            CgsModule::BaseEventReceiverQueue* lpRecv = mMovieManager.GetReceiverQueue();
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            s32 liId = lpRecv->GetFirstEvent(&lpEvent, &liSize);
            while (liId >= 0 && lpEvent != 0)
            {
                mMovieManager.RecvEvent(lpEvent, liId);
                const CgsModule::Event* lpNext = 0;
                liId = lpRecv->GetNextEvent(lpEvent, &lpNext, &liSize);
                lpEvent = lpNext;
            }
            lpRecv->Clear();
        }
        // (MovieManager::Update advances from the render pass now -- the console's
        // UpdateAndRenderMovieManager @0x82511240 -- so the two arms below read the state
        // that pass left.)
        //
        // X360 GuiModule::Update @0x82527A58, the two MovieManager arms it drives by hand,
        // in the console's order (`*(gm + 301604)` == the manager's meState, `gm + 304976`
        // == its playing VideoDefinition, dword_830082A8 == K_NULL_NAME == MakeHash("") == 0):
        //     if (state == 12 /*REPORTING_FINISHED*/) { def = playing; state = 13 /*IDLE*/;
        //         AddGuiOutEvent<GuiEventPlayMusicOnMenuStream>({K_NULL_NAME, 0, 0});  // stop the video's stream
        //         modelInput.GetEventQueue().AddEvent(&def, 510, 48); }              // "video finished"
        //     if (state == 5 /*REQUESTING_AUDIO*/)     { def = playing; state = 6 /*WAITING_FOR_AUDIO*/;
        //         AddGuiOutEvent<GuiEventPlayMusicOnMenuStream>({def.mSoundStreamName, 0,
        //                                                        def.mbDisableCustomSoundtracks}); }
        // The movie's sound is the sound module's MENU stream: MusicEffect resolves the
        // name (GetStreamFromVideoName), queues it on slot 12 and answers GuiOut 504, which
        // case 504 above hands to MovieManager::RecvEvent (6 -> 7 PLAYING). The typed
        // menu-stream event goes out on the module output buffer under its own id 23,
        // exactly as the flow-originated channel-155 records are re-posted below.
        if (mMovieManager.GetState() == MovieManager::E_MOVIEMANAGERSTATE_REPORTING_FINISHED)
        {
            const MovieManager::VideoDefinition lFinished = mMovieManager.GetPlayingMovieDefinition();
            mMovieManager.SetState(MovieManager::E_MOVIEMANAGERSTATE_IDLE);
            CgsGui::GuiEventPlayMusicOnMenuStream lStopStream(0u, false, false);
            mpOutputBuffer->GetOutEventQueueNonConst()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStopStream),
                                     23, static_cast<s32>(sizeof(lStopStream)));
            lpModelInput->LockForWrite();
            lpModelInput->GetEventQueueNonConst()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lFinished), 510,
                static_cast<s32>(sizeof(lFinished)));
            lpModelInput->UnlockForWrite();
            CgsDev::Log::WriteToLog("[GuiModule] video finished -> menu stream cleared, fed 510 to the flow.\n");
        }
        if (mMovieManager.GetState() == MovieManager::E_MOVIEMANAGERSTATE_REQUESTING_AUDIO)
        {
            const MovieManager::VideoDefinition lPlaying = mMovieManager.GetPlayingMovieDefinition();
            mMovieManager.SetState(MovieManager::E_MOVIEMANAGERSTATE_WAITING_FOR_AUDIO);
            CgsGui::GuiEventPlayMusicOnMenuStream lVideoStream(
                lPlaying.mSoundStreamName, false, lPlaying.mbDisableCustomSoundtracks);
            mpOutputBuffer->GetOutEventQueueNonConst()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lVideoStream),
                                     23, static_cast<s32>(sizeof(lVideoStream)));
            char lac[160];
            std::snprintf(lac, sizeof(lac),
                          "[GuiModule] video audio requested: menu stream 0x%08X (disableCustom=%d) -> WAITING_FOR_AUDIO\n",
                          static_cast<unsigned>(lPlaying.mSoundStreamName),
                          lPlaying.mbDisableCustomSoundtracks ? 1 : 0);
            CgsDev::Log::WriteToLog(lac);
        }

        // ---- 7. the view frame (the real per-frame owner) -----------------------------
        // Post the frame time step (view event 26) onto the view-state queue and run
        // CgsGui::ViewModule::Update -- which dispatches the view events (incl. the
        // bridged play-movie 18 + notifications 14), advances the view clock, and ticks
        // AptAux::Update (the component flush + the engine AptUpdateTarget frame pacer).
        // FLAG (PC time source): the console's step rides the module scheduler's clock;
        // the wall clock is the host stand-in.
        // Original pre-base cache and controller/effect resource consumers.
        lpModelInput->LockForWrite();
        mGuiCache.Update(lpModelInput);
        lpModelInput->UnlockForWrite();
        UpdateNativeSystemNotifications(lpModelInput);
        lpModelInput->LockForWrite();
        lpModelOutput->LockForRead();

        // ARTIST GuiCache::RecEvent consumes load/unload completion before the cache
        // update publishes its next double-buffered request batch.
        mFsmController.Update(lpModelInput, lpModelOutput);
        lpModelOutput->UnlockForRead();
        lpModelInput->UnlockForWrite();

        // X360 GuiModule::Update: EffectsArbitrator::ResourceUpdate(modelIn, modelOut)
        // @0x825177E8 -- the hook bundle (request 228) and colour-cube (229..) load
        // notifications, consumed before the tail clears the queue; a fresh bundle posts the
        // cube requests onto the model input.
        lpModelInput->LockForWrite();
        lpModelOutput->LockForRead();
        mEffectsArbitrator.ResourceUpdate(lpModelInput, lpModelOutput);
        lpModelOutput->UnlockForRead();
        lpModelInput->UnlockForWrite();

        // The real Update clears the notification queue at its tail (the per-frame IO
        // buffer lifecycle); the controller has consumed this frame's records.
        lpModelOutput->LockForWrite();
        lpModelOutput->GetLoadNotificationsNonConst()->Clear();
        lpModelOutput->UnlockForWrite();        const bool lbAlwaysAvailReady =
            mAlwaysAvailableComponentsManager.Prepare(&mGuiAccessPointers);
        if (lbAlwaysAvailReady)
        {
            if ((muFrameUpdateSet & 8) != 0)
            {
                // 0x8252A288..0x8252A2B0 -- the EATrax / achievement timeout tick.
                // Preserve the existing gameplay-frame-bit gate for these two components.
                mAlwaysAvailableComponentsManager.UpdateTimedComponents();
            }
            // 0x8252A2C8 -- tick invite state whenever Prepare and container-playing pass.
            mAlwaysAvailableComponentsManager.UpdateOnlineInviteComponent();
        }
        mBurnoutSkillsManager.Update();
        mFreeburnChallengeManager.Update();

        // The single actual owner now performs Input->Model, Input->View,
        // Model.Update, Model->View, View.Update and every original output bridge.
        CgsGui::GuiModule::Update(lpInputStack, lpOutputStack, mpGuiEventInputBuffer,
            lpGuiOutput, lpModelInput, lpModelOutput,
            lpViewInput, lpViewOutput);
        mpOutputBuffer = 0;
        lpGuiOutput->UnlockForWrite();
        HandleEventsPostBaseModuleUpdate(lpGuiOutput);
        mCustomRendererManager.Update();
        // FLAG PC-platform leaf: existing raw consumers read one exact mirror.
        // The actual GUI output, including pre-world records, is never cleared here.
        mGuiOutQueue.Clear();
        lpGuiOutput->LockForRead();
        mGuiOutQueue.Append(*lpGuiOutput->GetOutEventQueue());
        lpGuiOutput->UnlockForRead();
        mGuiAccessPointers.SetGDMInput(0);
    }

    // ⭐⭐⭐ THE GUI END-OF-FRAME NOTIFY, restored 2026-09-07. X360 @0x824F1008, verbatim:
    //     addis r3, r3, 5 ; addi r3, r3, -0x3D70 ; b BrnGui__CustomRendererManager__EndOfFrame
    // -- +327680-15728 == +311952 == mCustomRendererManager, tail-called. That in turn is
    //     addis r3, r3, 2 ; addi r3, r3, -0x49C0 ; b BrnGui__NetworkPlayerImageRenderer__SwapBuffers
    // -- +131072-18880 == +112192 == the embedded NetworkPlayerImageRenderer. So the whole
    // notify is one thing: SWAP THE PLAYER-IMAGE TRIPLE BUFFER.
    //
    // ⛔ IT WAS DECLARED NOWHERE, DEFINED NOWHERE AND CALLED BY NOTHING -- which links in
    // total silence. Both bodies below it were already correct and had NEVER EXECUTED. This
    // is the exact sibling of ParticleModule::EndOfFrame (c227a165, issue #17), named by the
    // same stale banner line in BrnGameModule::OnEndOfUpdateFrame.
    //
    // ⭐ WHAT ITS ABSENCE COSTS, from the console's own arithmetic (no eyeballing):
    //   NetworkPlayerImageRenderer::Construct @0x82445A50 seeds `*(this+736) = 1` and
    //   `*(this+740) = 2` -- miCurrentRenderTexture = 1, miCurrentCopyToTexture = 2 -- and
    //   SwapBuffers @0x82445E58 is the ONLY writer of either word in the whole image (both
    //   `(x+1) % 3`, via the 0x55555556 magic-multiply pair at 0x82445E9C/0x82445EA4).
    //   RecvEvent @0x82449CA0 case 258 copies an arriving picture into slot `v4[185]` ==
    //   miCurrentCopyToTexture and flags `maabRender*Texture[idx][thatSlot]`; GetRenderOutput
    //   @0x82445CC0 reads slot `miCurrentRenderTexture` and consults the flags at THAT slot.
    //   With the cursors frozen at 1 and 2, every picture is written to slot 2 and every read
    //   asks slot 1, whose flags no code path can ever set. ⇒ A RECEIVED PLAYER PICTURE CAN
    //   NEVER BE DISPLAYED. The two type-258 producers are single-player:
    //   LicenseComponent::SendPlayerPictureEvent and PhotoBoothComponent (see the bridge note
    //   in DispatchInboundGuiEvents) -- the licence card and the photo booth.
    //   Secondary: SwapBuffers is also the only per-frame writer of the Flapt special texture
    //   ("CustomComponentTexture.tif", FlaptFile::SetSpecialTexture @0x8246D750) and of
    //   FlaptRenderer::miSpecialTextureShaderProgram, so both stayed at whatever
    //   NetworkPlayerImageRenderer::Prepare @0x82451560 left on the frame it ran.
    //
    // ⚠️ NOT a fix for GitHub #9 (minimap blips) or #11 (in-game event UI): this chain is
    // three tail calls long and touches nothing but the player-image buffer set. Measured,
    // not assumed -- see the wave report.
    void GuiModule::EndOfFrame()
    {
        mCustomRendererManager.EndOfFrame();
    }

    // ARTIST 0x825146B8 plus its CgsGui::GuiModule::Render core @0x8285AF38.
    // That core is inlined here while the canonical base/model class hierarchy
    // remains unrecovered; this preserves its operations without a proxy base.
    // The scheduler supplies the real view/module inputs and renderer output.
    // Preparation gates the entire original pass; no renderer pointer is selected here.
    // Perfmon aliases: dword_82F2767C = miGuiModuleRender, dword_82F330B8 = miGuiRender.
    void GuiModule::Render(CgsGui::ViewIO::InputBuffer* lpViewInput,
                           CgsGui::CgsGuiModuleIO::InputBuffer* lpInput,
                           RendererIO::OutputBuffer* lpRenderOutput)
    {
        CgsDev::PerfMonCpu::StartMonitor(GuiPerfmons::miGuiModuleRender);
        CGS_ASSERT(lpInput != nullptr, "lpInput");
        CGS_ASSERT(lpRenderOutput != nullptr, "lpRenderOutput");
        if (mbPrepared && mDebugComponent.IsGuiRenderingEnabled())
        {
            CgsDev::PerfMonCpu::StartMonitor(GuiPerfmons::miGuiRender);
            // 0x823B6FE0/7060 carry these non-gating checks around the bridge.
            CGS_ASSERT(lpViewInput != nullptr, "lpInputBuffer");
            CGS_ASSERT(lpInput != nullptr, "lpOutputBuffer0");
            CgsModule::LockBuffersForIO(lpViewInput, lpInput);
            lpViewInput->SetImRenderers(lpInput->GetImRenderers());
            CGS_ASSERT(lpViewInput != nullptr, "lpInputBuffer");
            CGS_ASSERT(lpInput != nullptr, "lpOutputBuffer0");
            CgsModule::UnlockBuffersForIO(lpViewInput, lpInput);
            mViewModule.Render(lpViewInput);
            CgsDev::PerfMonCpu::StopMonitor(GuiPerfmons::miGuiRender);

            UpdateAndRenderMovieManager(lpViewInput);
            mEffectsArbitrator.GenerateEffectFrameEvents(lpRenderOutput);
        }
        CgsDev::PerfMonCpu::StopMonitor(GuiPerfmons::miGuiModuleRender);
    }

    // FLAG PC-platform leaf: retain the completed camera/input across a present
    // with no new simulation step. The next dispatch refreshes its renderer set
    // through the original bridge before Render consumes it.
    void GuiModule::CaptureRenderInputPC(const CgsGui::CgsGuiModuleIO::InputBuffer* lpInput)
    {
        CGS_ASSERT(lpInput != nullptr, "lpInput");
        CgsModule::LockBuffersForIO(&mCompletedRenderInputPC, lpInput);
        const CgsGui::ImRendererSet& lrRenderers = lpInput->GetImRenderers();
        CgsGraphics::Camera lCamera = lrRenderers.mCamera;
        mCompletedRenderInputPC.SetCamera(lCamera);
        mCompletedRenderInputPC.SetImRenderers(lrRenderers);
        CgsModule::UnlockBuffersForIO(&mCompletedRenderInputPC, lpInput);
        mbRenderInputCompletedPC = true;
    }

    CgsGui::CgsGuiModuleIO::InputBuffer* GuiModule::GetCompletedRenderInputPC()
    {
        return mbRenderInputCompletedPC ? &mCompletedRenderInputPC : nullptr;
    }

    // BridgeFromViewToOutput @0x8285DE10 -- an export hole, read with tools/re/ppcdis.py:
    //     Append<18432,16>(this + 111644, ViewIO::OutputBuffer::GetGuiEventQueue(viewOut));
    //     OutputBuffer::AddGuiOutEvents(moduleOut, *(view + 57352) + 12);
    //     OutputBuffer::AddGuiOutEvents(moduleOut, view + 56768);   // ViewModule::mOutputEventQueue
    // (called from CgsGui::GuiModule::Update @0x82860548, right after the view Update).
    // The first two carry the view OUTPUT buffer's queue -- the Apt trigger records 21/22
    // that Update above already drains by hand. The third had NO PC seat: the view module's
    // OWN out queue, where ProcessIncomingAptEvent @0x8285EAE8 posts 33
    // GuiEventLoadingScreenState {visible} for PlayAptLoadingMovie 19 / StopAptLoadingMovie
    // 20. The sound logic module drains the module out queue (BridgeGuiToSound @0x823C0A58)
    // and turns 33 into bit 1 of its dispatch flags word -- half of MusicEffect::
    // UpdateParams' stream-pause gate -- so on PC that gate could never hold while the
    // loading screen was up, and HUDEffect::Notify's loading-screen test never tripped.


    // @ 0x82511240 -- pump the movie manager (the movie pass of the GUI render).
    // ARTIST 0x82511240: movie update precedes the view-input read bracket.
    // MoviePlayer::Render records into the same supplied Im2d command buffer.
    void GuiModule::UpdateAndRenderMovieManager(CgsGui::ViewIO::InputBuffer* lpViewInput)
    {
        mMovieManager.Update();
        lpViewInput->LockForRead();
        mMovieManager.Render(lpViewInput->GetImRenderers().mpIm2dRenderBuffer);
        lpViewInput->UnlockForRead();
    }

}

// ---- GetAlwaysAvailableComponentsManager (free accessor) ----------------------------
// Header-declared in BrnGuiAlwaysAvailableComponentsManager.h; homed here because this TU
// owns the GuiModule layout.
namespace BrnGui
{
    AlwaysAvailableComponentsManager* GetAlwaysAvailableComponentsManager(GuiModule* lpGuiModule)
    {
        return lpGuiModule->GetAlwaysAvailableComponentsManager();
    }

}
