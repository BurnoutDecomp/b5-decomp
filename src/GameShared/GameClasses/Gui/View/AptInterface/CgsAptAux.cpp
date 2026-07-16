#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCallbackRender.h"  // the render + render-flag callback family
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h"       // CgsGui::AptDataHeader (LoadAnimation's FindAptData result)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"     // CgsGui::AptCommunicator (the ext-object phase + the flush)
#include "SDKs/EATech/include/Apt/Apt.h"                                         // AptUserFunctions gAptFuncs (the host table)
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT (the host-callback asserts)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"        // CgsDev::PerfMonCpu (the Update phase brackets)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                         // CgsCore::SPrintf (the "_level%d" target path)
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                          // BrnFlapt::TextFieldRef (its GetLanguageManager is bridged here)

#include "SDKs/EATech/Apt/AptInit.h"                                            // Apt bring-up entry points (InitializeApt callees)
#include "SDKs/EATech/include/Apt/AptTarget.h"                                  // AptCreateTargetInstance / AptChangeTargetInstance
#include "SDKs/EATech/include/Apt/AptRenderWalk.h"                              // AptRenderTarget (the AptAux::Render walk entry)
#include "SDKs/EATech/include/Apt/AptLoader.h"                                  // AptCompleteAnimationAsyncLoad (LoadAnimation forwards to it)
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"                               // AptSharedPtrIncRef/DecRef/Delete (LoadAnimation refcount)
#include "SDKs/EATech/include/Apt/AptConstFile.h"                               // AptConstFile (the resolved const-file pointer type)
#include "SDKs/EATech/include/Apt/AptGC.h"                                      // AptPartialGarbageCollection / AptFlushInputQueue

#include <cstring>   // memset (zero-install the gAptFuncs slots)
#include <cstdint>   // uintptr_t / uint64_t (x64 header-field resolution)

// Dynamic-text render-data hooks: the Apt engine stores these next to the render
// callback slots and AptRenderItemDynamicText calls them directly.
extern void (*gpfnAptDrawTextRenderData)(intptr_t nZId, AptMaskRenderOperation eOp, int nTick);  // dword_8324E868
extern void (*gpfnAptReleaseTextRenderData)(intptr_t nZId, int nOp);                             // dword_8324E864
extern void (*gpAptFreeAnimationHook)(void* pDataBlock);                                        // off_1059C66C

// AptLoadAnimation @0x82B07AC8 -- the engine "load a movie onto a target path" public
// entry (LoadFlashAnimation @0x82849080 calls it with the "_level%d" path). Faithful
// body at its SDK home, SDKs/EATech/Apt/Apt.cpp; declared by Apt.h (included above).

namespace
{
    void DrawTextRenderData(intptr_t nZId, AptMaskRenderOperation eOp, int nTick)
    {
        (void)nTick;
        CgsGui::AptCallbackRender::DrawString(
            reinterpret_cast<AptAssetString>(nZId), eOp, 0);
    }

    void ReleaseTextRenderData(intptr_t nZId, int nOp)
    {
        CgsGui::AptCallbackRender::DeallocateString(
            reinterpret_cast<AptAssetString>(nZId),
            static_cast<u32>(nOp));
    }
}

// =============================================================================
// CgsGui::AptAux / CgsGui::AptAuxPointer + the Apt host user-function table.
//
// AptAuxPointer::mpAptAuxInst is the singleton handle EVERY render callback resolves through.
// AptAux::Construct @0x5C4B6C is the bring-up the GUI's view module calls; it constructs the
// embedded data + render handlers (the render-state seeds land in AptRenderHandler::Construct),
// publishes the singleton, and installs the host callback table via ConstructApt @0x5BA0F8.
//
// ConstructApt installs the gAptFuncs slots: it points each memory / debug / file / variable /
// render / deprecated slot at the matching host free function. This TU installs the RENDER +
// RENDER-FLAG slots (the family this goal reconstructs) into the REAL gAptFuncs table (the
// EATech Apt user-function table, Apt.h) -- so the Apt engine's render dispatch reaches the
// committed CgsGui::AptCallbackRender free functions (DrawRenderingUnit / SetVertexMatrix / ...).
// The remaining families (Memory / Debug / File / Variable / Custom / Deprecated) are installed
// by the same ConstructApt body but are FLAG'd below -- their host callback functions are not
// reconstructed yet.
// =============================================================================

// -----------------------------------------------------------------------------
// gAptFuncs - the single global Apt host user-function table (X360 dword_8324E818). Homed here
// (the Apt host-install TU). The ctor zero-installs every slot; ConstructApt then overrides the
// render family. Sibling Apt TUs (AptRenderingContext, the AptHook boundary) reference this same
// underlying table by name.
// -----------------------------------------------------------------------------
AptUserFunctions gAptFuncs;

// Apt.h:859 - AptUserFunctions(). The guest ctor null-installs every slot before the host
// overrides each family. Modelled as a value-init of the whole table.
AptUserFunctions::AptUserFunctions()
{
    // Every pfn slot starts null until the host install overrides it. The table is a flat run of
    // function pointers (a POD dispatch table), so a single zero-fill is the faithful "null every
    // slot" the guest ctor performs.
    std::memset(this, 0, sizeof(*this));
}

namespace CgsGui
{
    // The singleton AptAux* the render callbacks load (guest _ZN6CgsGui13AptAuxPointer12mpAptAuxInstE).
    // AptAux::Construct @0x5C4B6C publishes the constructed instance here. Starts null.
    AptAux* AptAuxPointer::mpAptAuxInst = nullptr;

    // -------------------------------------------------------------------------
    // AptAux::Construct - PS3 0x5C4B6C. Bring up the AptAux singleton.
    //
    // Faithful to the guest body:
    //   *(a1+4) = 3; *a1 = 0;                                  -- the two leading state words
    //   AptDataHandler::Construct(a1+12);                      -- the embedded data registry
    //   AptRenderHandler::Construct(a1+0x420, a2..a8);         -- the render-state seeds
    //   AptAuxPointer::mpAptAuxInst = a1;                      -- publish the singleton
    //   EA::Thread::Mutex::Init(a1+109672, ...);              -- the data-handler mutex
    //   AptAux::ConstructApt(a1);                              -- install the host callback table
    // -------------------------------------------------------------------------
    void AptAux::Construct(CgsGuiModuleIO::ImRendererSet* lpImRenderers,
                           CgsGraphics::TextRenderer* lpTextRenderer,
                           CgsLanguage::LanguageManager* lpLanguageManager,
                           const FontCollection* lpFonts,
                           f32 lfAspectRatio,
                           const rw::RGBA* lpAlternateTextColours,
                           s32 liNumAlternateColours)
    {
        // The two leading state words the guest seeds (`*(a1+4) = 3; *a1 = 0`).
        miState4 = 3;
        miState0 = 0;
        mpAptTargetInstance = nullptr;

        // The communicator ext object does not exist yet (InitializeApt's ext phase
        // creates + registers it); null until then so UpdateComponents can gate on it.
        mpAptCommunicator = nullptr;

        // Construct the embedded APT data registry/allocator front-end (guest a1+12).
        // FLAG: AptDataHandler::Construct is homed by the data-handler's own ledger TU (the DWARF
        // marks it so); its bring-up is not reconstructed in this slice. The member exists at the
        // faithful offset; its internal state is brought up by that TU when it lands.

        // Construct the embedded render handler (guest a1+0x420). This is where the render-state
        // field seeds the wave-3 slice deferred land (mpImRenderers / mpTextRenderer /
        // mpFontCollection / the stage resolution / the AptString pool). Arguments forwarded
        // straight through.
        mRenderHandler.Construct(lpImRenderers, lpTextRenderer, lpLanguageManager, lpFonts,
                                 lfAspectRatio, lpAlternateTextColours, liNumAlternateColours);

        // Publish this as the AptAux singleton the render callbacks resolve through.
        AptAuxPointer::mpAptAuxInst = this;

        // FLAG: the data-handler mutex (guest EA::Thread::Mutex::Init at a1+109672) is owned by the
        // AptDataHandler bring-up; not initialised in this slice (no concurrent data-handler access
        // exists until that TU lands).

        // Install the Apt host callback table (the render family into gAptFuncs).
        ConstructApt();
    }

    // -------------------------------------------------------------------------
    // AptAux::ConstructApt - PS3/X360 0x5BA0F8. Install the host callback table (gAptFuncs).
    //
    // The render + render-flag family (the slots this goal reconstructs) are pointed at the
    // committed CgsGui::AptCallbackRender / AptCallbackRenderFlags free functions, so the Apt
    // engine's render dispatch reaches AptRenderHandler::Render through them.
    // -------------------------------------------------------------------------
    void AptAux::ConstructApt()
    {
        // ---- the render + render-flag family (the slots this goal reconstructs) -------------
        gAptFuncs.pfnSetBackgroundColour = &AptCallbackRender::SetBackgroundColour;
        gAptFuncs.pfnAllocateString      = &AptCallbackRender::AllocateString;
        gAptFuncs.pfnDeallocateString    = &AptCallbackRender::DeallocateString;
        gAptFuncs.pfnDrawString          = &AptCallbackRender::DrawString;
        gAptFuncs.pfnLoadTexture         = &AptCallbackRender::LoadTexture;
        gAptFuncs.pfnFreeTexture         = &AptCallbackRender::FreeTexture;
        gAptFuncs.pfnBindTexture         = &AptCallbackRender::BindTexture;
        gAptFuncs.pfnLoadRenderingUnit   = &AptCallbackRender::LoadRenderingUnit;
        gAptFuncs.pfnFreeRenderingUnit   = &AptCallbackRender::FreeRenderingUnit;
        gAptFuncs.pfnSetVertexMatrix     = &AptCallbackRender::SetVertexMatrix;
        gAptFuncs.pfnSetColourTransform  = &AptCallbackRender::SetColourTransform;
        gAptFuncs.pfnDrawRenderingUnit   = &AptCallbackRender::DrawRenderingUnit;
        gAptFuncs.pfnGetStageHeight      = &AptCallbackRender::GetStageHeight;
        gAptFuncs.pfnGetStageWidth       = &AptCallbackRender::GetStageWidth;
        gAptFuncs.pfnPushRenderFlags     = &AptCallbackRenderFlags::Push;
        gAptFuncs.pfnPopRenderFlags      = &AptCallbackRenderFlags::Pop;

        // Dynamic text render-data slots live beside the host render table, but
        // AptRenderItemDynamicText reaches them directly rather than through gAptFuncs.
        gpfnAptDrawTextRenderData    = &DrawTextRenderData;
        gpfnAptReleaseTextRenderData = &ReleaseTextRenderData;

        // ---- the non-render families this TU now homes (their hosts above) -------------------
        gAptFuncs.pfnMemFree               = &AptCallbackMemory::Free;
        gAptFuncs.pfnDebugAddSavedInput    = &AptCallbackDebug::AddSavedInput;
        gAptFuncs.pfnDebugSetScreenGrabPending = &AptCallbackDebug::SetScreenGrabPending;
        gAptFuncs.pfnFreeAnimation          = &AptCallbackFile::FreeAnimation;
        gAptFuncs.pfnLoadAnimationCompleted = &AptCallbackFile::LoadAnimationCompleted;
        gAptFuncs.pfnGetBytesTotal          = &AptCallbackFile::GetBytesTotal;
        gAptFuncs.pfnGetBytesLoaded         = &AptCallbackFile::GetBytesLoaded;
        gAptFuncs.pfnOnUnload               = &AptCallbackFile::OnUnload;
        gAptFuncs.pfnSetExternVariable      = &AptCallbackVariable::SetExternVariable;
        gAptFuncs.pfnGetExternVariable      = &AptCallbackVariable::GetExternVariable;
        gAptFuncs.pfnSendVariables          = &AptCallbackDeprecated::SendVariables;
        gAptFuncs.pfnCommand                = &AptCallbackDeprecated::FsCommand;
        gAptFuncs.pfnLoadVariablesNULL      = &AptCallbackDeprecated::LoadVariablesNULL;
        gAptFuncs.pfnPointHitTest           = &AptCallbackDeprecated::PointHitTest;
        gAptFuncs.pfnGetRealTimeClock       = &AptCallbackDeprecated::GetRealTimeClock;
        gpAptFreeAnimationHook              = &AptCallbackFile::FreeAnimation;

        // FLAG: the remaining gAptFuncs slots ConstructApt @0x5BA0F8 also installs --
        //   Memory      (pfnMemAlloc / pfnMemFreeSize)
        //   Debug       (pfnAssertFail / pfnDebugPrint)
        //   File        (pfnLoadAnimation / pfnFreeConstantTable)
        //   Variable    (pfnLoadVariables)
        //   Custom      (pfnCustomControlRender / pfnCustomControlUpdate / the Zid family)
        //   Deprecated  (pfnUninitializedVarAccess / pfnCustomSavedInputHandler /
        //                pfnPlaySavedInputsDone / pfnHandleZombieState)
        // are NOT installed here: those hosts are not reconstructed yet (several depend on the
        // callbacks that do not yet have a reconstructed Apt/GUI owner. They remain null
        // (the ctor's value-init); installing them is a follow-on once those families land.
    }

    // -------------------------------------------------------------------------
    // AptAux::InitializeApt - X360 0x82848E50. The keystone Apt runtime bring-up
    // AptAux::Prepare @0x828503E0 runs (after AptDataHandler::Prepare + AptAux::Construct).
    //
    // Faithful to the X360 body: build the AptUpdateInitialize config block (v8[15] +
    // trailing bytes) and the AptCreateTargetInstance config block (v7[8]), then in ORDER:
    //   1. AptAllocatorInitialize(0x10000, 0x4000, 0x10000, 0x4000)
    //   2. updated = AptUpdateInitialize(v8, 0)
    //   3. AptRenderInitialize(updated)
    //   4. *(this+8) = AptCreateTargetInstance(v7)          [the AptAux target cache]
    //   5. AptChangeTargetInstance(that target)
    //   6. ext-object phase: AptExtObject::operator new(16); ctor(6); *p = off_820E0A20;
    //      *(this+109664) = p; AptRegisterExtension(p)      [FLAG'd -- see below]
    //
    // The v8 / v7 values are transcribed verbatim from the @0x82848E50 asm store set.
    // -------------------------------------------------------------------------
    void AptAux::InitializeApt()
    {
        // v8[15] -- the AptUpdateInitialize config block (X360 sp+0x70..; + trailing bytes
        // v9..v13 == byte[60..64], all 0 except v12 == byte[63] == 1). 17 words so byte[64]
        // (the interpreter skip-trace flag at config +0x40) is addressable.
        //   [0]0 [1]512 [2]8 [3]256 [4]0 [5]0 [6]656 [7]1 [8]384 [9]8 [10]624 [11]1024
        //   [12]128 [13]128 [14]8  + bytes {0,0,0,1,0}.
        unsigned int lauUpdateCfg[17] = {
            0u, 512u, 8u, 256u, 0u, 0u, 656u, 1u,
            384u, 8u, 624u, 1024u, 128u, 128u, 8u, 0u, 0u
        };
        reinterpret_cast<unsigned char*>(lauUpdateCfg)[63] = 1u;   // v12 (byte[63]) == 1

        // v7[8] -- the AptCreateTargetInstance config block (X360 sp+0x50..).
        //   [0]0 [1]1 [2]8 [3]8 [4]0 [5]512 [6]? [7]?  (words 6,7 are uninitialised stack
        //   in the console; seeded 0 here -- the same values the prior working bring-up used).
        unsigned int lauCreateCfg[8] = { 0u, 1u, 8u, 8u, 0u, 512u, 0u, 0u };

        // 1. AptAllocatorInitialize(0x10000, 0x4000, 0x10000, 0x4000).
        AptAllocatorInitialize(0x10000, 0x4000, 0x10000, 0x4000);

        // 2. updated = AptUpdateInitialize(v8, 0).
        int liUpdated = AptUpdateInitialize(lauUpdateCfg, 0);

        // 3. AptRenderInitialize(updated).  (the arg is carried through but unread.)
        AptRenderInitialize(liUpdated);

        // 4. *(this+8) = AptCreateTargetInstance(v7); 5. AptChangeTargetInstance(target).
        mpAptTargetInstance = AptCreateTargetInstance(lauCreateCfg);
        AptChangeTargetInstance(mpAptTargetInstance);

        // 6. The ext-object phase (UN-DEFERRED 2026-07-05): build the AS communicator
        //    extension and register it with the AS VM. The console does
        //      p = AptExtObject::operator new(16); AptExtObject(p, 6); *p = off_820E0A20;
        //      *(this+109664) = p;  AptRegisterExtension(p);
        //    off_820E0A20 is CgsGui::AptCommunicator's vtable and 6 == its native-method
        //    count, i.e. the object IS an AptCommunicator (its ctor is exactly the base
        //    ctor + vtable install -- CgsAptCommunicator.h). AptRegisterExtension
        //    @0x82AF7330 (now homed, AptExtObject.cpp) GetName()s it ("CAptCommunicator"),
        //    virtual-calls Initialize() @0x8285F0D8 (which SetFunction's the 6 sMethod_*
        //    natives + Constructs the out queue), and Sets it into the _global extension
        //    object's native hash -- whose singleton AptValueInitialize built in step 2's
        //    AptUpdateInitialize, so the registration lands on a live hash.
        mpAptCommunicator = new AptCommunicator();   // pooled AptExtObject::operator new
        AptRegisterExtension(mpAptCommunicator);
    }

    bool AptAux::Prepare(CgsMemory::HeapMalloc* lpAllocator)
    {
        switch (miState0)
        {
        case 0:
            miState0 = 1;
            // fall through
        case 1:
            if (!mAptDataHandler.Prepare(lpAllocator))
                return false;
            miState0 = 2;
            // fall through
        case 2:
            InitializeApt();
            miState0 = 3;
            // fall through
        case 3:
            miState4 = 0;
            return true;
        default:
            CGS_ASSERT(false, "CgsGUI::AptAux::Prepare() Invalid State!");
            return false;
        }
    }

    bool AptAux::Release()
    {
        switch (miState4)
        {
        case 0:
            miState4 = 1;
            // The engine shutdown chain is reconstructed separately from ownership.
            // Keep the live target intact until that chain is available.
            // fall through
        case 1:
            miState4 = 2;
            // fall through
        case 2:
            miState4 = 3;
            miState0 = 0;
            return true;
        case 3:
            miState0 = 0;
            return true;
        default:
            CGS_ASSERT(false, "CgsGUI::AptAux::Release() Invalid State!");
            return false;
        }
    }

    // The per-frame CPU monitors AptAux::Update/Render bracket their phases with
    // (X360 dword_82F33138 "AptAux - Upd Comps" / dword_82F33134 the movie-tick
    // phase / dword_82F33130 the render walk). Registered by the un-homed
    // perf-monitor setup TU; -1 handles no-op Start/StopMonitor until it lands.
    static s32 giAptAuxUpdateComponentsMonitor = -1;   // dword_82F33138
    static s32 giAptAuxUpdateTargetMonitor     = -1;   // dword_82F33134
    static s32 giAptAuxRenderMonitor           = -1;   // dword_82F33130

    // -------------------------------------------------------------------------
    // AptAux::LoadFlashAnimation - X360 0x82849080. Load a movie onto a GUI level:
    // assert the name (the console streams it -- the StrStream text collapses to the
    // plain string per project convention) and the level (< 100), format the
    // "_level%d" target path, and hand it to the engine's AptLoadAnimation.
    // -------------------------------------------------------------------------
    void AptAux::LoadFlashAnimation(const char* lpacFileName, s32 liTargetLevel)
    {
        CGS_ASSERT(lpacFileName != 0,
                   "Invalid Flash file to load in AptDataHandler::LoadFlashAnimation");
        CGS_ASSERT(liTargetLevel < 100, "Invalid level in AptAux::LoadFlashAnimation");

        char lacTargetPath[16];
        CgsCore::SPrintf(lacTargetPath, sizeof(lacTargetPath), "_level%d", liTargetLevel);
        AptLoadAnimation(lpacFileName, lacTargetPath);
    }

    // -------------------------------------------------------------------------
    // AptAux::Update - X360 0x82853B20. The per-frame Apt drive: assert Prepare
    // finished (miState0 == 3), flush the component key/value store to the movie
    // AS (UpdateComponents), then -- under the Apt mutex -- run the engine's
    // per-target frame pacer (AptUpdateTarget(mpAptTargetInstance, ms, -1, 16):
    // every depth layer, at most 16 banked frames). The X360 hard-returns 1.
    // -------------------------------------------------------------------------
    void AptAux::Update(s32 liDeltaMs)
    {
        CGS_ASSERT(miState0 == 3, "Update being called before Prepare is finished.");

        CgsDev::PerfMonCpu::StartMonitor(giAptAuxUpdateComponentsMonitor);
        UpdateComponents();
        CgsDev::PerfMonCpu::StopMonitor(giAptAuxUpdateComponentsMonitor);

        CgsDev::PerfMonCpu::StartMonitor(giAptAuxUpdateTargetMonitor);
        mAptMutex.Lock();
        AptUpdateTarget(mpAptTargetInstance, liDeltaMs, -1, 16);
        mAptMutex.Unlock();
        CgsDev::PerfMonCpu::StopMonitor(giAptAuxUpdateTargetMonitor);
    }

    // -------------------------------------------------------------------------
    // AptAux::Render - X360 0x82848FB8. The per-frame Apt render drive
    // ViewModule::RenderInternal @0x82858AF8 runs: assert Prepare finished
    // (miState0 == 3; the console streams "Render being called before Prepare is
    // finished." -- the StrStream text collapses to the plain string per project
    // convention), then walk the target's render tree: AptRenderTarget(
    // mpAptTargetInstance, liDeltaMs, -1) -- the elapsed milliseconds feed the
    // consumed render-tick bank, every layer drawn. No mutex (the walk only READS
    // the tree the update side committed). The X360 hard-returns 1.
    // -------------------------------------------------------------------------
    void AptAux::Render(s32 liDeltaMs)
    {
        CGS_ASSERT(miState0 == 3, "Render being called before Prepare is finished.");

        CgsDev::PerfMonCpu::StartMonitor(giAptAuxRenderMonitor);
        AptRenderTarget(mpAptTargetInstance, liDeltaMs, -1);
        CgsDev::PerfMonCpu::StopMonitor(giAptAuxRenderMonitor);
    }

    // -------------------------------------------------------------------------
    // AptAux::UpdateComponents - X360 0x82850570. The per-frame component flush
    // AptAux::Update @0x82853B20 runs FIRST (perfmon "AptAux - Upd Comps"), BEFORE
    // AptUpdateTarget ticks the movies; XB1 sub_1400C7090 preserves the same order.
    // Assert the communicator ext object (the console fires "Invalid pointer to apt
    // communicator" at CgsAptAux.cpp:660), then flush via UpdateAllComponents.
    // -------------------------------------------------------------------------
    void AptAux::UpdateComponents()
    {
        CGS_ASSERT(mpAptCommunicator != nullptr,
                   "Invalid pointer to apt communicator");
        if (mpAptCommunicator != nullptr)
            mpAptCommunicator->UpdateAllComponents();
    }

    // -------------------------------------------------------------------------
    // AptAux::UpdateFlashComponent - X360 0x82853C28 (not exported; semantics pinned
    // by the XB1 x64 arbiter, which INLINES this hop at every call site to
    // AptCommunicator::UpdateComponent(componentName, key, value) -- e.g. XB1
    // sub_1401AF470's UpdateComponent(this, this->macName[+8], "SignName", value)).
    // Mirror the (key, value) pair onto the named component's key/value store; the
    // per-frame UpdateComponents flush then pushes it to the movie AS ("UpdateAll").
    // lbImmediate is the console's queued-vs-immediate hint; both orderings land
    // before the same frame's flush on the single-threaded host (see the header
    // note; the Res/NRes timer-format split is off the boot flow).
    // -------------------------------------------------------------------------
    void AptAux::UpdateFlashComponent(AptAux* lpAptAux, const char* lpacAptName,
                                      const char* lpacViewState, const char* lpacParam,
                                      bool /*lbImmediate*/)
    {
        CGS_ASSERT(lpAptAux != nullptr, "Invalid AptAux in AptAux::UpdateFlashComponent");
        if (lpAptAux == nullptr || lpAptAux->mpAptCommunicator == nullptr)
            return;
        lpAptAux->mpAptCommunicator->UpdateComponent(lpacAptName, lpacViewState, lpacParam);
    }

    // =========================================================================
    // The non-render Apt host callback families.
    //
    // AptCallbackMemory::Free is the one trivial REAL callback (forwards a free through the
    // singleton's embedded data-handler allocator). The remaining Debug / File / Variable /
    // Deprecated callbacks are the build's guarded not-yet-implemented entry points: each
    // fires the not-implemented assert and returns a null/zero result -- that assert-and-
    // return is the faithful body the X360 binary executes (NOT a reconstruction stub).
    // =========================================================================

    // ---- Memory -------------------------------------------------------------
    // X360 0x828492A8 (CgsGui::AptCallbackMemory::Free). Free an Apt allocation through the
    // singleton AptAux's embedded data-handler allocator. The guest loads off_8305A6C8
    // (mpAptAuxInst), adds 12 to reach &mAptDataHandler, asserts the singleton is live, then
    // calls AptDataHandler::AptFree(&mAptDataHandler, a1).
    void AptCallbackMemory::Free(void* lpBlock)
    {
        AptAux* lpAptAux = AptAuxPointer::mpAptAuxInst;
        CGS_ASSERT(lpAptAux != nullptr, "Invalid AptDataHandler in AptCallbackMemory::Free");
        lpAptAux->mAptDataHandler.AptFree(lpBlock);
    }

    // ---- Debug --------------------------------------------------------------
    // X360 0x82849528 (CgsGui::AptCallbackDebug::AddSavedInput). Guarded not-yet-implemented.
    void AptCallbackDebug::AddSavedInput(AptSavedInputRecord* /*lpRecord*/, s32 /*liCount*/)
    {
        CGS_ASSERT(false, "AptCallbackDebug::AddSavedInput() has not been implemented but is being used, please implement before utilising.");
    }

    // X360 0x82849568 (CgsGui::AptCallbackDebug::SetScreenGrabPending). Guarded not-yet-implemented.
    void AptCallbackDebug::SetScreenGrabPending(const char* /*lpacName*/)
    {
        CGS_ASSERT(false, "AptCallbackDebug::SetScreenGrabPending() has not been implemented but is being used, please implement before utilising.");
    }

    // ---- File ---------------------------------------------------------------
    // X360 0x828495E0 (CgsGui::AptCallbackFile::GetBytesTotal). Guarded not-yet-implemented.
    s32 AptCallbackFile::GetBytesTotal(const char* /*lpacFileName*/, AptGetBytesEnum /*leWhich*/)
    {
        CGS_ASSERT(false, "AptCallbackFile::GetBytesTotal() has not been implemented but is being used, please implement before utilising.");
        return 0;
    }

    // X360 0x82849620 (CgsGui::AptCallbackFile::GetBytesLoaded). Guarded not-yet-implemented.
    s32 AptCallbackFile::GetBytesLoaded(const char* /*lpacFileName*/, AptGetBytesEnum /*leWhich*/)
    {
        CGS_ASSERT(false, "AptCallbackFile::GetBytesLoaded() has not been implemented but is being used, please implement before utilising.");
        return 0;
    }

    // X360 0x828495A8 (CgsGui::AptCallbackFile::FreeAnimation). The whole body is
    // `li r11, 1; stw r11, 0x14(r3); blr`: mark the AptDataHeader state as loaded/free.
    void AptCallbackFile::FreeAnimation(void* lpDataBlock)
    {
        static_cast<AptDataHeader*>(lpDataBlock)->meCurrentState =
            AptDataHeader::E_APTDATASTATE_LOADED;
    }

    // X360 0x828540E0 (CgsGui::AptCallbackFile::OnUnload). Assert the AptAux singleton
    // exists, then remove the unloading movieclip from the communicator component table.
    void AptCallbackFile::OnUnload(AptValue* lpValue)
    {
        CGS_ASSERT(AptAuxPointer::mpAptAuxInst != nullptr, "AptAuxPointer::mpAptAuxInst");
        AptCommunicator::RemoveExpiredAptComponent(lpValue);
    }

    // X360 0x828495B8 (CgsGui::AptCallbackFile::LoadAnimationCompleted). The
    // whole body is two calls: AptPartialGarbageCollection(); AptFlushInputQueue().
    void AptCallbackFile::LoadAnimationCompleted(const char* /*lpacBaseName*/,
                                                 const char* /*lpacTargetName*/)
    {
        AptPartialGarbageCollection();
        AptFlushInputQueue();
    }

    // The AptData resource span (base + size) LoadAnimation derives from the registered
    // header before driving the completion, so the native-8 AptMovie::resolve64 relocation
    // walk can bounds-check every serialised offset slot (see the header note). Zero => the
    // walk treats every non-zero slot as a live offset (the pre-bounds behaviour). base ==
    // the load base == the AptData resource base (the header sits at the resource start).
    uintptr_t gAptResourceSpanBase = 0;
    uint32_t  gAptResourceSpanSize = 0;

    // =========================================================================
    // AptCallbackFile::LoadAnimation @0x82853E68 -- the host "load this .apt now" callback.
    // DECOMPILED FAITHFULLY from BURNOUT_X360_ARTIST.XEX (the asserts name this file at
    // lines 830/833/838). The X360 body (a1 = name, a2 = &handle):
    //   assert(mpAptAuxInst valid);                                    // "Invalid AptDataHandler..."
    //   AptData = FindAptData(&mAptDataHandler, a1); assert(AptData);  // "Could not locate AptDataHeader..."
    //   assert(AptData[5] == 1 || AptData[5] == 2);                    // "File not in a state..."
    //   v24 = *a2; if (v24) IncRef(v24);                              // pin the handle
    //   AptCompleteAnimationAsyncLoad(&v24, AptData[1], AptData[2], AptData);
    //   AptData[5] = 2;                                               // state -> ACTIVE
    //   r = *a2; *a2 = 0; if (r && --r.count == 0) AptSharedPtrDelete(r);   // drop the original
    //
    // AptData[1]/AptData[2] are the header's mpAptData / mpConstData; AptData (the header)
    // is threaded as a5 all the way to Fixup (LoadRenderingUnit reads AptData+12 = geometry).
    //
    // FLAG (x64 fork): the console's mpAptData/mpConstData are relocated 32-bit pointers; our
    // converted header keeps them as raw file OFFSETS in 8-byte fields (the no-op FixUp -- see
    // CgsAptDataHeader.cpp). The resource base == the header address (the header sits at the
    // resource start), so they resolve as headerAddr + offset. The reloc base passed as pBase
    // is mpConstData (== the "Apt Data:1:7:8" aptDataOffset the Fixup relocates against) -- the
    // converted bundle collapses pBase and pConstFile to that one address (verified), unlike the
    // console where pBase == mpAptData. The a2-by-reference form matches the asm's `*a2` reads.
    // =========================================================================
    void AptCallbackFile::LoadAnimation(const char* lpacName, AptFilePtr* lpHandle)
    {
        AptAux* lpAptAux = AptAuxPointer::mpAptAuxInst;
        CGS_ASSERT(lpAptAux != nullptr, "Invalid AptDataHandler in AptCallbackFile::LoadAnimation");

        AptDataHeader* lpAptData = lpAptAux->mAptDataHandler.FindAptData(lpacName);
        CGS_ASSERT(lpAptData != nullptr,
                   "Could not locate AptDataHeader in AptCallbackFile::LoadAnimation Check file has been loaded");
        if (lpAptData == nullptr)
            return;

        // FLAG (x64 native-8 bundle): the console asserts AptData[5] (meCurrentState) is
        // LOADED(1)/ACTIVE(2) and then sets it to ACTIVE(2). The native-8 header is 8-byte-widened,
        // so meCurrentState does NOT sit at the u32-struct's +20 (that overlaps mpConstData's high
        // half at +16); its real offset is ambiguous in the widened layout. The state precondition
        // is moot here anyway -- the host loads the .apt SYNCHRONOUSLY before this call, so it is
        // always fully loaded, and the host's own load-once guard replaces the console's state=2
        // re-load gate. So the on-disk state read/write is skipped (not asserted / not written) to
        // avoid corrupting the widened mpConstData field. The rest of the control flow is faithful.

        // Resolve the header's serialised offset fields to real pointers (x64 substitute for the
        // console's relocated mpAptData/mpConstData -- FLAG). The header sits at the resource base,
        // so base == the header address.
        //
        // UN-COLLAPSED (2026-07-01): the native-8 (libapt2 SerializeChunks) header carries SIX
        // 8-byte fields [name@0, baseName@8, aptData@0x10, const@0x18, geom@0x20, size@0x28] -- the
        // extra baseName shifts every field one slot past the console's [name, aptData, const, ...]
        // order (FLAG: native-8 header field order, see APT_CONVERTER_BUGS.md #2). pBase = the
        // "Apt Data:1:7:8" chunk (every serialised offset is chunk-relative), pConstFile = the
        // "Apt constant file" chunk (movieOffset@+0x18 locates the root; itemStart@+0x28 is the
        // constant-record table _parseStream resolves Push/DefineDictionary entries through) --
        // exactly the console's AptData[1]/AptData[2] pair and the XB1 CompleteLoad's a3/a4 pair.
        const uintptr_t luHeaderBase  = reinterpret_cast<uintptr_t>(lpAptData);
        const uintptr_t luAptDataOff  = static_cast<uintptr_t>(
            *reinterpret_cast<const uint64_t*>(luHeaderBase + 0x10u));  // serialized .apt header: aptData off @+0x10
        const uintptr_t luConstOff    = static_cast<uintptr_t>(
            *reinterpret_cast<const uint64_t*>(luHeaderBase + 0x18u));  // serialized .apt header: const off @+0x18
        void* lpBase      = reinterpret_cast<void*>(luHeaderBase + luAptDataOff);
        void* lpConstFile = (luConstOff != 0)
                                ? reinterpret_cast<void*>(luHeaderBase + luConstOff)
                                : lpBase;   // degenerate header: keep the old collapse

        // Publish the resource span for the native-8 relocation walk's bounds checks
        // (FLAG x64; see the gAptResourceSpan* note): base = the header (== the resource
        // start), size = the converted 6-field header's size slot @+0x28. Restored after
        // the completion so nested loads (the import graph) re-derive their own span.
        const uintptr_t luPrevSpanBase = gAptResourceSpanBase;
        const uint32_t  luPrevSpanSize = gAptResourceSpanSize;
        gAptResourceSpanBase = luHeaderBase;
        gAptResourceSpanSize = static_cast<uint32_t>(
            *reinterpret_cast<const uint64_t*>(luHeaderBase + 0x28u));  // serialized .apt header: size @+0x28

        // Pin the caller's handle (the asm's leading lwarx/stwcx. IncRef on *a2).
        AptFilePtr laHandle;
        laHandle.pData = lpHandle->pData;
        if (laHandle.pData)
            AptSharedPtrIncRef(laHandle.pData);

        // Forward to the async-completion glue (-> AptLoader::CompleteLoad -> Resolve -> Fixup).
        AptCompleteAnimationAsyncLoad(&laHandle, lpBase,
                                      reinterpret_cast<AptConstFile*>(lpConstFile),
                                      /*pAptDataHeader (a5)*/ lpAptData);

        gAptResourceSpanBase = luPrevSpanBase;
        gAptResourceSpanSize = luPrevSpanSize;

        // (The console then sets AptData[5] = 2 (ACTIVE). FLAG-skipped -- see the state note above:
        // the widened header's state slot is ambiguous and the host's load-once guard covers it.)

        // Drop the caller's original handle reference (the asm's trailing DecRef + null on *a2).
        AptFile* lpConsumed = lpHandle->pData;
        lpHandle->pData = nullptr;
        if (lpConsumed && AptSharedPtrDecRef(lpConsumed) == 0)
            AptSharedPtrDelete(lpConsumed);
    }

    // ---- Variable -----------------------------------------------------------
    // X360 0x82849660 (CgsGui::AptCallbackVariable::SetExternVariable). Guarded not-yet-implemented.
    void AptCallbackVariable::SetExternVariable(const char* /*lpacName*/, const char* /*lpacValue*/)
    {
        CGS_ASSERT(false, "AptCallbackVariable::SetExternVariable() has not been implemented but is being used, please implement before utilising.");
    }

    // X360 0x828496A0 (CgsGui::AptCallbackVariable::GetExternVariable). Guarded not-yet-implemented.
    AptValue* AptCallbackVariable::GetExternVariable(const char* /*lpacName*/)
    {
        CGS_ASSERT(false, "AptCallbackVariable::GetExternVariable() has not been implemented but is being used, please implement before utilising.");
        return nullptr;
    }

    // ---- Deprecated ---------------------------------------------------------
    // X360 0x828496E0 (CgsGui::AptCallbackDeprecated::SendVariables). Guarded not-yet-implemented.
    void AptCallbackDeprecated::SendVariables(const char* /*lpacUrl*/, const char* /*lpacTarget*/,
                                              const char* /*lpacVariables*/, const char* /*lpacMethod*/,
                                              s32 /*liFlags*/)
    {
        CGS_ASSERT(false, "SendVariables() has not been implemented but is being used, please implement before utilising.");
    }

    // X360 0x82849720 (CgsGui::AptCallbackDeprecated::FsCommand). Guarded not-yet-implemented.
    void AptCallbackDeprecated::FsCommand(const char* /*lpacCommand*/, const char* /*lpacArgs*/)
    {
        CGS_ASSERT(false, "Command() has not been implemented but is being used, please implement before utilising.");
    }

    // X360 0x828497A0 (CgsGui::AptCallbackDeprecated::LoadVariablesNULL). Guarded not-yet-implemented.
    AptValue* AptCallbackDeprecated::LoadVariablesNULL()
    {
        CGS_ASSERT(false, "LoadVariablesNULL() has not been implemented but is being used, please implement before utilising.");
        return nullptr;
    }

    // X360 0x828497E0 (CgsGui::AptCallbackDeprecated::PointHitTest). Guarded not-yet-implemented.
    s32 AptCallbackDeprecated::PointHitTest(f32 /*lfX*/, f32 /*lfY*/, AptAssetMoiveClip /*leClip*/)
    {
        CGS_ASSERT(false, "PointHitTest() has not been implemented but is being used, please implement before utilising.");
        return 0;
    }

    // X360 0x82849820 (CgsGui::AptCallbackDeprecated::GetRealTimeClock). Guarded not-yet-implemented.
    void AptCallbackDeprecated::GetRealTimeClock(AptSysClock* /*lpSysClock*/, bool /*lbUseUtc*/)
    {
        CGS_ASSERT(false, "GetRealTimeClock() has not been implemented but is being used, please implement before utilising.");
    }

    // The language-manager accessor CgsAptString::Prepare (CgsAptString.cpp) reaches the manager
    // through. The console Prepare loads the AptAux singleton (off_8305A6C8 == AptAuxPointer::
    // mpAptAuxInst), adds 0x420 to reach the embedded render handler, and reads mpLanguageManager
    // at +99776. This TU owns the full AptAux / AptRenderHandler layout, so it can return the
    // manager by name; the real-CgsAptString TU cannot include those types (the opaque pool
    // CgsAptString stand-in clashes), hence this out-of-line bridge. Asserts mirror the console's.
    CgsLanguage::LanguageManager* GetAptRenderHandlerLanguageManager()
    {
        AptAux* lpAptAux = AptAuxPointer::mpAptAuxInst;
        CGS_ASSERT(lpAptAux != 0, "Invalid Apt Aux instance in AptCallbackRender::AllocateString");
        AptRenderHandler* lpRenderHandler = &lpAptAux->mRenderHandler;
        CGS_ASSERT(lpRenderHandler != 0, "Invalid render handler instance in AptCallbackRender::AllocateString");
        CgsLanguage::LanguageManager* lpLanguage = lpRenderHandler->GetLanguageManager();
        CGS_ASSERT(lpLanguage != 0, "Invalid language manager in CgsAptString::Prepare");
        return lpLanguage;
    }
}

// BrnFlapt::TextFieldRef::GetLanguageManager @ 0x8246CB40 -- the same
// singleton -> render-handler -> language-manager walk with the Ref's own baked
// asserts (BrnFlaptTextFieldRef.cpp:47/48; `this` is never touched). DWARF home is
// BrnFlaptTextFieldRef.cpp, but that TU must see the REAL CgsAptString (through the
// text-field instance it drives) which clashes with the render handler's opaque
// pool stand-in -- so the body lives here with the rest of the AptAux layout users
// (same rationale as the GetAptRenderHandlerLanguageManager bridge above).
namespace BrnFlapt
{
    CgsLanguage::LanguageManager* TextFieldRef::GetLanguageManager()
    {
        CgsGui::AptRenderHandler* lpRenderHandler =
            &CgsGui::AptAuxPointer::mpAptAuxInst->mRenderHandler;
        CGS_ASSERT(lpRenderHandler != 0, "lpRenderHandler");
        CGS_ASSERT(lpRenderHandler->GetLanguageManager() != 0, "lpRenderHandler->mpLanguageManager");
        return lpRenderHandler->GetLanguageManager();
    }
}
