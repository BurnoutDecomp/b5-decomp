#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCallbackRender.h"  // the render + render-flag callback family
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h"       // CgsGui::AptDataHeader (LoadAnimation's FindAptData result)
#include "SDKs/EATech/include/Apt/Apt.h"                                         // AptUserFunctions gAptFuncs (the host table)
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT (the host-callback asserts)

#include "SDKs/EATech/Apt/AptInit.h"                                            // Apt bring-up entry points (InitializeApt callees)
#include "SDKs/EATech/include/Apt/AptTarget.h"                                  // AptCreateTargetInstance / AptChangeTargetInstance
#include "SDKs/EATech/include/Apt/AptLoader.h"                                  // AptCompleteAnimationAsyncLoad (LoadAnimation forwards to it)
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"                               // AptSharedPtrIncRef/DecRef/Delete (LoadAnimation refcount)
#include "SDKs/EATech/include/Apt/AptConstFile.h"                               // AptConstFile (the resolved const-file pointer type)

#include <cstring>   // memset (zero-install the gAptFuncs slots)
#include <cstdint>   // uintptr_t / uint64_t (x64 header-field resolution)

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

        // ---- the non-render families this TU now homes (their hosts above) -------------------
        gAptFuncs.pfnMemFree               = &AptCallbackMemory::Free;
        gAptFuncs.pfnDebugAddSavedInput    = &AptCallbackDebug::AddSavedInput;
        gAptFuncs.pfnDebugSetScreenGrabPending = &AptCallbackDebug::SetScreenGrabPending;
        gAptFuncs.pfnGetBytesTotal         = &AptCallbackFile::GetBytesTotal;
        gAptFuncs.pfnGetBytesLoaded        = &AptCallbackFile::GetBytesLoaded;
        gAptFuncs.pfnSetExternVariable     = &AptCallbackVariable::SetExternVariable;
        gAptFuncs.pfnGetExternVariable     = &AptCallbackVariable::GetExternVariable;
        gAptFuncs.pfnSendVariables         = &AptCallbackDeprecated::SendVariables;
        gAptFuncs.pfnCommand               = &AptCallbackDeprecated::FsCommand;
        gAptFuncs.pfnLoadVariablesNULL     = &AptCallbackDeprecated::LoadVariablesNULL;
        gAptFuncs.pfnPointHitTest          = &AptCallbackDeprecated::PointHitTest;
        gAptFuncs.pfnGetRealTimeClock      = &AptCallbackDeprecated::GetRealTimeClock;

        // FLAG: the remaining gAptFuncs slots ConstructApt @0x5BA0F8 also installs --
        //   Memory      (pfnMemAlloc / pfnMemFreeSize)
        //   Debug       (pfnAssertFail / pfnDebugPrint)
        //   File        (pfnLoadAnimation / pfnFreeAnimation / pfnFreeConstantTable /
        //                pfnLoadAnimationCompleted / pfnOnUnload)
        //   Variable    (pfnLoadVariables)
        //   Custom      (pfnCustomControlRender / pfnCustomControlUpdate / the Zid family)
        //   Deprecated  (pfnUninitializedVarAccess / pfnCustomSavedInputHandler /
        //                pfnPlaySavedInputsDone / pfnHandleZombieState)
        // are NOT installed here: those hosts are not reconstructed yet (several depend on the
        // undeclared Apt C-API -- AptLoadAnimation / AptPartialGarbageCollection / ... -- or on
        // CgsGui::AptCommunicator, neither of which has a reconstructed home). They remain null
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
        AptTarget* lpTarget = AptCreateTargetInstance(lauCreateCfg);
        // FLAG (partial AptAux slice): the console caches the created target at *(this+8);
        // this AptAux slice models only mAptDataHandler/mRenderHandler (the +8 word overlaps
        // mAptDataHandler), so the cache store is OMITTED. The target is globally reachable
        // via GetTarget() (AptChangeTargetInstance publishes it into gpAptTarget + the TLS
        // mirror), which is what every downstream reader actually uses -- so the observable
        // "current context" is set faithfully without corrupting the partial-slice AptAux.
        AptChangeTargetInstance(lpTarget);

        // 6. FLAG (ext-object phase deferred): the console then builds the AS extension object
        //    (AptExtObject::operator new(16); AptExtObject(6); *p = off_820E0A20 [its vtable];
        //    *(this+109664) = p) and registers it via AptRegisterExtension(p). AptRegister-
        //    Extension @0x82AF7330 dereferences off_8324E37C (== gpGlobalExtensionObject) +8
        //    to reach the AS extension object's native hash -- but gpGlobalExtensionObject is
        //    built by AptValueInitialize @0x82B02800, which is FLAG-deferred (its ~15 value
        //    singletons have protected reconstruction ctors; see AptInit.cpp). With the
        //    extension object null, AptRegisterExtension would null-deref, and the +109664
        //    cache member is outside this AptAux slice. So the whole ext-object phase is
        //    OMITTED here; it comes online with AptValueInitialize. (The runtime is otherwise
        //    fully bootstrapped: allocators + update + render + the live AptTarget context.)
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

    // =========================================================================
    // FLAG (x64 converted 8-byte bundle): the host bring-up pre-locates the movie-root
    // CHARACTER HEADER (0x09876543 signature scan -- it has the resource size the scan
    // needs) and stashes it here before calling LoadAnimation. The console reads the root
    // via pConstFile->mnDataRootOffset, but our converted .apt's dataRootOffset does not
    // locate the type-9 movie root (its console layout diverged). When null, CompleteLoad
    // falls back to the faithful console formula. Reset to null by the host after each load.
    // =========================================================================
    void* gAptLoadAnimRootOverride = nullptr;   // the located root char header (x64)

    // The AptData resource span (base + size) the host stashes before LoadAnimation, so the
    // native-8 AptMovie::resolve64 relocation walk can bounds-check every serialised offset
    // slot (see the header). Zero => the walk treats every non-zero slot as a live offset
    // (the pre-bounds behaviour). base == the load base == the AptData resource base.
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

        // FLAG (x64 converted 8-byte bundle): the console asserts AptData[5] (meCurrentState) is
        // LOADED(1)/ACTIVE(2) and then sets it to ACTIVE(2). Our converted header is 8-byte-widened,
        // so meCurrentState does NOT sit at the u32-struct's +20 (that overlaps mpConstData's high
        // half at +16); its real offset is ambiguous in the converted layout. The state precondition
        // is moot here anyway -- the host loads the .apt SYNCHRONOUSLY before this call, so it is
        // always fully loaded, and the host's own load-once guard replaces the console's state=2
        // re-load gate. So the on-disk state read/write is skipped (not asserted / not written) to
        // avoid corrupting the widened mpConstData field. The rest of the control flow is faithful.

        // Resolve the header's serialised offset fields to real pointers (x64 substitute for the
        // console's relocated mpAptData/mpConstData -- FLAG). The header sits at the resource base,
        // so base == the header address.
        //
        // UN-COLLAPSED (2026-07-01): the converted (libapt2 SerializeChunks) header carries SIX
        // 8-byte fields [name@0, baseName@8, aptData@0x10, const@0x18, geom@0x20, size@0x28] -- the
        // extra baseName shifts every field one slot past the console's [name, aptData, const, ...]
        // order (FLAG: converter-format accommodation, see APT_CONVERTER_BUGS.md #2). pBase = the
        // "Apt Data:1:7:8" chunk (every serialised offset is chunk-relative), pConstFile = the
        // "Apt constant file" chunk (movieOffset@+0x18 locates the root; itemStart@+0x28 is the
        // constant-record table _parseStream resolves Push/DefineDictionary entries through) --
        // exactly the console's AptData[1]/AptData[2] pair and the XB1 CompleteLoad's a3/a4 pair.
        const uintptr_t luHeaderBase  = reinterpret_cast<uintptr_t>(lpAptData);
        const uintptr_t luAptDataOff  = static_cast<uintptr_t>(
            *reinterpret_cast<const uint64_t*>(luHeaderBase + 0x10u));  // aptData (u64 @ +0x10)
        const uintptr_t luConstOff    = static_cast<uintptr_t>(
            *reinterpret_cast<const uint64_t*>(luHeaderBase + 0x18u));  // const   (u64 @ +0x18)
        void* lpBase      = reinterpret_cast<void*>(luHeaderBase + luAptDataOff);
        void* lpConstFile = (luConstOff != 0)
                                ? reinterpret_cast<void*>(luHeaderBase + luConstOff)
                                : lpBase;   // degenerate header: keep the old collapse

        // Pin the caller's handle (the asm's leading lwarx/stwcx. IncRef on *a2).
        AptFilePtr laHandle;
        laHandle.pData = lpHandle->pData;
        if (laHandle.pData)
            AptSharedPtrIncRef(laHandle.pData);

        // Forward to the async-completion glue (-> AptLoader::CompleteLoad -> Resolve -> Fixup).
        AptCompleteAnimationAsyncLoad(&laHandle, lpBase,
                                      reinterpret_cast<AptConstFile*>(lpConstFile),
                                      /*pAptDataHeader (a5)*/ lpAptData,
                                      /*pPreResolvedRoot (x64 FLAG)*/ gAptLoadAnimRootOverride);

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
