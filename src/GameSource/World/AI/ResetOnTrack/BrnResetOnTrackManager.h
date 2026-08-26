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
// ⚠️⚠️ THE PARAGRAPH BELOW IS 2026-08-25 HISTORY AND ITS HEADLINE IS NOW FALSE -- READ THIS
// FIRST. It said "THE MANAGER IS NOT THE BLOCKER -- the AI module does not run at all", and it
// was true on the day it was written. It stopped being true on 2026-08-26 (aimodule slice 1):
// AIModule::{Construct, Prepare, LoadMapData} are real bodies in GameSource/World/AI/
// BrnAIModule.cpp, AI.dat loads, "WorldMapData" resolves, and THIS OBJECT IS CONSTRUCTED against
// a bound road network -- measured on the boot log with a falsifying control
// (AISectionsData::muVersion reads 12 over 7639 sections / 3273824 B). AIModuleIO::OutputBuffer
// has a real member layout and a real Construct. AIModule::Construct now also builds the
// 35-entry AI-car array this manager indexes (2026-08-26, aicar_reset).
// ⭐⭐⭐ UPDATED 2026-08-26 (resetpump wave): THE FIRST TWO ITEMS BELOW ARE DONE AND THIS
// CLASS'S Update NOW HAS A CALLER. WriteUpdatedAIData and GenerateAboveGroundLineTests are
// both landed, AIModule::Update runs (a minimal-complete slice), and a real reset-on-track
// request has been resolved by ProcessResetOnTrackRequest on a booted run --
// `[rot] request resolved: car 0 type 1 -> FAILURE (consumer uses GetResetCoords) resetCount 1`
// -- after which RCEM::ProcessResetOnTrackResultQueue put the car back on the road.
// ⚠️ AND THE ANSWER IS STILL E_STATE_FAILURE ON EVERY REQUEST, exactly as this file's own
// pump banner predicts: every AICar is INACTIVE, so ComputeInitialCoordinatesStandard refuses at
// the console's own gate and the consumer falls back to the car's own ring. That is the console's
// designed fallback, and it is what recovers the car today.
// ⭐ WHAT IS ACTUALLY LEFT (2026-08-26, measured):
//     * [DONE] RaceCarEntityModule::WriteUpdatedAIData @0x822D1FC8 -- landed; mbPlayerDataSet is
//       set every frame and AIModule::Update's body runs.
//     * [LANDED, AND MEASURED TO CHANGE NOTHING YET] VehicleManager::GenerateAboveGroundLine-
//       Tests @0x82633990. The producer runs and posts the query; [collision-tag]
//       aboveGroundValid is STILL 0 on every sample, because the SceneManager fine-query
//       pipeline that would ANSWER it is stubbed in five places. So no car is in the AI section
//       system yet and this manager's ring stays empty -- see that function's own banner.
//     * the 28 geometry siblings (Scan*/Avoid*/Convert*/Test*HNG/...) are still absent; they are
//       parked at their own sites in the .cpp, each behind the console's own gate. They are what
//       would turn the FAILURE answers into SUCCESS answers (a road pose instead of the car's
//       own last pose) -- an improvement, not a blocker.
//
// ---- 2026-08-25, superseded: ----------------------------------------------------------------
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
// ---- end 2026-08-25 -------------------------------------------------------------------------
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
//   mResetOnTrackDebugComponent@0x540 opaque 0x870 -> footprint 0xDB0
//
// ⚠️⚠️ SIZE CORRECTED 2026-08-25 (aimodule wave) -- THE DEBUG COMPONENT WAS DECLARED
// 0x540 BYTES TOO SMALL, AND THE OLD COMMENT DESCRIBED THE MISTAKE. It read "opaque 0x330 ->
// footprint 0x870", i.e. it used 0x870 as the manager's END. The X360 Construct @0x82791A48
// settles it -- its LAST store is `*(this + 3488) = 60` (0xDA0), and between 1344 (0x540) and
// there it builds TWO ring buffers inside the component (mpData = this+1392 cap 16, and
// mpData = this+2704 cap 16) plus a 7-byte flag block at this+3480. So 0x870 is the
// COMPONENT'S OWN SIZE (1344 + 2160 == 3504 == 0xDB0) and 0xDB0 is the footprint. Everything
// below 0x540 cross-checks exactly (mCamera 0x38C + 0x164 == 0x4F0 == mRandom; mRandom + 0x30
// == 0x520; two 0x10 helper nodes == 0x540), which is what makes the last member the only one
// that could be wrong -- and it was.
// A faithful Construct against the old declaration would have written 1344 bytes past the end
// of the member, into the AIModule this object is EMBEDDED IN. Nothing had fired only because
// nothing had ever constructed it. Same family as the console-sized blob memcpy'd out of a
// differently-sized host type.
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
    // ⚠️ CORRECTED 2026-08-26 (aicar_reset wave). This was `struct AIModuleResultInterface;` at
    // BrnAI scope -- a DIFFERENT, never-defined type from the real one, which lives in the nested
    // AIModuleIO namespace (BrnAIModuleResultInterface.h). Nothing had noticed because Update was
    // declaration-only: the moment a body dereferenced the parameter it would have been an
    // incomplete-type error at best, and had a BrnAI::AIModuleResultInterface ever been defined,
    // an ODR fork at worst. [[shadowing redeclarations]] / [[ODR forks link silently]].
    namespace AIModuleIO { struct AIModuleResultInterface; }
    using AIModuleIO::AIModuleResultInterface;

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

        // The 48-byte out-parameter every Compute*/Reset* placement strategy fills.
        // ComputeInitialCoordinatesStandard @0x82783DD8 writes it as
        //     `*out = lpAISection`                      (stw  r26)
        //     `stvx128 v126, r26, 16`  -> the position
        //     `stvx128 v127, r26, 32`  -> the direction
        // and ComputeResetOnTrack passes the same 48-byte stack block to every strategy, so the
        // shape is the strategies' shared contract rather than one function's local. The section
        // pointer is opaque here (AISection's own home is the AI section-data TU, unmounted).
        struct alignas(16) ResetOnTrackCoords
        {
            const void* mpAISection;   // +0x00  (AISection*, opaque on this build)
            Vector3     mPosition;     // +0x10
            Vector3     mDirection;    // +0x20
        };

        // ---- public API -------------------------------------------------------------------
        // X360 0x82791A48. Called from EXACTLY ONE site in the whole image: AIModule::Prepare
        // @0x82798070 stage 3.
        void Construct(CgsResource::ResourcePtr<AISectionsData> lAISectionData, AICar* lpaAICars);

        // Has Construct bound a road network? (Raw test, no assert.)
        bool HasAISectionData() const { return mpAISectionData.HasMemoryResource(); }
        const AICar* GetAICarArray() const { return mpaAICars; }

        // ⭐⭐⭐ @0x8279A890. THE PUMP (aicar_reset wave 2026-08-26). Age the recent-reset ring,
        // drain the pending request queue into ProcessResetOnTrackRequest, then refresh every
        // ACTIVE car's reset-on-track section.
        //
        // ⚠️ THE CONSOLE PASSES A FOURTH ARGUMENT AND IT IS DROPPED HERE, DELIBERATELY.
        // AIModule::UpdateResetOnTrackManager @0x8279AC20 copy-constructs a stack Camera from
        // `module + 322048` and passes it in r7; the callee's first act is
        // `Camera::operator=(this + 0x38C, r7)` -- i.e. it fills mCamera, this class's SCRATCH
        // camera. r6 is never set at the call site: that is not a dropped argument, it is the GPR
        // slot the f1 float parameter burns on this ABI (the same trap
        // BrnRaceCarEntityModule_CrashExit.cpp:78 documents for RequestResetOnTrack).
        // mCamera is `u8[0x164]` here -- an opaque blob with no named interior -- and its ONLY
        // readers are the parked geometry arms (ComputeInitialCoordinates* / the debug component).
        // Copying a Camera into an untyped 356-byte hole to satisfy a parameter nothing live reads
        // is exactly the offset-poke this tree keeps paying for. Restore the parameter WITH the
        // Camera member's real type.
        void Update(AIModuleResultInterface* lpResults, EGlobalRaceCarIndex lePlayer, f32 lfTime);

        // @0x82799D38. Resolve ONE request: compute a reset pose, publish a ResetOnTrackResult
        // (SUCCESS with the pose, or FAILURE so the consumer falls back to the car's own
        // reset-coords ring), and remember it in mRecentResets.
        void ProcessResetOnTrackRequest(const AIModuleIO::ResetOnTrackRequest* lpRequest,
                                        AIModuleResultInterface* lpResults, f32 lfTime);

        // @0x82797D78. Dispatch on the request's reset type to one of seven placement
        // strategies, then run AvoidObstacles. Returns false when no pose could be found.
        bool ComputeResetOnTrack(ResetOnTrackCoords* lpOutCoords,
                                 const AIModuleIO::ResetOnTrackRequest* lpRequest);

        // @0x82783DD8. Reset type 1 (E_RESET_TYPE_STANDARD) -- the type the CRASH EXIT uses.
        bool ComputeInitialCoordinatesStandard(ResetOnTrackCoords* lpOutCoords,
                                               EGlobalRaceCarIndex leGlobalRaceCarIndex);

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

        // +0x540 : embedded debug component (opaque; 0x870 bytes -- the manager footprint
        // runs to 0xDB0). See the SIZE CORRECTED note in the banner: this was 0x330.
        u8 mResetOnTrackDebugComponent[0x870];

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
