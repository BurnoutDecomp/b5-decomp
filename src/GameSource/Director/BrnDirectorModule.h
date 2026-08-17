#ifndef BRN_DIRECTOR_MODULE_H
#define BRN_DIRECTOR_MODULE_H

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"        // CgsModule::ModuleSingleBuffered base
#include "GameShared/GameClasses/Graphics/CgsCamera.h"                    // CgsGraphics::Camera (mCgsCamera)
#include "GameSource/Director/BrnDirectorICEWrapper.h"                    // BrnDirector::ICEWrapper (DirectorResourceManager needs the full type)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h" // BrnDirector::DebugComponent (mDebugComponent, real committed type)
#include "GameSource/Director/DirectorModule/BrnDirectorInputOutput.h"    // BrnDirector::DirectorInputOutput (the per-frame IO bundle)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOSceneQuery.h" // DirectorIO::SceneQuery{Input,Output}Buffer
#include "GameSource/Director/BrnDirectorResourceManager.h"               // BrnDirector::DirectorResourceManager (mDirectorResourceManager)
#include "GameSource/Director/BrnMainDirector.h"                          // BrnDirector::MainDirector (mMainDirector)
#include "GameSource/Director/Camera/Camera.h"                            // BrnDirector::Camera::Camera (mCamera)
#include "GameSource/Director/Utils/BrnDirectorWorldMap.h"                // BrnDirector::WorldMap (mWorldMap)
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"                   // BrnReplays::BaseSerialiser (mDirectorSerialiser)

namespace BrnResource { namespace GameDataIO { class AllocatorList; } }   // DirectorModule::Prepare's 2nd arg (boot audit F-P6-16)

// ============================================================================
// GameSource/Director/BrnDirectorModule.h
//
// BrnDirector::DirectorModule -- the top-level director engine module (a
// CgsModule::ModuleSingleBuffered derivative, as declared at the BrnGameModule member
// site). DWARF home: GameSource/Director/DirectorModule/BrnDirectorModule.h. It owns
// the debug component, the resource manager, the top-level MainDirector, a replay
// serialiser, the active Camera, plus a very large fleet of gameplay/camera/debug
// sub-objects (WorldMap, Arbitrator, MomentController, BehaviourManager, GameState,
// VehicleTracker, ...) that the 3 functions reconstructed in this wave never touch.
//
// LAYOUT MODEL -- NAMED MEMBERS AT ASM-PROVEN OFFSETS, PADDING FOR THE REST.
//
// This class was previously reconstructed (and reverted after fresh-eyes review) as one
// opaque `alignas(16) u8 maStorage[233000];` buffer accessed everywhere through raw
// reinterpret_cast offset pokes -- forbidden by AGENTS.md's "NO RAW OFFSET POINTER
// HACKS" rule for an in-memory engine object. This version instead follows the
// established BrnGuiCache.h / BrnWorldModule.h convention: every field the 3 bodied
// functions (Construct/Destruct/Release) actually touch is a NAMED member (its own real
// committed type where one exists -- DebugComponent, DirectorResourceManager,
// MainDirector, Camera::Camera, BaseSerialiser -- else a scalar/scratch buffer at the
// asm-proven byte offset), and every gap between named members the bodied functions
// never reach is an explicit `u8 mPad_0xNNNN[SIZE]` buffer. Per BrnWorldModule.h's
// precedent for the same "huge object, 3-of-many functions reconstructed" situation,
// this header does NOT attempt to model (or size-pin) the WHOLE ~0x38F00-byte object:
// only the touched members, in ascending asm-proven offset order, with padding filling
// the measured gaps between them. The untouched tail past the last function-touched
// field (miPerfCount_PostGuiUpdate @0x38B0C / meReleaseStage @0x38B14) is intentionally
// ABSENT from this slice -- the object's true total size is not attested by any function
// reconstructed so far, so no static_assert(sizeof(DirectorModule)==...) is made here
// (matching BrnWorldModule.h, which explicitly declines to pin a size it cannot prove).
//
// OFFSETS quoted in comments below are X360 CONSOLE (4-byte-pointer) byte offsets --
// provenance for the reconstructed .cpp bodies, not host layout facts (the real
// sub-objects such as DirectorResourceManager/MainDirector are wider on this 64-bit
// host; access is by name, never by offsetof-against-console-numbers).
//
// TWO UNREPRODUCED SIDE EFFECTS (flagged rather than raw-offset-poked into another
// class's private/opaque storage -- see the per-member comments below):
//   * Construct's inlined DirectorResourceManager::Construct() body (an
//     EventReceiverQueue<512,16>::Construct() per the DecFIGS DWARF dump for
//     BrnDirectorResourceManager.cpp) is called via mDirectorResourceManager.Construct(),
//     but the currently-committed BrnDirectorResourceManager.h version of that method is
//     `inline void Construct() {}` (a stub) -- the queue-init side effect is not
//     reproduced. That gap belongs to the resource-manager TU.
//   * Construct stores &mDebugComponent into a word at this+0xB40 -- MainDirector's own
//     CONSOLE +0x40 (the module places MainDirector at +0xB00). ⭐ RESOLVED (BehaviourManager
//     wave): MainDirector is no longer a console-sized opaque buffer -- it is a named-member
//     class -- and it now exposes `MainDirector::SetDebugComponent(DebugComponent*)`.
//     Construct should call `mMainDirector.SetDebugComponent(&mDebugComponent);`
//     (see the wave log's PART 4 step 2.5). The .cpp still has it unreproduced + flagged.
//
// NOTE: a SEPARATE, already-landed TU (GameSource/Director/BrnDirectorModule.cpp, the
// real `DirectorModule::DirectorModule()` constructor) defines its OWN small,
// TU-private/local `struct DirectorModule` and does not include this header -- the two
// coexist under this project's per-TU `cl /c` compile gate (no link step), exactly like
// every other declaration-only external stub in this codebase. Do not conflate that
// file's ctor with the Construct()/Destruct()/Release() staged-lifecycle methods
// reconstructed here.
// ----------------------------------------------------------------------------

namespace BrnDirector
{

class DirectorModule : public CgsModule::ModuleSingleBuffered
{
public:
    // ---- staged Prepare/Release stage enums (DWARF BrnDirectorModule.h:136 / :149) ----
    enum EPrepareStage
    {
        E_PREPARESTAGE_START                = 0,
        E_PREPARESTAGE_RESOURCES            = 1,
        E_PREPARESTAGE_ICE                  = 2,
        E_PREPARESTAGE_WORLDMAP             = 3,
        E_PREPARESTAGE_MANAGER              = 4,
        E_PREPARESTAGE_BEHAVIOUR_CONTROLLER = 5,
        E_PREPARESTAGE_MOMENT_CONTROLLER    = 6,
        E_PREPARESTAGE_ARBITRATOR           = 7,
        E_PREPARESTAGE_DONE                 = 8,
    };

    enum EReleaseStage
    {
        E_RELEASESTAGE_START                = 0,
        E_RELEASESTAGE_ARBITRATOR           = 1,
        E_RELEASESTAGE_MOMENT_CONTROLLER    = 2,
        E_RELEASESTAGE_BEHAVIOUR_CONTROLLER = 3,
        E_RELEASESTAGE_MANAGER              = 4,
        E_RELEASESTAGE_WORLDMAP             = 5,
        E_RELEASESTAGE_DONE                 = 6,
    };

    // ---- staged runtime lifecycle (NOT the C++ ctor -- see NOTE above) --------------

    // X360 0x8225C590 (EXECUTED in goal trace). Chains to the ModuleSingleBuffered base
    // Construct, builds the debug component, resource manager and camera, seeds the
    // prepare/release stage words + five handle/index sentinels, constructs the
    // top-level MainDirector and the director replay serialiser, and registers three
    // CgsDev::PerfMonCpu monitors (Pre SQ Update / Main Update / Post Gui Update).
    // Reconstructed in the .cpp (BODIED).
    void Construct(f32 lfTime);

    // X360 0x82239198 (recovered; not executed in goal trace). Staged RELEASE state
    // machine mirroring MainDirector::Release's shape: release the owned MainDirector,
    // then the ModuleSingleBuffered base, advancing the release-stage word at each step.
    // Reconstructed in the .cpp (BODIED) -- both callees (MainDirector::Release,
    // ModuleSingleBuffered::Release) are landed.
    bool Release();

    // X360 0x82250D98 (recovered; not executed in goal trace). Destructs the owned
    // MainDirector, then the ModuleSingleBuffered base. Reconstructed in the .cpp
    // (BODIED) -- both callees are landed.
    void Destruct();

    // ---- staged Prepare / per-frame Update spine (RECONSTRUCTED, director wave) -----
    //
    // The five entry points below are now BODIED (DirectorModule/BrnDirectorModule.cpp).
    // The earlier "BrnDirector::WorldMap has no homed type at all" note is SUPERSEDED:
    // WorldMap has a real home (Utils/BrnDirectorWorldMap.h/.cpp) and its LoadData
    // @0x8225F5A0 landed with this wave, as did BrnDirector::InertiaController::Update
    // @0x8221ECD0. What is still gated inside them is the REPLAY leg only
    // (BrnReplays::DirectorSerialiser and ReplayDirector::Update / ::PreSceneQueryUpdate
    // remain un-homed). Every gate is QUIET (never a trap on a per-frame path) and carries
    // its X360 address plus a DELETE-when note in the .cpp.
    //
    // ARGUMENT SHAPE. The IO buffers arrive as separate parameters, exactly as the X360
    // module framework passes them; each entry point then bundles four of them (plus the
    // module's own resource manager and world map) into the stack-local
    // DirectorInputOutput that every director sub-update consumes.

    // X360 0x822712D8. Staged PREPARE state machine: register the debug component, base
    // Prepare, DirectorResourceManager::Prepare, WorldMap::LoadData, MainDirector::Prepare.
    // Write-locks the output buffer for the whole call. BODIED.
    bool Prepare(DirectorIO::OutputBuffer* lpOutputBuffer,
                 const BrnResource::GameDataIO::AllocatorList* lpAllocatorList);

    // X360 0x8225C768. Pre-scene-query update: latch the replay flag, Clear the per-frame
    // scene-query post office, then run MainDirector::PreSceneQueryUpdate (live leg) or
    // ReplayDirector::PreSceneQueryUpdate (replay leg -- GATED). BODIED.
    s32 PreSceneQueryUpdate(s32 liUnusedA, s32 liUnusedB,
                            const DirectorIO::InputBuffer* lpInputBuffer,
                            DirectorIO::OutputBuffer* lpOutputBuffer,
                            DirectorIO::SceneQueryOutputBuffer* lpSceneQueryOutputBuffer,
                            bool lbIsReplaying);

    // X360 0x82275300. The per-frame Update spine: drain the scene-query results, run
    // MainDirector::Update (live leg) or ReplayDirector::Update (replay leg -- GATED), copy
    // the produced graphics camera into the module's own, then the replay-serialiser
    // bookkeeping + serialiser registration. BODIED.
    s32 Update(s32 liUnusedA, s32 liUnusedB,
               const DirectorIO::InputBuffer* lpInputBuffer,
               DirectorIO::OutputBuffer* lpOutputBuffer,
               const DirectorIO::SceneQueryInputBuffer* lpSceneQueryInputBuffer,
               DirectorIO::SceneQueryOutputBuffer* lpSceneQueryOutputBuffer);

    // X360 0x82250DD0. Post-GUI update: run MainDirector::PostGuiUpdate when not replaying,
    // then latch the input buffer's post-GUI car index. BODIED.
    s32 PostGuiUpdate(s32 liUnusedA, s32 liUnusedB,
                      const DirectorIO::InputBuffer* lpInputBuffer,
                      DirectorIO::OutputBuffer* lpOutputBuffer);

    // X360 0x82239278. Drain the scene-query RESULTS queue the scene manager published and
    // deliver each result to the post office that minted its query id. BODIED.
    void ProcessSceneQueryResults(const DirectorIO::SceneQueryInputBuffer* lpSceneQueryInputBuffer);

    // ADDITIVE accessor: the owned top-level director. The X360 reaches it as `this + 0xB00`
    // from inside this class's own bodies; this accessor is what lets the outside world raise
    // MainDirector::GetArbitrator().SetDoAttractMode(true) -- the single input that moves the
    // arbitrator onto the attract-mode (DJ fly-by) path -- without forming that offset.
    MainDirector& GetMainDirector() { return mMainDirector; }

    // ADDITIVE query: has the staged Prepare above finished? The console's module scheduler
    // only dispatches a module's per-frame entry points once its Prepare has reported true;
    // the PC drives the three passes by hand, so it needs to ask. (Driving Update before
    // Prepare's stage 7 has run means the arbitrator constructs behaviours out of a
    // BehaviourManager whose pools have not been carved -- BehaviourManager.h:782's
    // `lHelperID >= 0` is the assert that catches it.)
    // NOTE ON THE VALUE TESTED: Prepare's own terminal stage in THIS reconstruction is 5
    // (E_PREPARESTAGE_BEHAVIOUR_CONTROLLER) -- its case 5 stores 5 and is the only path that
    // returns true. The enum runs to E_PREPARESTAGE_DONE (8) because the console splits what
    // this body folds into a single MainDirector::Prepare call (which carries its OWN stage
    // machine for the manager / behaviour controller / moment controller / arbitrator). So the
    // test is against 5, not 8, and it is exact: no other path stores 5.
    // DELETE-WHEN: Prepare grows the remaining console stages -- then test E_PREPARESTAGE_DONE.
    bool IsPrepared() const { return mePrepareStage >= E_PREPARESTAGE_BEHAVIOUR_CONTROLLER; }

private:
    // ====================================================================
    //  DATA LAYOUT -- named members at asm-proven `this+offset` (X360 CONSOLE bytes),
    //  gaps reserved with explicit padding (AGENTS.md "LAYOUT RECOVERY WITH PADDING",
    //  the BrnGuiCache.h / BrnWorldModule.h convention). Only members the reconstructed
    //  Construct/Destruct/Release bodies touch are named; this is an intentionally
    //  MINIMAL SLICE of the real ~0x38F00-byte object (BrnWorldModule.h precedent) --
    //  the untouched fleet of gameplay sub-objects (WorldMap, Arbitrator,
    //  MomentController, BehaviourManager, GameState, VehicleTracker, the ~40 debug
    //  toggles, ...) is NOT modelled here and the object's true total size is NOT
    //  pinned by a static_assert.
    //
    //  this+0x000 / this+0x004: CgsModule::Module's own vptr + `mbIsNewModule` live in
    //  the inherited base -- NOT redeclared here. Construct's asm sets mbIsNewModule =
    //  true as its very last store; reached through the inherited protected member by
    //  name (no local field needed).
    // ====================================================================

    // NOTE ON PADDING SIZES BELOW: this class embeds several already-homed sub-object
    // types (DebugComponent/DirectorResourceManager/MainDirector/Camera::Camera/
    // BaseSerialiser) whose HOST (x64) sizeof is wider than their X360 CONSOLE size (the
    // widened embedded pointers -- exactly the situation AGENTS.md's x64-gate convention
    // expects: parity is by name, not by byte, and the compiler places each subsequent
    // named member sequentially regardless of the true host/console gap). The u8[N]
    // padding buffers below are sized from the CONSOLE byte deltas between asm-proven
    // offsets purely as ORGANISATIONAL documentation of "how much untouched material sits
    // here on the original console layout" -- they are not, and cannot be, claimed to
    // reproduce the true host byte gap (which is unrecoverable/inapplicable on x64).

    u8 mPad_0x008[0x228 - 0x008];                           // +0x008..+0x227 (untouched)

    // this+0x228 (552). DebugComponent::Construct(this+0x228, this) -- the module's
    // in-game debug menu component (registered as "Camera"). Real committed type.
    DebugComponent mDebugComponent;

    // this+0x248 (584). DirectorResourceManager region. MainDirector::Construct's own
    // declared signature (BrnMainDirector.h) takes `const DirectorResourceManager*`, and
    // the DirectorModule ctor (separate TU) places the resource manager at this same
    // console offset -- both independently corroborate it. See the header-level FLAG
    // above: the queue-init side effect the X360 binary performs here (inlined from
    // DirectorResourceManager::Construct) is not reproduced by the currently-committed
    // (stub) DirectorResourceManager::Construct().
    DirectorResourceManager mDirectorResourceManager;

    // ================= the +0x8A8..+0xAFF span, NOW RESOLVED (director wave) ==========
    // This whole span used to be one opaque `mPad_0x8A8[0xB00-0x8A8]`. The three per-frame
    // entry points (Prepare / PreSceneQueryUpdate / Update / PostGuiUpdate) resolve every
    // byte of it, and the sizes cross-check EXACTLY end to end -- see each member.

    // this+0x8A8 (2216). BrnDirector::WorldMap -- the director's queryable world map (safe
    // camera positions, lane topology, traffic/trigger/AI-section resources). Attested by
    // `WorldMap::LoadData(a1 + 2216, ...)` in Prepare @0x822712D8 and by slot 3 of the
    // DirectorInputOutput bundle in all three per-frame entry points. Real committed type.
    //
    // CROSS-CHECK: WorldMap's own CONSOLE span is 0xFC bytes (mpTrafficData @+0, +0x20,
    // +0x40 -- the three 32-byte ResourcePtrs -- mReceiverQueue @+0x60 = 24 base + 128
    // buffer, meLoadingState @+0xF8). 0x8A8 + 0xFC == 0x9A4, i.e. it ends EXACTLY where the
    // first scene-query post office begins. Independently corroborated by Construct's
    // inlined queue init at this+0x908 (== WorldMap +0x60: buffer ptr <- this+0x920,
    // capacity 128 @+0x10, alignment 16 @+0x14 -- the committed BaseEventReceiverQueue
    // field order, exactly).
    WorldMap mWorldMap;

    // this+0x9A4 .. +0xAFF: the SIX scene-query "post office" hand-off objects, one per
    // query kind. Each mints the SceneQueryId for its query kind and receives the matching
    // result back. Their ELEMENT TYPES are not recovered (the X360 delivery helpers are
    // IDA-truncated to e.g. `OutEventLineTestNearestResult_40_::`), so each is modelled as
    // correctly-SIZED opaque storage carrying its recovered role -- HONEST PLACEHOLDER, in
    // the BrnDirectorModuleIO.h house style. Their addresses (and only their addresses) are
    // what the module publishes into the per-frame BrnDirector::SceneQueryInterface.
    //
    // Sizes are next-minus-this. Every one of them is triple-attested:
    //   * the slot ORDER + clear-field offsets in SceneQueryInterface::Clear @0x8221CD38,
    //   * the per-result routing in ProcessSceneQueryResults @0x82239278,
    //   * and the ctor/Construct sentinel stores, each of which lands on the LAST word of
    //     its post office (+0x28 for the 44-byte ones, +0xA0 for the 164-byte one, +0x04
    //     for the 8-byte one) -- i.e. the "current query id" field. That the six sentinels
    //     (0x9CC / 0xA70 / 0xA9C / 0xAC8 / 0xAD0 / 0xAFC) fall exactly on those six words
    //     is what pins the whole partition.
    u8 mSceneQueryPostBoxA[0x9D0 - 0x9A4];   // +0x9A4 ( 44) result type 1; Clear -> sub_8221CC98
    u8 mPostBoxLineTestNearest[0xA74 - 0x9D0];   // +0x9D0 (164) result type 2; Clear zeroes +0xA0
    u8 mPostBoxLineTestFastDoubleSided[0xAA0 - 0xA74]; // +0xA74 ( 44) result type 3; Clear zeroes +0x28
    u8 mPostBoxSphereTestFast[0xACC - 0xAA0];    // +0xAA0 ( 44) result type 4; Clear zeroes +0x28
    u8 mPostBoxVolumeTestFine[0xAD4 - 0xACC];    // +0xACC (  8) result type 6; Clear zeroes +0x04
    u8 mPostBoxVolumeTestDeepest[0xB00 - 0xAD4]; // +0xAD4 ( 44) result type 5; Clear zeroes +0x28
    // ⚠️ NOTE the type-5/type-6 CROSSOVER: SceneQueryInterface::Clear walks the post offices
    // in DECLARATION order (…, VolumeTestFine @0xACC, VolumeTestDeepest @0xAD4) but
    // ProcessSceneQueryResults routes result type 5 to 0xAD4 and type 6 to 0xACC -- i.e. the
    // two are swapped relative to slot order. Reproduced verbatim; do not "fix" it.

    // this+0xB00 (2816). BrnDirector::MainDirector::Construct(&mDirectorResourceManager,
    // lfTime) / ::Destruct() / ::Release() -- the top-level cinematic camera director.
    // Real committed type (BrnMainDirector.h/.cpp). Its CONSOLE placement window is
    // this+0xB00 .. this+0x35F50 (0x35450 bytes), which is the evidence mCamera below sits at
    // CONSOLE +0x36380 overall.
    // ⚠️ NOTE (BehaviourManager wave): MainDirector no longer ASSERTS that console size --
    // it was rewritten from a console-sized opaque buffer into a named-member class with
    // host-native sub-object sizes (the console windows cannot host x64-width types; see that
    // header's LAYOUT MODEL banner). So sizeof(MainDirector) != 0x35450 here and the padding
    // below re-flows. Nothing depends on the absolute value: every member of this class is
    // named, and the console offsets quoted throughout are provenance only.
    MainDirector mMainDirector;

    u8 mPad_0x35F50[0x36380 - 0x35F50];                     // CONSOLE +0x35F50..+0x3637F.
    // ⚠️ THIS IS THE ReplayDirector REGION. The (separate) DirectorModule C++ ctor
    // placement-builds BrnDirector::ReplayDirector at exactly +0x35F50, and the replay leg
    // of Update / PreSceneQueryUpdate calls `ReplayDirector::{Update,PreSceneQueryUpdate}
    // (this + 221008)` == this+0x35F50, reading its own CgsGraphics::Camera back out at
    // this+221712 == +0x36190 (ReplayDirector +0x240).
    //
    // It is deliberately NOT declared as a named `ReplayDirector mReplayDirector;` member
    // here, because two attested facts CONFLICT and neither is safe to prefer:
    //   * Camera::Construct(this + 222080) in DirectorModule::Construct @0x8225C590 pins
    //     mCamera at +0x36380, i.e. only 0x430 bytes after the ReplayDirector base, yet
    //   * BrnReplayDirector.h records that the ReplayDirector ctor reaches fields out to
    //     its own +0x2788.
    // Declaring the member at 0x35F50 would silently pick one. Left as reserved storage +
    // documented until the ReplayDirector layout is recovered; the replay leg is gated
    // anyway (see the .cpp). Nothing in this span is touched by Construct/Destruct/Release.

    // this+0x36380 (222080). BrnDirector::Camera::Camera::Construct() -- the module's
    // active/current camera. Real committed type (Camera.h/.cpp).
    Camera::Camera mCamera;

    u8 mPad_0x36380_tail[0x38920 - 0x36380];                // CONSOLE tail after the Camera span
    // -- CameraDebugInfo / debug-toggle bytes / DebugLog + 3x DebugPrinter / GameState /
    // VehicleTracker / EffectInterface -- untouched by Construct/Destruct/Release.

    // this+0x38920 (231712). RECOVERED by PostGuiUpdate @0x82250DD0, whose last act is
    //   `v10 = lpInputBuffer[7854];  if ( v10 > -1 )  *(this + 231712) = v10;`
    // i.e. latch the input buffer's word at ITS +0x7AB8 whenever it is not the -1 "none"
    // sentinel.
    // ⚠ NAME CORRECTION (fly-by wave): the source word is NOT a car index. It is now the named
    // DirectorIO::InputBuffer::miCameraType -- the slot BridgeGuiToDirector @0x823CBF70 writes
    // on GUI command 591, whose own assert text is "Unhandled camera type : ". The member name
    // below is kept (renaming re-keys the lint fingerprint for no behavioural gain) but it
    // latches a CAMERA TYPE, not a car index. FLAG: name inferred, not DWARF.
    s32 miPostGuiCarIndexLatch;                              // +0x38920

    // this+0x38924 (231716). Construct zeroes it; PreSceneQueryUpdate @0x8225C768 SETS IT
    // TO 1 on the frame the replay flag transitions false->true (together with clearing
    // +0x38B01). So it is the "a replay just started" edge latch that the replay leg
    // consumes. Zero-store attested by Construct, the =1 store by PreSceneQueryUpdate.
    // FLAG: name inferred from that role; the trimmed DWARF does not name it.
    u8 mbReplayStartedThisFrame;                             // +0x38924

    u8 mPad_0x38925[0x38930 - 0x38925];                      // +0x38925..+0x3892F (untouched)

    // this+0x38930 (231728). BrnReplays::BaseSerialiser::Construct(this, 4, 0, 1024, 384,
    // "DirectorModule", 0) -- the director's own replay serialiser record (DWARF calls
    // the derived type BrnReplays::DirectorSerialiser; that derived type is not homed
    // yet, so this is declared at its real touched base type). Real committed base type.
    // Prepare/Update (declaration-only) reach the same field via
    // BrnReplays::DirectorSerialiser::GetStaticLayout(this+231728) -- same offset,
    // corroborating this placement.
    BrnReplays::BaseSerialiser mDirectorSerialiser;

    // this+0x3898C (231820): one zeroed byte immediately after mDirectorSerialiser's
    // BaseSerialiser-sized (0x5C CONSOLE bytes) tail -- a DirectorSerialiser-added field;
    // that derived type is not homed, so this is named as an explicit scratch byte rather
    // than folded into BaseSerialiser itself.
    u8 mDirectorSerialiserExtra_0x3898C;                     // +0x3898C

    u8 mPad_0x3898D[0x38990 - 0x3898D];                      // +0x3898D..+0x3898F (untouched)

    // this+0x38990 (231824). THE MODULE'S PUBLISHED GRAPHICS CAMERA. Update @0x82275300's
    // one unconditional camera act is
    //   CgsGraphics::Camera::operator=(this + 231824, <the director that ran this frame>)
    // taking MainDirector's own graphics camera (module +0x354D0 == MainDirector +0x349D0)
    // on the live leg, or the ReplayDirector's (module +0x36190) on the replay leg.
    //
    // CROSS-CHECK: CgsGraphics::Camera self-asserts sizeof == 0x170, and 0x38990 + 0x170 ==
    // 0x38B00 -- it ends EXACTLY on the replay flag pair below. That the recovered offset
    // and the committed type's own size close the gap to the byte is the attestation that
    // this member IS a CgsGraphics::Camera.
    CgsGraphics::Camera mCgsCamera;                          // +0x38990

    // this+0x38B00 / +0x38B01 (232192 / 232193). Both zeroed by Construct; both RECOVERED by
    // PreSceneQueryUpdate @0x8225C768 + Update @0x82275300:
    //   +0x38B00 is written with the incoming "is replaying" argument every
    //            PreSceneQueryUpdate, and is the branch key BOTH Update and PostGuiUpdate
    //            read to choose the live (MainDirector) vs replay (ReplayDirector) leg.
    //   +0x38B01 is cleared on the false->true replay edge and set when the director
    //            serialiser is in E_MODE_RESTORING (7).
    // FLAG: names inferred from those roles; the trimmed DWARF does not name either.
    bool mbIsReplaying;                                      // +0x38B00
    bool mbReplayRestoring;                                  // +0x38B01

    u8 mPad_0x38B02[0x38B04 - 0x38B02];                      // +0x38B02..+0x38B03 (untouched)

    // this+0x38B04 / +0x38B08 / +0x38B0C (232196 / 232200 / 232204). The three
    // CgsDev::PerfMonCpu monitor handles Construct registers (DWARF-named, adjacent,
    // ascending order -- BrnDirectorModule.h:329-331).
    s32 miPerfCount_PreSQUpdate;                             // +0x38B04
    s32 miPerfCount_MainUpdate;                              // +0x38B08
    s32 miPerfCount_PostGuiUpdate;                            // +0x38B0C

    // this+0x38B10 / +0x38B14 (232208 / 232212). The staged prepare/release stage words
    // (DWARF-named, adjacent, ascending order -- BrnDirectorModule.h:291-292). Construct
    // seeds mePrepareStage=E_PREPARESTAGE_START and meReleaseStage to the literal value 4
    // (asm-proven; NOT E_RELEASESTAGE_DONE=6 -- see the .cpp comment on Release for the
    // same literal-vs-enum-value note).
    EPrepareStage mePrepareStage;                            // +0x38B10
    EReleaseStage meReleaseStage;                             // +0x38B14

    // Everything past this point (the ~0x400-byte-per-instance debug/perf tail plus the
    // untouched gameplay-object fleet noted above) is intentionally ABSENT from this
    // minimal slice -- see the header-level note on why no total-size static_assert is
    // made.
};

}

#endif
