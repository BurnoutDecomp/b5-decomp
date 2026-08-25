#pragma once

// BrnAI::ResetOnTrackManager -- owner of the AI "reset a stranded/off-track race car back
// onto the route" subsystem. In this batch ONLY GetAICar (@0x82765878) is bodied; the
// remaining ~50 DWARF methods (including Construct @0x82791A48) are declared-only for shape
// coherence.
//
// ⭐⭐ WHY THIS MATTERS NOW, AND WHAT IT ACTUALLY COSTS (MEASURED 2026-08-25, crash-recovery
// wave). This class is the ONLY thing standing between a crashed car and a drivable one: the
// crash exit lands mbToBeResetOnTrack on the RaceCar, and everything below
// ActiveRaceCar::RequestPlaceOnTrack is already real and live. Counted from the ARTIST export
// set, the direct closure is 37 functions / 5,307 instructions (33 of them this class,
// ~4,750 insns) -- more than double the "~2500" the crash briefs have been carrying.
//
// ⛔⛔ BUT THE MANAGER IS NOT THE BLOCKER. It is an EMBEDDED MEMBER of AIModule at +286128 and
// its ONLY constructor call site is AIModule::Prepare @0x82798070 stage 3:
//     ResetOnTrackManager::Construct(module+286128, GetAISectionsData(), module+560)
// AIModule::{Construct,Prepare,Update,PostPhysicsUpdate,Release,Destruct} are ALL quiet boot-gate
// stubs in WorldLinkStubs.cpp today, and the live log says so on every run ("AIModule::Prepare:
// inert", "AIModule::Update: inert"). Consequences, all three measured:
//   * this object is NEVER Constructed -- mpAISectionData is null, mpaAICars is garbage.
//     Bodying methods on it is [[valid-pointer-invalid-object]]: no assert can see it.
//   * AIModule::Prepare stage 2 (LoadMapData @0x82795340, 167 insns) LoadBundle()s "AI.dat" and
//     requests HashString("WorldMapData") type 5 -- so THE AI ROAD NETWORK IS NEVER LOADED.
//     The DATA is fine: build/game/AI.DAT is present and already ported (bnd2 platform byte
//     @+8 == 4, 3.27 MB). The hole is entirely code.
//   * AIModuleIO::OutputBuffer is a 1-byte placeholder on the host, which is already why
//     WorldModule::BridgeAIToEntityModules_PrePhysics is PARKED -- and it is where the
//     ResetOnTrackResult ring has to live.
// ⇒ ORDER OF WORK: AIModule named members + lifecycle + the AIModuleIO buffer layouts (with
//   real Construct overrides -- un-gating a producer into an unconstructed queue is how the
//   crash-exit wave earned two access violations on the same day), THEN this class.
// Reference: scratchpad resetontrack_log.md. OFFSET AUTHORITY = the X360 asm of GetAICar; member NAMES/TYPES/ORDER = DWARF
// (references/DecFIGS/.../BrnResetOnTrackManager.h). Pinned layout:
//   mResetOnTrackRequestQueue @0x000  Array<ResetOnTrackRequest,35u>  (35*16 + s32 miCount
//                                     == 0x230 + 4 == 0x234)
//   <pad>                     @0x234  0xC bytes -> 16-align the ring at 0x240
//   mRecentResets             @0x240  FixedRingBuffer<RecentResetEntry,8> (mpData=this+0x260)
//   mpAISectionData           @0x360  ResourcePtr<AISectionsData> (0x20)
//   mpaAICars                 @0x380  AICar*                       (GetAICar base; stride 0x1560)
//   mePlayerGlobalRaceCarIndex@0x384  EGlobalRaceCarIndex
//   miResetCount              @0x388  s32
//   mCamera                   @0x38C  Camera (opaque, 0x164 -> ends 0x4F0)
//   mRandom                   @0x4F0  CgsNumeric::Random (0x30)
//   mHelperNodeNext/Prev      @0x520 / @0x530 RouteNode (16B each)
//   mResetOnTrackDebugComponent@0x540 opaque 0x330 -> footprint 0x870
//
// NOTE vs container namespaces: `Array<T,N>` is GLOBAL (CgsArray.h declares it with no
// namespace; CgsContainers::Array is only the DWARF mangling). FixedRingBuffer is in
// CgsContainers. RecentResetEntry (32B) is defined nested here matching the committed
// BrnResetOnTrackManagerTypes.h layout; this header intentionally does NOT include that
// types header, since C++ forbids re-opening the ResetOnTrackManager struct the types header
// partially declares -- the two headers own layout-identical RecentResetEntry definitions,
// mirroring the established placeholder-vs-real-home pattern elsewhere in the tree.

#include <cstddef>   // offsetof

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                               // EGlobalRaceCarIndex
#include "BrnCommonTypes.h"                                            // Vector3
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h" // AIModuleIO::ResetOnTrackRequest (16B)
#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"          // CgsContainers::FixedRingBuffer
#include "GameShared/GameClasses/Containers/CgsArray.h"               // Array<T,N> (global)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                 // CgsNumeric::Random (0x30)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"   // CgsResource::ResourcePtr

namespace BrnAI
{
    struct AICar;                 // pointer-only here (opaque, sizeof == 0x1560 == 5472)
    struct AISectionsData;        // resource type held by ResourcePtr
    struct AIModuleResultInterface;

    struct ResetOnTrackManager
    {
        // DWARF BrnResetOnTrackManager.h:78. 32-byte element. Layout-identical to the copy in
        // the committed BrnResetOnTrackManagerTypes.h (Vector3 mPosition@0 + f32 mfTime@0x10,
        // padded to 32 by the 16-byte align).
        struct alignas(16) RecentResetEntry
        {
            Vector3 mPosition; // +0x00
            f32     mfTime;    // +0x10
        };

        // ---- public API (only GetAICar bodied in this batch) -----------------------------
        void Construct(CgsResource::ResourcePtr<AISectionsData> lAISectionData, AICar* lpaAICars);
        void Update(AIModuleResultInterface* lpResults, EGlobalRaceCarIndex lePlayer, f32 lfTime);

        // @0x82769E88. Append a reset-on-track request to mResetOnTrackRequestQueue
        // (forwards to Array<ResetOnTrackRequest,35>::Append). Declared-only here; the body
        // lands with the ResetOnTrackManager TU. Called by BrnAI::BuzzBy::StartABuzzBy.
        void PushResetOnTrackRequest(const AIModuleIO::ResetOnTrackRequest* lpRequest);
        // ...remaining DWARF methods elided from this minimal home...

    private:
        AICar* GetAICar(EGlobalRaceCarIndex leGlobalRaceCarIndex);   // @0x82765878 (this batch)

    private:
        // +0x000 : 35 pending reset requests (16B each) + trailing s32 miCount@0x230.
        //          sizeof == 0x234. `Array` is the GLOBAL container (CgsArray.h).
        Array<AIModuleIO::ResetOnTrackRequest, 35u> mResetOnTrackRequestQueue;

        // +0x234 : align pad to the 16-aligned ring buffer at 0x240.
        u8 mPad0234[0x0C];

        // +0x240 : 8-deep recent-reset history ring (backing maData @+0x260..+0x360).
        CgsContainers::FixedRingBuffer<RecentResetEntry, 8> mRecentResets;

        // +0x360 : the AI section data the manager resets cars onto.
        CgsResource::ResourcePtr<AISectionsData> mpAISectionData;

        AICar*              mpaAICars;                  // +0x380  race-car array (GetAICar base)
        EGlobalRaceCarIndex mePlayerGlobalRaceCarIndex; // +0x384
        s32                 miResetCount;               // +0x388

        // +0x38C : scratch Camera used while computing reset coordinates (opaque, 0x164).
        u8 mCamera[0x164];

        // +0x4F0 : buffered PRNG for reset-position jitter.
        CgsNumeric::Random mRandom;

        // +0x520 / +0x530 : two scratch route nodes (16B each; full RouteNode in BrnRoute.h).
        u8 mHelperNodeNext[0x10];
        u8 mHelperNodePrev[0x10];

        // +0x540 : embedded debug component (opaque; footprint runs to 0x870).
        u8 mResetOnTrackDebugComponent[0x330];

    private:
        // ---- DWARF static perf-mon handles (BrnResetOnTrackManager.h:349-351). ----
        static s32 miInitialCoordinatesPM;   // "ROT, Initial coordinates"
        static s32 muAvoidHNGPM;             // "ROT, avoid obstacles"
        static s32 muTestLineHNGPM;          // "ROT, test HNG"

        // Pointer-free spine offset pin (never called; offsetof reaches the private member from
        // inside the class).
        static void _AssertLayout();
    };
}
