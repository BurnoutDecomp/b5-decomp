#include "GameSource/Gui/BrnAptRuntimeBringUp.h"

#include <cstdio>    // std::snprintf (probe logging)
#include <cstring>   // std::strncpy

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::WriteToLog

// ---- the Apt host adaptor + render handler (the render bridge) --------------
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"          // CgsGui::AptAux / AptAuxPointer
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"// CgsGui::AptImRendererSet / AptIm2dRenderBuffer
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"                       // CgsGuiModuleIO::ImRendererSet

// ---- the Apt engine leaves that ARE bodied (the bring-up drives these) ------
#include "SDKs/EATech/Apt/DogmaAllocator.h"                   // DOGMA_PoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"            // AptValueGC_PoolManager
#include "SDKs/EATech/include/Apt/AptDefine.h"                // gpGCPoolManager / gpNonGCPoolManager
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"     // AptActionInterpreter + AptInitParmsT
#include "SDKs/EATech/include/Apt/AptTarget.h"                // AptTarget + gpAptTarget singletons
#include "SDKs/EATech/include/Apt/AptString/EAString.h"       // EAStringC (loader file name)
#include "SDKs/EATech/include/Apt/AptLoader.h"                // AptLoader / GetTarget / AptFilePtr

// ---- the D3D9 2D immediate render buffer the Apt rasteriser fills + we flush --
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
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h" // CgsResource::GuiGeometryObject / File

// ---- STEP 1 FAITHFUL INSTANTIATION (gated; the direct-geometry render stays the fallback) ----
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"    // AptCharacterAnimation::Fixup (FixupTranscode/InPlace)
#include "SDKs/EATech/include/Apt/AptConstFile.h"             // AptConstFile (pointer-size dispatch)
#include "SDKs/EATech/include/Apt/AptFile.h"                  // AptFile (synthesised loaded-file handle for instantiation)
#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"       // AptGetAnimationAtLevel (the root level-0 CIH)
#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"// MakeCharacterAnimationInst (loaded movie -> live inst)
#include "SDKs/EATech/include/Apt/AptCIH.h"                   // AptCIH (the root display-object node)
#include "SDKs/EATech/include/Apt/AptMovie.h"                 // gAptCmd8 / gAptCmd8Bound* / gAptPlacedNodes (native-8 place gates)
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h" // mDisplayList (the root CIH's child display list)
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"       // AptAnimationTarget::GetRootDisplayList (the director's root list)
#include "SDKs/EATech/include/Apt/AptDisplayList.h"           // AptDisplayList::AsState
#include "SDKs/EATech/include/Apt/AptDisplayListState.h"      // AptDisplayListState::mpFirst (the placed-node chain)
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"         // AptCharacterInst::GetRenderItem/GetDepth (probe)

// Minimal Win32 VirtualAlloc/VirtualFree decls (NOT #include <windows.h> -- that pollutes the
// namespace with min/max/Render/etc. macros that clash with the EA/Cgs/rw headers above). We need
// a LOW-4GB buffer for FixupTranscode's in-place 32-bit Reloc32 (the codebase has no low allocator;
// VirtualAlloc with a fixed lpAddress below 0x100000000 is the only way -- see the low-mem analysis).
extern "C" __declspec(dllimport) void* __stdcall VirtualAlloc(void* lpAddress, size_t dwSize,
                                                              unsigned long flAllocationType,
                                                              unsigned long flProtect);
extern "C" __declspec(dllimport) int   __stdcall VirtualFree(void* lpAddress, size_t dwSize,
                                                             unsigned long dwFreeType);
#define BRNAPT_MEM_COMMIT    0x1000u
#define BRNAPT_MEM_RESERVE   0x2000u
#define BRNAPT_MEM_RELEASE   0x8000u
#define BRNAPT_PAGE_READWRITE 0x04u

// The per-character FixupWalk probe sink the engine TU calls (declared in AptCharacterAnimation.h).
// Logs "[AptRT] fixup: char[i] type=T @off=.." for the first N characters so a run names the exact
// character index + type + record offset reached before any (now-guarded) skip.
void CgsApt_FixupProbe(int nCharIndex, int nType, long long nCharOffset)
{
    char lac[96];
    std::snprintf(lac, sizeof(lac), "[AptRT] fixup: char[%d] type=%d @off=0x%llX\n",
                  nCharIndex, nType, static_cast<unsigned long long>(nCharOffset));
    CgsDev::Log::WriteToLog(lac);
}

// The AptGetAnimationAtLevel step-probe sink (declared in AptCharacterHelper.cpp). Names each step +
// pointer so a single run shows the exact line reached before any AV inside the homed function. This
// strong definition overrides the weak CgsApt_GalProbeDefault no-op in that TU.
extern "C" void CgsApt_GalProbe(const char* pcStep, const void* p)
{
    char lac[128];
    std::snprintf(lac, sizeof(lac), "[AptRT] gal: %s = %p\n", pcStep ? pcStep : "?", p);
    CgsDev::Log::WriteToLog(lac);
}

// The per-tick step-probe sink (declared in AptCIH.cpp). Throttled to the first ~120 calls (a handful
// of ticks) so it pinpoints the early-deref AV without flooding the per-frame log. Strong def overrides
// the weak default in AptCIH.cpp.
extern "C" void CgsApt_TickProbe(const char* pcStep, const void* p)
{
    static int s_iTickProbeBudget = 0;
    if (s_iTickProbeBudget >= 120)
        return;
    ++s_iTickProbeBudget;
    char lac[128];
    std::snprintf(lac, sizeof(lac), "[AptRT] tick: %s = %p\n", pcStep ? pcStep : "?", p);
    CgsDev::Log::WriteToLog(lac);
}

// The render-item-creation probe sink (declared in AptCharacterInst.cpp): logs each AptCharacterInst's
// render-item creation -- the char inst, the character's type, the character, the created render item,
// and the item's stored mpCharacter. Throttled. Confirms Manager_CreateItem ran + set mpCharacter.
extern "C" void CgsApt_MkItemProbe(const void* pCharInst, int nCharType, const void* pCharacter,
                                   const void* pRenderItem, const void* pItemCharacter)
{
    static int s_iMkItemBudget = 0;
    if (s_iMkItemBudget >= 32)
        return;
    ++s_iMkItemBudget;
    char lac[200];
    std::snprintf(lac, sizeof(lac),
        "[AptRT] mkitem: charInst=%p type=%d character=%p -> renderItem=%p (item.mpCharacter=%p)\n",
        pCharInst, nCharType, pCharacter, pRenderItem, pItemCharacter);
    CgsDev::Log::WriteToLog(lac);
}

// The char-list probe sink (declared in AptCharacterAnimationInst.cpp): logs the embedded movie's
// AptCharacterAnimation character-table head/count just before MakeCharacterAnimationInst would walk it
// in IncCharacterList, plus whether the skip gate is engaged. On the native-8 path the table pointer is
// un-relocated (a serialized offset) -- this is the exact data IncCharacterList would AV on. Throttled.
extern "C" void CgsApt_CharListProbe(const void* pAnim, const void* pTable, int nCount, unsigned int uSkip)
{
    static int s_iCharListBudget = 0;
    if (s_iCharListBudget >= 32)
        return;
    ++s_iCharListBudget;
    char lac[200];
    std::snprintf(lac, sizeof(lac),
        "[AptRT] charlist: anim=%p table=%p count=%d skip=%u %s\n",
        pAnim, pTable, nCount, uSkip,
        uSkip ? "(SKIPPED -- un-relocated native-8 table)" : "(walking)");
    CgsDev::Log::WriteToLog(lac);
}

// The native-8 timeline-relocation probe sink (declared in AptCharacterAnimation.h): logs each embedded
// movie's relocated frameCount + frameTable + the count of command records relocated. Throttled.
void CgsApt_TimelineProbe(const void* pMovie, int nFrameCount, const void* pFrameTable, int nCmdsTotal)
{
    static int s_iTimelineBudget = 0;
    if (s_iTimelineBudget >= 32)
        return;
    ++s_iTimelineBudget;
    char lac[176];
    std::snprintf(lac, sizeof(lac),
        "[AptRT] timeline: movie=%p frameCount=%d frameTable=%p commands=%d\n",
        pMovie, nFrameCount, pFrameTable, nCmdsTotal);
    CgsDev::Log::WriteToLog(lac);
}

// Per-frame (frame 0) timeline probe sink (declared in AptCharacterAnimation.h): the frame's command
// count + mpCommands pointer before/after relocation (offset -> in-resource pointer). Throttled.
void CgsApt_TimelineFrameProbe(int nFrame, int nCmdCount, unsigned long long luCmdsBefore,
                              unsigned long long luCmdsAfter)
{
    static int s_iTLFrameBudget = 0;
    if (s_iTLFrameBudget >= 16)
        return;
    ++s_iTLFrameBudget;
    char lac[176];
    std::snprintf(lac, sizeof(lac),
        "[AptRT] timeline: frame=%d cmdCount=%d mpCommands(before)=0x%llX (after)=0x%llX\n",
        nFrame, nCmdCount, luCmdsBefore, luCmdsAfter);
    CgsDev::Log::WriteToLog(lac);
}

// The native-8 place-command probe sink (declared in AptMovie.h): logs each PlaceObject(3)/RemoveObject(4)
// command the native-8 place path processed -- its char id, depth, flags, matrix (first 6 floats), and
// the placed node pointer (null = skipped). Throttled.
extern "C" void CgsApt_PlaceProbe(int nTag, int nCharId, int nDepth, unsigned int nFlags,
                                  const float* pMatrix, const void* pPlacedNode)
{
    static int s_iPlaceBudget = 0;
    if (s_iPlaceBudget >= 96)
        return;
    ++s_iPlaceBudget;
    char lac[256];
    if (pMatrix != nullptr)
    {
        std::snprintf(lac, sizeof(lac),
            "[AptRT] place: tag=%d charId=%d depth=%d flags=0x%X matrix=(%.3f %.3f %.3f %.3f %.3f %.3f) -> node %p\n",
            nTag, nCharId, nDepth, nFlags,
            pMatrix[0], pMatrix[1], pMatrix[2], pMatrix[3], pMatrix[4], pMatrix[5], pPlacedNode);
    }
    else
    {
        std::snprintf(lac, sizeof(lac),
            "[AptRT] place: tag=%d charId=%d depth=%d flags=0x%X matrix=(none) -> node %p\n",
            nTag, nCharId, nDepth, nFlags, pPlacedNode);
    }
    CgsDev::Log::WriteToLog(lac);
}

// =============================================================================
// THE APT RUNTIME BRING-UP / PER-FRAME DRIVER.
//
// This is the host facade that the X360 AptInit / AptUpdateInitialize / AptRender-
// Initialize / AptCreateTargetInstance chain (all UN-RECONSTRUCTED -- they exist
// only as comments in the tree) would normally provide. It stands up the engine
// pieces that DO exist and drives them each frame, logging [AptRT] probes and
// bailing cleanly the instant it crosses an un-homed piece, so the run-log reveals
// exactly how far BootLegal's Title_Screen02 render path gets.
//
// The init values (allocator sizes, interpreter stack sizes, the AptUpdate/Create
// param blocks) are transcribed from the REAL X360 CgsGui::AptAux::InitializeApt
// @0x82848E50 + AptAllocatorInitialize @0x82ADD118 (see the matching .ida-exports
// JSONs) so they are faithful, not invented.
// =============================================================================

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
extern void*              gpAptValueGCPool;        // off_8324D834 (type-erased GC-pool view)

// The interpreter VM singleton (X360 &dword_8324E760) -- defined in AptGlobals.cpp.
extern AptActionInterpreter gAptActionInterpreter;

namespace
{
    using namespace BrnGui;

    // ---- run-state flags --------------------------------------------------------
    bool s_bAllocatorReady   = false;   // DOGMA + AptValueGC pools constructed + wired
    bool s_bInterpreterReady = false;   // AptActionInterpreter::initialize done
    bool s_bAuxReady         = false;   // AptAux::Construct done (gAptFuncs render slots live)
    bool s_bTargetReady      = false;   // an AptTarget context exists (FLAG: see below)
    bool s_bRuntimeReady     = false;   // the whole bring-up succeeded
    bool s_bBringUpAttempted = false;   // ran the (idempotent) bring-up at least once

    // ---- the host AptAux singleton ---------------------------------------------
    // The real AptAux is large (mRenderHandler alone is ~108 KB); allocate it as one
    // static object (the X360 holds a single static instance the callbacks resolve
    // through AptAuxPointer::mpAptAuxInst). Constructed in AptRuntimeBringUp.
    CgsGui::AptAux s_AptAux;

    // ---- the Apt render buffer the engine's render callbacks fill --------------
    // AptRenderHandler::GetIm2dRendererType() returns mpImRenderers->mpIm2dRenderer
    // (an AptIm2dRenderBuffer*), and AptRenderHandler::Render appends its draw
    // commands to that buffer's mCommandBuffer (a real CgsGraphics::ImRenderBuffer<V>).
    // We OWN that buffer here and flush it to D3D9 each frame via Dispatch() -- this is
    // how Apt geometry would reach the screen. (Reuse note: this is the SAME
    // ImRenderBuffer<Basic2dColouredTexturedVertex> family the loading screen + debug
    // HUD draw through; the Dispatch() path is the fully-reconstructed PC D3D9 flush.)
    CgsGui::AptIm2dRenderBuffer s_AptRenderBuffer;
    bool                        s_bRenderBufferReady = false;

    // ---- the ImRendererSet handed to AptAux::Construct -------------------------
    // AptAux::Construct reinterpret_cast<AptImRendererSet*>(this set) and reads:
    //   +0x00 mpIm2dRenderer  -> our s_AptRenderBuffer  (the 2D render buffer)
    //   +0x10 mp3dRenderer    -> a non-null sentinel    (Render asserts it != 0)
    // CgsGuiModuleIO::ImRendererSet's leading bytes are an opaque 5-dword blob
    // (maRendererPtrs[20]); the AptImRendererSet view aliases the same first/4th
    // pointer slots. We build the AptImRendererSet directly and hand its address
    // (re-typed) to Construct so the aliasing is exact + obvious.
    CgsGui::AptRenderHandler::AptImRendererSet s_AptImRendererSet;

    // A non-null 3D-renderer sentinel so AptRenderHandler::Render's
    // `mpImRenderers->mp3dRenderer != 0` assert passes. The Apt boot/title movies are
    // 2D-only, so the 3D renderer is never dereferenced on this path; a sentinel keeps
    // the assert quiet without dragging in the (out-of-scope) 3D render set.
    // FLAG: sentinel only -- a real Im3dRenderBuffer is out of this slice's scope (the
    // title movie draws 2D). If a 3D Apt path is ever exercised this must become real.
    int s_i3dRendererSentinel = 0;

    // ---- the loaded movie handle (channel-41 result) ---------------------------
    char        s_acLoadedMovieName[64] = { 0 };
    bool        s_bMovieRequested        = false;   // a PlayAptMovie request is in flight (load attempted)
    bool        s_bMovieLoaded           = false;   // the movie bundle loaded + the AptDataHeader resolved
    s32         s_iFrameCounter          = 0;       // per-frame probe throttle

    // ---- the loaded movie's resource pool + its relocated Apt geometry ---------
    // The movie bundle (GUIAPT\<NAME>.bundle) is loaded SYNCHRONOUSLY into this pool via
    // BundleLoader::LoadBundle (the same async-FS/DeviceManager path the FSM + VIDEOLIST
    // bundles use). Its single AptData resource (type 0x1E == 30) is FixUp'd by the
    // registered CgsResource::AptDataHeaderType handler into a live CgsGui::AptDataHeader,
    // whose mpGeomStruct is a relocated CgsResource::GuiGeometryObject (the renderable shape
    // geometry). We hold that object + render its files each frame through the homed
    // AptRenderHandler::Render -- the achievable render milestone (geometry -> the buffer
    // that is already Dispatched to D3D9), bypassing the un-homed engine render-tree walk.
    CgsResource::Pool*               s_pMoviePool  = nullptr;   // holds the loaded movie bundle
    CgsGui::AptDataHeader*           s_pAptHeader  = nullptr;   // the movie header (fields are RAW offsets on x64)

    // x64 TRANSCODE STATE (USER-CONFIRMED 2026-06-30): the .apt payload kept the console
    // 4-byte serialised pointer format and the resource backing is a HIGH x64 address, so the
    // in-place u32 FixUp is impossible (it was made a no-op in CgsAptDataHeader.cpp). Instead
    // we keep the FULL 64-bit resource base and treat every serialised u32 field as an OFFSET
    // relative to it, resolving `(T*)(base64 + offset)` at every level -- never storing a 64-bit
    // address back into a u32. s_uGeomOffset is mpGeomStruct (the offset of the GuiGeometryObject).
    uintptr_t                        s_uAptResourceBase = 0;    // m_baseResources[0] (the 64-bit load base)
    u32                              s_uGeomOffset      = 0;    // mpGeomStruct (offset of the geometry object)
    u32                              s_uAptResourceSize = 0;    // the AptData resource size (REAL bound for ResolveOff)
    bool                             s_bGeomResolved    = false;// a sane geometry object was found+validated
    bool                             s_bAptBufferHasFrame = false; // a render block was opened this frame (gate the flush)
    bool                             s_bRenderProbed    = false;// emitted the one-shot fine render probes yet
    bool                             s_bFlushProbed     = false;// emitted the one-shot fine flush probe yet
    s32                              s_iLastVertsSubmitted = 0; // verts submitted last frame (for the summary log)

    // The .apt serialised POINTER SIZE (USER design: the "1:7:<n>" descriptor's third value).
    //   4 = console 32-bit pointers (offset transcode); 8 = native x64 64-bit pointers (in-place).
    // The "Apt Data:1:7:4" descriptor lives in the BUNDLE DEBUG-NAME table, which the Pool does NOT
    // retain in the Entry after load -- so it is not in the loaded resource payload. We therefore
    // DISPATCH by structural validation: try the 4-byte interpretation, and if its top-level fields
    // are implausible try the 8-byte one; the validated size wins. (A future 8-byte bundle resolves
    // through the native path unchanged.) FLAG: the explicit descriptor field is unreachable post-
    // load, so the size is inferred structurally rather than read; 4-byte is the validated default.
    s32                              s_iAptPointerSize  = 4;

    // The movie pool backing (3 mem types). The title movie is ~1.6 MB; reserve 8 MB/type
    // (the bundle's main-memory resources + the heap node overhead). Static BSS storage.
    const u32 KU_MOVIE_POOL_BYTES = 8u * 1024u * 1024u;
    u8 s_moviePoolBacking[CgsResource::E_MEMTYPE_NUMTYPES][KU_MOVIE_POOL_BYTES];
    CgsResource::Pool s_MoviePoolStorage;

    // The Apt-data resource type id (X360 0x1E == 30; CgsResource::AptDataHeaderType::GetTypeID).
    const u32 KU_APTDATA_RESOURCE_TYPE_ID = 30u;

    // ====================================================================================
    // STEP 1 -- FAITHFUL INSTANTIATION STATE (gated; direct-geometry render stays the fallback).
    //
    // The faithful path runs the WHOLE .apt through the homed AptCharacterAnimation::FixupTranscode
    // (the 4-byte->x64 in-place relocate) to produce a live AptCharacterAnimation, then instantiates
    // it as a root AptCIH on the director's root display list via the homed chain:
    //   AptGetAnimationAtLevel(0) -> MakeCharacterAnimationInst(pFile) -> root->SetCharacterInst(inst).
    //
    // x64 KEY: FixupTranscode's Reloc32 writes (offset + base) back into 32-bit slots, so the blob
    // MUST live in the LOW 4 GB (else the 64-bit base truncates -> garbage). The codebase has no low
    // allocator, so we copy the .apt resource into a VirtualAlloc'd low-4GB buffer and run the fixup
    // there. The geometry it references (LoadRenderingUnit -> GuiGeometryFile*) is in that SAME low
    // copy, so those host-pointer stores into 32-bit slots are ALSO lossless. (Imports go through the
    // AptLoader_LoadX360 stub -> null, harmless.)
    //
    // DEFENSIVE: the whole faithful path is one-shot + heavily guarded; ANY failure leaves
    // s_bFaithfulInstantiated false and the direct-geometry render (s_bGeomResolved) intact, so the
    // game stays up + still shows the title art. The faithful per-frame TICK + RENDER are steps 2/3
    // (NOT this pass) -- this pass only stands up the AptCharacterAnimation + root AptCIH tree.
    // ====================================================================================
    const bool KB_FAITHFUL_PATH_ENABLED = true;   // master gate (GUIAPT64 native 1:7:8). 8-byte serialized
                                                  // layout now pinned (natural x64 widening) -- FixupWalk
                                                  // widened + movie root located by 0x09876543 signature.
                                                  // OFF: faithful instantiation blocked on the converted
                                                  // .apt container layout (AptCharacterAnimation root not
                                                  // at console dataRoot+16); restore the working geometry
                                                  // render until the converted-.apt root offset is known.

    void*  s_pLowAptCopy        = nullptr;   // VirtualAlloc'd low-4GB copy of the .apt resource
    u32    s_uLowAptCopyBytes   = 0;
    AptCharacterAnimation* s_pFaithfulCharAnim = nullptr;   // FixupTranscode result (the movie root)
    AptFile*               s_pFaithfulAptFile  = nullptr;   // synthesised loaded-file handle (mpData = root)
    void*                  s_pFaithfulRootCIH  = nullptr;   // the root AptCIH placed on the director
    bool   s_bFaithfulAttempted    = false;
    bool   s_bFaithfulInstantiated = false;
    s32    s_iTickFrame            = 0;     // STEP-2 per-frame tick counter (for the placement probes)

    // Allocate a buffer in the LOW 4 GB (32-bit-addressable) via a fixed-address VirtualAlloc probe.
    // A fixed lpAddress makes VirtualAlloc return THAT address or fail (never relocates), so any
    // non-null result is < 0x100000000. Returns null if no low region of luBytes is free.
    static void* AllocLow4GB(u32 luBytes)
    {
        const uintptr_t kGran  = 0x10000u;            // 64 KB allocation granularity
        const uintptr_t kLimit = 0x100000000ull;
        u32 luRounded = (luBytes + static_cast<u32>(kGran) - 1u) & ~(static_cast<u32>(kGran) - 1u);
        for (uintptr_t luAddr = 0x00100000u; luAddr + luRounded <= kLimit; luAddr += kGran)
        {
            void* lpMem = VirtualAlloc(reinterpret_cast<void*>(luAddr), luRounded,
                                       BRNAPT_MEM_RESERVE | BRNAPT_MEM_COMMIT, BRNAPT_PAGE_READWRITE);
            if (lpMem != nullptr)
                return lpMem;   // guaranteed < 4 GB by the fixed lpAddress
        }
        return nullptr;
    }

    // The X360 interpreter stack sizes (CgsGui::AptAux::InitializeApt @0x82848E50:
    // AptUpdateInitialize's v8 block -> the AptActionInterpreter init parms). The
    // interpreter's operand-stack capacity / call-stack depth are carried in that
    // param block (v8[1]=512 etc); the AptScriptFunctionBase register window count is
    // v8[3]=256. We size the operand stack generously (the engine reads iStackSize for
    // the operand stack and iCallStackDepth for the four call-depth stacks).
    const s32 KI_APT_STACK_SIZE       = 512;   // operand-stack capacity
    const s32 KI_APT_CALLSTACK_DEPTH  = 64;    // four call-depth stacks' capacity

    // The Apt allocator sizes -- VERBATIM from AptAllocatorInitialize @0x82ADD118's
    // single caller (InitializeApt @0x82848E50 passes AptAllocatorInitialize(0x10000,
    // 0x4000, 0x10000, 0x4000)): a1/a2 = AptValueGC main/overflow, a3/a4 = DOGMA
    // main/overflow. The DOGMA fixed-size params (minSize 4, maxSize 256, the three
    // free-item bookkeeping offsets 0/0/0, bTrackOutsideAllocations 1) are transcribed
    // from the @0x82ADD118 asm (li r6,4 / li r7,0x100 / li r8,0 / li r9,0 / li r10,0 /
    // stb r11(=1)).
    const size_t KU_DOGMA_MAIN     = 0x10000;  // 64 KB main pool
    const size_t KU_DOGMA_OVERFLOW = 0x4000;   // 16 KB overflow
    const size_t KU_GC_MAIN        = 0x10000;  // 64 KB GC main pool
    const size_t KU_GC_OVERFLOW    = 0x4000;   // 16 KB GC overflow

    // The single shared DOGMA fixed-size pool (the X360 off_8324D808 all five
    // gpApt*Pool aliases point at) + the AptValueGC pool (off_8324D834). Held as raw
    // storage we placement-construct so we control lifetime + can wire the globals.
    DOGMA_PoolManager*      s_pDogmaPool = nullptr;
    AptValueGC_PoolManager* s_pGCPool    = nullptr;


    // ---- the Apt allocator + interpreter globals the engine reads (declared in the
    //      Apt SDK; defined in AptGlobals.cpp / AptTarget.cpp; null until WE wire them,
    //      because the X360 AptInit that wires them is un-reconstructed) -------------
    void WireAllocatorGlobals()
    {
        // The five operand/pseudo/render/shared/single-list pool aliases all point at
        // the one shared DOGMA pool (faithful: X360 off_8324D808).
        gpAptOperandStackPool  = s_pDogmaPool;
        gpAptPseudoDataPool    = s_pDogmaPool;
        gpAptRenderManagerPool = s_pDogmaPool;
        gpAptSharedPtrPool     = s_pDogmaPool;
        gpAptSingleListPool    = s_pDogmaPool;

        // The non-GC value pool (AptDefine.h gpNonGCPoolManager) also aliases the DOGMA
        // pool; the GC value pool pointer (gpGCPoolManager) points at the AptValueGC pool.
        gpNonGCPoolManager = s_pDogmaPool;
        gpGCPoolManager    = s_pGCPool;

        // The type-erased GC-pool view the engine stamps (off_8324D834).
        gpAptValueGCPool = s_pGCPool;
    }

    // AptAllocatorInitialize @0x82ADD118 -- FAITHFUL decompile. StaticInitialize, then construct the
    // non-GC (DOGMA) pool manager into off_8324D808 and the GC (AptValueGC) pool manager into
    // off_8324D834. X360 sig (a1=GC main, a2=GC ovf, a3=DOGMA main, a4=DOGMA ovf); AptAux::InitializeApt
    // calls it with (0x10000,0x4000,0x10000,0x4000). The X360 allocates each manager via the base
    // allocator dword_8324E818(48); on PC that hook is not installed at bring-up, so we back them with
    // process-lifetime static storage (marked PC allocation -- same lifetime as the console heap pools).
    // FLAG (x64): byte_82144A18 is zeroed so StaticInitialize leaves the GC maxSize 0 -> override the GC
    // size statics with x64-correct values BEFORE the GC pool ctor reads them (else AptCIH/AptValue
    // allocs take the invalid 0-bucket path and AV). WireAllocatorGlobals sets off_8324D808/off_8324D834
    // (+ the operand/pseudo/render/shared/single-list aliases the engine reads off the non-GC pool).
    void* AptAllocatorInitialize(int nGcMain, int nGcOvf, int nDogmaMain, int nDogmaOvf)
    {
        AptValueGC_PoolManager::StaticInitialize();
        gAptValueGCMinItemSize = 4u;
        gAptValueGCMaxItemSize = 256u;
        static DOGMA_PoolManager s_DogmaStorage(nDogmaMain, nDogmaOvf,
                                                /*minSize*/ 4, /*maxSize*/ 256,
                                                /*nOffsetToStoreNextInFreeItem*/ 0,
                                                /*bStoreFreeBlockSize*/ false,
                                                /*nOffsetToStoreSizeInFreeItem*/ 0,
                                                /*bTrackOutsideAllocations*/ true);
        s_pDogmaPool = &s_DogmaStorage;
        static AptValueGC_PoolManager s_GCStorage(nGcMain, nGcOvf);
        s_pGCPool = &s_GCStorage;
        WireAllocatorGlobals();
        return s_pGCPool;
    }
}

namespace BrnGui
{
    // Forward declarations (definitions are `static` later in this BrnGui namespace; called
    // from PlayMovie / Update which appear before their definitions). Must be in BrnGui (not
    // the anon namespace) so the in-namespace calls bind to the static definitions.
    static void ResolveMovieGeometry(CgsResource::Entry* lpEntry);
    static s32  RenderLoadedGeometryDirect(CgsGraphics::Im2d* lpIm2d);
    static void DumpResourceBytes(const char* lpcTag, void* lpBase, u32 luOffset,
                                  u32 luResourceSize, u32 luBytes);
    static renderengine::Texture* ResolveMeshTexture(const u32* lpMesh, s32 liTexMode);
    static void TryFaithfulInstantiate(CgsResource::Entry* lpEntry);
    static bool ValidateCharAnimRoot(uintptr_t luBase, u32 luCAOff, u32 luSize, int liPtrSize, u32* lpuScoreOut);
    static u64  ReadSlot(uintptr_t luBase, u32 luOff, int liPtrSize);

    bool AptRuntimeIsReady() { return s_bRuntimeReady; }

    // -------------------------------------------------------------------------
    // AptRuntimeBringUp -- the once-only host bring-up (idempotent). Mirrors the
    // X360 CgsGui::AptAux::InitializeApt @0x82848E50 + AptAllocatorInitialize
    // @0x82ADD118, but only the pieces whose engine bodies exist; every step that
    // crosses an un-homed engine routine is // FLAG'd and skipped defensively.
    // -------------------------------------------------------------------------
    bool AptRuntimeBringUp()
    {
        if (s_bRuntimeReady)
            return true;
        // Idempotent: each step is guarded by its own *Ready flag, so re-entry only runs
        // the steps that have not yet succeeded (e.g. the render buffer waiting on the rw
        // allocator). Log "begin" only on the first attempt to avoid per-frame spam.
        if (!s_bBringUpAttempted)
            CgsDev::Log::WriteToLog("[AptRT] bring-up: begin.\n");
        s_bBringUpAttempted = true;

        // ---- STEP 1: the Apt allocators (DOGMA fixed-size + AptValueGC pools) -----
        // StaticInitialize() computes the per-VFT min/max object sizes the GC pool
        // needs from byte_82144A18; then construct both pools with the real X360 sizes
        // and wire every global the engine reads off them.
        if (!s_bAllocatorReady)
        {
            // FAITHFUL: the extracted AptAllocatorInitialize @0x82ADD118 (StaticInitialize + construct
            // the DOGMA non-GC + AptValueGC pools + WireAllocatorGlobals). AptAux::InitializeApt calls it
            // with (GC main, GC ovf, DOGMA main, DOGMA ovf). The x64 GC-size override + PC static backing
            // live inside the function (marked FLAGs there). This retires the inline invented body -- the
            // first of the faithful bring-up entry points (Step 8) to replace BrnAptRuntimeBringUp.
            AptAllocatorInitialize(KU_GC_MAIN, KU_GC_OVERFLOW, KU_DOGMA_MAIN, KU_DOGMA_OVERFLOW);
            s_bAllocatorReady = true;

            char lac[160];
            std::snprintf(lac, sizeof(lac),
                "[AptRT] step1 allocators (AptAllocatorInitialize): DOGMA(0x%X/0x%X)=%p GC(0x%X/0x%X)=%p wired.\n",
                (unsigned)KU_DOGMA_MAIN, (unsigned)KU_DOGMA_OVERFLOW, (void*)s_pDogmaPool,
                (unsigned)KU_GC_MAIN, (unsigned)KU_GC_OVERFLOW, (void*)s_pGCPool);
            CgsDev::Log::WriteToLog(lac);
        }

        // ---- STEP 2: the AptActionInterpreter (the ActionScript VM stacks) -------
        // initialize() allocates the five {count,capacity,array} stacks from the
        // operand-stack pool (now non-null). Needs gpAptOperandStackPool wired (step 1).
        if (!s_bInterpreterReady)
        {
            if (gpAptOperandStackPool == nullptr)
            {
                CgsDev::Log::WriteToLog("[AptRT] step2 interpreter: SKIP -- operand-stack pool null (allocator wiring failed).\n");
            }
            else
            {
                AptInitParmsT lParms;
                std::memset(&lParms, 0, sizeof(lParms));
                lParms.iStackSize          = KI_APT_STACK_SIZE;
                lParms.iCallStackDepth     = KI_APT_CALLSTACK_DEPTH;
                lParms.mbSkipTraceBytecodes = 1;   // trace bytecodes skipped on a release host

                // FLAG: AptActionInterpreter::initialize tail-calls AptScriptFunctionBase_
                // InitializeStaticData (the AS register window) -- that path reaches the
                // register/frame machinery which is only partly reconstructed. If it faults
                // it does so HERE; this is logged before + after so the run-log brackets it.
                CgsDev::Log::WriteToLog("[AptRT] step2 interpreter: calling AptActionInterpreter::initialize ...\n");
                gAptActionInterpreter.initialize(&lParms);
                s_bInterpreterReady = true;
                char lac[128];
                std::snprintf(lac, sizeof(lac),
                    "[AptRT] step2 interpreter: initialized (stack=%d callstack=%d).\n",
                    KI_APT_STACK_SIZE, KI_APT_CALLSTACK_DEPTH);
                CgsDev::Log::WriteToLog(lac);
            }
        }

        // ---- STEP 3: the Apt render buffer (the D3D9 2D buffer the engine fills) --
        // Construct + Prepare the ImRenderBuffer<V> that AptRenderHandler::Render
        // appends to (and we flush via Dispatch each frame). Prepare carves its
        // command + vertex storage from the RenderWare default resource allocator.
        if (!s_bRenderBufferReady)
        {
            s_AptRenderBuffer.mu32Head = 0;
            s_AptRenderBuffer.mCommandBuffer.Construct();

            rw::IResourceAllocator* lpAllocator = rw::ResourceAllocatorRegistry::GetDefaultAllocator();
            if (lpAllocator == nullptr)
            {
                // FLAG: no RW default allocator yet -> the buffer cannot carve its storage.
                // Defer (the renderer is up by the loading screen, so this is normally fine).
                CgsDev::Log::WriteToLog("[AptRT] step3 renderbuffer: SKIP -- rw default allocator null (deferred).\n");
            }
            else
            {
                // 256 KB command stream + 256 KB vertex stream (generous for a single
                // boot/title movie's per-frame geometry). failGracefully=true so a carve
                // failure returns false rather than asserting.
                const bool lbOk = s_AptRenderBuffer.mCommandBuffer.Prepare(
                    256u * 1024u, 256u * 1024u, lpAllocator, /*failGracefully*/ true);
                s_bRenderBufferReady = lbOk;
                CgsDev::Log::WriteToLog(lbOk
                    ? "[AptRT] step3 renderbuffer: Construct+Prepare ok (256KB cmd / 256KB vtx).\n"
                    : "[AptRT] step3 renderbuffer: Prepare FAILED (carve) -- buffer unusable (FLAG).\n");
            }
        }

        // ---- STEP 4: AptAux::Construct (the host callback table + render handler) -
        // Build the ImRendererSet (slot0 = our render buffer, 3d slot = sentinel) and
        // hand it to AptAux::Construct, which seeds the render handler + installs the
        // gAptFuncs render-callback family (the engine's render dispatch reaches our
        // AptRenderHandler::Render through it).
        if (!s_bAuxReady)
        {
            s_AptImRendererSet.mpIm2dRenderer = &s_AptRenderBuffer;     // GetIm2dRendererType() target
            s_AptImRendererSet.mpReserved04   = nullptr;
            s_AptImRendererSet.mpReserved08   = nullptr;
            s_AptImRendererSet.mpReserved0C   = nullptr;
            s_AptImRendererSet.mp3dRenderer   = &s_i3dRendererSentinel; // Render asserts != 0

            // AptAux::Construct args:
            //   ImRendererSet*       -> our AptImRendererSet (re-typed; bit-aliased, see header)
            //   TextRenderer*        -> null   FLAG: the glyph batcher is out of scope (no Apt
            //                                  static-text on the title screen is required to render
            //                                  the movie's shapes; DrawString would need it)
            //   LanguageManager*     -> null   FLAG: localised-string lookup unused for shape draw
            //   FontCollection*      -> null   FLAG: text layout unused for shape draw
            //   aspectRatio          -> 16:9 (1280/720) so the stage resolution lands at 1280x720
            //   alt-colour table     -> null / 0 entries (no alt text colours needed for shapes)
            const f32 lfAspect = 1280.0f / 720.0f;
            CgsDev::Log::WriteToLog("[AptRT] step4 aux: calling AptAux::Construct ...\n");
            s_AptAux.Construct(reinterpret_cast<CgsGuiModuleIO::ImRendererSet*>(&s_AptImRendererSet),
                               /*TextRenderer*/   nullptr,
                               /*LanguageManager*/nullptr,
                               /*FontCollection*/ nullptr,
                               lfAspect,
                               /*AlternateTextColours*/ nullptr,
                               /*NumAlternateColours*/  0);
            s_bAuxReady = (CgsGui::AptAuxPointer::mpAptAuxInst == &s_AptAux);
            char lac[160];
            std::snprintf(lac, sizeof(lac),
                "[AptRT] step4 aux: Construct done. singleton=%p im2d=%p (== &renderbuf %p)\n",
                (void*)CgsGui::AptAuxPointer::mpAptAuxInst,
                (void*)s_AptImRendererSet.mpIm2dRenderer, (void*)&s_AptRenderBuffer);
            CgsDev::Log::WriteToLog(lac);
        }

        // ---- STEP 5: the AptTarget context (the engine's per-process director) ----
        // Create the Apt context (AptCreateTargetInstance) + select it as current
        // (AptChangeTargetInstance) so GetTarget() returns a live context -- the X360
        // does exactly this inside InitializeApt (`*(a1+8)=AptCreateTargetInstance(v7);
        // AptChangeTargetInstance()`), and those two are now HOMED in AptTarget.cpp.
        // The create-params block is the v7[8] InitializeApt @0x82848E50 builds:
        //   v7 = {0, 1, 8, 8, 0, 512, 0, 0}
        // The AptTarget ctor reads config from p[4]/p[7]/p[2]/p[1]/p[0]/p[3] and the
        // animation director (MakeAptAnimationTarget) from words 0/1/2/3/5 (p[5]=512).
        if (!s_bTargetReady)
        {
            // Pre-existing context? (idempotent: don't create a second instance.)
            AptTarget* lpExisting = GetTarget();
            if (lpExisting != nullptr)
            {
                s_bTargetReady = true;
            }
            else if (gpAptPseudoDataPool == nullptr)
            {
                CgsDev::Log::WriteToLog("[AptRT] step5 target: SKIP -- DOGMA pool null (allocator wiring failed).\n");
            }
            else
            {
                static const u32 s_auAptCreateParams[8] = { 0u, 1u, 8u, 8u, 0u, 512u, 0u, 0u };  // InitializeApt v7
                CgsDev::Log::WriteToLog("[AptRT] step5 target: AptCreateTargetInstance + AptChangeTargetInstance ...\n");

                // FLAG: the AptTarget ctor builds an AptAnimationTarget director + an AptLoader
                // + an AptLinker (all homed) from the shared DOGMA pool. If any sub-construct
                // crosses an un-homed callee it faults HERE; bracketed by the probes so the
                // run-log pinpoints it.
                AptTarget* lpCreated = AptCreateTargetInstance(s_auAptCreateParams);
                if (lpCreated == nullptr)
                {
                    CgsDev::Log::WriteToLog("[AptRT] step5 target: AptCreateTargetInstance returned null "
                                            "(pool carve failed) -- FLAG.\n");
                }
                else
                {
                    AptChangeTargetInstance(lpCreated);   // sets gpAptTarget/TLS + the GetTarget() mirror
                    AptTarget* lpNow = GetTarget();
                    s_bTargetReady = (lpNow != nullptr);
                    char lac[200];
                    std::snprintf(lac, sizeof(lac),
                        "[AptRT] step5 target: created %p, gpAptTarget set; GetTarget()=%p; "
                        "director=%p loader=%p linker=%p.\n",
                        (void*)lpCreated, (void*)lpNow,
                        (void*)lpCreated->mpAnimationTarget, (void*)lpCreated->mpLoader,
                        (void*)lpCreated->mpLinker);
                    CgsDev::Log::WriteToLog(lac);
                }
            }
        }

        // The runtime is "ready" (for channel-41 routing) once the allocator + render
        // buffer + AptAux host are up. The target now being live (step 5) lets PlayMovie +
        // the per-frame tick reach a real GetTarget(); a failed target is a downstream bail,
        // not a bring-up failure.
        s_bRuntimeReady = s_bAllocatorReady && s_bAuxReady;
        CgsDev::Log::WriteToLog(s_bRuntimeReady
            ? (s_bTargetReady
                ? "[AptRT] bring-up: READY (alloc+interp+aux+renderbuf+TARGET up; GetTarget() live).\n"
                : "[AptRT] bring-up: READY (alloc+interp+aux+renderbuf up; target FLAG'd -- see step5).\n")
            : "[AptRT] bring-up: INCOMPLETE -- see step probes above.\n");
        return s_bRuntimeReady;
    }

    // -------------------------------------------------------------------------
    // AptRuntimePlayMovie -- consume a channel-41 GuiEventPlayAptMovie. Records the
    // movie name and ATTEMPTS to load it through the homed Apt loader. Defensive:
    // bails (logs) wherever the load path crosses an un-homed piece.
    // -------------------------------------------------------------------------
    void AptRuntimePlayMovie(const char* lpacMovieName, s32 liLevelNum)
    {
        if (lpacMovieName == nullptr || lpacMovieName[0] == '\0')
        {
            // null / empty channel-41 name: a spurious re-post (BootLegal interleaves empty events).
            // Ignore it -- do NOT clobber the loaded-movie name or trigger a (failed) reload.
            return;
        }

        // LOAD-ONCE GUARD: the channel-41 event re-fires EVERY frame (BootLegal keeps the movie
        // playing), so without this guard the bundle would be (re)loaded thousands of times, each
        // allocating CgsResource pool entries until the pool overflows. Load each distinct movie
        // exactly once; once REQUESTED or LOADED, ignore re-fires of the same name. (Guarding on
        // s_bMovieLoaded too is essential: an interleaved empty event used to reset s_bMovieRequested,
        // so the same movie reloaded every cycle and exhausted CgsResourcePool.)
        if ((s_bMovieRequested || s_bMovieLoaded) &&
            std::strncmp(s_acLoadedMovieName, lpacMovieName, sizeof(s_acLoadedMovieName) - 1) == 0)
            return;   // same movie already attempted/loaded -- silent (avoids per-frame reload+spam)

        std::strncpy(s_acLoadedMovieName, lpacMovieName, sizeof(s_acLoadedMovieName) - 1);
        s_acLoadedMovieName[sizeof(s_acLoadedMovieName) - 1] = '\0';
        s_bMovieRequested = true;

        char lac[200];
        std::snprintf(lac, sizeof(lac), "[AptRT] PlayMovie: consume channel-41 '%s' (level %d).\n",
                      s_acLoadedMovieName, liLevelNum);
        CgsDev::Log::WriteToLog(lac);

        if (!s_bRuntimeReady)
        {
            CgsDev::Log::WriteToLog("[AptRT] PlayMovie: runtime not ready -- load deferred.\n");
            s_bMovieRequested = false;   // allow a retry next time the event fires
            return;
        }

        // ---- SYNCHRONOUS bundle load: GUIAPT\<NAME>.BUNDLE (the FAITHFUL bundle path) ------------
        // FAITHFULNESS (2026-06-30, user): the GUIAPT32/GUIAPT64 split was NON-FAITHFUL test
        // scaffolding -- the original game loads apt movies from GUIAPT\. Our PC build's GUIAPT
        // bundle carries the converted "Apt Data:1:7:8" (native 8-byte) data, so the path stays
        // faithful (GUIAPT) while the data content is the x64-converted form. Load through
        // BundleLoader::LoadBundle; the AptData resource (type 0x1E == 30) is FixUp'd by the
        // registered CgsResource::AptDataHeaderType handler.
        CgsResource::RegisterAllResourceTypes();   // idempotent: ensure AptDataHeaderType (0x1E) is live

        // Build the FAITHFUL bundle path "GuiApt\<NAME>.bundle" -- the exact format string the X360
        // apt loader uses (verified @0x828504B0), name passed as-is (Windows resolves case/separator).
        // The loader returns <=0 if missing -> bails cleanly.
        char lacBundlePath[160];
        std::snprintf(lacBundlePath, sizeof(lacBundlePath), "GuiApt\\%s.bundle", s_acLoadedMovieName);

        s_pMoviePool = &s_MoviePoolStorage;
        CgsResource::Pool::InitOptions lOptions;
        lOptions.miId   = 4;
        lOptions.mpcName = "AptMovie";
        for (u32 lt = 0; lt < CgsResource::E_MEMTYPE_NUMTYPES; ++lt)
        {
            lOptions.maHeapInfo[lt].muMaxNodes       = 256u;
            lOptions.maHeapInfo[lt].muHeapMemorySize = KU_MOVIE_POOL_BYTES - 128u * 1024u;
            lOptions.maHeapInfo[lt].muHeapAlignment  = 16u;
            lOptions.mResource.m_baseResources[lt]   = s_moviePoolBacking[lt];
            lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_size      = KU_MOVIE_POOL_BYTES;
            lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_alignment = 16u;
        }
        lOptions.muMaxResources         = 64u;
        lOptions.muMaxImports           = 64u;
        lOptions.miRefCountThreshold    = 0;
        lOptions.miNumDependencies      = 0;
        lOptions.miBankId               = 0;
        lOptions.mbAllowDefragmentation = false;
        s_pMoviePool->InitPool(&lOptions);

        CgsResource::BundleLoader lLoader;
        const s32 liLoaded = lLoader.LoadBundle(lacBundlePath, s_pMoviePool,
                                                CgsResource::ResolveResourceType);
        std::snprintf(lac, sizeof(lac), "[AptRT] load: '%s' -> %d resources.\n", lacBundlePath, liLoaded);
        CgsDev::Log::WriteToLog(lac);
        if (liLoaded <= 0)
        {
            // FLAG: bundle missing / not platform-4 / unreadable. Bail cleanly.
            CgsDev::Log::WriteToLog("[AptRT] load: bundle missing/unreadable or not platform-4 -- bail (FLAG).\n");
            return;
        }

        // Pull the single AptData (0x1E) resource; its bytes are the relocated AptDataHeader.
        s32 liIndex = -1;
        CgsResource::Entry* lpEntry =
            s_pMoviePool->FindFirstResourceOfType(KU_APTDATA_RESOURCE_TYPE_ID, &liIndex);
        if (lpEntry == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] load: no AptData (type 0x1E) resource in bundle -- bail (FLAG).\n");
            return;
        }
        // PER-MEM-TYPE BASE PROBE: log all three m_baseResources[] + their sizes. The platform-4
        // resource can span multiple memory-type sections; the geometry/const offsets may be
        // relative to one of these. This shows which base is which at runtime (ground truth).
        std::snprintf(lac, sizeof(lac),
            "[AptRT] probe: AptData entry idx=%d  mem0=%p (size=%u)  mem1=%p (size=%u)  mem2=%p (size=%u)\n",
            liIndex,
            lpEntry->mResource.m_baseResources[0],
            lpEntry->mResourceDescriptor.m_baseResourceDescriptors[0].m_size,
            lpEntry->mResource.m_baseResources[1],
            lpEntry->mResourceDescriptor.m_baseResourceDescriptors[1].m_size,
            lpEntry->mResource.m_baseResources[2],
            lpEntry->mResourceDescriptor.m_baseResourceDescriptors[2].m_size);
        CgsDev::Log::WriteToLog(lac);

        void* lpRes = lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY];
        if (lpRes == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] load: AptData resource has null main-memory bytes -- bail (FLAG).\n");
            return;
        }
        s_pAptHeader = reinterpret_cast<CgsGui::AptDataHeader*>(lpRes);

        // x64 TRANSCODE (USER-CONFIRMED 2026-06-30): AptDataHeader::FixUp is a NO-OP on x64 (the
        // 4-byte .apt format cannot be in-place-relocated into a high x64 backing -- see
        // CgsAptDataHeader.cpp). So the header's pointer fields are RAW file-relative OFFSETS. We
        // keep the FULL 64-bit resource base and resolve every offset as (T*)(base64 + offset).
        s_uAptResourceBase = reinterpret_cast<uintptr_t>(lpRes);
        s_uAptResourceSize =
            lpEntry->mResourceDescriptor.m_baseResourceDescriptors[CgsResource::E_MEMTYPE_MAINMEMORY].m_size;

        // CORRECTED HEADER LAYOUT (verified against the real TITLE_SCREEN02.bundle bytes, this
        // session). The reconstructed CgsGui::AptDataHeader puts mpGeomStruct at +12, but the REAL
        // serialised .apt header has SIX leading offset words and the GuiGeometryObject lives at
        // word[4] (+16), NOT word[3] (+12). The verified field run for Title_Screen02:
        //   word[0]=0x30   -> "Title_Screen02"      (mpacMovieName)
        //   word[1]=0x20   -> "Title_Screen02"      (mpAptData / asset-name block)
        //   word[2]=0x40   -> "Apt Data:1:7:4"+desc (mpConstData -- the AptConstFile, 1:7:4 here)
        //   word[3]=0x7100 -> "Apt constant file"+t (a SECOND const block -- NOT the geometry)
        //   word[4]=0x7200 -> {numFiles=7,numTexPages=2,fileTableOff=0x7210}  <-- GuiGeometryObject
        //   word[5]=0x7838 -> the import table (4 imports at 0x7840)
        // So read the geometry offset from word[4] directly off the resource bytes. (The struct's
        // mpGeomStruct/+12 is left to the FLAG'd no-op FixUp; we do not trust it for the geometry.)
        const u32* lpHeaderWords = reinterpret_cast<const u32*>(lpRes);
        s_uGeomOffset = lpHeaderWords[4];   // +16: the real GuiGeometryObject offset (verified)
        std::snprintf(lac, sizeof(lac),
            "[AptRT] xcode: base=0x%016llX hdr[0..5]=0x%X 0x%X 0x%X 0x%X 0x%X 0x%X (geomOff=word[4]=0x%X)\n",
            (unsigned long long)s_uAptResourceBase,
            lpHeaderWords[0], lpHeaderWords[1], lpHeaderWords[2],
            lpHeaderWords[3], lpHeaderWords[4], lpHeaderWords[5], s_uGeomOffset);
        CgsDev::Log::WriteToLog(lac);

        // HEX-DUMP PROBE (read-only, bounded): dump the first 64 bytes of the AptData resource (the
        // AptDataHeader region) so the run shows the REAL on-disk header layout. The offline bundle
        // parse showed entry layout ambiguity, so this is the ground truth.
        DumpResourceBytes("hdr@+0",   lpRes, 0,
                          lpEntry->mResourceDescriptor.m_baseResourceDescriptors[0].m_size, 64);
        // And the region mpGeomStruct points at (if in range), to see the GuiGeometryObject bytes.
        DumpResourceBytes("geom@off", lpRes, s_uGeomOffset,
                          lpEntry->mResourceDescriptor.m_baseResourceDescriptors[0].m_size, 48);

        s_bMovieLoaded = true;

        // Resolve + sanity-validate the geometry object now (one-time), choosing the pointer-size
        // path (4-byte transcode vs 8-byte native). Sets s_bGeomResolved if sane geometry is found.
        // This is the DIRECT-GEOMETRY render path -- it stays the active render + the FALLBACK.
        ResolveMovieGeometry(lpEntry);

        // STEP 1 (gated): attempt the FAITHFUL instantiation (FixupTranscode -> AptCharacterAnimation
        // -> root AptCIH on the director). One-shot + heavily guarded; on any failure the direct-
        // geometry render above stays intact so the title art still shows. The faithful per-frame
        // tick/AS render are steps 2/3 (next passes) -- this only stands up the display tree.
        TryFaithfulInstantiate(lpEntry);
    }

    // =========================================================================
    // x64 GEOMETRY TRANSCODE WALK.
    //
    // The serialised gui-geometry tree (GuiGeometryObject -> file table -> GuiGeometryFile
    // -> mesh table -> GuiGeometryMesh -> vertex table -> vertex run) is a relocatable blob:
    // every "pointer" is a file-relative OFFSET from the resource base. The homed
    // AptRenderHandler::Render + the homed GuiGeometry*::FixUp read those fields as u32 and
    // cast them to absolute addresses -- which only works when the data is in the low 4 GB
    // (it is not on x64). So we do NOT use those; we resolve every offset against the FULL
    // 64-bit base (s_uAptResourceBase) and submit the vertex runs directly to the command
    // buffer. Dual-path by pointer size (4 = console offsets, 8 = native 64-bit fields).
    //
    // 4-BYTE SERIALISED STRIDES (console .apt, little-endian post-convert):
    //   GuiGeometryObject : {u32 numFiles, u32 numTexPages, u32 fileTableOff}            (12B)
    //   file table        : u32[numFiles] of GuiGeometryFile offsets
    //   GuiGeometryFile   : {u32 id, u32 numMeshes, u32 meshTableOff}                    (12B)
    //   mesh table        : u32[numMeshes] of GuiGeometryMesh offsets
    //   GuiGeometryMesh   : {s32 type, s32 texMode, s32 texId, u32 texPtr,
    //                        u32 numVerts, u32 vertTableOff}                             (24B)
    //   vertex table      : u32[...] -- first entry is the vertex-run offset
    //   vertex run        : Basic2dColouredTexturedVertex[numVerts]                      (20B each)
    // The 8-byte path widens each offset/pointer field to 64-bit (native x64 layout) and
    // resolves it as an absolute pointer (relocated in place by the load) -- not reached by
    // this 4-byte bundle, but wired so a future 1:7:8 bundle renders unchanged.
    // =========================================================================

    // Resolve a serialised offset to a real 64-bit pointer, bounds-checked against the
    // resource size so a garbage offset logs + yields null instead of AV'ing.
    template <typename T>
    static T* ResolveOff(u32 luOffset, u32 luResourceSize)
    {
        if (luOffset == 0 || luOffset >= luResourceSize)
            return nullptr;
        return reinterpret_cast<T*>(s_uAptResourceBase + luOffset);
    }

    // Read the geometry object's three leading u32s at s_uGeomOffset and sanity-check them.
    // Returns true (and fills the out params) if the 4-byte interpretation looks plausible.
    static bool ValidateGeom4(u32 luResourceSize, u32* lpuNumFiles, u32* lpuFileTableOff)
    {
        const u32* lpObj = ResolveOff<const u32>(s_uGeomOffset, luResourceSize);
        if (lpObj == nullptr)
            return false;
        const u32 luNumFiles     = lpObj[0];
        const u32 luFileTableOff = lpObj[2];
        // Plausible: a handful of files, a file-table offset inside the resource.
        if (luNumFiles == 0 || luNumFiles > 4096u)
            return false;
        if (luFileTableOff == 0 || luFileTableOff >= luResourceSize)
            return false;
        *lpuNumFiles     = luNumFiles;
        *lpuFileTableOff = luFileTableOff;
        return true;
    }

    // Read-only, bounded hex dump of a resource region to the log (diagnostic ground truth).
    // Logs luBytes bytes starting at lpBase+luOffset, clamped to [luOffset, luResourceSize), as
    // hex + the u32 interpretation. Never reads past the resource size (no AV).
    static void DumpResourceBytes(const char* lpcTag, void* lpBase, u32 luOffset,
                                  u32 luResourceSize, u32 luBytes)
    {
        if (lpBase == nullptr)
            return;
        if (luOffset >= luResourceSize)
        {
            char lacOOR[128];
            std::snprintf(lacOOR, sizeof(lacOOR),
                "[AptRT] dump %s: offset 0x%X out of range (resSize=0x%X) -- skipped.\n",
                lpcTag, luOffset, luResourceSize);
            CgsDev::Log::WriteToLog(lacOOR);
            return;
        }
        u32 luAvail = luResourceSize - luOffset;
        if (luBytes > luAvail) luBytes = luAvail;
        if (luBytes > 64u)     luBytes = 64u;   // cap the log line length

        const u8* lpb = reinterpret_cast<const u8*>(lpBase) + luOffset;
        char lac[320];
        int liPos = std::snprintf(lac, sizeof(lac), "[AptRT] dump %s (@+0x%X, %u bytes): ",
                                  lpcTag, luOffset, luBytes);
        for (u32 lu = 0; lu < luBytes && liPos < static_cast<int>(sizeof(lac)) - 4; ++lu)
            liPos += std::snprintf(lac + liPos, sizeof(lac) - liPos, "%02x ", lpb[lu]);
        std::snprintf(lac + liPos, sizeof(lac) - liPos, "\n");
        CgsDev::Log::WriteToLog(lac);

        // u32 view (first up to 8 words).
        const u32 luWords = (luBytes / 4u) > 8u ? 8u : (luBytes / 4u);
        if (luWords > 0)
        {
            liPos = std::snprintf(lac, sizeof(lac), "[AptRT] dump %s u32: ", lpcTag);
            for (u32 lu = 0; lu < luWords && liPos < static_cast<int>(sizeof(lac)) - 12; ++lu)
            {
                u32 luWord;
                std::memcpy(&luWord, lpb + lu * 4u, sizeof(luWord));
                liPos += std::snprintf(lac + liPos, sizeof(lac) - liPos, "0x%08X ", luWord);
            }
            std::snprintf(lac + liPos, sizeof(lac) - liPos, "\n");
            CgsDev::Log::WriteToLog(lac);
        }
    }

    // Resolve a textured mesh's renderengine::Texture* from the import-written pointer.
    //
    // For a texMode==1 mesh, the bundle's import-resolve (Pool::ResolveImportForEntry) wrote the
    // imported TEXTURE resource's m_baseResources[0] -- which IS a fully-realised renderengine::
    // Texture (its FixUp ran first and created mpD3DTexture via Texture::Create) -- as a 64-bit
    // pointer into the mesh's texPtr slot (mesh+12). On x64 that 8-byte write also occupies mesh+16
    // (the old numVerts slot; we recover numVerts from the vert table instead). So the texture is
    // simply `*(renderengine::Texture**)(mesh+12)` == (lpMesh[3] | lpMesh[4]<<32). The raster has no
    // x64 self-pointer issue (only mpD3DTexture, set at native width). Im2d::SetTexture binds
    // texture->mpD3DTexture directly.
    //
    // DEFENSIVE: validate the pointer looks like a live texture (non-null, mpD3DTexture non-null)
    // before returning it; otherwise fall back to null (untextured vertex-colour) so a bad/unresolved
    // import never AVs in the draw. mode-0 (texId 6969 sentinel) always returns null.
    static renderengine::Texture* ResolveMeshTexture(const u32* lpMesh, s32 liTexMode)
    {
        if (liTexMode != 1)
            return nullptr;   // mode 0 / unknown -> untextured (vertex colour)

        const u64 luLow  = static_cast<u64>(lpMesh[3]);   // mesh+12 low32
        const u64 luHigh = static_cast<u64>(lpMesh[4]);   // mesh+16 high32 (the import's 8-byte write)
        const uintptr_t luTexPtr = static_cast<uintptr_t>(luLow | (luHigh << 32));
        if (luTexPtr == 0)
            return nullptr;   // import unresolved -> untextured fallback

        renderengine::Texture* lpTexture = reinterpret_cast<renderengine::Texture*>(luTexPtr);
        // The pointer must look like a heap/resource address (reject obviously-bad low values) and
        // carry a live D3D texture, else fall back to untextured (no AV).
        if (luTexPtr < 0x10000u)
            return nullptr;
        if (lpTexture->mpD3DTexture == nullptr)
            return nullptr;   // texture not realised (FixUp didn't create it) -> untextured fallback
        return lpTexture;
    }

    // Read a TOff-wide (4 or 8) pointer SLOT from a serialised record (the file offset, pre-fixup),
    // mirroring exactly how FixupWalk<TOff>::SlotPtr reads it. liPtrSize is the .apt pointer size.
    static u64 ReadSlot(uintptr_t luBase, u32 luOff, int liPtrSize)
    {
        if (liPtrSize == 8)
            return *reinterpret_cast<const u64*>(luBase + luOff);
        return static_cast<u64>(*reinterpret_cast<const u32*>(luBase + luOff));
    }

    // Validate a candidate AptCharacterAnimation BEFORE handing it to the homed Fixup -- the Fixup
    // blindly derefs the character table, so a bad charTable/charCount AVs. POINTER-SIZE AWARE: the
    // FixupWalk reads charCount as a 4-byte int at +0x0C and the pointer slots (charTable +0x10,
    // importTable +0x24, initList +0x2C) at the .apt's TOff width (4 or 8) with the table stride ==
    // TOff. We read with the SAME widths so the validation matches exactly what Fixup will touch.
    // Requires: sane charCount (1..512), an in-range charTable, and EVERY table entry an in-range
    // offset to a record whose type word is a known Apt character type. Returns true + a confidence
    // score only when it really looks like a char anim (so a string/import region is rejected).
    static bool ValidateCharAnimRoot(uintptr_t luBase, u32 luCAOff, u32 luSize, int liPtrSize,
                                     u32* lpuScoreOut)
    {
        if (lpuScoreOut) *lpuScoreOut = 0;
        if (luCAOff == 0 || luCAOff + 0x30u > luSize)
            return false;
        const u32 luStride = (liPtrSize == 8) ? 8u : 4u;
        const u32 luCharCount = *reinterpret_cast<const u32*>(luBase + luCAOff + 0x0Cu);     // +0x0C count
        const u64 luCharTable64 = ReadSlot(luBase, luCAOff + 0x10u, liPtrSize);              // +0x10 table off
        if (luCharCount == 0 || luCharCount > 512u)
            return false;
        if (luCharTable64 == 0 || luCharTable64 >= luSize || (luCharTable64 & 3u) != 0)
            return false;
        if (luCharTable64 + static_cast<u64>(luCharCount) * luStride > luSize)
            return false;
        const u32 luCharTable = static_cast<u32>(luCharTable64);

        // Every char-table entry (Fixup walks all of them): an in-range offset to a record whose type
        // word is a known Apt character type (1 shape, 2 button/morph, 3 text, 5 sprite, 9 movie,
        // 10 font, 15, or 0 placeholder). Reject on the first bad one so Fixup never AVs.
        u32 luScore = 0;
        for (u32 lk = 0; lk < luCharCount; ++lk)
        {
            const u64 luEntry = ReadSlot(luBase, luCharTable + lk * luStride, liPtrSize);
            if (luEntry == 0) { ++luScore; continue; }   // null entry allowed
            if (luEntry >= luSize || (luEntry & 3u) != 0)
                return false;
            const u32 luType = *reinterpret_cast<const u32*>(luBase + static_cast<u32>(luEntry));
            if (luType == 0u || luType == 1u || luType == 2u || luType == 3u ||
                luType == 5u || luType == 9u || luType == 10u || luType == 15u)
                ++luScore;
            else
                return false;
        }
        // The init-list + import-table FixupWalk also walks: counts sane + table offsets in-range.
        const u32 luImpCount  = *reinterpret_cast<const u32*>(luBase + luCAOff + 0x20u);
        const u64 luImpTable  = ReadSlot(luBase, luCAOff + 0x24u, liPtrSize);
        const u32 luInitCount = *reinterpret_cast<const u32*>(luBase + luCAOff + 0x28u);
        const u64 luInitList  = ReadSlot(luBase, luCAOff + 0x2Cu, liPtrSize);
        const u32 luImpStride  = (liPtrSize == 8) ? 0x18u : 0x10u;   // {name*,class*,id,AptFile*}
        const u32 luInitStride = (liPtrSize == 8) ? 0x10u : 0x08u;   // {ptr, int32 indicator}
        if (luImpCount > 4096u || luInitCount > 4096u)
            return false;
        if (luImpCount > 0u && (luImpTable == 0u || luImpTable >= luSize ||
                                luImpTable + static_cast<u64>(luImpCount) * luImpStride > luSize))
            return false;
        if (luInitCount > 0u && (luInitList == 0u || luInitList >= luSize ||
                                 luInitList + static_cast<u64>(luInitCount) * luInitStride > luSize))
            return false;

        if (lpuScoreOut) *lpuScoreOut = luScore;
        return luScore >= ((luCharCount < 4u) ? luCharCount : 4u);
    }

    // =========================================================================
    // STEP 1 -- FAITHFUL INSTANTIATION (gated, one-shot, heavily guarded).
    //
    // Copy the .apt resource into a LOW-4GB buffer, run the homed AptCharacterAnimation::Fixup
    // (FixupTranscode, the 4-byte path) over it to produce a live AptCharacterAnimation, synthesise
    // a loaded AptFile around it, and instantiate it as a root AptCIH on the director's root display
    // list (AptGetAnimationAtLevel(0) -> MakeCharacterAnimationInst -> SetCharacterInst). On ANY
    // failure: bail cleanly (s_bFaithfulInstantiated stays false), leaving the direct-geometry render
    // intact. Per-step [AptRT] faithful: probes so the run shows exactly how far it got.
    // =========================================================================
    static void TryFaithfulInstantiate(CgsResource::Entry* lpEntry)
    {
        if (!KB_FAITHFUL_PATH_ENABLED || s_bFaithfulAttempted)
            return;
        s_bFaithfulAttempted = true;

        char lac[224];
        CgsDev::Log::WriteToLog("[AptRT] faithful: STEP 1 instantiation begin.\n");

        // Need a live Apt context (the director) + the GC pool (the root CIH allocates from it).
        AptTarget* lpTarget = GetTarget();
        if (lpTarget == nullptr || lpTarget->mpAnimationTarget == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: no Apt director (GetTarget/mpAnimationTarget null) "
                                    "-- bail, fallback to direct geometry (FLAG).\n");
            return;
        }
        if (gpGCPoolManager == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: GC pool null -- bail, fallback (FLAG).\n");
            return;
        }

        // --- 1. determine the .apt pointer size + the working base ---------------------------------
        // GUIAPT64 is NATIVE 8-byte ("Apt Data:1:7:8"): FixupInPlace relocates IN PLACE at the REAL
        // (high) resource address -- no low-4GB copy, no transcode (8-byte slots hold full x64
        // addresses). GUIAPT32 is 4-byte: FixupTranscode needs the low-4GB copy. We detect the size
        // from the const-file signature and pick the base accordingly.
        const u32 luSize = s_uAptResourceSize;
        void* lpHigh = reinterpret_cast<void*>(s_uAptResourceBase);
        if (luSize == 0 || lpHigh == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: no resource bytes -- bail (FLAG).\n");
            return;
        }

        // The header's mpConstData field is itself pointer-sized; read it ptr-size-aware. We first
        // peek the const-file at BOTH the 4-byte (word[2]) and 8-byte (u64[2]) header positions to
        // find the "Apt Data:1:7:N" signature, then trust GetPointerSizeBytes().
        // Read the constData offset at the 8-byte header position first (GUIAPT64 is the target).
        u32 luConstOff = static_cast<u32>(*reinterpret_cast<const u64*>(s_uAptResourceBase + 16u)); // u64[2]
        if (luConstOff == 0 || luConstOff >= luSize)
            luConstOff = reinterpret_cast<const u32*>(s_uAptResourceBase)[2];   // fall back to 4-byte word[2]
        if (luConstOff >= luSize)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: constData offset out of range -- bail (FLAG).\n");
            return;
        }
        AptConstFile* lpConstFileHigh =
            reinterpret_cast<AptConstFile*>(s_uAptResourceBase + luConstOff);
        const int liPtrSize = lpConstFileHigh->GetPointerSizeBytes();   // 8 (GUIAPT64) or 4 (GUIAPT32)

        // Working base: 8-byte -> the REAL resource address (in-place); 4-byte -> a low-4GB copy.
        uintptr_t luBase;
        if (liPtrSize == 8)
        {
            luBase = s_uAptResourceBase;   // FixupInPlace relocates in place at the real address
            std::snprintf(lac, sizeof(lac),
                "[AptRT] faithful: GUIAPT64 1:7:8 native -> FixupInPlace at real base 0x%016llX (%u bytes).\n",
                (unsigned long long)luBase, luSize);
            CgsDev::Log::WriteToLog(lac);
        }
        else
        {
            s_pLowAptCopy = AllocLow4GB(luSize);
            if (s_pLowAptCopy == nullptr || reinterpret_cast<uintptr_t>(s_pLowAptCopy) >= 0x100000000ull)
            {
                std::snprintf(lac, sizeof(lac),
                    "[AptRT] faithful: low-4GB alloc FAILED (%p) -- 4-byte transcode impossible -- bail (FLAG).\n",
                    s_pLowAptCopy);
                CgsDev::Log::WriteToLog(lac);
                return;
            }
            s_uLowAptCopyBytes = luSize;
            std::memcpy(s_pLowAptCopy, lpHigh, luSize);
            luBase = reinterpret_cast<uintptr_t>(s_pLowAptCopy);
            // re-point the const file into the low copy.
            luConstOff = luConstOff;   // same offset, different base
            std::snprintf(lac, sizeof(lac),
                "[AptRT] faithful: GUIAPT32 1:7:4 -> low copy @ 0x%08X (%u bytes), FixupTranscode.\n",
                static_cast<u32>(luBase), luSize);
            CgsDev::Log::WriteToLog(lac);
        }

        AptConstFile* lpConstFile = reinterpret_cast<AptConstFile*>(luBase + luConstOff);
        // mnDataRootOffset: the reconstructed AptConstFile reads it as a u32 at +0x14 (the 4-byte
        // format). For the 8-byte format the signature is 16 bytes followed by a u64 data-root offset,
        // so read it ptr-size-aware (u64 @ const+16 for 8-byte; the struct field for 4-byte).
        u32 luDataRootOff = (liPtrSize == 8)
            ? static_cast<u32>(*reinterpret_cast<const u64*>(luBase + luConstOff + 16u))
            : lpConstFile->mnDataRootOffset;
        std::snprintf(lac, sizeof(lac),
            "[AptRT] faithful: constFile@0x%X ptrSize=%d dataRootOff=0x%X.\n",
            luConstOff, liPtrSize, luDataRootOff);
        CgsDev::Log::WriteToLog(lac);

        // --- 2. locate the MOVIE ROOT (def base) -- VERIFIED scheme -------------------------------
        // USER (2026-06-30, verified vs the real GUIAPT64/TITLE_SCREEN02 bytes): the const-file
        // dataRootOffset does NOT point at the movie root on the native bundle (it reads 0xB0). Instead
        // SCAN for the character signature 0x09876543: for each hit header = sigOff-8, def-base =
        // header+0x20 (native-8; +0x10 console). The MOVIE ROOT is the UNIQUE type-9 record with sane
        // screen dims (w 320..2048, h 240..1536, ms 10..100). For TITLE_SCREEN02 that's the only one of
        // 36 signatures that validates (def-base @res+0x4950: charCount=41, w=1280, h=720, ms=33).
        // The FixupWalk reads its def-base via the (now ptr-size-aware) AptOffsets; we hand it the def
        // base directly as `pThis`. [4-byte path: keep the CompleteLoad dataRoot+16 location.]
        u32 luCAOff = 0;   // byte offset of the AptCharacterAnimation def base
        if (liPtrSize == 8)
        {
            const u32 kHdrToDefBase = 0x20u;   // native-8 character header size (def base @ header+0x20)
            for (u32 luScan = 0; luScan + 4u <= luSize; luScan += 4u)
            {
                if (*reinterpret_cast<const u32*>(luBase + luScan) != 0x09876543u)
                    continue;
                const u32 luHdr = luScan - 8u;             // sig @ header+8
                if (luScan < 8u) continue;
                const u32 luType = *reinterpret_cast<const u32*>(luBase + luHdr);   // header+0 type
                const u32 luDB   = luHdr + kHdrToDefBase;
                if (luDB + 0x50u > luSize) continue;
                const u32 luW  = *reinterpret_cast<const u32*>(luBase + luDB + 0x28u);
                const u32 luH  = *reinterpret_cast<const u32*>(luBase + luDB + 0x2Cu);
                const u32 luMs = *reinterpret_cast<const u32*>(luBase + luDB + 0x30u);
                const u32 luCC = *reinterpret_cast<const u32*>(luBase + luDB + 0x18u);
                if (luType == 9u && luW >= 320u && luW <= 2048u && luH >= 240u && luH <= 1536u &&
                    luMs >= 10u && luMs <= 100u && luCC >= 1u && luCC <= 512u)
                {
                    luCAOff = luDB;
                    const u32 luFC = *reinterpret_cast<const u32*>(luBase + luDB + 0x00u);
                    std::snprintf(lac, sizeof(lac),
                        "[AptRT] faithful: movie root sig@0x%X defbase@0x%X type=9 frameCount=%u "
                        "charCount=%u w=%u h=%u ms=%u\n",
                        luScan, luDB, luFC, luCC, luW, luH, luMs);
                    CgsDev::Log::WriteToLog(lac);
                    break;
                }
            }
            if (luCAOff == 0)
                CgsDev::Log::WriteToLog("[AptRT] faithful: 8-byte signature scan found no sane type-9 "
                                        "movie root -- bail to fallback (FLAG).\n");
        }
        else
        {
            // 4-byte (GUIAPT32) path: the CompleteLoad-faithful dataRoot+16, validated.
            if (luDataRootOff != 0 && luDataRootOff + 16u + 0x30u <= luSize)
            {
                const u32 luTry = luDataRootOff + 16u;
                u32 luScore = 0;
                if (ValidateCharAnimRoot(luBase, luTry, luSize, 4, &luScore))
                    luCAOff = luTry;
            }
            if (luCAOff == 0)
            {
                for (u32 luScan = 16u; luScan + 0x30u <= luSize; luScan += 4u)
                {
                    u32 luScore = 0;
                    if (ValidateCharAnimRoot(luBase, luScan, luSize, 4, &luScore)) { luCAOff = luScan; break; }
                }
            }
        }

        if (luCAOff == 0)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: no valid AptCharacterAnimation found -- bail to "
                                    "geometry fallback, game stays up (FLAG).\n");
            return;
        }

        // The def base IS what the (ptr-size-aware) FixupWalk reads as `pThis`. The movie-root CHARACTER
        // header (where mnType==9 lives, read by the AptCharacterInst ctor) is at def-base - headerSize
        // (native-8 header 0x20; console 0x10). pFile->mpData must point at that CHARACTER header (not
        // the def base) -- MakeCharacterAnimationInst builds the inst over `(AptCharacter*)mpData`.
        const u32 luHdrSize = (liPtrSize == 8) ? 0x20u : 0x10u;
        const u32 luCharHdrOff = (luCAOff >= luHdrSize) ? (luCAOff - luHdrSize) : luCAOff;
        void* lpResolveBase = reinterpret_cast<void*>(luBase);
        void* lpDataRoot    = reinterpret_cast<void*>(luBase + luCharHdrOff);  // movie-root character header
        AptCharacterAnimation* lpCharAnim =
            reinterpret_cast<AptCharacterAnimation*>(luBase + luCAOff);

        // --- 3. Fixup the movie: relocate the serialised root IN PLACE against the load base. -------
        // The faithful Fixup (single native-64-bit path, no bounds guards) relocates every file offset
        // to a live pointer. The relocation base is the memory address of aptDataOffset (the "Apt
        // Data:1:7:8" magic) -- serialised offsets are file-relative to THAT, not the resource start.
        // Locate it by scanning the loaded resource for the magic. FLAG (Step 9): AptDataHeader::FixUp
        // will supply aptDataOffset directly; until then the bring-up finds it here.
        uintptr_t luAptDataOff = 0;
        {
            static const char kMagic[] = "Apt Data:1:7:8";
            const u32 kMagicLen = (u32)(sizeof(kMagic) - 1);
            for (u32 s = 0; s + kMagicLen <= luSize; ++s)
            {
                const char* p = reinterpret_cast<const char*>(luBase + s);
                u32 k = 0;
                for (; k < kMagicLen; ++k) { if (p[k] != kMagic[k]) break; }
                if (k == kMagicLen) { luAptDataOff = luBase + s; break; }
            }
        }
        void* lpFixupBase = reinterpret_cast<void*>(luAptDataOff ? luAptDataOff : luBase);
        std::snprintf(lac, sizeof(lac),
            "[AptRT] faithful: Fixup base (aptDataOffset)=0x%llX; calling Fixup on charAnim@0x%X ...\n",
            (unsigned long long)reinterpret_cast<uintptr_t>(lpFixupBase), luCAOff);
        CgsDev::Log::WriteToLog(lac);
        AptCharacterAnimation* lpFixed = lpCharAnim->Fixup(lpFixupBase, lpConstFile, lpFixupBase);
        std::snprintf(lac, sizeof(lac),
            "[AptRT] faithful: Fixup COMPLETED. charCount@+0x18=%d importCount@+0x34=%d\n",
            *reinterpret_cast<const int*>(reinterpret_cast<char*>(lpCharAnim) + 0x18),
            *reinterpret_cast<const int*>(reinterpret_cast<char*>(lpCharAnim) + 0x34));
        CgsDev::Log::WriteToLog(lac);

        // --- 3b. THE FRAME TIMELINE is NOT relocated here. -----------------------------------------
        // FLAG (Step 5/6): the root movie's frame table (framesOffset@def+0x08) + the case-5/9 sub-movie
        // timelines are relocated by the faithful AptMovie::resolve (called from Fixup) + AptLoader::
        // CompleteLoad -- NOT by a host routine. The invented AptRelocateTimeline8* + the frame-array
        // "fallback" (a wrong-base artifact: the old code used the resource start, so def+0x08 read a
        // mis-based offset) are gone. Until the 64-bit AptMovie::resolve lands, the timeline stays
        // un-resolved (the geometry fallback still renders); the composed animation arrives with Step 5/6.
        // Keep the resource bounds for the native-8 PLACE path's deref guard (gAptCmd8BoundLo/Hi):
        // doFrameControls reads the relocated command records + char table; bound them so an un-widened
        // record/char is skipped (named via the place probe) instead of AV'ing. Only on the native-8 path.
        if (liPtrSize == 8)
        {
            gAptCmd8BoundLo = luBase;
            gAptCmd8BoundHi = luBase + luSize;
        }
        else
        {
            gAptCmd8BoundLo = 0; gAptCmd8BoundHi = 0;
        }
        s_pFaithfulCharAnim = lpFixed;
        if (lpFixed == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: Fixup returned null -- bail (FLAG).\n");
            return;
        }
        {
            // Read the counts via the SERIALISED def-base offsets the Fixup walks (ptr-size aware:
            // 8-byte -> charCount+0x18, importCount+0x34, initCount+0x40; 4-byte -> +0x0C/+0x20/+0x28).
            // RE-VALIDATE they are sane post-fixup.
            const u32 luOffCC = (liPtrSize == 8) ? 0x18u : 0x0Cu;
            const u32 luOffIC = (liPtrSize == 8) ? 0x34u : 0x20u;
            const u32 luOffNC = (liPtrSize == 8) ? 0x40u : 0x28u;
            const u32 luPostCharCount = *reinterpret_cast<const u32*>(luBase + luCAOff + luOffCC);
            const u32 luPostImpCount  = *reinterpret_cast<const u32*>(luBase + luCAOff + luOffIC);
            const u32 luPostInitCount = *reinterpret_cast<const u32*>(luBase + luCAOff + luOffNC);
            std::snprintf(lac, sizeof(lac),
                "[AptRT] faithful: Fixup -> charAnim %p (chars=%u imports=%u init/export=%u)\n",
                (void*)lpFixed, luPostCharCount, luPostImpCount, luPostInitCount);
            CgsDev::Log::WriteToLog(lac);
            if (luPostCharCount == 0u || luPostCharCount > 512u ||
                luPostImpCount > 4096u || luPostInitCount > 4096u)
            {
                CgsDev::Log::WriteToLog("[AptRT] faithful: post-fixup counts INSANE -- bail to fallback (FLAG).\n");
                return;
            }
        }

        // --- 4. synthesise a loaded AptFile around the fixed-up root (MakeCharacterAnimationInst reads
        //        pFile->mpData as the movie root) -------------------------------------------------
        // The instantiation reads pFile->mpData (the AptMovieData root, whose +0x10 is the embedded
        // AptCharacterAnimation). We point mpData at lpDataRoot (the root; charAnim is at +16).
        s_pFaithfulAptFile = static_cast<AptFile*>(gpNonGCPoolManager->Allocate(sizeof(AptFile)));
        if (s_pFaithfulAptFile == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: AptFile alloc failed -- bail (FLAG).\n");
            return;
        }
        std::memset(s_pFaithfulAptFile, 0, sizeof(AptFile));
        s_pFaithfulAptFile->mnRefCount      = 1;
        s_pFaithfulAptFile->mnState         = 4;          // loaded
        s_pFaithfulAptFile->mnField12       = 1;
        s_pFaithfulAptFile->mpData          = lpDataRoot;    // the movie root (charAnim @ +16)
        s_pFaithfulAptFile->mpResolveContext = lpResolveBase; // the real (8-byte) or low-copy (4-byte) base
        s_pFaithfulAptFile->mpDataBlock     = lpResolveBase;

        // --- 5. the root level-0 CIH on the director's root display list ---------------------------
        // SELF-TEST the GC pool first: AptGetAnimationAtLevel(0) allocates an AptCIH(40) from the GC
        // pool (gpGCPoolManager). With the x64 size-statics override above (max=256), a 40-byte carve
        // must succeed. Probe it BEFORE the engine call so the run shows whether the GC alloc is the
        // crash (vs the display-list search). Free the probe alloc so it does not leak the slot.
        std::snprintf(lac, sizeof(lac),
            "[AptRT] faithful: GC pool self-test: gcMax=%u gcMin=%u sizeOff=%u; Allocate(40) ...\n",
            (unsigned)gAptValueGCMaxItemSize, (unsigned)gAptValueGCMinItemSize,
            (unsigned)gAptValueGCSizeOffset);
        CgsDev::Log::WriteToLog(lac);
        if (gpGCPoolManager == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: GC pool null -- bail (FLAG).\n");
            return;
        }
        {
            void* lpGCTest = gpGCPoolManager->Allocate(40);
            std::snprintf(lac, sizeof(lac), "[AptRT] faithful: GC Allocate(40) = %p %s\n",
                          lpGCTest, lpGCTest ? "(ok)" : "(FAILED)");
            CgsDev::Log::WriteToLog(lac);
            if (lpGCTest == nullptr)
            {
                CgsDev::Log::WriteToLog("[AptRT] faithful: GC pool cannot carve 40 bytes -- bail (FLAG).\n");
                return;
            }
            gpGCPoolManager->Deallocate(lpGCTest, 40);   // return the probe slot
        }

        // FLAG: AptGetAnimationAtLevel(0) searches the director's root display list then (if absent)
        // creates an AptCIH via AptCIH::operator new(40) -> the GC pool (now correctly sized) and derefs
        // mpCharacterInst->mpRenderItem. Guarded below (null -> bail to fallback).
        CgsDev::Log::WriteToLog("[AptRT] faithful: AptGetAnimationAtLevel(0) (create root CIH) ...\n");
        AptCIH* lpRootCIH = AptGetAnimationAtLevel(0);
        if (lpRootCIH == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: root CIH null (GC pool / display list) -- bail (FLAG).\n");
            return;
        }
        s_pFaithfulRootCIH = lpRootCIH;
        std::snprintf(lac, sizeof(lac), "[AptRT] faithful: root CIH %p created on director's root list.\n",
                      (void*)lpRootCIH);
        CgsDev::Log::WriteToLog(lac);

        // --- 6. instantiate the movie + bind it to the root CIH -----------------------------------
        // The native-8 movie's embedded AptCharacterAnimation character TABLE is only partly relocated
        // by FixupInPlace, so MakeCharacterAnimationInst's IncCharacterList walk (mpCharacterTable[i]
        // -> pCharacter->*) would read serialized 4-byte offsets as 64-bit pointers and AV. Gate it off
        // on the native-8 path (the char-list registration is pure AS-lookup bookkeeping, not needed for
        // the tick/render path yet); the walk resumes once the character records are fully widened. Set
        // BEFORE MakeCharacterAnimationInst so the gate is live when IncCharacterList is reached.
        gAptSkipCharList = (liPtrSize == 8) ? 1u : 0u;
        CgsDev::Log::WriteToLog("[AptRT] faithful: MakeCharacterAnimationInst(pFile) ...\n");
        AptCharacterAnimationInst* lpInst = MakeCharacterAnimationInst(s_pFaithfulAptFile);
        if (lpInst == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: MakeCharacterAnimationInst null -- bail (FLAG).\n");
            return;
        }
        std::snprintf(lac, sizeof(lac), "[AptRT] faithful: animInst %p; SetCharacterInst on root CIH ...\n",
                      (void*)lpInst);
        CgsDev::Log::WriteToLog(lac);
        lpRootCIH->SetCharacterInst(reinterpret_cast<AptCharacterInst*>(lpInst), true);

        // Set the embedded-movie offset for the per-frame tick (AptCIH_GetClipMovie reads
        // character + gAptCharMovieOffset): native-8 -> 0x20, console-4 -> 0x10.
        gAptCharMovieOffset = (liPtrSize == 8) ? 0x20u : 0x10u;
        // The native-8 TIMELINE is now RELOCATED (FixupWalk case-5/9) and the PLACE path is widened
        // (AptMovie::doFrameControls / gAptCmd8). So:
        //   * CLEAR gAptSkipTimeline -> doFrameControls runs frame 0 and PLACES its characters.
        //   * SET gAptCmd8 -> doFrameControls reads the x64 command-record offsets + uses placeObjectNCXForm.
        //   * SET gAptSkipFrameActions -> queueFrameActions + the AS clip events stay skipped (the AS
        //     director queue / interpreter scope is the NEXT pass).
        gAptSkipTimeline     = 0u;
        gAptCmd8             = (liPtrSize == 8) ? 1u : 0u;
        gAptSkipFrameActions = (liPtrSize == 8) ? 1u : 0u;
        gAptPlacedNodes      = 0;

        s_bFaithfulInstantiated = true;
        std::snprintf(lac, sizeof(lac),
            "[AptRT] faithful: INSTANTIATED -- root CIH %p <- animInst %p (movie root %p); "
            "displayList live. charMovieOff=0x%X. (STEP 2 tick next.)\n",
            (void*)lpRootCIH, (void*)lpInst, lpDataRoot, gAptCharMovieOffset);
        CgsDev::Log::WriteToLog(lac);
    }

    // Resolve + validate the movie geometry once at load time, choosing the pointer-size path.
    static void ResolveMovieGeometry(CgsResource::Entry* lpEntry)
    {
        s_bGeomResolved = false;
        if (s_uAptResourceBase == 0 || s_uGeomOffset == 0)
        {
            CgsDev::Log::WriteToLog("[AptRT] xcode: no geometry offset (mpGeomStruct==0) -- bail (FLAG).\n");
            return;
        }
        const u32 luResourceSize =
            lpEntry->mResourceDescriptor.m_baseResourceDescriptors[CgsResource::E_MEMTYPE_MAINMEMORY].m_size;

        char lac[200];
        u32 luNumFiles = 0, luFileTableOff = 0;

        // DISPATCH: try 4-byte (this bundle's format). The explicit "1:7:<n>" descriptor lives in
        // the bundle debug-name table (discarded by the Pool), so we infer structurally: if the
        // 4-byte read is plausible, it is the console 4-byte format. (A native 1:7:8 bundle would
        // fail this 4-byte read -- its 64-bit fields make numFiles/offset implausible -- and take
        // the 8-byte branch.)
        if (ValidateGeom4(luResourceSize, &luNumFiles, &luFileTableOff))
        {
            s_iAptPointerSize = 4;
            s_bGeomResolved   = true;
            std::snprintf(lac, sizeof(lac),
                "[AptRT] xcode: aptPtrSize=4 -> transcode. geom@0x%X resSize=%u files=%u fileTableOff=0x%X\n",
                s_uGeomOffset, luResourceSize, luNumFiles, luFileTableOff);
            CgsDev::Log::WriteToLog(lac);

            // One-time probe of file 0 + its first mesh, so the run shows real geometry resolving.
            const u32* lpFileTable = ResolveOff<const u32>(luFileTableOff, luResourceSize);
            if (lpFileTable != nullptr)
            {
                const u32* lpFile0 = ResolveOff<const u32>(lpFileTable[0], luResourceSize);
                if (lpFile0 != nullptr)
                {
                    const u32 luNumMeshes0   = lpFile0[1];
                    const u32 luMeshTableOff0 = lpFile0[2];
                    u32 luVerts0 = 0, luMeshType0 = 0xFFFFFFFFu, luVertOff0 = 0;
                    const u32* lpMeshTable0 = ResolveOff<const u32>(luMeshTableOff0, luResourceSize);
                    if (lpMeshTable0 != nullptr)
                    {
                        const u32* lpMesh0 = ResolveOff<const u32>(lpMeshTable0[0], luResourceSize);
                        if (lpMesh0 != nullptr)
                        {
                            luMeshType0 = lpMesh0[0];
                            luVerts0    = lpMesh0[4];
                            luVertOff0  = lpMesh0[5];
                        }
                    }
                    std::snprintf(lac, sizeof(lac),
                        "[AptRT] xcode: file0 id=0x%X meshes=%u | mesh0 type=%u verts=%u vertTableOff=0x%X\n",
                        lpFile0[0], luNumMeshes0, luMeshType0, luVerts0, luVertOff0);
                    CgsDev::Log::WriteToLog(lac);
                }
            }
            return;
        }

        // 8-byte native path: the fields are real 64-bit pointers (relocated in place by the load).
        // FLAG: not reached by the current 4-byte bundle; wired so a converted 1:7:8 bundle resolves
        // its geometry natively. We validate the 64-bit numFiles at the geom object's first dword.
        // (The 8-byte object layout is {u32 numFiles, u32 numTexPages, u64 fileTablePtr}.)
        const u32* lpObj8 = ResolveOff<const u32>(s_uGeomOffset, luResourceSize);
        if (lpObj8 != nullptr && lpObj8[0] != 0 && lpObj8[0] <= 4096u)
        {
            s_iAptPointerSize = 8;
            s_bGeomResolved   = true;
            CgsDev::Log::WriteToLog("[AptRT] xcode: aptPtrSize=8 -> in-place (native x64). "
                                    "8-byte geometry walk wired (FLAG: untested -- no 8-byte bundle yet).\n");
            return;
        }

        CgsDev::Log::WriteToLog("[AptRT] xcode: geometry failed BOTH 4-byte and 8-byte validation "
                                "(garbage offsets / unexpected format) -- bail (FLAG). Default would be 4-byte.\n");
    }

    // -------------------------------------------------------------------------
    // RenderLoadedGeometryDirect -- the achievable render milestone. Walk the loaded movie's
    // geometry (resolving every serialised offset against the FULL 64-bit base) and draw each
    // mesh's vertex run through the PROVEN immediate-mode renderer lpIm2d (CgsGraphics::Im2d --
    // the SAME one the loading screen + debug HUD draw through). Returns the number of meshes drawn.
    //
    // RENDER PATH CHOICE: the raw ImRenderBuffer<V> double-buffer path (Clear/BeginRendering/Swap/
    // Dispatch) AV'd on first runtime use -- it was reconstructed but NEVER exercised (the working
    // 2D paths all use the Im2d IMMEDIATE wrapper). So we draw through lpIm2d->Render directly: it
    // immediately folds each run to screen-space and issues DrawPrimitiveUP -- no double-buffer, no
    // Dispatch. lpIm2d is BrnRendererModule's own mIm2dRenderer (battle-tested), passed in by the
    // render hook. Im2d::Render ALWAYS draws a triangle STRIP (it ignores the primitive type) and
    // caps at 64 verts/call; a type-0 (tri-LIST) run is drawn as separate 3-vertex strips (each ==
    // one triangle) so the list tessellation is exact. Mode-1 meshes bind their real imported texture
    // (ResolveMeshTexture); mode-0 draw untextured (vertex colour). FLAG: the per-shape AS transforms/
    // visibility (the final animated layout) are absent -- geometry is DRAWN at its authored coords,
    // textured where a texture resolves, but not animated (that needs the un-homed display-list/runStream).
    // -------------------------------------------------------------------------
    static s32 RenderLoadedGeometryDirect(CgsGraphics::Im2d* lpIm2d)
    {
        if (!s_bGeomResolved || !s_bAuxReady || lpIm2d == nullptr)
            return 0;
        if (s_iAptPointerSize != 4)
            return 0;   // FLAG: 8-byte native walk untested (no 1:7:8 bundle) -- submit nothing.

        const bool lbProbe = !s_bRenderProbed;

        // No texture (untextured -> vertex colour); the Im2d untextured path drives the stage from
        // DIFFUSE. FLAG: real per-mesh textures are the follow-on.
        if (lbProbe) CgsDev::Log::WriteToLog("[AptRT] render: enter -> SetTexture(null) ...\n");
        lpIm2d->SetTexture(static_cast<renderengine::Texture*>(nullptr));

        // REAL resource-size bound: every offset must land INSIDE the AptData resource.
        const u32 luBound = (s_uAptResourceSize != 0u) ? s_uAptResourceSize : KU_MOVIE_POOL_BYTES;

        const u32* lpObj = ResolveOff<const u32>(s_uGeomOffset, luBound);
        if (lpObj == nullptr)
            return 0;
        const u32 luNumFiles     = lpObj[0];
        const u32 luFileTableOff = lpObj[2];
        if (luNumFiles == 0 || luNumFiles > 4096u)
            return 0;
        const u32* lpFileTable = ResolveOff<const u32>(luFileTableOff, luBound);
        if (lpFileTable == nullptr)
            return 0;
        if (lbProbe) CgsDev::Log::WriteToLog("[AptRT] render: enter mesh loop ...\n");

        s32 liMeshesSubmitted = 0;
        s32 liVertsSubmitted   = 0;
        for (u32 luFile = 0; luFile < luNumFiles; ++luFile)
        {
            const u32* lpFile = ResolveOff<const u32>(lpFileTable[luFile], luBound);
            if (lpFile == nullptr)
            {
                if (lbProbe) { char lf[96]; std::snprintf(lf, sizeof(lf),
                    "[AptRT] render: file%u SKIP (file offset out of range)\n", luFile);
                    CgsDev::Log::WriteToLog(lf); }
                continue;
            }
            const u32 luNumMeshes   = lpFile[1];
            const u32 luMeshTableOff = lpFile[2];
            if (lbProbe)
            {
                char lf[128];
                std::snprintf(lf, sizeof(lf), "[AptRT] render: file%u numMeshes=%u meshTableOff=0x%X\n",
                              luFile, luNumMeshes, luMeshTableOff);
                CgsDev::Log::WriteToLog(lf);
            }
            if (luNumMeshes == 0 || luNumMeshes > 65536u)
                continue;
            const u32* lpMeshTable = ResolveOff<const u32>(luMeshTableOff, luBound);
            if (lpMeshTable == nullptr)
                continue;

            for (u32 luMesh = 0; luMesh < luNumMeshes; ++luMesh)
            {
                const u32* lpMesh = ResolveOff<const u32>(lpMeshTable[luMesh], luBound);
                if (lpMesh == nullptr)
                    continue;
                const s32 liMeshType    = static_cast<s32>(lpMesh[0]);   // +0
                const s32 liTexMode     = static_cast<s32>(lpMesh[1]);   // +4
                u32       luNumVerts    = lpMesh[4];                     // +16 (CLOBBERED for mode-1, see below)
                const u32 luVertTableOff = lpMesh[5];                    // +20

                // The vertex table: ONE u32 offset per vertex (each points at a Basic2dColoured
                // TexturedVertex; consecutive entries are 20 bytes apart). We always derive the run
                // start + the COUNT from this table, NOT from mesh+16 -- because for a textured
                // (texMode==1) mesh the bundle's import-resolve wrote an 8-byte texture pointer into
                // the texPtr slot (mesh+12), and on x64 that 8-byte write CLOBBERS mesh+16 (numVerts)
                // with the high 32 bits of the pointer (-> a huge garbage count). So count vertices by
                // walking the vert table while each entry is a sane, in-range, monotonically-advancing
                // vertex offset. This recovers the real count for both clean + import-clobbered meshes.
                const u32* lpVertTable = ResolveOff<const u32>(luVertTableOff, luBound);
                if (lpVertTable == nullptr)
                {
                    if (lbProbe) { char ls[128]; std::snprintf(ls, sizeof(ls),
                        "[AptRT] render: file%u mesh%u SKIP (vertTableOff 0x%X out of range)\n",
                        luFile, luMesh, luVertTableOff); CgsDev::Log::WriteToLog(ls); }
                    continue;
                }
                const u32 luVertStride = static_cast<u32>(sizeof(CgsGraphics::Basic2dColouredTexturedVertex));
                const u32 luVertRunOff = lpVertTable[0];
                // Count consecutive verts (table[i] == table[0] + i*stride, all in-range), capped at 64
                // (Im2d's per-call limit) -- this is the AUTHORITATIVE count, replacing the clobbered field.
                // Bound the TABLE read itself to the resource so reading lpVertTable[lv] never over-reads.
                u32 luCountedVerts = 0;
                for (u32 lv = 0; lv < 64u; ++lv)
                {
                    // The table entry lpVertTable[lv] lives at (luVertTableOff + lv*4); stop before it
                    // would read past the resource.
                    if (luVertTableOff + (lv + 1u) * 4u > luBound)
                        break;
                    const u32 luVOff = lpVertTable[lv];
                    if (luVOff != luVertRunOff + lv * luVertStride)
                        break;
                    if (luVOff == 0 || luVOff + luVertStride > luBound)
                        break;
                    ++luCountedVerts;
                }
                luNumVerts = luCountedVerts;

                if (luNumVerts < 3u)
                {
                    if (lbProbe) { char ls[160]; std::snprintf(ls, sizeof(ls),
                        "[AptRT] render: file%u mesh%u type=%d texMode=%d SKIP (recovered verts=%u < 3)\n",
                        luFile, luMesh, liMeshType, liTexMode, luNumVerts); CgsDev::Log::WriteToLog(ls); }
                    continue;
                }
                const u32 luRunBytes = luNumVerts * luVertStride;
                if (luVertRunOff == 0 || luVertRunOff >= luBound || luRunBytes > luBound - luVertRunOff)
                {
                    if (lbProbe) { char ls[160]; std::snprintf(ls, sizeof(ls),
                        "[AptRT] render: file%u mesh%u SKIP (vert run 0x%X+%u out of range)\n",
                        luFile, luMesh, luVertRunOff, luRunBytes); CgsDev::Log::WriteToLog(ls); }
                    continue;
                }
                const CgsGraphics::Basic2dColouredTexturedVertex* lpVerts =
                    reinterpret_cast<const CgsGraphics::Basic2dColouredTexturedVertex*>(
                        s_uAptResourceBase + luVertRunOff);

                // ---- TEXTURE: bind the real renderengine::Texture* for mode-1 meshes. The bundle's
                // import-resolve wrote the imported texture resource's m_baseResources[0] (a realised
                // renderengine::Texture, mpD3DTexture already created by its FixUp) as a 64-bit pointer
                // into the mesh's texPtr slot (mesh+12). ResolveMeshTexture reassembles + validates it;
                // Im2d::SetTexture binds texture->mpD3DTexture. mode-0 / unresolved -> null (untextured
                // vertex colour). No AV (validated before binding).
                renderengine::Texture* lpMeshTexture = ResolveMeshTexture(lpMesh, liTexMode);
                lpIm2d->SetTexture(lpMeshTexture);

                if (lbProbe)
                {
                    char lacm[224];
                    std::snprintf(lacm, sizeof(lacm),
                        "[AptRT] render: file%u mesh%u type=%d texMode=%d verts=%u(recovered) runOff=0x%X "
                        "tex=%p v0=(%.1f,%.1f) -> drawn\n",
                        luFile, luMesh, liMeshType, liTexMode, luNumVerts, luVertRunOff,
                        (void*)lpMeshTexture, lpVerts[0].mv2Pos.x, lpVerts[0].mv2Pos.y);
                    CgsDev::Log::WriteToLog(lacm);
                }

                // Draw through the PROVEN immediate path. Im2d::Render ALWAYS draws a triangle STRIP
                // (it ignores the primitive type). For a type-0 tri-LIST run we draw it as separate
                // 3-vertex strips (each == one triangle), reproducing the list exactly. Type 1/2/other
                // draw as one strip over the run.
                if (liMeshType == 0)
                {
                    for (u32 luBase = 0; luBase + 3u <= luNumVerts; luBase += 3u)
                        lpIm2d->Render(static_cast<renderengine::PrimitiveType>(6), lpVerts + luBase, 3u);
                }
                else
                {
                    lpIm2d->Render(static_cast<renderengine::PrimitiveType>(6), lpVerts, luNumVerts);
                }
                ++liMeshesSubmitted;
                liVertsSubmitted += static_cast<s32>(luNumVerts);
            }
        }

        if (lbProbe)
        {
            s_bRenderProbed = true;
            char lacd[128];
            std::snprintf(lacd, sizeof(lacd),
                "[AptRT] render: DONE first pass -- %d meshes (%d verts) drawn via Im2d.\n",
                liMeshesSubmitted, liVertsSubmitted);
            CgsDev::Log::WriteToLog(lacd);
        }
        s_iLastVertsSubmitted = liVertsSubmitted;
        return liMeshesSubmitted;
    }

    // -------------------------------------------------------------------------
    // AptRuntimeUpdate -- per-frame TICK only. Advances the frame counter and (when a movie is
    // loaded) would tick its timeline + ActionScript. The RENDER moved to AptRuntimeRender (the
    // proven immediate-mode Im2d path). Called from GuiModule::Update (runs before the render hook).
    // -------------------------------------------------------------------------
    void AptRuntimeUpdate()
    {
        if (!s_bRuntimeReady)
            return;

        ++s_iFrameCounter;
        const bool lbProbeFrame = (s_iFrameCounter % 30) == 1;   // ~every 30 frames

        // Nothing loaded yet -> nothing to tick/render this frame.
        if (!s_bMovieLoaded)
            return;

        // ---- STEP 2: TICK the instantiated faithful root CIH ---------------------------------------
        // The homed AptCIH::tick advances the clip's play-head; for a fresh clip it runs frame 0's
        // place/remove timeline commands (doFrameControls -> placeObject), POPULATING the display list
        // with child AptCIHs, then queues the frame's ActionScript. We drive it on the instantiated root
        // CIH each frame. The render-tree manager is the null stub, but AptRTM_CreateItem now falls back
        // to the homed Manager_CreateItem factory, so each placed inst gets a real render item (carrying
        // its character) -- which is what tick / AptCIH_GetClipMovie reach the movie through. Every deref
        // is guarded (the engine null-safety we added) so the tick completes structurally or bails.
        // FLAG (Step 5/6): AptCIH::tick -> AptMovie::doFrameControls reads the movie's frame table
        // (framesOffset@def+0x08). Step 1's faithful Fixup does NOT relocate that -- the frame table is
        // AptMovie::resolve / AptLoader::CompleteLoad's job, not Fixup's (the invented AptRelocateTimeline8
        // wrongly relocated it here, which is why the tick "worked" before). Until the faithful 64-bit
        // resolve/CompleteLoad lands, framesOffset is an un-relocated file offset, so ticking would AV.
        // Deferred here (marked boundary); the instantiated display list + the geometry fallback still
        // render. Set lbTickReady = true (or remove the guard) when Step 5/6 relocates the frame table.
        bool lbTickReady = false;   // non-const: flip (or remove the guard) when Step 5/6 lands
        if (lbTickReady && s_bFaithfulInstantiated && s_pFaithfulRootCIH != nullptr)
        {
            AptCIH* lpRoot = static_cast<AptCIH*>(s_pFaithfulRootCIH);
            // Mark the root clip "dirty" so AptCIH::tick processes it (mFlagsA bit25 == GetDirtyState,
            // the tick gate; the ctor leaves it clear). SetDirtyState(true,...) sets bit25. (FLAG: the
            // X360 dirties via the render-tree manager's propagation; we set it directly each frame.)
            if (!lpRoot->GetDirtyState())
                lpRoot->SetDirtyState(true, false);

            if (s_iTickFrame < 4)   // probe the first few frames (frame 0 is where placement happens)
            {
                char lacp[128];
                std::snprintf(lacp, sizeof(lacp), "[AptRT] tick: frame=%d -> AptCIH::tick(root %p) ...\n",
                              s_iTickFrame, (void*)lpRoot);
                CgsDev::Log::WriteToLog(lacp);
            }

            const int liTickResult = lpRoot->tick();   // advance frame 0 -> place characters

            // Walk the director's root display list + count the placed nodes (the placement result).
            s32 liNodes = 0;
            AptTarget* lpTgt = GetTarget();
            if (lpTgt != nullptr && lpTgt->mpAnimationTarget != nullptr)
            {
                AptDisplayList* lpRootList = lpTgt->mpAnimationTarget->GetRootDisplayList();
                AptDisplayListState* lpState = (lpRootList != nullptr) ? lpRootList->AsState() : nullptr;
                if (lpState != nullptr)
                {
                    for (AptCIH* lpN = lpState->mpFirst; lpN != nullptr && liNodes < 4096;
                         lpN = lpN->GetDisplayListNext())
                    {
                        ++liNodes;
                        if (s_iTickFrame < 2 && liNodes <= 6)
                        {
                            AptCharacterInst* lpCI = lpN->GetCharacterInst();
                            char lacn[160];
                            std::snprintf(lacn, sizeof(lacn),
                                "[AptRT] tick:   node %p charInst=%p renderItem=%p depth=%d\n",
                                (void*)lpN, (void*)lpCI,
                                (void*)(lpCI ? lpCI->GetRenderItem() : nullptr),
                                lpCI ? lpCI->GetDepth() : -1);
                            CgsDev::Log::WriteToLog(lacn);
                        }
                    }
                }
            }

            // Also count the ROOT CIH's CHILD display list -- doFrameControls places into the animInst's
            // own mDisplayList (the sprite/animation child list), not the director root. This is where the
            // native-8 place path's nodes land.
            s32 liChildNodes = 0;
            {
                AptCharacterInst* lpRootCI = lpRoot->GetCharacterInst();
                if (lpRootCI != nullptr)
                {
                    AptCharacterSpriteInstBase* lpSprite =
                        static_cast<AptCharacterSpriteInstBase*>(lpRootCI);
                    AptDisplayListState* lpChildState = lpSprite->mDisplayList.AsState();
                    if (lpChildState != nullptr)
                    {
                        for (AptCIH* lpN = lpChildState->mpFirst; lpN != nullptr && liChildNodes < 4096;
                             lpN = lpN->GetDisplayListNext())
                        {
                            ++liChildNodes;
                            if (s_iTickFrame < 2 && liChildNodes <= 8)
                            {
                                AptCharacterInst* lpCI = lpN->GetCharacterInst();
                                char lacn[160];
                                std::snprintf(lacn, sizeof(lacn),
                                    "[AptRT] tick:   child node %p charInst=%p renderItem=%p depth=%d\n",
                                    (void*)lpN, (void*)lpCI,
                                    (void*)(lpCI ? lpCI->GetRenderItem() : nullptr),
                                    lpCI ? lpCI->GetDepth() : -1);
                                CgsDev::Log::WriteToLog(lacn);
                            }
                        }
                    }
                }
            }

            if (s_iTickFrame < 4)
            {
                char lacd[192];
                std::snprintf(lacd, sizeof(lacd),
                    "[AptRT] tick: frame=%d displayList nodes=%d childNodes=%d placedByCmd=%d (tick result=%d)\n",
                    s_iTickFrame, liNodes, liChildNodes, gAptPlacedNodes, liTickResult);
                CgsDev::Log::WriteToLog(lacd);
            }
            ++s_iTickFrame;
        }
        else if (lbProbeFrame)
        {
            CgsDev::Log::WriteToLog("[AptRT] frame: no faithful root CIH -- tick skipped (geometry "
                                    "fallback render only) (FLAG).\n");
        }

        // The RENDER moved to AptRuntimeRender (the proven immediate-mode Im2d path, driven by the
        // renderer hook with BrnRendererModule's mIm2dRenderer).
    }

    // -------------------------------------------------------------------------
    // AptRuntimeRender -- draw the loaded movie's geometry THROUGH THE PROVEN IMMEDIATE PATH.
    // Called from BrnRendererModule::Render each frame with that module's mIm2dRenderer (a live,
    // battle-tested CgsGraphics::Im2d that the loading screen + debug HUD already draw through).
    // This REPLACES the raw ImRenderBuffer<V> Clear/BeginRendering/Swap/Dispatch path that AV'd on
    // first use. Bracketed by Im2d BeginRendering/EndRendering (installs the 2D D3D state); the
    // per-mesh runs draw immediately via DrawPrimitiveUP. Hard-gated + bounded -> no AV / no-op when
    // unresolved.
    // -------------------------------------------------------------------------
    void AptRuntimeRender(CgsGraphics::Im2d* lpIm2d)
    {
        if (!s_bRuntimeReady || !s_bMovieLoaded || !s_bGeomResolved || lpIm2d == nullptr)
            return;

        // (s_iFrameCounter is advanced by AptRuntimeUpdate, which runs before this each frame.)
        const bool lbProbeFrame = (s_iFrameCounter % 30) == 1;

        // Install the 2D immediate render state (no depth/cull, alpha-blend, vertex-colour modulate),
        // draw every mesh, then close the block. Im2d::BeginRendering no-ops cleanly when gDevice is
        // null, and Im2d::Render guards null/short runs -- so this stays up even mid-bring-up.
        lpIm2d->BeginRendering();
        const s32 liUnits = RenderLoadedGeometryDirect(lpIm2d);
        lpIm2d->EndRendering();

        if (lbProbeFrame)
        {
            char lac[128];
            std::snprintf(lac, sizeof(lac),
                "[AptRT] frame %d: drew %d quads (%d verts) via Im2d immediate path.\n",
                s_iFrameCounter, liUnits, s_iLastVertsSubmitted);
            CgsDev::Log::WriteToLog(lac);
        }
    }
}
