// ===========================================================================
// EATech Apt -- the per-frame update drivers. Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//
//   AptUpdateTarget          @ 0x82B0DE80  -- swap pTarget in as the current
//                                            context and run AptUpdate on it
//   AptUpdate                @ 0x82B0DB68  -- the public per-frame entry: replay
//                                            or live-tick the current target,
//                                            then flush the GC deferred releases
//   AptUpdateRunTargetFrames @ 0x82B0D608  -- (file-static sub_82B0D608) the
//                                            banked-milliseconds frame pacer +
//                                            tick loop for the current target
//   AptUpdateRecordFrame     @ 0x82AD9048  -- (file-static sub_82AD9048) the
//                                            input-recorder frame stamp
//
// B4Extern's PDB names the public entry `void AptUpdate(unsigned int nDeltaTime)`
// (apt_module_syms.txt); the Paradise-era build widens it with the depth-layer
// mask + banked-frame cap that AptAux::Update @0x82853B20 passes as (-1, 16).
// ===========================================================================

#include "types.hpp"

#include <cstdio>   // snprintf (the input-recorder frame stamp, gated off by default)

#include "SDKs/EATech/include/Apt/AptTarget.h"                    // AptTarget + gpAptTarget + the target API
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"           // TickIntervalTimers / RunActions / ProcessInputs + mDisplayList
#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"    // the root animation inst (mnAccumulatedUpdateMs @+0x24)
#include "SDKs/EATech/include/Apt/AptRenderItem.h"                // mpCharacter
#include "SDKs/EATech/include/Apt/AptCharacter.h"                 // the movie root character
#include "SDKs/EATech/include/Apt/AptFile.h"                      // AptMovieData (mnMillisecondsPerFrame view)
#include "SDKs/EATech/include/Apt/AptLinker.h"                    // AptLinker::Update
#include "SDKs/EATech/include/Apt/AptCIH.h"                       // ProcessTextInst / ProcessCustomControls / ProcessMaskMatricies
#include "SDKs/EATech/include/Apt/AptValue/AptGCReleaseVector.h"  // gValuesToRelease (deferred-release flush)
#include "SDKs/EATech/include/Apt/AptGC.h"                        // AptGC::CleanUnreachable (the partial sweep)

// ---- collaborator globals (all defined elsewhere; console addresses noted) ----------
extern bool gbAptZombiesDirty;                  // byte_8324E38F (AptGC.cpp; raised by AptPartialGarbageCollection)
extern int  gAptInputRecorderEnabled;           // dword_8324E518 (AptGlobals.cpp)
extern int  gbAptSavedInputActive;              // dword_8324D7F0 (AptGlobals.cpp)
extern uint32_t gAptInputRecorderTag;           // dword_8324D820 (AptGlobals.cpp; advanced per banked frame)
extern int  gnCurrUpdateTick;                   // dword_8324E520 (AptGlobals.cpp; the update-side tick bank)
extern int  gnCurrRenderTickConsumed;           // dword_8324E524 (AptGlobals.cpp; the render-side consumed tick)
extern int  (*gpAptInputRecorderSink)(void*, int);          // dword_8324E830 (AptGlobals.cpp)
extern void (*gpAptInputRecorderTagSink)(const char*);      // dword_8324E834 (AptGlobals.cpp)

// The three per-node generalised-process callback slots the update pass installs
// around AptDisplayList::GeneralisedProcess (dword_8324E41C/420/424; defined in
// AptRenderLinkStubs.cpp, read per node by AptCIH::GeneralisedProcess).
extern unsigned int (*AptCIH_sCIHProcessCb)(AptCIH*, AptCIH*, void*);    // dword_8324E41C
extern unsigned int (*AptCIH_sCIHProcessCb1)(AptCIH*, AptCIH*, void*);   // dword_8324E420
extern unsigned int (*AptCIH_sCIHProcessCb2)(AptCIH*, AptCIH*, void*);   // dword_8324E424

// The animation-unresolve current-target TLS object (unk_8324E814; defined at global
// scope in AptRenderLinkStubs.cpp -- single-threaded slot on the PC). The minimal
// ThreadLocalStorage view matches the AptRenderLinkStubs.cpp home declaration.
namespace EA { namespace Thread {
    class ThreadLocalStorage
    {
    public:
        ThreadLocalStorage() : mTlsIndex(0) {}
        bool  SetValue(const void* pData);
        void* GetValue();
        u32   mTlsIndex;
    };
}}
extern EA::Thread::ThreadLocalStorage gAptTargetTls;

// The saved-input REPLAY driver (sub_82B0D7E8, ~4.3KB): drains the recorded input
// stream instead of live-ticking. Gated on gbAptSavedInputActive (boot default 0);
// declared here, no-op link-stub in AptRenderLinkStubs.cpp until its own TU lands.
void AptUpdateReplaySavedInputs(int nElapsedMs, int nDepthLayerMask);

// The free-function adapters matching the process-callback slot signature; forward
// to the node's member pass (same pattern as AptProcessTextInstCb in
// AptCIHBehaviour.cpp, which homes the text adapter).
extern unsigned int AptProcessTextInstCb(AptCIH* pNode, AptCIH* pRoot, void* pCtx);
// FLAG PC-platform leaf: the console installs the member entry point into the plain
// callback slot directly (r3 == this); the x64 ABI needs a free-function adapter.
unsigned int AptCIH_ProcessCustomControlsCb(AptCIH* pNode, AptCIH* /*pRoot*/, void* /*pCtx*/)
{
    return pNode->ProcessCustomControls() ? 1u : 0u;
}
// FLAG PC-platform leaf: the console installs the member entry point into the plain
// callback slot directly (r3 == this); the x64 ABI needs a free-function adapter.
unsigned int AptCIH_ProcessMaskMatriciesCb(AptCIH* pNode, AptCIH* /*pRoot*/, void* /*pCtx*/)
{
    return pNode->ProcessMaskMatricies() ? 1u : 0u;
}

// ---------------------------------------------------------------------------
// AptUpdateRecordFrame (sub_82AD9048) -- when the input recorder is armed, stamp
// the current frame tag through the two host recorder sinks: the "%06d" text tag,
// then an 8-byte {tag, 0x03000000} record.
// ---------------------------------------------------------------------------
static void AptUpdateRecordFrame()
{
    if (gAptInputRecorderEnabled)
    {
        char lacTag[24];
        std::snprintf(lacTag, sizeof(lacTag), "%06d", gAptInputRecorderTag);
        if (gpAptInputRecorderTagSink)
            gpAptInputRecorderTagSink(lacTag);

        int laRecord[2];
        laRecord[0] = static_cast<int>(gAptInputRecorderTag);
        laRecord[1] = 0x03000000;
        if (gpAptInputRecorderSink)
            gpAptInputRecorderSink(laRecord, 8);
    }
}

// ---------------------------------------------------------------------------
// AptUpdateRunTargetFrames (sub_82B0D608) -- bank the elapsed milliseconds on the
// current target's root movie instance and run one full tick sequence (interval
// timers -> display-list tick -> deferred actions -> queued inputs -> linker) per
// authored frame period banked. After ticking, run the deferred generalised-process
// pass (text / custom controls / mask matrices) over the tree and bank one frame
// of update-tick credit (capped at nMaxBankedFrames ahead of the render side).
// Returns 1 when at least one frame ticked.
// ---------------------------------------------------------------------------
static int AptUpdateRunTargetFrames(int nElapsedMs, int nDepthLayerMask, int nMaxBankedFrames)
{
    int bTicked = 0;
    AptAnimationTarget* pAnim = gpAptTarget->mpAnimationTarget;

    AptCharacterInst* pRootInst = pAnim->mDisplayList.mpHead->mpFirst->GetCharacterInst();
    if ((pRootInst->mTypeFlags & 0x3Fu) != 9u)   // x64: tag in LOW 6 bits (X360 form 0xFC000000/0x24000000)
        return 0;

    AptCharacterAnimationInst* pRootAnimInst =
        static_cast<AptCharacterAnimationInst*>(pRootInst);
    uint32_t nBankedMs = pRootAnimInst->mnAccumulatedUpdateMs + nElapsedMs;
    uint32_t nMsPerFrame = reinterpret_cast<const AptMovieData*>(
        pRootInst->mpRenderItem->mpCharacter)->mnMillisecondsPerFrame;

    // The console TRAPS on a non-positive frame period (the twllei before the
    // banked-credit divide) -- a 0-period movie cannot be paced (both this loop
    // and the console's would never bank down). The MAIN framework bundle carries
    // an authored 0 in this field -- a known bundle-data defect (see the GUIAPT
    // bundle-defect notes). FLAG PC-platform leaf (data-defect guard, host-driver precedent): pace an
    // invalid period at the 30fps stand-in the retired host driver used. (A
    // CGS_ASSERT here PAUSES the game loop on the dev-assert screen every boot, so
    // the guard is silent by design.)
    if (nMsPerFrame == 0 || nMsPerFrame > 1000u)
        nMsPerFrame = 33;

    if (nBankedMs >= nMsPerFrame)
    {
        bTicked = 1;
        for (;;)
        {
            pAnim->TickIntervalTimers(nMsPerFrame);
            pAnim->mDisplayList.tick(nDepthLayerMask, 1);
            pAnim->RunActions();
            pAnim->ProcessInputs();
            gpAptTarget->mpLinker->Update();

            nBankedMs -= nMsPerFrame;
            gAptInputRecorderTag += nMsPerFrame;

            // The tick can retire the root (level unload): when the root instance is
            // no longer a class-tagged movie, bail WITHOUT the accumulator store or
            // the generalised-process pass (the X360 break path).
            AptCharacterInst* pCurrentRoot =
                gpAptTarget->mpAnimationTarget->mDisplayList.mpHead->mpFirst->GetCharacterInst();
            if ((pCurrentRoot->mTypeFlags & 0x3Fu) != 9u)   // x64 low-6-bit tag
                return 1;

            // Single-step when the input recorder is armed; otherwise keep catching
            // up while a full frame is banked.
            if (gAptInputRecorderEnabled || nBankedMs < nMsPerFrame)
                break;
        }
    }

    // Store the (partial-frame) remainder back on the root instance.
    {
        AptCharacterInst* pStoreRoot =
            gpAptTarget->mpAnimationTarget->mDisplayList.mpHead->mpFirst->GetCharacterInst();
        if ((pStoreRoot->mTypeFlags & 0x3Fu) == 9u)   // x64 low-6-bit tag
            static_cast<AptCharacterAnimationInst*>(pStoreRoot)->mnAccumulatedUpdateMs = nBankedMs;
    }

    if (bTicked)
    {
        // The deferred generalised-process pass: install the three per-node passes,
        // walk the tree once, restore the previous slots.
        unsigned int (*pPrevCb)(AptCIH*, AptCIH*, void*)  = AptCIH_sCIHProcessCb;
        unsigned int (*pPrevCb1)(AptCIH*, AptCIH*, void*) = AptCIH_sCIHProcessCb1;
        unsigned int (*pPrevCb2)(AptCIH*, AptCIH*, void*) = AptCIH_sCIHProcessCb2;
        AptCIH_sCIHProcessCb  = &AptProcessTextInstCb;
        AptCIH_sCIHProcessCb1 = &AptCIH_ProcessCustomControlsCb;
        AptCIH_sCIHProcessCb2 = &AptCIH_ProcessMaskMatriciesCb;
        pAnim->mDisplayList.GeneralisedProcess(0, nDepthLayerMask, 1);
        AptCIH_sCIHProcessCb  = pPrevCb;
        AptCIH_sCIHProcessCb1 = pPrevCb1;
        AptCIH_sCIHProcessCb2 = pPrevCb2;

        // Bank one frame of update-tick credit unless the update side is already the
        // full banked-frame cap ahead of the render side. The console performs the
        // add with an interrupt-masked lwarx/stwcx. reservation; the PC gate is
        // single-threaded, so a plain add reproduces the observable state.
        // (The X360 also traps on nMsPerFrame == 0 -- twllei -- before the divide.)
        if (nMsPerFrame != 0
            && static_cast<int>(static_cast<unsigned int>(gnCurrUpdateTick - gnCurrRenderTickConsumed) / nMsPerFrame)
                   < nMaxBankedFrames)
        {
            gnCurrUpdateTick += nMsPerFrame;
        }
    }
    return bTicked;
}

// ---------------------------------------------------------------------------
// AptUpdate @0x82B0DB68 -- the public per-frame entry for the CURRENT target.
// Replay recorded inputs when the saved-input driver is active; otherwise run the
// frame pacer over the live target (falling back to just pumping the linker when
// no class-tagged root movie is instantiated). Then flush the GC deferred-release
// vector and, when the zombies-dirty flag was raised, run the partial GC.
// ---------------------------------------------------------------------------
void AptUpdate(int nElapsedMs, int nDepthLayerMask, int nMaxBankedFrames)
{
    gAptTargetTls.SetValue(gpAptTarget);

    if (gbAptSavedInputActive)
    {
        AptUpdateReplaySavedInputs(nElapsedMs, nDepthLayerMask);
    }
    else
    {
        AptAnimationTarget* pAnim = gpAptTarget->mpAnimationTarget;
        AptCIH* pRootNode = pAnim->mDisplayList.mpHead->mpFirst;
        if (pRootNode != nullptr
            && (pRootNode->GetCharacterInst()->mTypeFlags & 0x3Fu) == 9u)   // x64 low-6-bit tag
        {
            if (AptUpdateRunTargetFrames(nElapsedMs, nDepthLayerMask, nMaxBankedFrames))
                AptUpdateRecordFrame();
        }
        else
        {
            // No live movie root yet: keep the loader/linker pumping and advance the
            // frame counter so pending imports still resolve.
            gpAptTarget->mpLinker->Update();
            ++gAptInputRecorderTag;
            pAnim->mnQueuedInputsCount = 0;
        }
    }

    gValuesToRelease.ReleaseValues();
    if (gbAptZombiesDirty)
    {
        AptGC::CleanUnreachable();
        gbAptZombiesDirty = false;
    }
}

// ---------------------------------------------------------------------------
// AptUpdateTarget @0x82B0DE80 -- run AptUpdate with pTarget swapped in as the
// current context (both the global and the TLS mirror), restoring the previous
// context afterwards.
// ---------------------------------------------------------------------------
void AptUpdateTarget(AptTarget* pTarget, int nElapsedMs, int nDepthLayerMask,
                     int nMaxBankedFrames)
{
    AptTarget* pPrevTarget = gpAptTarget;
    gpAptTarget = pTarget;
    gAptTargetTls.SetValue(pTarget);

    AptUpdate(nElapsedMs, nDepthLayerMask, nMaxBankedFrames);

    gpAptTarget = pPrevTarget;
    gAptTargetTls.SetValue(pPrevTarget);
}
