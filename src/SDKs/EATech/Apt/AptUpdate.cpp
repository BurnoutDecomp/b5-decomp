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

#include <cstdio>   // sprintf (the input-recorder frame stamp, gated off by default)
#include <cstring>  // memcpy / strlen (the saved-input stream reads)

#include "SDKs/EATech/include/Apt/AptTarget.h"                    // AptTarget + gpAptTarget + the target API
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"           // TickIntervalTimers / RunActions / ProcessInputs + mDisplayList
#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"    // the root animation inst (mnAccumulatedUpdateMs @+0x24)
#include "SDKs/EATech/include/Apt/AptRenderItem.h"                // mpCharacter
#include "SDKs/EATech/include/Apt/AptCharacter.h"                 // the movie root character
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"        // native authored frame period
#include "SDKs/EATech/include/Apt/AptLinker.h"                    // AptLinker::Update
#include "SDKs/EATech/include/Apt/AptCIH.h"                       // ProcessTextInst / ProcessCustomControls / ProcessMaskMatricies
#include "SDKs/EATech/include/Apt/AptValue/AptGCReleaseVector.h"  // gValuesToRelease (deferred-release flush)
#include "SDKs/EATech/include/Apt/AptGC.h"                        // AptGC::CleanUnreachable (the partial sweep)
#include "SDKs/EATech/include/Apt/Apt.h"                          // AptUserFunctions (the gAptFuncs recorder-sink slots)
#include "SDKs/EATech/include/Apt/AptSavedInputCheckpoints.h"     // CanContinueSavedInputs / Checkpoint (the replay gate)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"           // EAStringC (the replay checkpoint name)
#include "eathread/eathread_storage.h"                            // EA::Thread::ThreadLocalStorage (gAptTargetTls)

// ---- collaborator globals (all defined elsewhere; console addresses noted) ----------
extern bool gbAptZombiesDirty;                  // byte_8324E38F (AptGC.cpp; raised by AptPartialGarbageCollection)
extern int  gAptInputRecorderEnabled;           // dword_8324E518 (AptGlobals.cpp)
// The saved-input playback stream (AptGlobals.cpp): base (non-null while replaying),
// read cursor, byte size, and the replay frame counter.
extern const unsigned char* gpAptSavedInputStream;
extern const unsigned char* gpAptSavedInputCursor;
extern int                  gnAptSavedInputStreamSize;
extern unsigned int         gnAptSavedInputFrame;
extern uint32_t gAptInputRecorderTag;           // dword_8324D820 (AptGlobals.cpp; advanced per banked frame)
extern int  gnCurrUpdateTick;                   // dword_8324E520 (AptGlobals.cpp; the update-side tick bank)
extern int  gnCurrRenderTickConsumed;           // dword_8324E524 (AptGlobals.cpp; the render-side consumed tick)
// The two recorder sinks are NOT standalone globals: dword_8324E830/834 are
// gAptFuncs+0x18/+0x1C -- the pfnDebugAddSavedInput / pfnDebugSetScreenGrabPending
// members of the host user-function table (installed by AptAux::ConstructApt).
extern AptUserFunctions gAptFuncs;                          // dword_8324E818 (CgsAptAux.cpp)

// The animation-unresolve current-target TLS object (unk_8324E814; defined at global
// scope in AptGlobals.cpp).
extern EA::Thread::ThreadLocalStorage gAptTargetTls;

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
        std::sprintf(lacTag, "%06d", gAptInputRecorderTag);
        gAptFuncs.pfnDebugSetScreenGrabPending(lacTag);

        int laRecord[2];
        laRecord[0] = static_cast<int>(gAptInputRecorderTag);
        laRecord[1] = 0x03000000;
        gAptFuncs.pfnDebugAddSavedInput(reinterpret_cast<AptSavedInputRecord*>(laRecord), 8);
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
    uint32_t nMsPerFrame = AptGetMovieCharacterAnimation(
        pRootInst->mpRenderItem->mpCharacter)->mnMillisecondsPerFrame;

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
        AptCIHProcessCallback pPrevCb = AptCIH_sCIHProcessCb;
        AptCIHProcessCallback pPrevCb1 = AptCIH_sCIHProcessCb1;
        AptCIHProcessCallback pPrevCb2 = AptCIH_sCIHProcessCb2;
        AptCIH_sCIHProcessCb  = &AptCIH::ProcessTextInst;
        AptCIH_sCIHProcessCb1 = &AptCIH::ProcessCustomControls;
        AptCIH_sCIHProcessCb2 = &AptCIH::ProcessMaskMatricies;
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
// AptUpdateReplaySavedInputs -- the saved-input REPLAY driver: advance nFrames frames
// of a recorded session instead of live-ticking. The stream is a run of records, each a
// u32 frame stamp followed by a record whose first byte selects the kind:
//   kind&3 < 2 : an input event word (stored byte-reversed by the recorder); the two
//                stick-snapshot ids (501/502) carry a trailing 16-byte analog sample;
//   kind&3 == 2: a checkpoint -- a NUL-terminated movie name padded to 4 bytes;
//   kind&15 == 3 : a screen-grab marker; 7 : a u16-sized custom block for the host;
//   11 : an analog sample.
// Every record stamped at or before the current replay frame is fed, then one frame is
// ticked; while a checkpoint movie is still loading, only the interval timers run (or the
// linker is pumped). The end of the stream turns replay (and the recorder) off.
// ---------------------------------------------------------------------------
static u32 AptSavedInputReadU32(const unsigned char* pData)
{
    u32 nValue;
    std::memcpy(&nValue, pData, sizeof(nValue));
    return nValue;
}

static void AptUpdateReplaySavedInputs(int nFrames, int nDepthLayerMask)
{
    const unsigned int nTargetFrame = gnAptSavedInputFrame + static_cast<unsigned int>(nFrames);

    while (gpAptSavedInputStream != 0 && gnAptSavedInputFrame != nTargetFrame)
    {
        if (AptSavedInputCheckpoints::CanContinueSavedInputs(*gpAptSavedInputCheckpoints))
        {
            // Feed every record stamped at or before the current frame.
            while (AptSavedInputReadU32(gpAptSavedInputCursor) <= gnAptSavedInputFrame)
            {
                gpAptSavedInputCursor += 4;                               // past the frame stamp
                const unsigned char* const pRecord = gpAptSavedInputCursor;
                const unsigned char nKind = pRecord[0];
                AptAnimationTarget* const pAnim = gpAptTarget->mpAnimationTarget;

                if ((nKind & 3u) < 2u)
                {
                    const u32 nStored = AptSavedInputReadU32(pRecord);
                    const u32 nPacked = ((nStored & 0x000000FFu) << 24)
                                      | ((nStored & 0x0000FF00u) << 8)
                                      | ((nStored & 0x00FF0000u) >> 8)
                                      | ((nStored & 0xFF000000u) >> 24);
                    gpAptSavedInputCursor = pRecord + 4;
                    if ((nPacked >> 17) - 501u <= 1u)
                    {
                        pAnim->AddAnalogInput(*reinterpret_cast<const AptAnimationTarget::AptAnalogInputEvent*>(
                            gpAptSavedInputCursor));
                        gpAptSavedInputCursor += sizeof(AptAnimationTarget::AptAnalogInputEvent);
                    }
                    else
                    {
                        pAnim->AddInput(static_cast<int>(nPacked));
                    }
                }
                else if ((nKind & 3u) == 2u)
                {
                    const char* const pName = reinterpret_cast<const char*>(pRecord + 1);
                    uintptr_t luNext = reinterpret_cast<uintptr_t>(pName) + std::strlen(pName) + 1u;
                    if ((luNext & 3u) != 0u)
                        luNext = (luNext + 4u) & ~static_cast<uintptr_t>(3u);
                    gpAptSavedInputCursor = reinterpret_cast<const unsigned char*>(luNext);
                    EAStringC name(pName);
                    AptSavedInputCheckpoints::Checkpoint(*gpAptSavedInputCheckpoints, name);
                }
                else
                {
                    switch (nKind & 0xFu)
                    {
                        case 3u:   // screen-grab marker
                            gpAptSavedInputCursor = pRecord + 4;
                            if (!gAptInputRecorderEnabled)
                            {
                                char lacTag[96];
                                std::sprintf(lacTag, "%06d", static_cast<int>(gnAptSavedInputFrame));
                                gAptFuncs.pfnDebugSetScreenGrabPending(lacTag);
                            }
                            break;
                        case 7u:   // custom block for the host handler
                        {
                            const u16 nSize = *reinterpret_cast<const u16*>(pRecord + 2);
                            gpAptSavedInputCursor = pRecord + 4;
                            if (gAptFuncs.pfnCustomSavedInputHandler)
                                gAptFuncs.pfnCustomSavedInputHandler(
                                    const_cast<unsigned char*>(gpAptSavedInputCursor), nSize);
                            gpAptSavedInputCursor += nSize;
                            break;
                        }
                        case 11u:  // analog sample
                            pAnim->AddAnalogInput(*reinterpret_cast<const AptAnimationTarget::AptAnalogInputEvent*>(
                                pRecord + 4));
                            gpAptSavedInputCursor = pRecord + 4 + sizeof(AptAnimationTarget::AptAnalogInputEvent);
                            break;
                        default:
                            break;
                    }
                }
            }
        }
        else if (static_cast<int>(AptSavedInputReadU32(gpAptSavedInputCursor) - gnAptSavedInputFrame) - 1 > 0)
        {
            // Waiting on a checkpoint load with frames still to go: keep the timers running.
            gpAptTarget->mpAnimationTarget->TickIntervalTimers(16);
        }

        if (static_cast<int>(gpAptSavedInputCursor - gpAptSavedInputStream) >= gnAptSavedInputStreamSize)
        {
            // End of the stream: replay off, tell the host, stop recording.
            gpAptSavedInputStream = 0;
            gpAptSavedInputCursor = 0;
            if (gAptFuncs.pfnPlaySavedInputsDone)
                gAptFuncs.pfnPlaySavedInputsDone(true, 0);
            if (gAptInputRecorderEnabled)
                gAptInputRecorderEnabled = 0;
            return;
        }

        AptCIH* const pRootNode = gpAptTarget->mpAnimationTarget->mDisplayList.mpHead->mpFirst;
        if (!AptSavedInputCheckpoints::CanContinueSavedInputs(*gpAptSavedInputCheckpoints)
            || pRootNode == 0
            || (pRootNode->GetCharacterInst()->mTypeFlags & 0x3Fu) != 9u)   // x64 low-6-bit tag
        {
            gpAptTarget->mpLinker->Update();
            return;
        }

        if (AptUpdateRunTargetFrames(1, nDepthLayerMask, 1024))
            AptUpdateRecordFrame();
        ++gnAptSavedInputFrame;
    }
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

    if (gpAptSavedInputStream)
    {
        AptUpdateReplaySavedInputs(nElapsedMs, nDepthLayerMask);
    }
    else
    {
        AptAnimationTarget* pAnim = gpAptTarget->mpAnimationTarget;
        AptCIH* pRootNode = pAnim->mDisplayList.mpHead->mpFirst;
        if (pRootNode != 0
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

    if (gpValuesToRelease != 0)
        gpValuesToRelease->ReleaseValues();
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
